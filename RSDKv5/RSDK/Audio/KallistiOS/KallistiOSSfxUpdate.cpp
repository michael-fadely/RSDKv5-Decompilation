#ifndef  __KALLISTIOS_SFX_UPDATE
#define  __KALLISTIOS_SFX_UPDATE
extern "C" {
#include "dc/sound/aica_comm.h"

extern int snd_sh4_to_aica(void *packet, uint32_t size);

// WARNING!
// in order to update a sound effect channel after it has started/while it is playing, 
// you need to do something that KOS does not provide an API for
// and to do that, unfortunately, we need to reproduce these type definitions here
// as they are internal types not exposed by KOS headers
// this will need to be maintained if KOS changes these in the future
// they have been stable for a long time but that is no guarantee things will stay that way
struct snd_effect;

typedef struct snd_effect
{
	uint32_t locl, locr;
	uint32_t len;
	uint32_t rate;
	uint32_t used;
	uint32_t fmt;
	uint16_t stereo;

	LIST_ENTRY(snd_effect)
	list;
} snd_effect_t;

void snd_sfx_update_ex(sfx_play_data_t *data)
{
	int size;
	snd_effect_t *t = (snd_effect_t *)data->idx;
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

	size = t->len;

	if (size >= 65535)
		size = 65534;

	cmd->cmd = AICA_CMD_CHAN;
	cmd->timestamp = 0;
	cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
	cmd->cmd_id = data->chn;

    // this allows you to change all of the useful parameters of a hardware playback channel in one go
    // for pitch shifting and panning and fadeout
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_PAN | AICA_CH_UPDATE_SET_VOL;
	chan->base = t->locl;
	chan->type = t->fmt;
	chan->length = size;

	chan->loop = data->loop;
	chan->loopstart = data->loopstart;
	chan->loopend = data->loopend ? data->loopend : size;
	chan->freq = data->freq > 0 ? data->freq : t->rate;
	chan->vol = data->vol;
	chan->pan = data->pan;

	snd_sh4_to_aica(tmp, cmd->size);
}
}
#endif //__KALLISTIOS_SFX_UPDATE