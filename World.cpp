#include "World.h"
#include "ChunkMesh.h"
#include "Block.h"
#include "Types.h"
#include "CaveGenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <mutex>
#include <future>
#include <random>

static std::mutex g_WorldLuaMutex;
static unsigned int g_WorldSeed = 1337;

World::World() {
    std::random_device rd;
    g_WorldSeed = rd() ^ static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::cout << "[World] Initialized with random seed: " << g_WorldSeed << std::endl;
}

World::~World() {
    chunks.clear();
}

void World::update(const glm::vec3& playerPos, sol::state& lua) {
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_WIDTH));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_WIDTH));

    int renderDistance = 3;
    int chunksLoadedThisFrame = 0;
    const int maxChunksLoadPerFrame = 2;

    for (int x = -renderDistance; x <= renderDistance; ++x) {
        for (int z = -renderDistance; z <= renderDistance; ++z) {
            int cx = playerChunkX + x;
            int cz = playerChunkZ + z;

            std::pair<int, int> coord = {cx, cz};
            if (chunks.find(coord) == chunks.end()) {
                if (chunksLoadedThisFrame < maxChunksLoadPerFrame) {
                    loadChunk(cx, cz, lua);
                    chunksLoadedThisFrame++;
                }
            }
        }
    }
}

void World::updateFluids() {
    struct FluidChange {
        int x, y, z;
        uint8_t id;
        uint8_t meta;
    };
    std::vector<FluidChange> pendingChanges;
    int maxUpdates = 100;

    uint8_t waterSourceId = BlockRegistry::getInstance().getBlockId("water");
    uint8_t flowingWaterId = BlockRegistry::getInstance().getBlockId("flowing_water");
    if (flowingWaterId == 0) flowingWaterId = waterSourceId;

    for (const auto& pair : chunks) {
        if (pendingChanges.size() >= static_cast<size_t>(maxUpdates)) break;
        int cx = pair.first.first;
        int cz = pair.first.second;
        auto& chunk = pair.second;

        for (int lx = 0; lx < CHUNK_WIDTH; ++lx) {
            for (int lz = 0; lz < CHUNK_WIDTH; ++lz) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    uint8_t currentId = chunk->getBlock(lx, y, lz);
                    if (currentId == 0) continue;

                    const auto& prop = BlockRegistry::getInstance().getBlock(currentId);
                    bool isWater = (prop.name == "water" || prop.name == "flowing_water");

                    if (isWater) {
                        int wx = (cx * CHUNK_WIDTH) + lx;
                        int wz = (cz * CHUNK_WIDTH) + lz;

                        if (y > 0 && getBlockAt(wx, y - 1, wz) == 0) {
                            pendingChanges.push_back({wx, y - 1, wz, flowingWaterId, 0});
                            continue;
                        }
                    }
                }
            }
        }
    }

    if (!pendingChanges.empty()) {
        for (const auto& change : pendingChanges) {
            int cx = static_cast<int>(std::floor(static_cast<float>(change.x) / CHUNK_WIDTH));
            int cz = static_cast<int>(std::floor(static_cast<float>(change.z) / CHUNK_WIDTH));
            auto it = chunks.find({cx, cz});
            if (it != chunks.end()) {
                int lx = change.x - (cx * CHUNK_WIDTH);
                int lz = change.z - (cz * CHUNK_WIDTH);
                if (lx < 0) lx += CHUNK_WIDTH;
                if (lz < 0) lz += CHUNK_WIDTH;

                it->second->setBlockWithMeta(lx, change.y, lz, change.id, change.meta);
                std::async(std::launch::async, [chunk = it->second]() {
                    chunk->generateMeshData();
                });
            }
        }
    }
}

void World::loadChunk(int chunkX, int chunkZ, sol::state& lua) {
    auto chunk = std::make_shared<ChunkMesh>();

    {
        std::lock_guard<std::mutex> lock(g_WorldLuaMutex);
        sol::protected_function genFunc = lua["generate_chunk"];
        if (genFunc.valid()) {
            sol::protected_function setSeedFunc = lua["set_world_seed"];
            if (setSeedFunc.valid()) {
                setSeedFunc(g_WorldSeed);
            }

            sol::protected_function_result result = genFunc(chunk.get(), chunkX, chunkZ);
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[Mapgen Error]: " << err.what() << std::endl;
            }
        } else {
            uint8_t stoneId = BlockRegistry::getInstance().getBlockId("stone");
            if (stoneId == 0) stoneId = 2;
            uint8_t grassId = BlockRegistry::getInstance().getBlockId("grass");
            if (grassId == 0) grassId = 3;
            uint8_t dirtId = BlockRegistry::getInstance().getBlockId("dirt");
            if (dirtId == 0) dirtId = 1;

            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                for (int z = 0; z < CHUNK_WIDTH; ++z) {
                    int worldX = (chunkX * CHUNK_WIDTH) + x;
                    int worldZ = (chunkZ * CHUNK_WIDTH) + z;

                    float hVal = std::sin(worldX * 0.1f + g_WorldSeed) * std::cos(worldZ * 0.1f + g_WorldSeed) * 5.0f;
                    int surfaceHeight = 55 + static_cast<int>(hVal);

                    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                        if (y <= surfaceHeight) {
                            if (CaveGenerator::isCarvedOut(worldX, y, worldZ)) {
                                chunk->setBlock(x, y, z, 0);
                            } else if (y == surfaceHeight) {
                                chunk->setBlock(x, y, z, grassId);
                            } else if (y > surfaceHeight - 4) {
                                chunk->setBlock(x, y, z, (dirtId != 0 ? dirtId : stoneId));
                            } else {
                                chunk->setBlock(x, y, z, stoneId);
                            }
                        }
                    }
                }
            }
        }
    }

    chunks[{chunkX, chunkZ}] = chunk;

    std::async(std::launch::async, [chunk]() {
        chunk->generateMeshData();
    });
}

void World::render(GLuint shaderProgram) const {
    this->upload();
    this->draw(shaderProgram);
}


void World::upload() const
{
    auto startTime = std::chrono::high_resolution_clock::now();
    constexpr double maxUploadTimeMs = 5.0;

    for ([[maybe_unused]] const auto& [_, chunk] : this->chunks) {
        if (chunk->meshReadyForUpload && chunk->isDirty) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(currentTime - startTime).count();

            if (elapsedMs < maxUploadTimeMs) {
                chunk->uploadMesh();
            }
        }
    }
}

void World::draw(GLuint shaderProgram) const
{
    for (const auto& [loc, chunk] : this->chunks) {

        if (chunk->opaqueVertexCount > 0 && chunk->opaqueVAO != 0) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(
                loc.first * CHUNK_WIDTH,
                0.0f,
                loc.second * CHUNK_WIDTH
            ));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            chunk->drawOpaque();
        }

        if (chunk->transparentVertexCount > 0 && chunk->transparentVAO != 0) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(
                loc.first * CHUNK_WIDTH,
                0.0f,
                loc.second * CHUNK_WIDTH
            ));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            chunk->drawTransparent();
        }
    }
}

void World::renderBlockHighlight(const glm::ivec3& blockPos, const glm::mat4& view, const glm::mat4& proj) const {
}

uint8_t World::getBlockAt(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;

    int cx = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_WIDTH));
    int cz = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_WIDTH));

    auto& mutableChunks = const_cast<std::unordered_map<std::pair<int, int>, std::shared_ptr<ChunkMesh>, PairHash>&>(chunks);

    auto it = mutableChunks.find({cx, cz});
    if (it == mutableChunks.end()) {
        return 0;
    }

    int lx = x - cx * CHUNK_WIDTH;
    int lz = z - cz * CHUNK_WIDTH;
    if (lx < 0) lx += CHUNK_WIDTH;
    if (lz < 0) lz += CHUNK_WIDTH;
    if (lx >= CHUNK_WIDTH) lx -= CHUNK_WIDTH;
    if (lz >= CHUNK_WIDTH) lz -= CHUNK_WIDTH;

    return it->second->getBlock(lx, y, lz);
}

void World::setBlockAt(int x, int y, int z, uint8_t blockId) {
    int cx = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_WIDTH));
    int cz = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_WIDTH));

    auto it = chunks.find({cx, cz});
    if (it != chunks.end()) {
        int lx = x - (cx * CHUNK_WIDTH);
        int lz = z - (cz * CHUNK_WIDTH);
        if (lx < 0) lx += CHUNK_WIDTH;
        if (lz < 0) lz += CHUNK_WIDTH;

        it->second->setBlock(lx, y, lz, blockId);
        std::async(std::launch::async, [chunk = it->second]() {
            chunk->generateMeshData();
        });
    }
}

uint8_t World::getMetadataAt(int x, int y, int z) const {
    int cx = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_WIDTH));
    int cz = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_WIDTH));

    auto it = chunks.find({cx, cz});
    if (it != chunks.end()) {
        int lx = x - (cx * CHUNK_WIDTH);
        int lz = z - (cz * CHUNK_WIDTH);
        if (lx < 0) lx += CHUNK_WIDTH;
        if (lz < 0) lz += CHUNK_WIDTH;
        return it->second->getMetadata(lx, y, lz);
    }
    return 0;
}

void World::setMetadataAt(int x, int y, int z, uint8_t meta) {
    int cx = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_WIDTH));
    int cz = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_WIDTH));

    auto it = chunks.find({cx, cz});
    if (it != chunks.end()) {
        int lx = x - (cx * CHUNK_WIDTH);
        int lz = z - (cz * CHUNK_WIDTH);
        if (lx < 0) lx += CHUNK_WIDTH;
        if (lz < 0) lz += CHUNK_WIDTH;

        it->second->setMetadata(lx, y, lz, meta);
        std::async(std::launch::async, [chunk = it->second]() {
            chunk->generateMeshData();
        });
    }
}
