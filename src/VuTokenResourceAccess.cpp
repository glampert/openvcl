#include "VuTokenResourceAccess.h"

#include "Dependency.h"
#include "Expression.h"
#include "Math.h"

#include <iomanip>
#include <math.h>
#include <sstream>

namespace vcl
{

namespace
{
	void appendFieldKeys( const std::string& base, unsigned int fields, std::list<std::string>& keys )
	{
		static const char* names = "xyzw";
		if( fields == 0 )
			fields = Token::X | Token::Y | Token::Z | Token::W;
		for( unsigned int i = 0; i < 4; ++i )
		{
			if( fields & (1 << i) )
			{
				std::string key = base;
				key += ".";
				key += names[i];
				keys.push_back( key );
			}
		}
	}

	unsigned int resourceForArgument( const Token::Argument& arg )
	{
		switch( arg.type() )
		{
			case Token::Argument::ACCUMULATOR: return VU_RESOURCE_ACC;
			case Token::Argument::I: return VU_RESOURCE_I;
			case Token::Argument::Q: return VU_RESOURCE_Q;
			case Token::Argument::P: return VU_RESOURCE_P;
			case Token::Argument::R: return VU_RESOURCE_R;
			default: return VU_RESOURCE_NONE;
		}
	}

	const VuInstructionInfo* tokenInstructionInfo( const Token& token )
	{
		if( !token.operand() )
			return 0;
		return findVuInstructionInfo( normalizeVuMnemonic( token.operand()->name() ) );
	}

	bool evaluateMemoryOffset( const Token::Argument& arg, long& offset )
	{
		Expression e;
		e.setCustomOperators( Math::mathOperators() );
		if( !e.process( arg.immediate() ) || !e.solve() )
			return false;
		offset = static_cast<long>( e.result() );
		return true;
	}

	// An integer literal as the assembler reads it (hex 0x.., binary 0b..,
	// octal 0.., or decimal), optionally signed.
	bool parseAssemblerInteger( const std::string& text, double& value )
	{
		std::string::size_type i = text.find_first_not_of( " \t" );
		const std::string::size_type end = text.find_last_not_of( " \t" );
		if( std::string::npos == i )
			return false;

		const bool negative = ('-' == text[i]);
		if( ('-' == text[i]) || ('+' == text[i]) )
			++i;
		if( i > end )
			return false;

		unsigned int base = 10;
		if( ('0' == text[i]) && (i < end) )
		{
			const char prefix = text[i+1];
			if( ('x' == prefix) || ('X' == prefix) )
				base = 16;
			else if( ('b' == prefix) || ('B' == prefix) )
				base = 2;
			else
				base = 8;
			i += (8 == base) ? 1 : 2;
			if( i > end )
				return false;
		}

		double result = 0;
		for( ; i <= end; ++i )
		{
			const char c = text[i];
			unsigned int digit;
			if( (c >= '0') && (c <= '9') )
				digit = c - '0';
			else if( (c >= 'a') && (c <= 'f') )
				digit = c - 'a' + 10;
			else if( (c >= 'A') && (c <= 'F') )
				digit = c - 'A' + 10;
			else
				return false;
			if( digit >= base )
				return false;
			result = (result * base) + digit;
		}

		value = negative ? -result : result;
		return true;
	}

	// The value an immediate is encoded as: CodeGenerator writes what Expression
	// can evaluate as an integer (truncated, as in C), and leaves any other text
	// to the assembler.
	bool evaluateEncodedImmediate( const Token::Argument& arg, double& value )
	{
		Expression e;
		e.setCustomOperators( Math::mathOperators() );
		if( (arg.flags() & Token::Argument::EVALUATE) && e.process( arg.immediate() ) && e.solve() )
		{
			const double result = e.result();
			if( result != result )	// NaN
				return false;
			value = (result < 0) ? ceil( result ) : floor( result );
			return true;
		}
		return parseAssemblerInteger( arg.immediate(), value );
	}
}

VuTokenResourceAccess::VuTokenResourceAccess()
{
	instructionFlags = VU_INSTR_NONE;
	implicitReads = VU_RESOURCE_NONE;
	implicitWrites = VU_RESOURCE_NONE;
	memoryKind = VU_MEMORY_NONE;
	memoryFlags = VU_MEMORY_FLAG_NONE;
	hasMemoryBase = false;
	hasMemoryOffset = false;
	memoryOffset = 0;
	branchDelaySlots = 0;
	bypassFlags = VU_BYPASS_NONE;
}

bool vuRegisterKey( const Token::Argument& arg, std::string& key )
{
	if( arg.type() != Token::Argument::FLOAT_REGISTER
	    && arg.type() != Token::Argument::INTEGER_REGISTER )
		return false;

	if( arg.content() == Token::Argument::ALIAS )
	{
		Dependency* dependency = arg.dependency();
		if( dependency && dependency->alias()
		    && dependency->alias()->allocatedRegister() )
		{
			key = dependency->alias()->allocatedRegister()->name();
			return true;
		}
		if( !arg.alias().empty() )
		{
			key = arg.alias();
			return true;
		}
	}

	std::stringstream s;
	s << ((arg.type() == Token::Argument::FLOAT_REGISTER) ? "VF" : "VI")
	  << std::setw(2) << std::setfill('0') << arg.regNumber();
	key = s.str();
	return true;
}

unsigned int vuReadFieldMask( const Token& token, const Token::Argument& arg )
{
	if( arg.type() != Token::Argument::FLOAT_REGISTER )
		return Token::X | Token::Y | Token::Z | Token::W;

	if( (arg.flags() & Token::Argument::BROADCAST) && token.broadcast() )
		return token.broadcast();

	unsigned int fields = arg.fields();
	if( fields == 0 )
		fields = token.fields();
	if( fields == 0 )
		fields = Token::X | Token::Y | Token::Z | Token::W;
	return fields;
}

unsigned int vuWriteFieldMask( const Token& token, const Token::Argument& arg )
{
	if( arg.type() != Token::Argument::FLOAT_REGISTER )
		return Token::X | Token::Y | Token::Z | Token::W;

	unsigned int fields = token.fields();
	if( fields == 0 )
		fields = arg.fields();
	if( fields == 0 )
		fields = Token::X | Token::Y | Token::Z | Token::W;
	return fields;
}

bool buildVuTokenResourceAccess( const Token& token, VuTokenResourceAccess& access )
{
	access = VuTokenResourceAccess();

	if( !token.operand() )
		return false;

	const VuInstructionInfo* info = tokenInstructionInfo( token );
	if( info )
	{
		access.instructionFlags = info->flags;
		access.implicitReads = info->implicitReads;
		access.implicitWrites = info->implicitWrites;
		access.memoryKind = info->memoryKind;
		access.memoryFlags = info->memoryFlags;
		access.branchDelaySlots = info->branchDelaySlots;
		access.bypassFlags = info->bypassFlags;
	}

	const std::list<Token::Argument>& args = token.arguments();
	for( std::list<Token::Argument>::const_iterator i = args.begin(); i != args.end(); ++i )
	{
		const unsigned int resource = resourceForArgument( *i );
		if( resource != VU_RESOURCE_NONE )
		{
			if( (*i).flags() & Token::Argument::WRITE )
				access.implicitWrites |= resource;
			else
				access.implicitReads |= resource;
		}

		std::string key;
		if( !vuRegisterKey( *i, key ) )
			continue;

		if( ((*i).flags() & Token::Argument::INDIRECT)
		    && (*i).type() == Token::Argument::INTEGER_REGISTER )
		{
			access.hasMemoryBase = true;
			access.memoryBaseRegister = key;
			access.hasMemoryOffset = evaluateMemoryOffset( *i, access.memoryOffset );
		}

		if( (*i).flags() & Token::Argument::WRITE )
		{
			if( (*i).type() == Token::Argument::FLOAT_REGISTER )
				appendFieldKeys( key, vuWriteFieldMask( token, *i ), access.registerWrites );
			else
				access.registerWrites.push_back( key );
		}
		else
		{
			if( (*i).type() == Token::Argument::FLOAT_REGISTER )
				appendFieldKeys( key, vuReadFieldMask( token, *i ), access.registerReads );
			else
				access.registerReads.push_back( key );
		}
	}

	return true;
}

const Token::Argument* findVuTokenImmediate( const Token& token, double& value, long& minimum, long& maximum )
{
	const VuInstructionInfo* info = tokenInstructionInfo( token );
	if( !info || !vuInstructionImmediateRange( info->opcode, minimum, maximum ) )
		return NULL;

	const std::list<Token::Argument>& args = token.arguments();
	for( std::list<Token::Argument>::const_iterator i = args.begin(); i != args.end(); ++i )
	{
		const bool offset = ((*i).type() == Token::Argument::INTEGER_REGISTER)
		                    && ((*i).flags() & Token::Argument::INDIRECT);
		if( ((*i).type() == Token::Argument::IMMEDIATE) || offset )
			return evaluateEncodedImmediate( *i, value ) ? &(*i) : NULL;
	}
	return NULL;
}

}
