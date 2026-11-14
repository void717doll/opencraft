#ifndef ENTITYNETWORKPERFORMANCETESTS_HPP_
#define ENTITYNETWORKPERFORMANCETESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "common/core/Registry.hpp"
#include "common/network/LocalSession.hpp"
#include "common/scene/Scene.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/controller/NetworkController.hpp"
#include "server/world/WorldController.hpp"

class EntityNetworkPerformanceTests : public CxxTest::TestSuite {
	public:
		void testIdleEntitiesSendNothingAndChangedHitboxesStillReachClients() {
			Registry definitions;
			Registry::setInstance(definitions);
			ScriptEngine scripts;
			WorldController worlds{definitions};
			PlayerList players;
			Server server;
			server.init();
			auto session = Network::LocalSession::connect();
			ClientInfo observer{0,session};
			auto *player = players.connectPlayer("tester",observer); player->setReady(true);
			ServerCommandHandler commands{scripts, server, worlds, players, definitions};
			NetworkController controller;
			controller.setServer(&commands);
			Scene scene;
			auto &registry = scene.registry();
			auto entity = registry.create();
			registry.emplace<NetworkComponent>(entity, entity);
			registry.emplace<PositionComponent>(entity, 1, 2, 3, 0);
			registry.emplace<RotationComponent>(entity);
			registry.emplace<AnimationComponent>(entity);
			registry.emplace<DrawableDef>(entity);
			registry.emplace<DoubleBox>(entity, -0.3, -0.3, 0, 0.6, 0.6, 1.8);
			auto drain = [&]() {
				unsigned count = 0;
				Network::Packet packet;
				while (session->receiveOnClient(packet)) ++count;
				return count;
			};
			controller.update(registry);
			TS_ASSERT_EQUALS(drain(), 6);
			for (unsigned tick = 0; tick < 100; ++tick) controller.update(registry);
			TS_ASSERT_EQUALS(drain(), 0);
			auto &position = registry.get<PositionComponent>(entity);
			position.x = 4;
			position.isUpdated = true;
			registry.get<DoubleBox>(entity).sizeZ = 0.9;
			controller.update(registry);
			unsigned positions = 0, hitboxes = 0;
			Network::Packet packet;
			while (session->receiveOnClient(packet)) {
				Network::Command command;
				entt::entity received;
				packet >> command >> received;
				TS_ASSERT_EQUALS(received, entity);
				if (command == Network::Command::EntityPosition) {
					PositionComponent value; packet >> value;
					TS_ASSERT_EQUALS(value.x, 4);
					++positions;
				} else if (command == Network::Command::EntityHitbox) {
					DoubleBox value; packet >> value;
					TS_ASSERT_EQUALS(value.sizeZ, 0.9);
					++hitboxes;
				} else TS_FAIL("unexpected unchanged entity update");
			}
			TS_ASSERT_EQUALS(positions, 1);
			TS_ASSERT_EQUALS(hitboxes, 1);
			controller.update(registry);
			TS_ASSERT_EQUALS(drain(), 0);
			ClientInfo client{1, session};
			controller.sendEntities(registry, client);
			TS_ASSERT_EQUALS(drain(), 6);
		}
	void testOtherDimensionsCannotOverwriteOrDespawnTheCurrentEntity() {
		Registry definitions; Registry::setInstance(definitions);
		ScriptEngine scripts; WorldController worlds(definitions); PlayerList players;
		Server server; server.init(); auto session=Network::LocalSession::connect();
		ClientInfo client{0,session}; auto *player=players.connectPlayer("tester",client); player->setReady(true);
		ServerCommandHandler commands(scripts,server,worlds,players,definitions);
		NetworkController controller; controller.setServer(&commands);
		entt::registry current,other;
		auto a=current.create(), b=other.create(); TS_ASSERT_EQUALS(a,b);
		current.emplace<NetworkComponent>(a,a); current.emplace<PositionComponent>(a,1,2,3,0);
		other.emplace<NetworkComponent>(b,b); other.emplace<PositionComponent>(b,90,80,70,1);
		controller.update(current); controller.update(other); commands.sendEntityDespawn(b,1);
		Network::Packet packet; unsigned count=0;
		while(session->receiveOnClient(packet)) {
			Network::Command command; entt::entity id; packet >> command >> id; ++count;
			TS_ASSERT(command != Network::Command::EntityDespawn);
			if(command==Network::Command::EntityPosition) { PositionComponent position; packet >> position; TS_ASSERT_EQUALS(position.dimension,0); }
		}
		TS_ASSERT_EQUALS(count,2u);
		player->setDimension(1);
		controller.sendEntities(other,client);
		TS_ASSERT(session->receiveOnClient(packet)); TS_ASSERT(session->receiveOnClient(packet));
		Network::Command command; entt::entity id; PositionComponent position; packet >> command >> id >> position;
		TS_ASSERT_EQUALS(position.dimension,1); TS_ASSERT_EQUALS(position.x,90);
	}

};

#endif // ENTITYNETWORKPERFORMANCETESTS_HPP_
