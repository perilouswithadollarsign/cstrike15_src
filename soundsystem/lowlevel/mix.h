#include "soundsystem/isoundsystem.h"
#include "soundsystem/audio_mix.h"


// full depth/join/resample/convert
extern int ConvertSourceToFloat( const audio_source_input_t &channel, float flPitch, float flOutput[MIX_BUFFER_SIZE], audio_source_indexstate_t *pOut );

// just update the output state as if we mixed
extern int AdvanceSource( const audio_source_input_t &source, float flPitch, audio_source_indexstate_t *pOut );
extern uint AdvanceSourceIndex( audio_source_indexstate_t *pOut, const audio_source_input_t &source, uint nAdvance );


