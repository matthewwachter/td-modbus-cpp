/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

//#include <inttypes.h>
//#include <stdio.h>
#include <modbus.h>
#include <iostream>

//#ifdef _WIN32
//# include <winsock2.h>
//#else
//# include <sys/socket.h>
//#endif

#include "ModbusInCHOP.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>
#include <string>

#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))



// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{


DLLEXPORT
void
FillCHOPPluginInfo(CHOP_PluginInfo *info)
{
	// Always set this to CHOPCPlusPlusAPIVersion.
	info->apiVersion = CHOPCPlusPlusAPIVersion;

	// The opType is the unique name for this CHOP. It must start with a 
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Modbus");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("Modbus");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Matthew Wachter");
	info->customOPInfo.authorEmail->setString("matthewwachter@gmail.com");

	// This CHOP can work with 0 inputs
	info->customOPInfo.minInputs = 0;

	// It can accept up to 1 input though, which changes it's behavior
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
CHOP_CPlusPlusBase*
CreateCHOPInstance(const OP_NodeInfo* info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per CHOP that is using the .dll
	return new ModbusInCHOP(info);
}

DLLEXPORT
void
DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the CHOP using that instance is deleted, or
	// if the CHOP loads a different DLL
	//std::cout << "connected\n";
	//if (instance)
	//{
	//	std::cout << "disconnected\n";
	//	modbus_close(ctx);
	//	modbus_free(ctx);
	//	myConnectionState = 0;
	//}
	delete (ModbusInCHOP*)instance;
}

};


ModbusInCHOP::ModbusInCHOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	isConnected = false;
	//myOffset = 0.0;
}

ModbusInCHOP::~ModbusInCHOP()
{

}

void
ModbusInCHOP::connect(const char *ip, int port,int raddr, int rwords)
{
	//std::cout << ip << "\n";
	//std::cout << port << "\n";
	//std::cout << modbus_new_tcp(ip, port);
	ctx = modbus_new_tcp(ip, port);
	if (ctx == NULL) {
		std::cout << fprintf(stderr, "Unable to allocate libmodbus context\n");
	}
	rc = modbus_connect(ctx);
	if (rc == -1) {
		std::cout << fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
	}
	else {
		std::cout << "connection established\n";
		isConnected = true;
		stopListening = false;
		//start listening here

		startListening(raddr, rwords);
	}
}

void
ModbusInCHOP::disconnect()
{
	// signal thread to stop listening
	if (isListening) {
		stopListening = true;
	}
	else
	{
		/*std::cout << "disconnected\n";*/
		modbus_close(ctx);
		modbus_free(ctx);
		isConnected = false;
	}
}

void
ModbusInCHOP::startListening(int raddr, int rwords)
{
	std::thread listen(raddr, rwords);
}

void
ModbusInCHOP::listen(int raddr, int rwords)
{
	isListening = true;
	listenError = false;
	while (!listenError && !stopListening)
	{
		// write registers (coils)
		rc = modbus_write_registers(ctx, raddr, rwords, write_coils);
		if (rc == -1)
		{
			listenError = true;
			std::cout << "ERROR\n";
			std::cout << (stderr, "%s\n", modbus_strerror(errno));
		}

		// read registers
		rc = modbus_read_registers(ctx, raddr, rwords, coils_tab_reg);
		if (rc == -1) {
			listenError = true;
			std::cout << "ERROR\n";
			std::cout << (stderr, "%s\n", modbus_strerror(errno));
		}

		// read input registers
		rc = modbus_read_input_registers(ctx, raddr, rwords, registers_tab_reg);
		if (rc == -1) {
			listenError = true;
			std::cout << "ERROR\n";
			std::cout << (stderr, "%s\n", modbus_strerror(errno));
		}
	}
	isListening = false;
}

void
ModbusInCHOP::copyWriteBuffer(int rwords, const OP_Inputs* inputs)
{
	// update write registers buffer
	if (inputs->getNumInputs() > 0)
	{
		const OP_CHOPInput *cinput = inputs->getInputCHOP(0);

		const float *c_data = cinput->getChannelData(0);
		uint16_t tmp_reg;

		// for each word
		for (int i = 0; i < rwords; i++) {
			tmp_reg = 0;

			// for each bit
			for (int j = 0; j < 16; j++) {
				// bit shift to correct position
				tmp_reg += bool(c_data[i * 16 + 15 - j]) << 15 - j;
			}

			write_coils[i] = tmp_reg;
		}
	}
}

void
ModbusInCHOP::copyReadBuffers(int rwords, CHOP_Output* output)
{
	// for each word
	for (int i = 0; i < rwords; i++)
	{
		// for each bit
		for (int j = 0; j < 16; j++)
		{
			// update registers chan sample
			output->channels[0][i * 16 + j] = float((bool)(coils_tab_reg[i] & (1U << j)));

			// update input registers chan sample
			output->channels[1][i * 16 + j] = float((bool)(registers_tab_reg[i] & (1U << j)));
		}
	}
}

void
ModbusInCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	// Note: To disable timeslicing you'll need to turn this off, as well as ensure that
	// getOutputInfo() returns true, and likely also set the info->numSamples to how many
	// samples you want to generate for this CHOP. Otherwise it'll take on length of the
	// input CHOP, which may be timesliced.
	ginfo->timeslice = false;

	ginfo->inputMatchIndex = 0;
}

bool
ModbusInCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{
	//// If there is an input connected, we are going to match it's channel names etc
	//// otherwise we'll specify our own.
	//if (inputs->getNumInputs() > 0)
	//{
	//	return false;
	//}
//else
//{
/*info->numChannels = inputs->getParInt("Rwords");*/
info->numChannels = 2;

// Since we are outputting a timeslice, the system will dictate
// the numSamples and startIndex of the CHOP data
info->numSamples = inputs->getParInt("Rwords") * 16;
info->startIndex = inputs->getParInt("Raddr") * 16;

// For illustration we are going to output 120hz data
info->sampleRate = 60;
return true;
//}
}

void
ModbusInCHOP::getChannelName(int32_t index, OP_String *name, const OP_Inputs* inputs, void* reserved1)
{
	//std::string result = "chan" + std::to_string(index);
	//name->setString(result.data());
	//std::cout << index;
	switch (index)
	{
	case 0:
		name->setString("coils");
		break;
	case 1:
		name->setString("input_registers");
		break;
	}

}

void
ModbusInCHOP::execute(CHOP_Output* output,
	const OP_Inputs* inputs,
	void* reserved)
{
	myExecuteCount++;
	int active = inputs->getParInt("Active");

	//std::cout << myExecuteCount % 3;

	// if we are not connected
	if (!isConnected)
	{
		// if we should connect
		if (active)
		{
			const char *ip = inputs->getParString("Ip");
			int port = inputs->getParInt("Port");
			int raddr = inputs->getParInt("Raddr");
			int rwords = inputs->getParInt("Rwords");
			connect(ip, port, raddr, rwords);
		}
	}
	// if we are connected
	else
	{
		// if we should not be connected, disconnect
		if (!active)
		{
			disconnect();
		}
		// if we are connected and should be
		else
		{
			int raddr = inputs->getParInt("Raddr");
			int rwords = inputs->getParInt("Rwords");

			copyWriteBuffer(rwords, inputs);
			copyReadBuffers(rwords, output);

			// if we are not listening, start the thread
			if (!isListening) {
				// start thread
				startListening(raddr, rwords);
			}
		}
	}
}

int32_t
ModbusInCHOP::getNumInfoCHOPChans(void * reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 1;
}

void
ModbusInCHOP::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan,
										void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}

	//if (index == 1)
	//{
	//	chan->name->setString("offset");
	//	chan->value = (float)myOffset;
	//}
}

bool		
ModbusInCHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
ModbusInCHOP::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries, 
										void* reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
		entries->values[0]->setString("executeCount");

		// Set the value for the second column
//#ifdef _WIN32
//		sprintf_s(tempBuffer, "%d", myExecuteCount);
//#else // macOS
        //snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
//#endif
		entries->values[1]->setString(tempBuffer);
	}

//	if (index == 1)
//	{
//		// Set the value for the first column
//		entries->values[0]->setString("offset");
//
//		// Set the value for the second column
//#ifdef _WIN32
//        sprintf_s(tempBuffer, "%g", myOffset);
//#else // macOS
//        snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
//#endif
//		entries->values[1]->setString( tempBuffer);
//	}
}

void
ModbusInCHOP::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
	// ip address
	{
		OP_StringParameter	sp;

		sp.name = "Ip";
		sp.label = "IP Address";
		sp.defaultValue = "127.0.0.1";

		OP_ParAppendResult res = manager->appendString(sp);
		assert(res == OP_ParAppendResult::Success);
	}
	
	// port
	{
		OP_NumericParameter  np;

		np.name = "Port";
		np.label = "Port";
		np.defaultValues[0] = 501;
		np.minSliders[0] = 0;
		np.maxSliders[0] = 10000;
		
		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// connect
	{
		OP_NumericParameter  np;

		np.name = "Active";
		np.label = "Active";
		np.defaultValues[0] = 0;
		np.minSliders[0] = 0;
		np.maxSliders[0] = 1;

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// connect
	{
		OP_NumericParameter  np;

		np.name = "Staggerreq";
		np.label = "Stagger Requests";
		np.defaultValues[0] = 0;
		np.minSliders[0] = 0;
		np.maxSliders[0] = 1;

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}


	//// register filter
	//{
	//	OP_StringParameter	sp;

	//	sp.name = "Rfilter";
	//	sp.label = "Register Filter";
	//	sp.defaultValue = "[1-10] 12 14";

	//	OP_ParAppendResult res = manager->appendString(sp);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	//// coil filter
	//{
	//	OP_StringParameter	sp;

	//	sp.name = "Coils";
	//	sp.label = "Coils";
	//	sp.defaultValue = "127.0.0.1";

	//	OP_ParAppendResult res = manager->appendString(sp);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	// Registers Address
	{
		OP_NumericParameter  np;

		np.name = "Raddr";
		np.label = "Register Address";
		np.defaultValues[0] = 0;
		np.minSliders[0] = 0;
		np.maxSliders[0] = 100;
		np.minValues[0] = 0;
		np.maxValues[0] = 100;
		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}
	
	// Registers Num Bits
	{
		OP_NumericParameter  np;

		np.name = "Rwords";
		np.label = "Register Words";
		np.defaultValues[0] = 100;
		np.minSliders[0] = 1;
		np.maxSliders[0] = 100;
		np.minValues[0] = 1;
		np.maxValues[0] = 100;
		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void 
ModbusInCHOP::pulsePressed(const char* name, void* reserved1)
{
	//if (!strcmp(name, "Reset"))
	//{
	//	myOffset = 0.0;
	//}
}