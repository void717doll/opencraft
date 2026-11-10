#------------------------------------------------------------------------------
# Set project name
#------------------------------------------------------------------------------
set(PROJECT_NAME ${CMAKE_PROJECT_NAME})

#------------------------------------------------------------------------------
# Get source files
#------------------------------------------------------------------------------
set(SOURCE_FILES
	audio/SoundManager.cpp
	core/ClientApplication.cpp
	core/ClientProfiler.cpp
	core/Config.cpp
	core/CoreApplication.cpp
	core/GameConfig.cpp
	core/KeyboardHandler.cpp
	core/Mouse.cpp
	core/SDLLoader.cpp
	core/Window.cpp
	core/WindowPlatformData.cpp
	core/input/GamePad.cpp

	graphics/BgfxCallback.cpp
	graphics/Camera.cpp
	graphics/CelestialObject.cpp
	graphics/ChunkRenderer.cpp
	graphics/Framebuffer.cpp
	graphics/Image.cpp
	graphics/NinePatch.cpp
	graphics/IndexBuffer.cpp
	graphics/ParticleRenderer.cpp
	graphics/MobModel.cpp
	graphics/ItemBillboard.cpp
	graphics/ItemMeshBuilder.cpp
	graphics/PlayerModel.cpp
	graphics/WieldRenderer.cpp
	graphics/RectangleShape.cpp
	graphics/RenderTarget.cpp
	graphics/Shader.cpp
	graphics/ShaderUniform.cpp
	graphics/Skybox.cpp
	graphics/Sprite.cpp
	graphics/SpriteAnimation.cpp
	graphics/Texture.cpp
	graphics/TextureAtlas.cpp
	graphics/Transform.cpp
	graphics/Transformable.cpp
	graphics/VertexBuffer.cpp
	graphics/View.cpp

	gui/AbstractInventoryWidget.cpp
	gui/CraftingWidget.cpp
	gui/Font.cpp
	gui/InventoryCube.cpp
	gui/InventoryWidget.cpp
	gui/ItemWidget.cpp
	gui/MenuWidget.cpp
	gui/MouseItemWidget.cpp
	gui/ProgressBarWidget.cpp
	gui/ScrollBarWidget.cpp
	gui/ScrollableList.cpp
	gui/SliderWidget.cpp
	gui/Text.cpp
	gui/TextButton.cpp
	gui/TextInput.cpp
	gui/Widget.cpp

	hud/BlockCursor.cpp
	hud/Crosshair.cpp
	hud/DebugLightmapViewer.cpp
	hud/DebugOverlay.cpp
	hud/DebugProfilerWindow.cpp
	hud/PaperMap.cpp
	hud/HUD.cpp
	hud/HeartBar.cpp
	hud/ArmorBar.cpp
	hud/Hotbar.cpp
	hud/Minimap.cpp

	main.cpp

	math/BlockCursorRaycast.cpp

	network/Client.cpp
	network/ClientCommandHandler.cpp

	resource/TextureLoader.cpp

	scene/ClientScene.cpp
	scene/controller/AnimationController.cpp
	scene/controller/RenderingController.cpp

	states/ConnectionErrorState.cpp
	states/GameState.cpp
	states/DeathState.cpp
	states/InterfaceState.cpp
	states/LuaGUIState.cpp
	states/PauseMenuState.cpp
	states/ServerLoadingState.cpp
	states/SignEditorState.cpp
	states/StatisticsState.cpp
	states/SettingsMenuState.cpp
	states/TitleScreenState.cpp
	states/WorldCreationState.cpp
	states/WorldDeletionState.cpp
	states/WorldSavingState.cpp
	states/WorldSelectionState.cpp

	world/ChunkMeshBuilder.cpp
	world/ClientChunk.cpp
	world/ClientPlayer.cpp
	world/ClientWorld.cpp
	world/mesh/BlockMesher.cpp
	world/mesh/LightUtils.cpp
	world/mesh/XShapeMesher.cpp
)

set(HEADER_FILES
	audio/SoundManager.hpp
	core/ClientApplication.hpp
	core/ClientProfiler.hpp
	core/Config.hpp
	core/CoreApplication.hpp
	core/GameConfig.hpp
	core/KeyboardHandler.hpp
	core/Mouse.hpp
	core/SDLLoader.hpp
	core/Window.hpp
	core/input/GamePad.hpp

	graphics/BgfxCallback.hpp
	graphics/BgfxView.hpp
	graphics/Camera.hpp
	graphics/CelestialObject.hpp
	graphics/ChunkRenderer.hpp
	graphics/Drawable.hpp
	graphics/Framebuffer.hpp
	graphics/Image.hpp
	graphics/IndexBuffer.hpp
	graphics/ParticleRenderer.hpp
	graphics/MobModel.hpp
	graphics/ItemBillboard.hpp
	graphics/WieldRenderer.hpp
	graphics/RectangleShape.hpp
	graphics/RenderStates.hpp
	graphics/RenderTarget.hpp
	graphics/Shader.hpp
	graphics/ShaderUniform.hpp
	graphics/Skybox.hpp
	graphics/Sprite.hpp
	graphics/SpriteAnimation.hpp
	graphics/Texture.hpp
	graphics/TextureAtlas.hpp
	graphics/Transform.hpp
	graphics/Transformable.hpp
	graphics/Vertex.hpp
	graphics/VertexBuffer.hpp
	graphics/View.hpp

	gui/AbstractInventoryWidget.hpp
	gui/CraftingWidget.hpp
	gui/Font.hpp
	gui/InventoryCube.hpp
	gui/InventoryWidget.hpp
	gui/ItemWidget.hpp
	gui/MenuWidget.hpp
	gui/MouseItemWidget.hpp
	gui/ProgressBarWidget.hpp
	gui/ScrollBarWidget.hpp
	gui/ScrollableList.hpp
	gui/SliderWidget.hpp
	gui/Text.hpp
	gui/TextButton.hpp
	gui/TextInput.hpp
	gui/Widget.hpp

	hud/BlockCursor.hpp
	hud/Crosshair.hpp
	hud/DebugLightmapViewer.hpp
	hud/DebugOverlay.hpp
	hud/DebugProfilerWindow.hpp
	hud/HUD.hpp
	hud/HeartBar.hpp
	hud/ArmorBar.hpp
	hud/Hotbar.hpp
	hud/Minimap.hpp

	math/BlockCursorRaycast.hpp

	network/Client.hpp
	network/ClientCommandHandler.hpp

	resource/TextureLoader.hpp

	scene/ClientScene.hpp
	scene/component/DrawableComponent.hpp
	scene/controller/AbstractClientController.hpp
	scene/controller/AnimationController.hpp
	scene/controller/RenderingController.hpp

	states/ConnectionErrorState.hpp
	states/DrawableState.hpp
	states/GameState.hpp
	states/DeathState.hpp
	states/InterfaceState.hpp
	states/LuaGUIState.hpp
	states/PauseMenuState.hpp
	states/ServerLoadingState.hpp
	states/StatisticsState.hpp
	states/SettingsMenuState.hpp
	states/TitleScreenState.hpp
	states/WorldCreationState.hpp
	states/WorldDeletionState.hpp
	states/WorldSavingState.hpp
	states/WorldSelectionState.hpp

	world/ChunkMeshBuilder.hpp
	world/ChunkMeshLayer.hpp
	world/ClientChunk.hpp
	world/ClientPlayer.hpp
	world/ClientWorld.hpp
	world/mesh/BlockMesher.hpp
	world/mesh/ChunkMeshBuildingJob.hpp
	world/mesh/LightUtils.hpp
	world/mesh/XShapeMesher.hpp
)

#------------------------------------------------------------------------------
# Add executables
#------------------------------------------------------------------------------
om_add_executable(${PROJECT_NAME} "${HEADER_FILES}" "${SOURCE_FILES}")

#------------------------------------------------------------------------------
# Link options
#------------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
	${CMAKE_PROJECT_NAME}_server_lib

	bgfx
	bimg
	bx
)

add_dependencies(${PROJECT_NAME} shaders)
