/*
inlets
	fasor[0,1] (expand o fasor)
	list notas
	list index
	list tiempo
outlets
	numero
*/

#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <time.h>
#include <stdlib.h>

#define MAXIMUM_SEQUENCE_LENGTH 512

#define DEFAULT_MSBEAT 500

enum PLAY_MODES { DONT_PLAY, FORWARD, BACKWARD, FOR_AND_BACK, BACK_AND_FOR };

enum INLETS { I_MSBEAT, I_NOTES, I_INDEX, I_TEMPOS, NUM_INLETS };
enum OUTLETS { O_NOTE, O_FLAG, NUM_OUTLETS };

static t_class* fl_numseq_class;

typedef struct _fl_numseq {
	t_pxobject obj;

	long m_in;
	void *m_proxy1;
	void *m_proxy2;
	void *m_proxy3;
	void *m_outlet;
	void *b_outlet;

	void *note_clock;
	void *final_clock;

	short msbeat_connected;

	float fs;

	float *note_palette;
	long note_palette_length;
	long *index_sequence;
	float *beatstart_sequence;
	long sequence_length;
	long n_seq;

	float note_out;
	short note_palette_init;
	short index_sequence_init;
	short beatstart_sequence_init;

	short multigate_mode;
		
	long samps_beat;

	short play_mode;
	short play_state;
	short first_seq_done;

	long samps_count;
	long samps_mgcount;
	long samps_bar;
	float beats_bar;

} t_fl_numseq;

void* fl_numseq_new(t_symbol* s, short argc, t_atom* argv);
void fl_numseq_assist(t_fl_numseq* x, void* b, long msg, long arg, char* dst);
void fl_numseq_free(t_fl_numseq *x);

void fl_numseq_lists(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv);
void fl_numseq_multigate_mode(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv);
void fl_numseq_manual_sequence(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv);
void fl_numseq_play_mode(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv);
void fl_numseq_bar_size(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv);

void fl_numseq_msbeat(t_fl_numseq *x, double n);

void fl_numseq_play_seq(t_fl_numseq *x);
void fl_numseq_out_note(t_fl_numseq *x);
void fl_numseq_final_flag(t_fl_numseq *x);

void fl_numseq_dsp64(t_fl_numseq *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void fl_numseq_perform64(t_fl_numseq *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs, long vectorsize, long flags, void *userparams);