#include "flnumseq~.h"

void ext_main(void *r)
{
	t_class *c = class_new("flnumseq~", (method)fl_numseq_new, (method)fl_numseq_free, sizeof(t_fl_numseq), 0, A_GIMME, 0);
	class_addmethod(c, (method)fl_numseq_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)fl_numseq_assist, "assist", A_CANT, 0);

	class_addmethod(c, (method)fl_numseq_lists, "list", A_GIMME, 0);
	class_addmethod(c, (method)fl_numseq_multigate_mode, "multigate_mode", A_GIMME, 0);
	class_addmethod(c, (method)fl_numseq_manual_sequence, "step", A_GIMME, 0);
	class_addmethod(c, (method)fl_numseq_play_mode, "play_mode", A_GIMME, 0);
	class_addmethod(c, (method)fl_numseq_bar_size, "bar_size", A_GIMME, 0);

	class_addmethod(c, (method)fl_numseq_msbeat, "float", A_FLOAT, 0);
	
	class_addmethod(c, (method)fl_numseq_play_seq, "bang", 0);

	class_dspinit(c);
	class_register(CLASS_BOX, c);
	fl_numseq_class = c;

	return;
}

void *fl_numseq_new(t_symbol *s, short argc, t_atom *argv)
{
	t_fl_numseq *x = (t_fl_numseq *)object_alloc(fl_numseq_class);
	
	x->m_proxy3 = proxy_new((t_object*)x, 3, &x->m_in);
	x->m_proxy2 = proxy_new((t_object*)x, 2, &x->m_in);
	x->m_proxy1 = proxy_new((t_object*)x, 1, &x->m_in);
	dsp_setup((t_pxobject*)x, 1);

	x->b_outlet = outlet_new((t_object *)x, "bang");
	x->m_outlet = outlet_new((t_object*)x, "float");
	x->obj.z_misc |= Z_NO_INPLACE;

	x->note_clock = clock_new(x, (method)fl_numseq_out_note);
	x->final_clock = clock_new(x, (method)fl_numseq_final_flag);

	x->fs = sys_getsr();

	x->note_palette_length = -1;
	x->sequence_length = -1;
	x->note_palette = (float *)sysmem_newptr(MAXIMUM_SEQUENCE_LENGTH * sizeof(float));
	if (!x->note_palette) { object_error((t_object*)x, "error: out of memory for note_sequence array"); }
	x->index_sequence = (long *)sysmem_newptr(MAXIMUM_SEQUENCE_LENGTH * sizeof(long));
	if (!x->index_sequence) { object_error((t_object *)x, "error: out of memory for index_sequence array"); }
	x->beatstart_sequence = (float *)sysmem_newptr(MAXIMUM_SEQUENCE_LENGTH * sizeof(float));
	if (!x->beatstart_sequence) { object_error((t_object *)x, "error: out of memory for tempo_sequence array"); }

	for (long i = 0; i < MAXIMUM_SEQUENCE_LENGTH; i++) {
		x->note_palette[i] = 0.;
		x->index_sequence[i] = 0;
		x->beatstart_sequence[i] = 0.;
	}
	x->note_out = 0.;

	x->note_palette_init = 0;
	x->index_sequence_init = 0;
	x->beatstart_sequence_init = 0;

	x->samps_beat = (long)(DEFAULT_MSBEAT * 0.001 * x->fs);
	x->multigate_mode = 0;
	x->play_mode = FORWARD;
	x->play_state = DONT_PLAY;

	x->first_seq_done = 0;
	x->samps_count = 0;
	x->samps_mgcount = 0;
	x->beats_bar = 4.f;
	x->samps_bar = (long)(x->beats_bar * x->samps_beat);
	x->n_seq = 0;

	return x;
}

void fl_numseq_assist(t_fl_numseq *x, void *b, long msg, long arg, char *dst)
{
	if (msg == ASSIST_INLET) {
		switch (arg) {
		case I_MSBEAT: sprintf(dst, "(float) beat period [ms]"); break;
		case I_NOTES: sprintf(dst, "(list) notes palette"); break;
		case I_INDEX: sprintf(dst, "(list) index sequence"); break;
		case I_TEMPOS: sprintf(dst, "(list) beat duration sequence"); break;
		}
	}
	else if (msg == ASSIST_OUTLET) {
		switch (arg) {
		case O_NOTE: sprintf(dst, "(float) note"); break;
		case O_FLAG: sprintf(dst, "(bang) final flag"); break;
		}
	}
}

void fl_numseq_free(t_fl_numseq *x)
{
	dsp_free((t_pxobject *)x);

	clock_free(x->note_clock);
	clock_free(x->final_clock);

	sysmem_freeptr(x->note_palette);
	sysmem_freeptr(x->index_sequence);
	sysmem_freeptr(x->beatstart_sequence);
}

void fl_numseq_lists(t_fl_numseq* x, t_symbol* msg, short argc, t_atom* argv)
{
	long ac = argc;
	t_atom *ap = argv;

	switch (proxy_getinlet((t_object *)x)) {
	case I_MSBEAT:
		break;

	case I_NOTES: 
		if (ac > MAXIMUM_SEQUENCE_LENGTH) {
			ac = MAXIMUM_SEQUENCE_LENGTH;
			object_warn((t_object *)x, "max list size reached: %d", MAXIMUM_SEQUENCE_LENGTH);
		}

		for (long i = 0; i < ac; i++) {
			x->note_palette[i] = (float)atom_getfloat(ap + i);
		}
		x->note_palette_length = ac; 
		x->note_palette_init = 1;
		break;

	case I_INDEX:  
		if (ac > MAXIMUM_SEQUENCE_LENGTH) {
			ac = MAXIMUM_SEQUENCE_LENGTH;
			object_warn((t_object *)x, "max list size reached: %d", MAXIMUM_SEQUENCE_LENGTH);
		}

		for (long i = 0; i < MAXIMUM_SEQUENCE_LENGTH; i++) {
			if (i < ac) { x->index_sequence[i] = (long)atom_getlong(ap + i); }
			else { x->index_sequence[i] = 0; }
		}
		x->index_sequence_init = 1;
		break;

	case I_TEMPOS: 
		if (ac > MAXIMUM_SEQUENCE_LENGTH) {
			ac = MAXIMUM_SEQUENCE_LENGTH;
			object_warn((t_object *)x, "max list size reached: %d", MAXIMUM_SEQUENCE_LENGTH);
		}

		long sum = 0;
		for (long i = 0; i < ac; i++) {
			x->beatstart_sequence[i] = (float)atom_getfloat(ap + i); 
		}
		x->sequence_length = ac;
		x->beatstart_sequence_init = 1;
		break;

	default: 
		break;
	}
}

void fl_numseq_multigate_mode(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv)
{
	long ac = argc;
	t_atom *ap = argv;
	short state = 0;

	if (ac != 1) { object_error((t_object *)x, "[1 arg] 1/0 state (on/off)"); return; }
	if(atom_gettype(ap)!=A_FLOAT && atom_gettype(ap) != A_LONG){ object_error((t_object *)x, "state must be a number"); return; }
	
	state = (short)atom_getlong(argv);
	x->multigate_mode = state ? 1 : 0;	
}

void fl_numseq_play_mode(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv)
{
	long ac = argc;
	t_atom *ap = argv;
	short mode = 0;

	if (ac != 1) { object_error((t_object *)x, "[1 arg] play mode (1-4)"); return; }
	if (atom_gettype(ap) != A_FLOAT && atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "play mode must be a number"); return; }

	mode = (short)atom_getlong(argv);
	x->play_mode = MAX(MIN(mode, 4), 1);
}

void fl_numseq_bar_size(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv)
{
	long ac = argc;
	t_atom *ap = argv;
	long bar_size = 0;

	if (ac != 1) { object_error((t_object *)x, "[1 arg] bar size (number of periods)"); return; }
	if (atom_gettype(ap) != A_FLOAT && atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "bar size must be a number"); return; }

	bar_size = (long)atom_getlong(argv);
	x->beats_bar = (float)MAX(bar_size, 1.);
	x->samps_bar = (long)(x->beats_bar * x->samps_beat);
}

void fl_numseq_play_seq(t_fl_numseq *x) 
{
	if (!x->beatstart_sequence_init ||
		!x->index_sequence_init ||
		!x->note_palette_init) {
		return;
	}

	switch (x->play_mode) {
	case DONT_PLAY:
		break;
	case FORWARD:	
		x->samps_count = 0;
		x->n_seq = 0;
		break;
	case BACKWARD:
		x->samps_count = x->samps_bar;
		x->n_seq = x->sequence_length - 1;
		break;
	case FOR_AND_BACK:
		x->samps_count = 0;
		x->n_seq = 0;
		x->first_seq_done = 0;
		break;
	case BACK_AND_FOR:
		x->samps_count = x->samps_bar - 1;
		x->n_seq = x->sequence_length - 1;
		x->first_seq_done = 0;
		break;
	default:
		break;
	}

	x->samps_mgcount = 0;

	x->play_state = x->play_mode;
}

void fl_numseq_out_note(t_fl_numseq* x)
{
	float note = x->note_out;
	outlet_float(x->m_outlet, note);
}

void fl_numseq_final_flag(t_fl_numseq* x)
{
	outlet_bang(x->b_outlet);
}

void fl_numseq_manual_sequence(t_fl_numseq *x, t_symbol *msg, short argc, t_atom *argv)
{
	long ac = argc;
	t_atom *ap = argv;

	if (ac != 1) { object_error((t_object *)x, "[1 arg] step index"); return; }
	if (atom_gettype(ap) != A_FLOAT && atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "step must be a number"); return; }
	
	long n = (long)atom_getlong(ap);
	
	if (n != n || n < 0) { return; }
	if (n > MAXIMUM_SEQUENCE_LENGTH) { object_error((t_object *)x, "index out of bounds"); return; }

	long index = x->index_sequence[n % x->sequence_length];
	float note = x->note_palette[index % x->note_palette_length];
	outlet_float(x->m_outlet, note);
}

void fl_numseq_msbeat(t_fl_numseq *x, double num)
{
	float n = (float)num;

	if (n != n || n < 0) { return; }

	x->samps_beat = (long)(x->fs * 0.001 * n);
	x->samps_bar = (long)(x->beats_bar * x->samps_beat);
}

void fl_numseq_dsp64(t_fl_numseq* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
	x->msbeat_connected = count[0];

	if (x->fs != samplerate) {
		x->samps_count *= (long)(samplerate / x->fs);
		x->samps_beat *= (long)(samplerate / x->fs);
		x->fs = (float)samplerate;
	}

	object_method(dsp64, gensym("dsp_add64"), x, fl_numseq_perform64, 0, NULL);
}

void fl_numseq_perform64(t_fl_numseq* x, t_object* dsp64, double** inputs, long numinputs, double** outputs, long numoutputs, long vectorsize, long flags, void* userparams)
{
	long n = vectorsize;

	t_double *msbeat_signal = inputs[0];

	float *pnote_palette = x->note_palette;
	long *pindex_sequence = x->index_sequence;
	float *pbeatstart_sequence = x->beatstart_sequence;
	long note_pal_len = x->note_palette_length;
	long seq_len = x->sequence_length;

	long samps_beat = x->samps_beat;
	long samps_step = samps_beat;
	long samps_mgcount = x->samps_mgcount;
	long samps_count = x->samps_count;
	float beats_bar = x->beats_bar;
	long samps_bar = x->samps_bar;

	short msbeat_connected = x->msbeat_connected;
	short play_state = x->play_state;
	short first_seq_done = x->first_seq_done;
	short multigate = x->multigate_mode;

	float mfs = x->fs * 0.001f;

	long index = x->n_seq;
	
	while (n--) {
		if (msbeat_connected) { 
			samps_step = (long)(*msbeat_signal++ * mfs);
			samps_bar = (long)(beats_bar * samps_step);
		}

		switch (play_state) {
		case DONT_PLAY:
			break;

		case FORWARD:
			samps_count++;
			if (index < seq_len) {
				samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
				if (samps_count >= samps_step) {
					x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
					clock_delay(x->note_clock, 0);

					index++;
				}
			}
			if (samps_count >= samps_bar) {
				play_state = 0;
				clock_delay(x->final_clock, 0);
			}
			break;

		case BACKWARD:
			samps_count--;
			if (index >= 0) {
				samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
				if (samps_count < samps_step) {
					x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
					clock_delay(x->note_clock, 0);

					index--;
				}
			}
			if (samps_count < 0) {
				play_state = 0;
				clock_delay(x->final_clock, 0);
			}
			break;
		
		case FOR_AND_BACK:
			if (first_seq_done) { 
				samps_count--;
				if (index >= 0) {
					samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
					if (samps_count < samps_step) {
						x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
						clock_delay(x->note_clock, 0);

						index--;
					}
				}
				if (samps_count < 0) {
					play_state = 0;
					clock_delay(x->final_clock, 0);
				}
			}
			else { 
				samps_count++;
				if (index < seq_len) {
					samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
					if (samps_count >= samps_step) {
						x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
						clock_delay(x->note_clock, 0);

						index++;
					}
				}
				if (samps_count > samps_bar) {
					first_seq_done = 1;
					index = seq_len - 1;
					samps_count = samps_bar;
				}
			}
			break;
		
		case BACK_AND_FOR:
			if(first_seq_done){
				samps_count++;
				if (index < seq_len) {
					samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
					if (samps_count >= samps_step) {
						x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
						clock_delay(x->note_clock, 0);

						index++;
					}
				}
				if (samps_count >= samps_bar) {
					play_state = 0;
					clock_delay(x->final_clock, 0);
				}
			}
			else{
				samps_count--;
				if (index >= 0) {
					samps_step = (long)(samps_beat * pbeatstart_sequence[index]);
					if (samps_count < samps_step) {
						x->note_out = pnote_palette[pindex_sequence[index] % note_pal_len];
						clock_delay(x->note_clock, 0);

						index--;
					}
				}
				if (samps_count < 0) {
					first_seq_done = 1;
					samps_count = 0; 
					index = 0;
				}
			}
			break;

		default:
			break;
		}			

		if (multigate) {
			if (samps_mgcount++ >= samps_beat) {
				clock_delay(x->note_clock, 0);
				samps_mgcount = 0;
			}
		}
	}
	x->play_state = play_state;
	x->n_seq = index;
	x->first_seq_done = first_seq_done;
	x->samps_count = samps_count;
	x->samps_mgcount = samps_mgcount;
}
