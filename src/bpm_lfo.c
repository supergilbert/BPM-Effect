#include <stdlib.h>

/* LFO API */

typedef double lfo_data_t;

typedef struct
{
  size_t       bufsz;
  lfo_data_t   *buffer;
  lfo_data_t   curpos;
  lfo_data_t   pad;
} handle_lfo_t;

handle_lfo_t *init_handle_lfo(unsigned int res)
{
  handle_lfo_t *hlfo = malloc(sizeof(handle_lfo_t));

  if (hlfo == NULL)
    return NULL;
  hlfo->bufsz = (size_t) res;
  hlfo->curpos = 0.0;
  hlfo->buffer = calloc(hlfo->bufsz, sizeof(lfo_data_t));
  if (hlfo->buffer == NULL)
    {
      free(hlfo);
      return NULL;
    }
  return hlfo;
}

void delete_handle_lfo(handle_lfo_t *hlfo)
{
  free(hlfo->buffer);
  free(hlfo);
}

lfo_data_t next_lfo_sample(handle_lfo_t *hlfo)
{
  lfo_data_t frame;
  unsigned int prev_idx = (unsigned int) hlfo->curpos;

  frame = hlfo->buffer[prev_idx];
  hlfo->curpos += hlfo->pad;
  if (hlfo->curpos >= hlfo->bufsz)
    hlfo->curpos = hlfo->curpos - hlfo->bufsz;
  return frame;
}

#include <math.h>

handle_lfo_t *init_handle_sin(unsigned int res)
{
  handle_lfo_t  *hsin = init_handle_lfo(res);
  unsigned int  idx;
  lfo_data_t    pi_pad;

  if (hsin == NULL)
    return NULL;

  pi_pad = 2 * M_PI / hsin->bufsz;
  for (idx = 0; idx < hsin->bufsz; idx++)
    hsin->buffer[idx] = sinf(pi_pad * idx);
  return hsin;
}

/* LADSPA plugin API */

#include "ladspa.h"

#define BL_OUTPUT       0
#define BL_RESET        1
#define BL_BPM          2
#define BL_RATE_NUM     3
#define BL_RATE_DEN     4
#define BL_AMPLITUDE    5
#define BL_PHASE        6
#define BL_PORT_SIZE    7

typedef struct
{
  LADSPA_Data   *output;
  LADSPA_Data   *reset;
  LADSPA_Data   *bpm;
  LADSPA_Data   *rate_num;
  LADSPA_Data   *rate_den;
  LADSPA_Data   *amplitude;
  LADSPA_Data   *phase;
  handle_lfo_t  *lfo;
  unsigned long samplerate;
  int           reset_flag;
} bpm_lfo_t;

#include <string.h>

LADSPA_Handle instantiate_bpm_lfo(const LADSPA_Descriptor *Descriptor,
                                  unsigned long SampleRate)
{
  bpm_lfo_t *bpm_lfo = malloc(sizeof(bpm_lfo_t));

  bzero(bpm_lfo, sizeof(bpm_lfo_t));
  if (bpm_lfo == NULL)
    return NULL;
  bpm_lfo->lfo = init_handle_sin((unsigned int) SampleRate * 2);
  if (bpm_lfo->lfo == NULL)
    {
      free(bpm_lfo);
      return NULL;
    }

  bpm_lfo->samplerate = SampleRate;

  return (LADSPA_Handle) bpm_lfo;
}

void activate_bpm_lfo(LADSPA_Handle Instance)
{
  bpm_lfo_t *bpm_lfo = (bpm_lfo_t *) Instance;

  bpm_lfo->output      = NULL;
  bpm_lfo->reset       = NULL;
  bpm_lfo->bpm         = NULL;
  bpm_lfo->rate_num    = NULL;
  bpm_lfo->rate_den    = NULL;
  bpm_lfo->amplitude   = NULL;
  bpm_lfo->phase       = NULL;
  bpm_lfo->lfo->curpos = 0;
  bpm_lfo->reset_flag  = 0;
}

void connect_bpm_lfo(LADSPA_Handle Instance,
                       unsigned long Port,
                       LADSPA_Data * DataLocation)
{
  bpm_lfo_t *bpm_lfo = (bpm_lfo_t *) Instance;

  switch (Port)
    {
    case BL_OUTPUT:
      bpm_lfo->output    = DataLocation; break;
    case BL_RESET:
      bpm_lfo->reset     = DataLocation; break;
    case BL_BPM:
      bpm_lfo->bpm       = DataLocation; break;
    case BL_RATE_NUM:
      bpm_lfo->rate_num  = DataLocation; break;
    case BL_RATE_DEN:
      bpm_lfo->rate_den  = DataLocation; break;
    case BL_AMPLITUDE:
      bpm_lfo->amplitude = DataLocation; break;
    case BL_PHASE:
      bpm_lfo->phase     = DataLocation; break;
    }
}

void run_bpm_lfo(LADSPA_Handle Instance,
                 unsigned long SampleCount)
{
  bpm_lfo_t *bpm_lfo = (bpm_lfo_t *) Instance;
  unsigned int idx;

  bpm_lfo->lfo->pad
    = (bpm_lfo->rate_den[0] * bpm_lfo->bpm[0] * bpm_lfo->lfo->bufsz)
    / (bpm_lfo->rate_num[0] * 60 * bpm_lfo->samplerate);
  for (idx = 0; idx < SampleCount; idx++)
    {
      /* Handling reset */
      if (bpm_lfo->reset_flag == 0)
        {
          if (bpm_lfo->reset[idx] == 1.0)
            {
              bpm_lfo->lfo->curpos
                = bpm_lfo->phase[0] * bpm_lfo->lfo->bufsz / 24;
              bpm_lfo->reset_flag = 1;
            }
        }
      else
        {
          if (bpm_lfo->reset[idx] != 1.0)
            bpm_lfo->reset_flag = 0;
        }
      /* Write the sample frame */
      bpm_lfo->output[idx]
        = next_lfo_sample(bpm_lfo->lfo) * bpm_lfo->amplitude[0];
    }
}

void cleanup_bpm_lfo(LADSPA_Handle Instance)
{
  bpm_lfo_t *bpm_lfo = (bpm_lfo_t *) Instance;

  delete_handle_lfo(bpm_lfo->lfo);
  free(bpm_lfo);
}

void set_output_port(char **PortName,
                     LADSPA_PortDescriptor *PortDescriptors,
                     LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("output");
  *PortDescriptors = (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO);
  PortRangeHints->HintDescriptor = 0;
}

void set_reset_port(char **PortName,
                    LADSPA_PortDescriptor *PortDescriptors,
                    LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Reset");
  *PortDescriptors = (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO);
  PortRangeHints->HintDescriptor = LADSPA_HINT_TOGGLED;
}

void set_bpm_port(char **PortName,
                  LADSPA_PortDescriptor *PortDescriptors,
                  LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Rate BPM");
  *PortDescriptors = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  PortRangeHints->LowerBound = 30;
  PortRangeHints->UpperBound = 300;
  PortRangeHints->HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                    | LADSPA_HINT_BOUNDED_ABOVE
                                    | LADSPA_HINT_DEFAULT_100);
}

void set_rate_num_port(char **PortName,
                       LADSPA_PortDescriptor *PortDescriptors,
                       LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Rate Numerator");
  *PortDescriptors = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  PortRangeHints->LowerBound = 1;
  PortRangeHints->UpperBound = 16;
  PortRangeHints->HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                    | LADSPA_HINT_BOUNDED_ABOVE
                                    | LADSPA_HINT_INTEGER);
}

void set_rate_den_port(char **PortName,
                       LADSPA_PortDescriptor *PortDescriptors,
                       LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Rate Denominator");
  *PortDescriptors = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  PortRangeHints->LowerBound = 1;
  PortRangeHints->UpperBound = 24;
  PortRangeHints->HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                    | LADSPA_HINT_BOUNDED_ABOVE
                                    | LADSPA_HINT_INTEGER);
}

void set_amplitude_port(char **PortName,
                        LADSPA_PortDescriptor *PortDescriptors,
                        LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Amplitude");
  *PortDescriptors = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  PortRangeHints->LowerBound = 0.0;
  PortRangeHints->UpperBound = 1.0;
  PortRangeHints->HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                    | LADSPA_HINT_BOUNDED_ABOVE
                                    | LADSPA_HINT_DEFAULT_MAXIMUM);
}

void set_phase_port(char **PortName,
                    LADSPA_PortDescriptor *PortDescriptors,
                    LADSPA_PortRangeHint *PortRangeHints)
{
  *PortName = strdup("Phase");
  *PortDescriptors = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  PortRangeHints->LowerBound = 0;
  PortRangeHints->UpperBound = 24;
  PortRangeHints->HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                    | LADSPA_HINT_BOUNDED_ABOVE
                                    | LADSPA_HINT_DEFAULT_MINIMUM);
}

LADSPA_Descriptor * g_psDescriptor = NULL;

void _init(void)
{
  char **PortNames = NULL;
  LADSPA_PortDescriptor *PortDescriptors;
  LADSPA_PortRangeHint *PortRangeHints;

  g_psDescriptor = (LADSPA_Descriptor *) malloc(sizeof(LADSPA_Descriptor));
  if (g_psDescriptor == NULL)
    return;
  bzero(g_psDescriptor, sizeof(LADSPA_Descriptor));

  g_psDescriptor->UniqueID   = 3725; /* TODO */
  g_psDescriptor->Label      = strdup("bpm_lfo");
  g_psDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE
    | LADSPA_PROPERTY_INPLACE_BROKEN;
  g_psDescriptor->Name
    = strdup("Sinusoid LFO that use bpm,"
             " a numerator and a denominator to setting rate");
  g_psDescriptor->Maker     = strdup("Gilbert");
  g_psDescriptor->Copyright = strdup("None");
  g_psDescriptor->PortCount = BL_PORT_SIZE;

  PortNames       = calloc(BL_PORT_SIZE, sizeof(char *));
  PortDescriptors = calloc(BL_PORT_SIZE, sizeof(LADSPA_PortDescriptor));
  PortRangeHints  = calloc(BL_PORT_SIZE, sizeof(LADSPA_PortRangeHint));

  set_output_port(&PortNames[BL_OUTPUT],
                  &PortDescriptors[BL_OUTPUT],
                  &PortRangeHints[BL_OUTPUT]);
  set_reset_port(&PortNames[BL_RESET],
                 &PortDescriptors[BL_RESET],
                 &PortRangeHints[BL_RESET]);
  set_bpm_port(&PortNames[BL_BPM],
               &PortDescriptors[BL_BPM],
               &PortRangeHints[BL_BPM]);
  set_rate_num_port(&PortNames[BL_RATE_NUM],
                    &PortDescriptors[BL_RATE_NUM],
                    &PortRangeHints[BL_RATE_NUM]);
  set_rate_den_port(&PortNames[BL_RATE_DEN],
                    &PortDescriptors[BL_RATE_DEN],
                    &PortRangeHints[BL_RATE_DEN]);
  set_amplitude_port(&PortNames[BL_AMPLITUDE],
                     &PortDescriptors[BL_AMPLITUDE],
                     &PortRangeHints[BL_AMPLITUDE]);
  set_phase_port(&PortNames[BL_PHASE],
                 &PortDescriptors[BL_PHASE],
                 &PortRangeHints[BL_PHASE]);

  g_psDescriptor->PortNames       = (const char **) PortNames;
  g_psDescriptor->PortDescriptors = PortDescriptors;
  g_psDescriptor->PortRangeHints  = PortRangeHints;

  g_psDescriptor->instantiate = instantiate_bpm_lfo;
  g_psDescriptor->connect_port = connect_bpm_lfo;
  g_psDescriptor->activate = activate_bpm_lfo;
  g_psDescriptor->run = run_bpm_lfo;
  g_psDescriptor->deactivate = NULL;
  g_psDescriptor->cleanup = cleanup_bpm_lfo;
}

void _fini()
{
  unsigned int idx;

  if (g_psDescriptor) {
    free((void *) g_psDescriptor->Label);
    free((void *) g_psDescriptor->Name);
    free((void *) g_psDescriptor->Maker);
    free((void *) g_psDescriptor->Copyright);
    free((void *) g_psDescriptor->PortDescriptors);
    for (idx = 0; idx < g_psDescriptor->PortCount; idx++)
      free((void *) g_psDescriptor->PortNames[idx]);
    free((void *) g_psDescriptor->PortNames);
    free((void *) g_psDescriptor->PortRangeHints);
    free((void *) g_psDescriptor);
  }
}

const LADSPA_Descriptor *ladspa_descriptor(unsigned long Index)
{
  if (Index == 0)
    return g_psDescriptor;
  else
    return NULL;
}
