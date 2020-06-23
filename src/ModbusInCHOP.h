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

#include "CHOP_CPlusPlusBase.h"
#include <thread>


// To get more help about these functions, look at CHOP_CPlusPlusBase.h
class ModbusInCHOP : public CHOP_CPlusPlusBase
{
public:
	ModbusInCHOP(const OP_NodeInfo* info);
	virtual ~ModbusInCHOP();

	virtual void		getGeneralInfo(CHOP_GeneralInfo*, const OP_Inputs*, void* ) override;
	virtual bool		getOutputInfo(CHOP_OutputInfo*, const OP_Inputs*, void*) override;
	virtual void		getChannelName(int32_t index, OP_String *name, const OP_Inputs*, void* reserved) override;

	virtual void		execute(CHOP_Output*,
								const OP_Inputs*,
								void* reserved) override;


	virtual int32_t		getNumInfoCHOPChans(void* reserved1) override;
	virtual void		getInfoCHOPChan(int index,
										OP_InfoCHOPChan* chan,
										void* reserved1) override;

	virtual bool		getInfoDATSize(OP_InfoDATSize* infoSize, void* resereved1) override;
	virtual void		getInfoDATEntries(int32_t index,
											int32_t nEntries,
											OP_InfoDATEntries* entries,
											void* reserved1) override;

	virtual void		setupParameters(OP_ParameterManager* manager, void *reserved1) override;
	virtual void		pulsePressed(const char* name, void* reserved1) override;



private:
	void connect(const char *ip, int port);
	void disconnect();
	void comWrite(int raddr, int rwords);
	void comRead(int raddr, int rwords);
	void copyWriteBuffer(const OP_Inputs*, int rwords);
	void copyReadBuffers(CHOP_Output*, int rwords);
	
	std::thread*		myThread;

	void listen();
	//void startListening();

	int				raddr;
	int				rwords;

	modbus_t*			ctx;

	bool				isActive;
	bool				isConnected;

	bool				isListening;
	bool				listenError;
	bool				stopListening;

	//std::thread			listenThread;

	uint16_t			write_coils[100 * sizeof(uint16_t)];
	uint16_t			last_write_coils[100 * sizeof(uint16_t)];

	uint16_t			coils_tab_reg[100 * sizeof(uint16_t)];
	uint16_t			registers_tab_reg[100 * sizeof(uint16_t)];

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo*	myNodeInfo;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the CHOP 
	int32_t				myExecuteCount;

	// Modbus
	
	//int			rc;

};
