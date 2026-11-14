#ifndef CHUNKDATABASE_HPP_
#define CHUNKDATABASE_HPP_

#include <functional>
#include <string>
#include <SFML/Network/Packet.hpp>
#include "common/core/IntTypes.hpp"

struct sqlite3;
class ChunkDatabase {
	public:
		ChunkDatabase(const std::string &path, bool readOnly);
		~ChunkDatabase();
		ChunkDatabase(const ChunkDatabase &) = delete;
		ChunkDatabase &operator=(const ChunkDatabase &) = delete;
		void begin();
		void commit();
		void writeChunk(u16 dimension, int x, int y, int z, const sf::Packet &packet);
		bool readChunk(u16 dimension, int x, int y, int z, sf::Packet &packet);
		void readChunks(u16 dimension, const std::function<void(int, int, int, sf::Packet &)> &read);
		void writeState(const sf::Packet &packet);
		sf::Packet readState();
		static bool isDatabase(const std::string &path);
	private:
		void execute(const char *sql);
		sqlite3 *m_database = nullptr;
		bool m_transaction = false;
};

#endif
