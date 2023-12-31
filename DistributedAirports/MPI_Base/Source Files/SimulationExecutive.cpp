#include <iostream>
#include "SimulationExecutive.h"
#include "Communication.h"

using namespace std;
#define COLOR_RED 2
#define COLOR_GREEN 3
#define START_GVT 4
#define COMPUTE_GVT 5

class SimulationExecutive
{
public:
	static void InitializeSimulation()
	{
		_simTime = 0.0;
		_sendArray = new int[CommunicationSize()];

		for(int i = 0; i < CommunicationSize(); i++)
		{
			_sendArray[i] = 0;
		}
	}

	static Time GetSimulationTime() { return _simTime; }
	static void RunSimulation()
	{
		_terminationMessagesReceived = 0;
		while (_eventList.HasEvent()) {
			Event* e = _eventList.GetEvent();

			if (e->_time < _simTime) {
				_events_executed_OoO++;
			}

			_simTime = e->_time;
			e->_ea->Execute();
			delete e;

			CheckAndProcessMessages();
		}

		std::cout << CommunicationRank() << ": Events executed out of order: " << _events_executed_OoO << std::endl;

		// "Hey im done over here!"
		//BroadcastTerminationMessage();

		// Wait for everyone else to finish
		//TerminationLoop();
	}

	static void RunSimulation(Time endTime)
	{
		_terminationMessagesReceived = 0;
		while (_eventList.HasEvent() && _simTime <= endTime) {
			Event* e = _eventList.GetEvent();

			if (e->_time < _simTime) {
				_events_executed_OoO++;
			}

			_simTime = e->_time;
			if (_simTime <= endTime) {
				e->_ea->Execute();
			}
			delete e;

			CheckAndProcessMessages();
		}

		std::cout << CommunicationRank() << ": Events executed out of order: " << _events_executed_OoO << std::endl;

		// "Hey im done over here!"
		//BroadcastTerminationMessage();

		// Wait for everyone else to finish
		//TerminationLoop();
	}

	static void ScheduleEventIn(Time delta, EventAction *ea)
	{
		_eventList.AddEvent(_simTime + delta, ea);
	}

	static void ScheduleEventAt(Time time, EventAction* ea)
	{
		_eventList.AddEvent(time, ea);
	}

	// NEW FUNCTIONALITY FOR THE SIMULATION EXECUTIVE FOR THIS
	// -----------------------------------------------
	static void RegisterMsgHandler(std::function<void(int)> msgHandler)
	{
		_msgHandler = msgHandler;
	}

	static void CaughtMsg(int source)
	{
		_msgHandler(source);
	}

	static void CheckAndProcessMessages()
	{
		int tag;
		int source;
		while (CheckForComm(tag, source)) {
			CaughtMsg(source);
		}
	}

	static void TerminationLoop()
	{
		std::cout << CommunicationRank() << ": Waiting for termination messages..." << std::endl;

		bool done = false;
		while (!done) {

			int tag;
			int source;
			while (!CheckForComm(tag, source))
			{
				if (tag == 0) {
					_terminationMessagesReceived++;
					std::cout << CommunicationRank() << ": Termination message received from " << source << std::endl;
					std::cout << "Number of termination messages: " << _terminationMessagesReceived << std::endl;
				}
			}

			if (_terminationMessagesReceived == CommunicationSize() - 1) {
				done = true;
			}
		}
	}

	void StartGVT(int* sendBuffer)
	{
		// Send a message to everyone to start GVT
		for (int i = 0; i < CommunicationSize(); i++) {
			if (i != CommunicationRank()) {
				// Send a message to everyone to start GVT
				int tag = START_GVT;

				_sendArray[i]++;

				int* data_buffer = (int*)malloc(sizeof(double) + sizeof(int) * CommunicationSize());
				memcpy(data_buffer, &min(_minRedTS, _simTime), sizeof(double));
				memcpy(data_buffer + 1, &_sendArray[0], sizeof(int) * CommunicationSize());

				SendMsg(i, tag, data_buffer);

				free(data_buffer);
			}
		}
	}

	bool ComputeGVT()
	{
		// Send a message to everyone to compute GVT
		for (int i = 0; i < CommunicationSize(); i++) {
			if (i != CommunicationRank()) {
				// Send a message to everyone to compute GVT
				int tag = COMPUTE_GVT;

				_sendArray[i]++;

				int* data_buffer = (int*)malloc(sizeof(double) + sizeof(int) * CommunicationSize());
				memcpy(data_buffer, &min(_minRedTS, _simTime), sizeof(double));
				memcpy(data_buffer + 1, &_sendArray[0], sizeof(int) * CommunicationSize());

				SendMsg(i, tag, data_buffer);

				free(data_buffer);
			}
		}

		// Wait for everyone else to finish
		TerminationLoop();

		return true;

	}
	// -----------------------------------------------

private:
	struct Event
	{
		Event(Time time, EventAction* ea)
		{
			_time = time;
			_ea = ea;
			_nextEvent = 0;
		}
		Time _time;
		EventAction* _ea;
		Event *_nextEvent;
	};

	class EventList
	{
	public:
		EventList()
		{
			_eventList = 0;
		}

		void AddEvent(Time time, EventAction*ea)
		{
			Event *e = new Event(time, ea);
			if (_eventList == 0) {
				//event list empty
				_eventList = e;
			}
			else if (time < _eventList->_time) {
				//goes at the head of the list
				e->_nextEvent = _eventList;
				_eventList = e;
			}
			else {
				//search for where to put the event
				Event *curr = _eventList;
				while ((curr->_nextEvent != 0) ? (e->_time >= curr->_nextEvent->_time) : false) {
					curr = curr->_nextEvent;
				}
				if (curr->_nextEvent == 0) {
					//goes at the end of the list
					curr->_nextEvent = e;
				}
				else {
					e->_nextEvent = curr->_nextEvent;
					curr->_nextEvent = e;
				}
			}
		}

		Event* GetEvent()
		{
			Event *next = _eventList;
			_eventList = _eventList->_nextEvent;
			return next;
		}

		bool HasEvent()
		{
			return _eventList != 0;
		}

	private:
		Event *_eventList;
	};

	static EventList _eventList;
	static Time _simTime;
	static int _events_executed_OoO;
	static int _terminationMessagesReceived; // Comm_World_Size - 1 stopping condition

	static int* _sendArray;
	static int _msgsReceived;
	static Time _minRedTS;
	static int _currentMsgColor;
	static bool _computeGVT;

	static std::function<void(int)> _msgHandler;
};

SimulationExecutive::EventList SimulationExecutive::_eventList;
Time SimulationExecutive::_simTime = 0.0;
int SimulationExecutive::_events_executed_OoO = 0;
int SimulationExecutive::_terminationMessagesReceived = 0;
int* SimulationExecutive::_sendArray = 0;
int SimulationExecutive::_msgsReceived = 0;
Time SimulationExecutive::_minRedTS = 0.0;
int SimulationExecutive::_currentMsgColor = COLOR_GREEN;

std::function<void(int)> SimulationExecutive::_msgHandler = 0;

void InitializeSimulation()
{
	SimulationExecutive::InitializeSimulation();
}

Time GetSimulationTime()
{
	return SimulationExecutive::GetSimulationTime();
}

void RunSimulation()
{
	SimulationExecutive::RunSimulation();
}

void RunSimulation(Time endTime)
{
	SimulationExecutive::RunSimulation(endTime);
}

void ScheduleEventIn(Time delta, EventAction*ea)
{
	SimulationExecutive::ScheduleEventIn(delta, ea);
}

void ScheduleEventAt(Time time, EventAction*ea)
{
	SimulationExecutive::ScheduleEventAt(time, ea);
}

void RegisterMsgHandler(std::function<void(int)> eventHandler)
{
	SimulationExecutive::RegisterMsgHandler(eventHandler);
}

bool ComputeGVT()
{
	return false;
}

