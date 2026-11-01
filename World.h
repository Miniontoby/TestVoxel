#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <sol/sol.hpp>
#include <memory>
#include <unordered_map>
#include <utility>
#include "Types.h"
#include "ChunkMesh.h" // Must be included before std::shared_ptr<ChunkMesh> is used

// Custom hash function for std::pair<int, int> chunk coordinates
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

class World {
public:
    World();
    ~World();

    void update(const glm::vec3& playerPos, sol::state& lua);
    void updateFluids();
    void render(GLuint shaderProgram) const;
    void renderBlockHighlight(const glm::ivec3& blockPos, const glm::mat4& view, const glm::mat4& proj) const;

    uint8_t getBlockAt(int x, int y, int z) const;
    void setBlockAt(int x, int y, int z, uint8_t blockId);

    uint8_t getMetadataAt(int x, int y, int z) const;
    void setMetadataAt(int x, int y, int z, uint8_t meta);

    void loadChunk(int chunkX, int chunkZ, sol::state& lua);

    std::unordered_map<std::pair<int, int>, std::shared_ptr<ChunkMesh>, PairHash> chunks;

private:
    void upload() const;

    void draw(GLuint shaderProgram) const;
};
