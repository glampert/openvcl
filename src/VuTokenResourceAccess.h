#ifndef __OPENVCL_VUTOKENRESOURCEACCESS_H__
#define __OPENVCL_VUTOKENRESOURCEACCESS_H__

/*
 * VuTokenResourceAccess.h
 *
 * Table-driven resource descriptors for parsed VU tokens.
 */

#include "Token.h"
#include "VuInstructionInfo.h"

#include <list>
#include <string>

namespace vcl
{

struct VuTokenResourceAccess
{
	VuTokenResourceAccess();

	std::list<std::string> registerReads;
	std::list<std::string> registerWrites;
	unsigned int instructionFlags;
	unsigned int implicitReads;
	unsigned int implicitWrites;
	VuMemoryKind memoryKind;
	unsigned int memoryFlags;
	bool hasMemoryBase;
	std::string memoryBaseRegister;
	bool hasMemoryOffset;
	long memoryOffset;
	unsigned int branchDelaySlots;
	unsigned int bypassFlags;
};

bool buildVuTokenResourceAccess( const Token& token, VuTokenResourceAccess& access );
bool vuRegisterKey( const Token::Argument& arg, std::string& key );
unsigned int vuReadFieldMask( const Token& token, const Token::Argument& arg );
unsigned int vuWriteFieldMask( const Token& token, const Token::Argument& arg );

// The argument holding token's integer immediate (or memory offset), the
// value it encodes as and the range its instruction's field can hold (see
// vuInstructionImmediateRange). Returns NULL when the instruction has no
// such field, or when the immediate is not an integer constant (a symbol,
// say), which only the assembler can evaluate.
const Token::Argument* findVuTokenImmediate( const Token& token, double& value, long& minimum, long& maximum );

}

#endif
