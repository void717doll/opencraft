#------------------------------------------------------------------------------
# Set project name
#------------------------------------------------------------------------------
set(PROJECT_NAME ${CMAKE_PROJECT_NAME}_common)

#------------------------------------------------------------------------------
# Get source files
#------------------------------------------------------------------------------
set(SOURCE_FILES
	core/ApplicationStateStack.cpp
	core/ArgumentParser.cpp
	core/BlockGeometry.cpp
	core/Color.cpp
	core/GameClock.cpp
	core/GameTime.cpp
	core/ISerializable.cpp
	core/Logger.cpp
	core/LoggerHandler.cpp
	core/LoggerUtils.cpp
	core/Registry.cpp
	core/TilesDef.cpp
	core/Timer.cpp
	core/Utils.cpp
	core/XMLFile.cpp
	core/input/InputHandler.cpp

	inventory/CraftingRecipe.cpp
	inventory/Inventory.cpp
	inventory/Item.cpp
	inventory/ItemStack.cpp
	inventory/SmeltingRecipe.cpp

	network/Network.cpp
	network/NetworkUtils.cpp

	resource/ResourceHandler.cpp

	scene/Scene.cpp
	scene/component/ComponentType.cpp
	scene/component/DrawableDef.cpp

	world/Biome.cpp
	world/Block.cpp
	world/BlockMetadata.cpp
	world/BlockParam.cpp
	world/BlockPlacementConstraints.cpp
	world/BlockState.cpp
	world/Chunk.cpp
	world/ChunkLightmap.cpp
	world/Dimension.cpp
	world/Heightmap.cpp
	world/Player.cpp
	world/Sky.cpp
	world/Tree.cpp
	world/World.cpp
)

set(HEADER_FILES
	core/ApplicationState.hpp
	core/ApplicationStateStack.hpp
	core/ArgumentParser.hpp
	core/BlockGeometry.hpp
	core/Box.hpp
	core/Color.hpp
	core/Debug.hpp
	core/EngineConfig.hpp
	core/EventHandler.hpp
	core/EventListenerList.hpp
	core/Exception.hpp
	core/GameClock.hpp
	core/GameTime.hpp
	core/HeartBarState.hpp
	core/ISerializable.hpp
	core/IntTypes.hpp
	core/LogStream.hpp
	core/Logger.hpp
	core/LoggerHandler.hpp
	core/LoggerUtils.hpp
	core/LuaWidget.hpp
	core/Platform.hpp
	core/Rect.hpp
	core/Registry.hpp
	core/TilesDef.hpp
	core/Timer.hpp
	core/Utils.hpp
	core/Vector2.hpp
	core/Vector3.hpp
	core/XMLFile.hpp
	core/input/GameKey.hpp
	core/input/InputHandler.hpp

	event/Events.hpp

	inventory/CraftingRecipe.hpp
	inventory/Inventory.hpp
	inventory/Item.hpp
	inventory/ItemStack.hpp
	inventory/ItemType.hpp
	inventory/Recipe.hpp
	inventory/SmeltingRecipe.hpp

	math/Math.hpp

	network/Network.hpp
	network/LocalSession.hpp
	network/NetworkUtils.hpp

	resource/IResourceLoader.hpp
	resource/ResourceHandler.hpp

	scene/EntityCallbackContainer.hpp
	scene/Scene.hpp
	scene/RetroMobMesh.hpp
	scene/component/AnimationComponent.hpp
	scene/component/ComponentType.hpp
	scene/component/DrawableDef.hpp
	scene/component/InventoryCubeDef.hpp
	scene/component/MobComponent.hpp
	scene/component/PhysicalComponent.hpp
	scene/component/FallingBlockComponent.hpp
	scene/component/NetworkComponent.hpp
	scene/component/PositionComponent.hpp
	scene/component/RotationComponent.hpp
	scene/controller/AbstractController.hpp

	utils/NonCopyable.hpp

	world/Biome.hpp
	world/Block.hpp
	world/BlockData.hpp
	world/BlockMetadata.hpp
	world/BlockParam.hpp
	world/BlockPlacementConstraints.hpp
	world/BlockState.hpp
	world/Chunk.hpp
	world/ChunkLightmap.hpp
	world/Dimension.hpp
	world/Heightmap.hpp
	world/RetroTerrain.hpp
	world/RetroRandom.hpp
	world/RetroClimate.hpp
	world/RetroCaves.hpp
	world/RetroNether.hpp
	world/RetroMobSpawn.hpp
	world/BetaMining.hpp
	world/BetaPlayerMovement.hpp
	world/BetaToolRules.hpp
	world/Key.hpp
	world/PlacementEntry.hpp
	world/Player.hpp
	world/Sky.hpp
	world/Tree.hpp
	world/World.hpp
	world/WorldCollision.hpp
	world/LunarchyCollision.hpp
	world/SurvivalMovement.hpp
	world/LiquidState.hpp
)

om_add_library(${PROJECT_NAME} "${SOURCE_FILES}" "${HEADER_FILES}")

# retain the save/message serializer without compiling socket transports.
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external/SFML/src/SFML/Network/Packet.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external/SFML/src)

if (WIN32)
	target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32)
endif ()


#------------------------------------------------------------------------------
# Dependencies
#------------------------------------------------------------------------------
if (UNIX)
	set (UNIX_LIBS -ldl -lpthread)
endif ()

target_link_libraries(${PROJECT_NAME} PUBLIC
	sfml-system
	sol2
	SDL2_image
	SDL2-static
	SDL2main
	tinyxml2
	glm_static
	libluajit
	zlibstatic
	EnTT
	${UNIX_LIBS}
)
