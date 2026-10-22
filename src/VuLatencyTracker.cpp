#include "VuLatencyTracker.h"

#include "Operand.h"
#include "VuInstructionInfo.h"
#include "VuSchedulingRules.h"
#include "VuTokenResourceAccess.h"

#include <list>

namespace vcl
{

namespace
{
	bool tokenReadsImplicitResource( const Token& token, unsigned int resource )
	{
		VuTokenResourceAccess access;
		return buildVuTokenResourceAccess( token, access )
		    && (access.implicitReads & resource) != 0;
	}

	bool tokenWritesImplicitResource( const Token& token, unsigned int resource )
	{
		VuTokenResourceAccess access;
		return buildVuTokenResourceAccess( token, access )
		    && (access.implicitWrites & resource) != 0;
	}

	int bypassLatencyReduction( const std::string& mnemonic, int fallback )
	{
		const VuInstructionInfo* info = findVuInstructionInfo( mnemonic );
		return info ? static_cast<int>( info->latency ) : fallback;
	}

	bool mergeCycle( int& cycle, int other, int pendingAfter )
	{
		if( other <= cycle )
			return false;
		cycle = other;
		return other > pendingAfter;
	}
}

VuLatencyTracker::VuLatencyTracker()
{
	reset();
}

void VuLatencyTracker::reset()
{
	m_qReadyCycle = -10;
	m_pReadyCycle = -10;
	m_lastFMACCycle = -10;
	m_lastClipwCycle = -10;
	m_registerReadyCycle.clear();
	m_registerProducerMnemonic.clear();
}

bool VuLatencyTracker::merge( const VuLatencyTracker& other, int shift, int now )
{
	bool changed = false;
	changed = mergeCycle( m_qReadyCycle, other.m_qReadyCycle + shift, now ) || changed;
	changed = mergeCycle( m_pReadyCycle, other.m_pReadyCycle + shift, now ) || changed;
	// the flags are read four cycles after they are written
	changed = mergeCycle( m_lastFMACCycle, other.m_lastFMACCycle + shift, now - 4 ) || changed;
	changed = mergeCycle( m_lastClipwCycle, other.m_lastClipwCycle + shift, now - 4 ) || changed;

	for( std::map<std::string, int>::const_iterator i = other.m_registerReadyCycle.begin();
	     i != other.m_registerReadyCycle.end(); ++i )
	{
		const int ready = i->second + shift;
		std::map<std::string, std::string>::const_iterator producer =
		    other.m_registerProducerMnemonic.find( i->first );
		std::map<std::string, int>::iterator mine = m_registerReadyCycle.find( i->first );
		if( mine == m_registerReadyCycle.end() )
		{
			m_registerReadyCycle[i->first] = ready;
			if( producer != other.m_registerProducerMnemonic.end() )
				m_registerProducerMnemonic[i->first] = producer->second;
			changed = (ready > now) || changed;
			continue;
		}

		changed = mergeCycle( mine->second, ready, now ) || changed;

		// a bypass only applies when the same instruction writes the register
		// on every path
		std::map<std::string, std::string>::iterator mineProducer = m_registerProducerMnemonic.find( i->first );
		if( mineProducer != m_registerProducerMnemonic.end()
		    && (producer == other.m_registerProducerMnemonic.end() || producer->second != mineProducer->second) )
			m_registerProducerMnemonic.erase( mineProducer );
	}

	return changed;
}

int VuLatencyTracker::readHazardDelay( const Token& token,
                                       const Token* partner,
                                       int currentCycle ) const
{
	std::list<std::string> reads;
	collectVuRegisterReadKeys( token, reads );
	if( partner )
		collectVuRegisterReadKeys( *partner, reads );

	bool readsQ = vuTokenReadsQ( token );
	bool readsP = vuTokenReadsP( token );
	if( partner )
	{
		readsQ = readsQ || vuTokenReadsQ( *partner );
		readsP = readsP || vuTokenReadsP( *partner );
	}

	int needed = 0;
	for( std::list<std::string>::const_iterator i = reads.begin(); i != reads.end(); ++i )
	{
		std::map<std::string, int>::const_iterator ready = m_registerReadyCycle.find( *i );
		if( ready == m_registerReadyCycle.end() )
			continue;

		int readyCycle = ready->second;
		std::map<std::string, std::string>::const_iterator producer =
		    m_registerProducerMnemonic.find( *i );
		if( producer != m_registerProducerMnemonic.end()
		    && isVuFtoiConversion( producer->second )
		    && ( (isVuMtir( token ) && vuTokenReadsRegister( token, *i ))
		         || (partner && isVuMtir( *partner ) && vuTokenReadsRegister( *partner, *i )) ) )
			readyCycle -= bypassLatencyReduction( producer->second, 4 );
		if( producer != m_registerProducerMnemonic.end()
		    && isVuLoadToFtoiBypassProducer( producer->second )
		    && ( (isVuFtoiConversion( lowerVuTokenName( token ) ) && vuTokenReadsRegister( token, *i ))
		         || (partner && isVuFtoiConversion( lowerVuTokenName( *partner ) )
		             && vuTokenReadsRegister( *partner, *i )) ) )
			readyCycle -= bypassLatencyReduction( producer->second, 4 );
		if( producer != m_registerProducerMnemonic.end()
		    && isVuLoadToMiniiBypassProducer( producer->second )
		    && ( (isVuMinii( lowerVuTokenName( token ) ) && vuTokenReadsRegister( token, *i ))
		         || (partner && isVuMinii( lowerVuTokenName( *partner ) )
		             && vuTokenReadsRegister( *partner, *i )) ) )
			readyCycle -= 2;

		const int gap = readyCycle - currentCycle;
		if( gap > needed )
			needed = gap;
	}

	if( readsQ )
	{
		const int gap = m_qReadyCycle - currentCycle;
		if( gap > needed )
			needed = gap;
	}
	if( readsP )
	{
		const int gap = m_pReadyCycle - currentCycle;
		if( gap > needed )
			needed = gap;
	}

	const int flagCycle = currentCycle + needed;
	bool readsMac = tokenReadsImplicitResource( token, VU_RESOURCE_MAC );
	bool readsClip = tokenReadsImplicitResource( token, VU_RESOURCE_CLIP );
	if( partner && partner->operand() )
	{
		readsMac = readsMac || tokenReadsImplicitResource( *partner, VU_RESOURCE_MAC );
		readsClip = readsClip || tokenReadsImplicitResource( *partner, VU_RESOURCE_CLIP );
	}

	int flagDelay = 0;
	if( readsMac )
	{
		const int gap = flagCycle - m_lastFMACCycle;
		if( 4 - gap > flagDelay )
			flagDelay = 4 - gap;
	}
	if( readsClip )
	{
		const int gap = flagCycle - m_lastClipwCycle;
		if( 4 - gap > flagDelay )
			flagDelay = 4 - gap;
	}
	if( flagDelay > 0 )
		needed += flagDelay;

	return needed;
}

void VuLatencyTracker::recordWrites( const Token& token, int issueCycle, bool forceMacFlagWrite )
{
	if( forceMacFlagWrite || tokenWritesImplicitResource( token, VU_RESOURCE_MAC ) )
		m_lastFMACCycle = issueCycle;
	if( tokenWritesImplicitResource( token, VU_RESOURCE_CLIP ) )
		m_lastClipwCycle = issueCycle;

	if( !token.operand() || token.operand()->latency() <= 1 )
		return;

	const int readyCycle = issueCycle + static_cast<int>( token.operand()->latency() ) + 1;
	std::list<std::string> writes;
	collectVuRegisterWriteKeys( token, writes );
	for( std::list<std::string>::const_iterator i = writes.begin(); i != writes.end(); ++i )
	{
		m_registerReadyCycle[*i] = readyCycle;
		m_registerProducerMnemonic[*i] = lowerVuTokenName( token );
	}

	if( vuTokenWritesQ( token ) )
		m_qReadyCycle = readyCycle;
	if( vuTokenWritesP( token ) )
		m_pReadyCycle = readyCycle;
}

int VuLatencyTracker::qReadyCycle() const
{
	return m_qReadyCycle;
}

int VuLatencyTracker::pReadyCycle() const
{
	return m_pReadyCycle;
}

}
