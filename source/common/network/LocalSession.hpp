#ifndef LOCALSESSION_HPP_
#define LOCALSESSION_HPP_

#include <chrono>
#include <condition_variable>
#include <list>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

#include "common/network/Network.hpp"

namespace Network {
class LocalSession {
	public:
		static std::shared_ptr<LocalSession> create() {
			std::lock_guard<std::mutex> lock(s_mutex);
			if (s_session)
				s_session->close();
			s_session = std::make_shared<LocalSession>();
			return s_session;
		}

		static std::shared_ptr<LocalSession> connect() {
			std::lock_guard<std::mutex> registryLock(s_mutex);
			if (!s_session)
				return nullptr;
			std::lock_guard<std::mutex> lock(s_session->m_mutex);
			if (s_session->m_closed || s_session->m_hasClient)
				return nullptr;
			s_session->m_hasClient = true;
			return s_session;
		}

		bool sendToServer(const Packet &packet) { return send(m_toServer, packet); }
		bool sendToClient(const Packet &packet) { return send(m_toClient, packet); }

		bool receiveOnServer(Packet &packet) { return receive(m_toServer, packet); }
		bool receiveOnClient(Packet &packet) { return receive(m_toClient, packet); }

		bool waitOnClient(Packet &packet, std::chrono::milliseconds timeout) {
			std::unique_lock<std::mutex> lock(m_mutex);
			m_changed.wait_for(lock, timeout, [this] { return m_closed || !m_toClient.empty(); });
			return pop(m_toClient, packet);
		}

		void waitOnServer(std::chrono::milliseconds timeout) {
			std::unique_lock<std::mutex> lock(m_mutex);
			m_changed.wait_for(lock, timeout, [this] { return m_closed || !m_toServer.empty(); });
		}

		void close() {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_closed = true;
			}
			m_changed.notify_all();
		}

		bool isClosed() const {
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_closed;
		}

	private:
		struct Message { std::vector<unsigned char> bytes; std::string key; };
		struct Queue {
			std::list<Message> messages;
			std::unordered_map<std::string, std::list<Message>::iterator> latest;
			std::size_t bytes = 0;
			bool empty() const { return messages.empty(); }
		};

		static std::string stateKey(const Packet &packet) {
			if (packet.getDataSize() < 6) return {};
			const auto *bytes = static_cast<const unsigned char *>(packet.getData());
			auto command = Command((unsigned(bytes[0]) << 8) | bytes[1]);
			switch (command) {
				case Command::EntityPosition: case Command::EntityRotation:
				case Command::EntityAnimation: case Command::EntityHitbox:
					return std::string(reinterpret_cast<const char *>(bytes), 6);
				default: return {};
			}
		}

		bool send(Queue &queue, const Packet &packet) {
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_closed) return false;
			Command command = Command::ClientConnect;
			if (packet.getDataSize() >= 2) {
				const auto *bytes = static_cast<const unsigned char *>(packet.getData());
				command = Command((unsigned(bytes[0]) << 8) | bytes[1]);
			}
			if (&queue == &m_toServer && command == Command::ClientDisconnect) {
				m_disconnectRequested = true;
				m_changed.notify_all();
			}
			bool toClient = &queue == &m_toClient;
			if (toClient && m_disconnectRequested && command != Command::ServerClosed) return false;
			std::string key = stateKey(packet);
			auto existing = queue.latest.find(key);
			const auto *data = static_cast<const unsigned char *>(packet.getData());
			if (!key.empty() && existing != queue.latest.end()) {
				auto &message = *existing->second;
				queue.bytes -= message.bytes.size();
				message.bytes.assign(data, data + packet.getDataSize());
				queue.bytes += message.bytes.size();
				return true;
			}
			// reliable events remain ordered; slow consumers put pressure on producers.
			m_changed.wait(lock, [&] {
				return m_closed || (toClient && m_disconnectRequested) || queue.empty() || (queue.messages.size() < 4096
					&& queue.bytes + packet.getDataSize() <= 16 * 1024 * 1024);
			});
			if (m_closed) return false;
			// disconnect must wake blocked world updates, and its save acknowledgement must still pass.
			if (toClient && m_disconnectRequested && command != Command::ServerClosed) return false;
			if (key.empty()) queue.latest.clear();
			queue.messages.emplace_back();
			auto message = std::prev(queue.messages.end());
			message->key = key;
			if (packet.getDataSize()) message->bytes.assign(data, data + packet.getDataSize());
			queue.bytes += message->bytes.size();
			if (!key.empty()) queue.latest[key] = message;
			lock.unlock();
			m_changed.notify_all();
			return true;
		}

		bool receive(Queue &queue, Packet &packet) {
			std::lock_guard<std::mutex> lock(m_mutex);
			return pop(queue, packet);
		}

		bool pop(Queue &queue, Packet &packet) {
			if (queue.empty()) return false;
			auto message = queue.messages.begin();
			packet.clear();
			if (!message->bytes.empty()) packet.append(message->bytes.data(), message->bytes.size());
			queue.bytes -= message->bytes.size();
			auto latest = queue.latest.find(message->key);
			if (latest != queue.latest.end() && latest->second == message) queue.latest.erase(latest);
			queue.messages.erase(message);
			m_changed.notify_all();
			return true;
		}

		inline static std::mutex s_mutex;
		inline static std::shared_ptr<LocalSession> s_session;

		mutable std::mutex m_mutex;
		std::condition_variable m_changed;
		Queue m_toServer;
		Queue m_toClient;
		bool m_hasClient = false;
		bool m_closed = false;
		bool m_disconnectRequested = false;
};
}

#endif // LOCALSESSION_HPP_
