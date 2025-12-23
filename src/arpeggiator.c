#include "arpeggiator.h"
#include "synth.h" // Include synth.h for synth_note_on/off declarations
#include <stdlib.h>
#include <string.h>

void arpeggiator_init(Arpeggiator *arp) {
  arp->enabled = 0;
  arp->mode = ARP_UP;
  memset(arp->pattern, 0, sizeof(arp->pattern)); // Initialize pattern to all zeros
  arp->step = 0;
  arp->tempo = 120.0f;
  arp->rate = RATE_EIGHTH;
  arp->step_phase = 0.0f;
  arp->held_count = 0;
  memset(arp->held_notes, 0, sizeof(arp->held_notes));
  arp->last_step_time = 0.0f;
  arp->polyphonic = 1;
  arp->hold = 1;
  arp->octave = 0;
  arp->octaves = 3;
  
  // Initialize chord generation parameters
  arp->chord_type = CHORD_MAJOR;
  arp->add_6 = 0;
  arp->add_m7 = 0;
  arp->add_M7 = 0;
  arp->add_9 = 0;
  arp->voicing = 0;
  
  // Initialize gate parameters
  arp->gate_length = 0.8f; // Default 80% gate length
  
  for (int i = 0; i < 16; ++i) {
      arp->active_arpeggiated_notes[i].active = 0;
  }
}

void arpeggiator_set_param(Arpeggiator *arp, const char *param, float value, struct Synth *synth) {
  if (!strcmp(param, "enabled")) {
    int old_enabled = arp->enabled;
    arp->enabled = (int)value;
    // If arpeggiator is being turned off, clear all playing notes
    if (old_enabled && !arp->enabled) {
      arpeggiator_clear_notes_with_synth(arp, synth);
    }
  }
  else if (!strcmp(param, "mode"))
    arp->mode = (ArpMode)((int)value);
  else if (!strcmp(param, "tempo"))
    arp->tempo = value;
  else if (!strcmp(param, "rate"))
    arp->rate = (ArpRate)((int)value);
  else if (!strcmp(param, "polyphonic"))
    arp->polyphonic = (int)value;
  else if (!strcmp(param, "hold")) {
    int old_hold = arp->hold;
    arp->hold = (int)value;
    // If hold is being turned off, clear all notes
    if (old_hold && !arp->hold) {
      arpeggiator_clear_notes(arp);
    }
  }
  else if (!strcmp(param, "octave"))
    arp->octave = (int)value;
  else if (!strcmp(param, "octaves"))
    arp->octaves = (int)value;
  else if (!strcmp(param, "chord_type"))
    arp->chord_type = (ChordType)((int)value);
  else if (!strcmp(param, "add_6"))
    arp->add_6 = (int)value;
  else if (!strcmp(param, "add_m7"))
    arp->add_m7 = (int)value;
  else if (!strcmp(param, "add_M7"))
    arp->add_M7 = (int)value;
  else if (!strcmp(param, "add_9"))
    arp->add_9 = (int)value;
  else if (!strcmp(param, "voicing"))
    arp->voicing = (int)value;
  else if (!strcmp(param, "gate_length"))
    arp->gate_length = value;
}

void arpeggiator_note_on(Arpeggiator *arp, int note) {
  for (int i = 0; i < arp->held_count; ++i)
    if (arp->held_notes[i] == note)
      return;
  if (arp->held_count < 16)
    arp->held_notes[arp->held_count++] = note;
}

void arpeggiator_note_off(Arpeggiator *arp, int note) {
  // If hold is enabled, don't remove notes on key release
  if (arp->hold) {
    return;
  }
  
  for (int i = 0; i < arp->held_count; ++i) {
    if (arp->held_notes[i] == note) {
      for (int j = i; j < arp->held_count - 1; ++j)
        arp->held_notes[j] = arp->held_notes[j + 1];
      arp->held_count--;
      break;
    }
  }
}

void arpeggiator_clear_notes(Arpeggiator *arp) {
  arp->held_count = 0;
  memset(arp->held_notes, 0, sizeof(arp->held_notes));
  
  // Turn off all currently playing arpeggiated notes
  for (int i = 0; i < 16; ++i) {
    if (arp->active_arpeggiated_notes[i].active) {
      arp->active_arpeggiated_notes[i].active = 0;
    }
  }
}

void arpeggiator_clear_notes_with_synth(Arpeggiator *arp, struct Synth *synth) {
  arp->held_count = 0;
  memset(arp->held_notes, 0, sizeof(arp->held_notes));
  
  // Turn off all currently playing arpeggiated notes with synth
  for (int i = 0; i < 16; ++i) {
    if (arp->active_arpeggiated_notes[i].active) {
      synth_note_off(synth, arp->active_arpeggiated_notes[i].note);
      arp->active_arpeggiated_notes[i].active = 0;
    }
  }
}

static int order_compare(const void *a, const void *b) {
  return (*(int *)a) - (*(int *)b);
}

int arpeggiator_step(Arpeggiator *arp, float samplerate, struct Synth *synth) {
  if (!arp->enabled || arp->held_count == 0)
    return -1;

  // Calculate step time based on rate
  float rate_divisor = 1.0f;
  switch (arp->rate) {
    case RATE_QUARTER:   rate_divisor = 1.0f; break;  // 1/4 notes
    case RATE_EIGHTH:    rate_divisor = 2.0f; break;  // 1/8 notes (default)
    case RATE_SIXTEENTH: rate_divisor = 4.0f; break;  // 1/16 notes
    case RATE_THIRTYSECOND: rate_divisor = 8.0f; break; // 1/32 notes
  }
  float step_time = 60.0f / arp->tempo / rate_divisor;

  // Turn off all notes from previous step before playing new ones
  for (int i = 0; i < 16; ++i) {
      if (arp->active_arpeggiated_notes[i].active) {
          synth_note_off(synth, arp->active_arpeggiated_notes[i].note);
          arp->active_arpeggiated_notes[i].active = 0;
      }
  }

  int idx = 0;
  int notes[16];
  memcpy(notes, arp->held_notes, sizeof(int) * arp->held_count);

  if (arp->polyphonic) {
      // Play notes cycling through octaves in polyphonic mode
      int total_notes = arp->held_count * arp->octaves;
      int notes_to_play[16];
      int note_count = 0;
      
      // Calculate which note to play this step based on current arpeggiator step
      // We cycle through all (held_notes × octaves) combinations
      int note_index = arp->step % total_notes;
      
      // Calculate which held note and octave this should be
      int held_note_index = note_index / arp->octaves;
      int octave_index = note_index % arp->octaves;
      
      if (held_note_index < arp->held_count) {
          int base_note = notes[held_note_index];
          int note_to_play = base_note + (arp->octave * 12) + (octave_index * 12);
          
          // Find a free slot for the new note
          int free_slot = -1;
          for (int j = 0; j < 16; ++j) {
              if (!arp->active_arpeggiated_notes[j].active) {
                  free_slot = j;
                  break;
              }
          }
           if (free_slot != -1) {
               arp->active_arpeggiated_notes[free_slot].note = note_to_play;
               arp->active_arpeggiated_notes[free_slot].remaining_duration = step_time * arp->gate_length; // Gate length controls note duration
               arp->active_arpeggiated_notes[free_slot].active = 1;
               synth_note_on(synth, note_to_play, 0.8f);
               note_count++;
          }
      }
  } else {
      // Monophonic behavior (existing logic)
      switch (arp->mode) {
      case ARP_CHORD: {
          // Generate chord from the first held note (root)
          if (arp->held_count > 0) {
              int chord_notes[8];
              int note_count = arpeggiator_generate_chord(arp, notes[0], chord_notes, 8);
              
              // Apply octave spread if requested
              int notes_to_play[8];
              int actual_count = 0;
              for (int i = 0; i < note_count && actual_count < 8; ++i) {
                  int base_note = chord_notes[i];
                  for (int oct = 0; oct < arp->octaves && actual_count < 8; ++oct) {
                      notes_to_play[actual_count++] = base_note + (arp->octave * 12) + (oct * 12);
                  }
              }
              
              // Play all chord notes simultaneously
              for (int i = 0; i < actual_count; ++i) {
                  int free_slot = -1;
                  for (int j = 0; j < 16; ++j) {
                      if (!arp->active_arpeggiated_notes[j].active) {
                          free_slot = j;
                          break;
                      }
                  }
                   if (free_slot != -1) {
                       arp->active_arpeggiated_notes[free_slot].note = notes_to_play[i];
                       arp->active_arpeggiated_notes[free_slot].remaining_duration = step_time * arp->gate_length;
                       arp->active_arpeggiated_notes[free_slot].active = 1;
                       synth_note_on(synth, notes_to_play[i], 0.8f);
                   }
              }
          }
          break;
      }
      case ARP_UP:
        qsort(notes, arp->held_count, sizeof(int), order_compare);
        idx = arp->step % arp->held_count;
        break;
      case ARP_DOWN:
        qsort(notes, arp->held_count, sizeof(int), order_compare);
        idx = arp->held_count - 1 - (arp->step % arp->held_count);
        break;
      case ARP_ORDER:
        idx = arp->step % arp->held_count;
        break;
      case ARP_RANDOM:
        idx = rand() % arp->held_count;
        break;
      case ARP_PENDULUM:
        // Pendulum mode: play up then down, creating a bounce pattern
        // For held notes [n0, n1, n2, n3], play: n0, n1, n2, n3, n2, n1, n0, n1, n2, n3...
        qsort(notes, arp->held_count, sizeof(int), order_compare);
        {
          int cycle_length = (arp->held_count * 2) - 2; // n0,n1,n2,n3,n2,n1 = 5 for 4 notes
          if (arp->held_count == 1) {
            cycle_length = 1; // Single note just repeats
          }
          int step_in_cycle = arp->step % cycle_length;
          
          if (step_in_cycle < arp->held_count) {
            // Ascending part
            idx = step_in_cycle;
          } else {
            // Descending part
            idx = (arp->held_count - 1) - (step_in_cycle - arp->held_count + 1);
          }
        }
        break;
      default:
        idx = 0;
      }
      
      if (arp->mode != ARP_CHORD) {
          int note_to_play = notes[idx];
          
          // Find a free slot for the new note
          int free_slot = -1;
          for (int j = 0; j < 16; ++j) {
              if (!arp->active_arpeggiated_notes[j].active) {
                  free_slot = j;
                  break;
              }
          }
           if (free_slot != -1) {
               arp->active_arpeggiated_notes[free_slot].note = note_to_play;
               arp->active_arpeggiated_notes[free_slot].remaining_duration = step_time * arp->gate_length; // Gate length controls note duration
               arp->active_arpeggiated_notes[free_slot].active = 1;
               synth_note_on(synth, note_to_play, 0.8f);
           }
      }
  }

  arp->step++;
  return 0; // Return 0 to indicate success
}

int arpeggiator_generate_chord(const Arpeggiator *arp, int root_note, int *chord_notes, int max_notes) {
    if (!chord_notes || max_notes < 8) return 0;
    
    int note_count = 0;
    int root_in_octave = root_note % 12;
    int octave = root_note / 12;
    
    // Basic chord tones based on chord type
    switch (arp->chord_type) {
        case CHORD_MAJOR:
            chord_notes[note_count++] = root_note;
            chord_notes[note_count++] = root_note + 4; // Major third
            chord_notes[note_count++] = root_note + 7; // Perfect fifth
            break;
        case CHORD_MINOR:
            chord_notes[note_count++] = root_note;
            chord_notes[note_count++] = root_note + 3; // Minor third
            chord_notes[note_count++] = root_note + 7; // Perfect fifth
            break;
        case CHORD_SUS:
            chord_notes[note_count++] = root_note;
            chord_notes[note_count++] = root_note + 5; // Perfect fourth
            chord_notes[note_count++] = root_note + 7; // Perfect fifth
            break;
        case CHORD_DIM:
            chord_notes[note_count++] = root_note;
            chord_notes[note_count++] = root_note + 3; // Minor third
            chord_notes[note_count++] = root_note + 6; // Diminished fifth
            break;
    }
    
    // Add extensions if enabled
    if (arp->add_6 && note_count < max_notes) {
        chord_notes[note_count++] = root_note + 9; // Sixth
    }
    if (arp->add_m7 && note_count < max_notes) {
        chord_notes[note_count++] = root_note + 10; // Minor seventh
    }
    if (arp->add_M7 && note_count < max_notes) {
        chord_notes[note_count++] = root_note + 11; // Major seventh
    }
    if (arp->add_9 && note_count < max_notes) {
        chord_notes[note_count++] = root_note + 14; // Ninth (one octave above second)
    }
    
    // Apply voicing/inversion if specified
    if (arp->voicing > 0 && note_count > 1) {
        int voicing_level = arp->voicing;
        
        // Voicing levels 1-8: Basic inversions (rotate notes up by octaves)
        if (voicing_level <= 8) {
            int num_inversions = voicing_level % note_count;
            for (int i = 0; i < num_inversions; ++i) {
                int bass_note = chord_notes[0];
                // Move bass note up one octave
                for (int j = 0; j < note_count - 1; ++j) {
                    chord_notes[j] = chord_notes[j + 1];
                }
                chord_notes[note_count - 1] = bass_note + 12;
            }
        }
        // Voicing levels 9-12: Drop voicings (drop 2, drop 3, etc.)
        else if (voicing_level <= 12 && note_count >= 3) {
            int drop_type = voicing_level - 8; // 1=drop2, 2=drop3, 3=drop2&4, 4=drop2&3
            int temp_notes[8];
            memcpy(temp_notes, chord_notes, sizeof(int) * note_count);
            
            switch (drop_type) {
                case 1: // Drop 2
                    if (note_count >= 2) {
                        chord_notes[0] = temp_notes[0];
                        chord_notes[1] = temp_notes[1] - 12; // Second down octave
                        for (int i = 2; i < note_count; ++i) {
                            chord_notes[i] = temp_notes[i];
                        }
                    }
                    break;
                case 2: // Drop 3
                    if (note_count >= 3) {
                        chord_notes[0] = temp_notes[0];
                        chord_notes[1] = temp_notes[1];
                        chord_notes[2] = temp_notes[2] - 12; // Third down octave
                        for (int i = 3; i < note_count; ++i) {
                            chord_notes[i] = temp_notes[i];
                        }
                    }
                    break;
                case 3: // Drop 2&4
                    if (note_count >= 4) {
                        chord_notes[0] = temp_notes[0];
                        chord_notes[1] = temp_notes[1] - 12; // Second down octave
                        chord_notes[2] = temp_notes[2];
                        chord_notes[3] = temp_notes[3] - 12; // Fourth down octave
                        for (int i = 4; i < note_count; ++i) {
                            chord_notes[i] = temp_notes[i];
                        }
                    }
                    break;
                case 4: // Drop 2&3
                    if (note_count >= 3) {
                        chord_notes[0] = temp_notes[0];
                        chord_notes[1] = temp_notes[1] - 12; // Second down octave
                        chord_notes[2] = temp_notes[2] - 12; // Third down octave
                        for (int i = 3; i < note_count; ++i) {
                            chord_notes[i] = temp_notes[i];
                        }
                    }
                    break;
            }
        }
        // Voicing levels 13-16: Open/Spread voicings
        else {
            int spread_type = voicing_level - 12; // 1=open harmony, 2=wide spread, 3=clustered, 4=alternating octaves
            int temp_notes[8];
            memcpy(temp_notes, chord_notes, sizeof(int) * note_count);
            
            switch (spread_type) {
                case 1: // Open harmony (spread notes across larger range)
                    for (int i = 0; i < note_count; ++i) {
                        chord_notes[i] = temp_notes[i] + (i * 5); // Spread by perfect 4ths
                    }
                    break;
                case 2: // Wide spread (alternating octaves)
                    for (int i = 0; i < note_count; ++i) {
                        chord_notes[i] = temp_notes[i] + ((i % 2) * 12); // Every other note up octave
                    }
                    break;
                case 3: // Clustered voicing (close harmony)
                    // Keep notes close together in one octave range
                    for (int i = 1; i < note_count; ++i) {
                        while (chord_notes[i] - chord_notes[i-1] > 12) {
                            chord_notes[i] -= 12;
                        }
                        while (chord_notes[i] - chord_notes[i-1] < 0) {
                            chord_notes[i] += 12;
                        }
                    }
                    break;
                case 4: // Alternating octaves (root position with octave displacement)
                    for (int i = 0; i < note_count; ++i) {
                        chord_notes[i] = temp_notes[i] + ((i / 2) * 12); // Groups of 2 in same octave
                    }
                    break;
            }
        }
    }
    
    return note_count;
}

const char *arpeggiator_rate_str(const Arpeggiator *arp) {
    switch (arp->rate) {
    case RATE_QUARTER:
        return "1/4";
    case RATE_EIGHTH:
        return "1/8";
    case RATE_SIXTEENTH:
        return "1/16";
    case RATE_THIRTYSECOND:
        return "1/32";
    default:
        return "-";
    }
}

const char *arpeggiator_chord_str(const Arpeggiator *arp) {
    switch (arp->chord_type) {
    case CHORD_MAJOR:
        return "MAJ";
    case CHORD_MINOR:
        return "MIN";
    case CHORD_SUS:
        return "SUS";
    case CHORD_DIM:
        return "DIM";
    default:
        return "-";
    }
}

const char *arpeggiator_mode_str(const Arpeggiator *arp) {
  switch (arp->mode) {
  case ARP_OFF:
    return "OFF";
  case ARP_CHORD:
    return "CHORD";
  case ARP_UP:
    return "UP";
  case ARP_DOWN:
    return "DOWN";
  case ARP_UP_DOWN:
    return "UP_DOWN";
  case ARP_PENDULUM:
    return "PENDULUM";
  case ARP_CONVERGE:
    return "CONVERGE";
  case ARP_DIVERGE:
    return "DIVERGE";
  case ARP_LEAPFROG:
    return "LEAPFROG";
  case ARP_THUMB_UP:
    return "THUMB_UP";
  case ARP_THUMB_DOWN:
    return "THUMB_DOWN";
  case ARP_PINKY_UP:
    return "PINKY_UP";
  case ARP_PINKY_DOWN:
    return "PINKY_DOWN";
  case ARP_REPEAT:
    return "REPEAT";
  case ARP_RANDOM:
    return "RANDOM";
  case ARP_RANDOM_WALK:
    return "RANDOM_WALK";
  case ARP_SHUFFLE:
    return "SHUFFLE";
  case ARP_ORDER:
    return "ORDER";
  default:
    return "-";
  }
}
