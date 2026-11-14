#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <sqlite3.h>
#include <zstd.h>
#include "server/world/save/ChunkDatabase.hpp"

namespace {
class Statement {
	public:
		Statement(sqlite3 *database, const char *sql) : m_database(database) {
			if (sqlite3_prepare_v2(database, sql, -1, &m_statement, nullptr) != SQLITE_OK)
				throw std::runtime_error(sqlite3_errmsg(database));
		}
		~Statement() { sqlite3_finalize(m_statement); }
		operator sqlite3_stmt *() const { return m_statement; }
		int step() {
			int result = sqlite3_step(m_statement);
			if (result != SQLITE_ROW && result != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(m_database));
			return result;
		}
	private:
		sqlite3 *m_database;
		sqlite3_stmt *m_statement = nullptr;
};

std::vector<char> compress(const sf::Packet &packet) {
	std::vector<char> result(ZSTD_compressBound(packet.getDataSize()));
	size_t size = ZSTD_compress(result.data(), result.size(), packet.getData(), packet.getDataSize(), 3);
	if (ZSTD_isError(size)) throw std::runtime_error(ZSTD_getErrorName(size));
	result.resize(size);
	return result;
}

sf::Packet decompress(sqlite3_stmt *statement, int column) {
	const void *blob = sqlite3_column_blob(statement, column);
	size_t length = sqlite3_column_bytes(statement, column);
	auto size = ZSTD_getFrameContentSize(blob, length);
	if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN || size > 64 * 1024 * 1024)
		throw std::runtime_error("invalid compressed world record");
	std::vector<char> buffer(size);
	size_t decoded = ZSTD_decompress(buffer.data(), buffer.size(), blob, length);
	if (ZSTD_isError(decoded) || decoded != size) throw std::runtime_error("damaged compressed world record");
	sf::Packet packet;
	packet.append(buffer.data(), buffer.size());
	return packet;
}
}

ChunkDatabase::ChunkDatabase(const std::string &path, bool readOnly) {
	int flags = readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (sqlite3_open_v2(path.c_str(), &m_database, flags, nullptr) != SQLITE_OK) {
		std::string message = sqlite3_errmsg(m_database);
		sqlite3_close(m_database); m_database = nullptr;
		throw std::runtime_error(message);
	}
	try {
		sqlite3_busy_timeout(m_database, 5000);
		if (!readOnly) {
			execute("PRAGMA synchronous=FULL");
			execute("CREATE TABLE IF NOT EXISTS blocks (dimension INTEGER, x INTEGER, y INTEGER, z INTEGER, data BLOB NOT NULL, PRIMARY KEY(dimension,x,y,z)) WITHOUT ROWID");
			execute("CREATE TABLE IF NOT EXISTS world (id INTEGER PRIMARY KEY CHECK(id=1), data BLOB NOT NULL)");
		}
	} catch (...) { sqlite3_close(m_database); m_database = nullptr; throw; }
}

ChunkDatabase::~ChunkDatabase() {
	if (m_transaction) sqlite3_exec(m_database, "ROLLBACK", nullptr, nullptr, nullptr);
	sqlite3_close(m_database);
}

void ChunkDatabase::execute(const char *sql) {
	if (sqlite3_exec(m_database, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(m_database));
}
void ChunkDatabase::begin() { execute("BEGIN IMMEDIATE"); m_transaction = true; }
void ChunkDatabase::commit() { execute("COMMIT"); m_transaction = false; }

bool ChunkDatabase::isDatabase(const std::string &path) {
	std::ifstream file(path, std::ios::binary);
	char header[16]{};
	file.read(header, sizeof(header));
	return file.gcount() == 16 && std::memcmp(header, "SQLite format 3", 16) == 0;
}

void ChunkDatabase::writeChunk(u16 dimension, int x, int y, int z, const sf::Packet &packet) {
	Statement statement(m_database, "INSERT INTO blocks VALUES(?,?,?,?,?) ON CONFLICT(dimension,x,y,z) DO UPDATE SET data=excluded.data WHERE blocks.data<>excluded.data");
	sqlite3_bind_int(statement, 1, dimension); sqlite3_bind_int(statement, 2, x);
	sqlite3_bind_int(statement, 3, y); sqlite3_bind_int(statement, 4, z);
	auto buffer = compress(packet);
	if (sqlite3_bind_blob(statement, 5, buffer.data(), int(buffer.size()), SQLITE_TRANSIENT) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(m_database));
	statement.step();
}

bool ChunkDatabase::readChunk(u16 dimension, int x, int y, int z, sf::Packet &packet) {
	Statement statement(m_database, "SELECT data FROM blocks WHERE dimension=? AND x=? AND y=? AND z=?");
	sqlite3_bind_int(statement, 1, dimension); sqlite3_bind_int(statement, 2, x);
	sqlite3_bind_int(statement, 3, y); sqlite3_bind_int(statement, 4, z);
	if (statement.step() != SQLITE_ROW) return false;
	packet = decompress(statement, 0);
	return true;
}

void ChunkDatabase::readChunks(u16 dimension, const std::function<void(int, int, int, sf::Packet &)> &read) {
	Statement statement(m_database, "SELECT x,y,z,data FROM blocks WHERE dimension=? ORDER BY x,y,z");
	sqlite3_bind_int(statement, 1, dimension);
	while (statement.step() == SQLITE_ROW) {
		auto packet = decompress(statement, 3);
		read(sqlite3_column_int(statement, 0), sqlite3_column_int(statement, 1), sqlite3_column_int(statement, 2), packet);
	}
}

void ChunkDatabase::writeState(const sf::Packet &packet) {
	Statement statement(m_database, "INSERT INTO world VALUES(1,?) ON CONFLICT(id) DO UPDATE SET data=excluded.data");
	auto buffer = compress(packet);
	if (sqlite3_bind_blob(statement, 1, buffer.data(), int(buffer.size()), SQLITE_TRANSIENT) != SQLITE_OK)
		throw std::runtime_error(sqlite3_errmsg(m_database));
	statement.step();
}

sf::Packet ChunkDatabase::readState() {
	Statement statement(m_database, "SELECT data FROM world WHERE id=1");
	if (statement.step() != SQLITE_ROW) throw std::runtime_error("world metadata is missing");
	return decompress(statement, 0);
}
