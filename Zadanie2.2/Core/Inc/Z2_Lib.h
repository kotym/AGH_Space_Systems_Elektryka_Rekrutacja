/*
 * Zadanie2.2Pill.h
 *
 *  Created on: Nov 15, 2025
 *      Author: Tymoteusz Kosetka
 *
 *
 *	This is header only library for commands
 *
 *	USAGE INSTRUCTIONS
 *	1. In one c file in global space place Z2_COMMANDS_VARIABLE.
 *	2. In main function execute Z2_CREATE_COMMANDS(n) where n is number of commands that will be created.
 *	3. In main function after Z2_CREATE_COMMANDS(n) place Z2_REGISTER_COMMAND or Z2_REGISTER_COMMAND_ARGS
 *	4. To retrieve arguments in callback function call Z2_GetArg_argDatatype where argDatatype is on of Z2_ArgType
 *
 */

#ifndef INC_Z2_LIB_H_
#define INC_Z2_LIB_H_


#include <alloca.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef void (*Z2_CallbackFnPtr)();

typedef enum
{
	Z2_ArgString,
	Z2_ArgInt,
	Z2_ArgFloat
} Z2_ArgType;

typedef struct
{
	const char* name;
	Z2_ArgType type;
} Z2_Arg;

typedef struct
{
	const char* name;
	Z2_CallbackFnPtr callback;
	int argCount;
	Z2_Arg* args;
} Z2_Command;

typedef struct
{
	uint32_t* argsBuff;
	const char* stringArgsBuff;
	uint16_t commandIndex;
} Z2_CurrentCommandData;

typedef struct
{
	Z2_Command* commands;
	uint16_t maxCommandsNum, registeredCommandsNum;
	Z2_CurrentCommandData currentCommandData;

} Z2_Commands;

void Z2_InitCommand(Z2_Arg* args, int n, ...)
{
	va_list vargs;
	va_start(vargs, n);
	for (int i = 0; i < n; ++i)
	{
		args[i].name = va_arg(vargs, const char*);
		args[i].type = va_arg(vargs, int);
	}
	va_end(vargs);
}

extern Z2_Commands* z2_Commands;

#define Z2_COMMANDS_VARIABLE Z2_Commands* z2_Commands = 0;

// Inits z2_Commands and allocates space for n commands
// !!! IMPORTANT !!!
// WHOLE z2_Commands IS STACK ALLOCATED. IT WILL BE DESTROYED ON SCOPE END
// CALL ONLY ONCE FROM THE MOST OUTER CODE, BEST PLACE IS main()
#define Z2_CREATE_COMMANDS(n) \
		z2_Commands = alloca(sizeof(Z2_Commands)); \
		z2_Commands->commands = alloca(sizeof(Z2_Command) * n); \
		z2_Commands->maxCommandsNum = n; z2_Commands->registeredCommandsNum = 0;\
		z2_Commands->currentCommandData = (Z2_CurrentCommandData){0, 0, 0};

// Registers command
// commandName = command name
// callbackFunc = function of type Z2_CallbackFnPtr
// argsN = number of arguments
// vargs... = argsN * ( argName, argType (Z2_ArgType) )
// example: Z2_REGISTER_COMMAND_ARGS(0, "commandName", &Func, 2, "arg0 name", Z2_ArgInt, "arg1 name", Z2_ArgFloat);
// !!! IMPORTANT !!!
// CALL AFTER Z2_CREATE_COMMANDS n times
#define Z2_REGISTER_COMMAND_ARGS(commandName, callbackFunc, argsN, vargs...) \
		if (!z2_Commands || z2_Commands->registeredCommandsNum == z2_Commands->maxCommandsNum) exit(EXIT_FAILURE); /*Z2_REGISTER_COMMAND_ARGS used before Z2_CREATE_COMMANDS or to many times*/ \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].name = commandName; \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].callback = (Z2_CallbackFnPtr){callbackFunc}; /* throws warning if callbackFunc is not Z2_CallbackFnPtr */ \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].args = alloca(argsN * sizeof(Z2_Arg)); \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].argCount = argsN; \
		Z2_InitCommand(z2_Commands->commands[z2_Commands->registeredCommandsNum++].args, argsN, vargs);\


// Z2_REGISTER_COMMAND_ARGS without args
#define Z2_REGISTER_COMMAND(commandName, callbackFunc) \
		if (!z2_Commands || z2_Commands->registeredCommandsNum == z2_Commands->maxCommandsNum) exit(EXIT_FAILURE); /*Z2_REGISTER_COMMAND_ARGS used before Z2_CREATE_COMMANDS or to many times*/ \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].name = commandName; \
		z2_Commands->commands[z2_Commands->registeredCommandsNum].argCount = 0; \
		z2_Commands->commands[z2_Commands->registeredCommandsNum++].callback = callbackFunc; /* throws warning if callbackFunc is not Z2_CallbackFnPtr */

// getSecondValue == 0 -> returns first value
// getSecondValue > 0 -> returns second value
uint16_t* Z2_ExtractStringArgData(uint16_t argIndex, uint8_t getSecondValue)
{
	return (uint16_t*)(z2_Commands->currentCommandData.argsBuff + argIndex) + (uint8_t)(getSecondValue > 0);
}

uint8_t Z2_ExecuteCommand(const char* command, uint16_t len)
{
	// extract command name
	uint16_t commandNameEnd = 0;
	for (; commandNameEnd < len && command[commandNameEnd] != '[' && isprint((int)command[commandNameEnd]); ++commandNameEnd);
	if (commandNameEnd == 0) return 1;

	// try to find command named like this
	uint16_t commandIndex = UINT16_MAX;
	for (uint16_t i = 0; i < z2_Commands->registeredCommandsNum; ++i)
	{
		uint8_t match = 1;
		for (uint16_t j = 0; j < commandNameEnd; ++j)
		{
			if (z2_Commands->commands[i].name[j] != command[j] || z2_Commands->commands[i].name[j] == '\0')
			{
				match = 0;
				break;
			}
		}

		if (match && z2_Commands->commands[i].name[commandNameEnd] == '\0')
		{
			commandIndex = i;
			break;
		}
	}
	if (commandIndex == UINT16_MAX) return 2;

	z2_Commands->currentCommandData.commandIndex = commandIndex;
	const uint16_t argCount = z2_Commands->commands[commandIndex].argCount;

	uint16_t stringsTotalSize = 0, hasStrings = 0;
	if (argCount > 0)
	{
		uint32_t* const argsBuff = alloca(argCount * sizeof(uint32_t)); // for now all args are 4 bytes long, but not necessarily uint32_t
		z2_Commands->currentCommandData.argsBuff = argsBuff;

		// Parse arguments
		uint16_t argIndex = 0, argBuffI = 0;
		char argBuff[32];
		for	(uint16_t i = commandNameEnd + 2; i < len && argIndex < argCount; ++i)
		{
			if (command[i] == ']') break;
			if (command[i] == ';')
			{
				switch(z2_Commands->commands[commandIndex].args[argIndex].type)
				{
				case Z2_ArgInt:
					argBuff[argBuffI] = '\0'; // add termination character for proper atoi functioning
					*(int*)(&argsBuff[argIndex]) = atoi(argBuff); // store int in argBuff
					break;
				case Z2_ArgString:
					*Z2_ExtractStringArgData(argIndex, 0) = i - argBuffI; // store string start in argBuff
					*Z2_ExtractStringArgData(argIndex, 1) = i; // store string size in argBuff
					stringsTotalSize += argBuffI + 1; // string len + '\0'
					hasStrings = 1;
					break;
				case Z2_ArgFloat:
					argBuff[argBuffI] = '\0'; // add termination character for proper atof functioning
					*(float*)(&argsBuff[argIndex]) = atof(argBuff); // store float in argBuff
					break;
				}
				++argIndex;
				argBuffI = 0;
				continue;
			}
			if (z2_Commands->commands[commandIndex].args[argIndex].type != Z2_ArgString)
			{
				argBuff[argBuffI] = command[i];
				if (argBuffI >= 30) argBuffI = 30;
			}
			++argBuffI;
		}

		// Taking care of not entered args
		// may want to add default arg values for not entered args
		for (; argIndex < argCount; ++argIndex)
		{
			switch(z2_Commands->commands[commandIndex].args[argIndex].type)
			{
			case Z2_ArgInt:
				argsBuff[argIndex] = 0;
				break;
			case Z2_ArgString:
				*Z2_ExtractStringArgData(argIndex, 0) = *Z2_ExtractStringArgData(argIndex, 1) = stringsTotalSize - hasStrings;
				break;
			case Z2_ArgFloat:
				argsBuff[argIndex] = 0;
				break;
			}
		}
	}

	// Copy strings form command to buffer
	// To limit memory allocation strings could be retrieved directly from command input and not copied to intermediary buffer
	char* const stringsArgsBuff = alloca(hasStrings ? stringsTotalSize : 1);
	z2_Commands->currentCommandData.stringArgsBuff = stringsArgsBuff;
	if (!hasStrings) stringsArgsBuff[0] = '\0';
	if (stringsTotalSize > 0)
	{
		uint16_t stringsBuffOffset = 0;
		for (uint16_t argIndex = 0; argIndex < argCount; ++argIndex)
		{
			if (z2_Commands->commands[commandIndex].args[argIndex].type != Z2_ArgString) continue;
			// string arg in argBuff is divided to two uint16_t
			uint16_t* const stringArg0 = Z2_ExtractStringArgData(argIndex, 0);
			uint16_t* const stringArg1 = Z2_ExtractStringArgData(argIndex, 1);

			uint16_t size = *stringArg1 - *stringArg0;
			memcpy(stringsArgsBuff + stringsBuffOffset, &command[*stringArg0], size);
			*stringArg0 = stringsBuffOffset;
			*stringArg1 = size;
			stringsArgsBuff[stringsBuffOffset + size] = '\0';
			stringsBuffOffset += size + 1; // size + '\0'
		}
	}

	z2_Commands->commands[commandIndex].callback();

	z2_Commands->currentCommandData = (Z2_CurrentCommandData){0, 0, 0};
	return 0;
}

// CALL ONLY FROM CALLBACK FUNCTION
void* Z2_GetArg(uint16_t argIndex)
{
	switch(z2_Commands->commands[z2_Commands->currentCommandData.commandIndex].args[argIndex].type)
	{
	case Z2_ArgInt:
	case Z2_ArgFloat:
		return (void*)&z2_Commands->currentCommandData.argsBuff[argIndex];
	case Z2_ArgString:
		{
			return (void*)&z2_Commands->currentCommandData.stringArgsBuff[*Z2_ExtractStringArgData(argIndex, 0)];
		}
	}
	return 0;
}

// CALL ONLY FROM CALLBACK FUNCTION
int Z2_GetArg_Int(uint16_t argIndex)
{
	return *((int*)Z2_GetArg(argIndex));
}

// CALL ONLY FROM CALLBACK FUNCTION
float Z2_GetArg_Float(uint16_t argIndex)
{
	return *((float*)Z2_GetArg(argIndex));
}

// CALL ONLY FROM CALLBACK FUNCTION
const char* Z2_GetArg_String(uint16_t argIndex)
{
	return (const char*)Z2_GetArg(argIndex);
}

const char* Z2_ArgTypeToString(Z2_ArgType argType)
{
	switch(argType)
	{
	case Z2_ArgInt: return "int";
	case Z2_ArgFloat: return "float";
	case Z2_ArgString: return "string";
	}
	return "";
}



#endif /* INC_Z2_LIB_H_ */
