#include "hunt/HuntRegister.h"
#include <iostream>
#include <functional>
#include "monitor/EventManager.h"
#include "util/log/Log.h"
#include "user/bluespawn.h"
#include "scan/CollectDetections.h"
#include "common/Utils.h"

HuntRegister::HuntRegister(const IOBase& io) : io(io) {}

void HuntRegister::RegisterHunt(std::shared_ptr<Hunt> hunt) {
	vRegisteredHunts.emplace_back(hunt);
}

std::vector<Detection> HuntRegister::RunHunts(DWORD dwTactics, DWORD dwDataSource, DWORD dwAffectedThings, const Scope& scope){
	io.InformUser(L"Starting a hunt for " + std::to_wstring(vRegisteredHunts.size()) + L" techniques.");

	std::vector<Detection> detections{};
	for(auto name : vRegisteredHunts) {
		ADD_ALL_VECTOR(detections, name->RunHunt(scope));
	}

	io.InformUser(L"Successfully ran " + std::to_wstring(vRegisteredHunts.size()) + L" hunts.");
	io.InformUser(L"Preparing to scan " + std::to_wstring(detections.size()) + L" possible detections.");

	DetectionCollector collector{};
	for(auto& detection : detections){
		collector.AddDetection(detection);
	}

	detections = collector.GetAllDetections();
	for(auto& detection : detections){
		switch(detection->Type){
		case DetectionType::Event:
			Bluespawn::reaction.EventIdentified(static_pointer_cast<EVENT_DETECTION>(detection));
			break;
		case DetectionType::File:
			Bluespawn::reaction.FileIdentified(static_pointer_cast<FILE_DETECTION>(detection));
			break;
		case DetectionType::Process:
			Bluespawn::reaction.ProcessIdentified(static_pointer_cast<PROCESS_DETECTION>(detection));
			break;
		case DetectionType::Registry:
			Bluespawn::reaction.RegistryKeyIdentified(static_pointer_cast<REGISTRY_DETECTION>(detection));
			break;
		case DetectionType::Service:
			Bluespawn::reaction.ServiceIdentified(static_pointer_cast<SERVICE_DETECTION>(detection));
			break;
		case DetectionType::Other:
			Bluespawn::reaction.OtherIdentified(static_pointer_cast<OTHER_DETECTION>(detection));
			break;
		}
	}

	return detections;
}

std::vector<std::shared_ptr<DETECTION>> HuntRegister::RunHunt(Hunt& hunt, const Scope& scope){
	io.InformUser(L"Starting scan for " + hunt.GetName());
	int huntRunStatus = 0;

	std::vector<std::shared_ptr<DETECTION>> detections{ hunt.RunHunt(scope) };

	io.InformUser(L"Successfully scanned for " + hunt.GetName());

	return detections;
}

void HuntRegister::SetupMonitoring() {
	auto& EvtManager = EventManager::GetInstance();
	for (auto name : vRegisteredHunts) {
		io.InformUser(L"Setting up monitoring for " + name->GetName());
			for(auto event : name->GetMonitoringEvents()) {
				std::function<void()> callback{ std::bind(&Hunt::RunHunt, name.get(), Scope{}) };
				DWORD status = EvtManager.SubscribeToEvent(event, callback);
				if(status != ERROR_SUCCESS){
					LOG_ERROR(L"Monitoring for " << name->GetName() << L" failed with error code " << status);
				}
		}
	}
}