/**
 * @file: main.cpp
 * @brief: Main src file for IMP project at FIT BUT 2021
 * @project: Piano simulator with play, record & replay
 * @authors: Jakub Bartko    xbartk07@stud.fit.vutbr.cz
 */

#include <Arduino.h>
#include <Keypad.h>

/*****************************************************************/
/**************************SCHEMA CONFIG**************************/
const int buzzer_pin = 18;
/***************pwm properties*******************/
// channel 0-15, resolution 1-16bits, freq limits depend on resolution
const int freq = 5000;
const int chnl = 0;
const int res = 8;
const int ROW_NUM = 4;    // four rows
const int COLUMN_NUM = 3; // three columns

/***************keypad config*******************/
char keys[ROW_NUM][COLUMN_NUM] = {{'1', '2', '3'},
                                  {'4', '5', '6'},
                                  {'7', '8', '9'},
                                  {'*', '0', '#'}};
byte pin_rows[ROW_NUM] = {14, 26, 25, 16};
byte pin_column[COLUMN_NUM] = {27, 12, 17};
Keypad keypad =
  Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

/****************************************************************/
/**************************PIANO CONFIG**************************/
const int n_tones = 21;
const int n_keys = 9;
const int C4 = 9;
int scale = C4;
const double tone_list[n_tones] = {
  110.0, 123.4708, 130.8128, 146.8324, 164.8138, 174.6141, 195.9977,
  220.0, 246.9417, 261.6256, 293.6648, 329.6276, 349.2282, 391.9954,
  440.0, 493.8833, 523.2511, 587.3295, 659.2551, 698.4565, 783.9909};
const double semitone_list[n_tones] = {
  116.5409, 130.8128, 138.5913, 155.5635, 174.6141, 184.9972, 207.6523,
  233.0819, 261.6256, 277.1826, 311.127,  349.2282, 369.9944, 415.3047,
  466.1638, 523.2511, 554.3653, 622.254,  698.4565, 739.9888, 830.6094};

/****************************************************************/
/**************************FLOW CONTROL**************************/
// loop modes
enum Mode
{
    song_wait, // wait for song to play / recording
    song_play, // playing song from db
    recording, // recording song
    saving,    // saving song
    play,      // free playing
};
// state of note playing -- for dual purpose keys
enum Play_state
{
    idle,        // not playing
    was_playing, // playing a note while pressing another key
    switching,   // not playing & already pressed
};
// song recording
typedef struct
{
    double tone;
    unsigned long time;
} Note;
typedef struct
{
    std::string code;
    std::vector<Note> notes;
} Song;

/****************************************************************/
/*****************************GLOBALS****************************/
Mode mode = play;               // current play mode
Play_state play_state = idle;   // current play state
int song = 0;                   // song to play
bool has_song = false;          // is song selected
double tone = 0;                // tone to play
int tone_i = -1;                // index of tone to play
const int MAX_CODE_L = 4;       // max length of song code
char song_code[MAX_CODE_L + 1]; // code of song to be saved
std::vector<Note> song_notes;   // notes recorded in song
std::vector<Song> songs;        // db of songs
unsigned long start;            // start time of key press/release

/******************************************************************/
/*****************************FUNCTIONS****************************/
// check whether key is pressed
bool is_pressed(char key);
// set tone to be played
void set_tone(int t);
// handle tone to be played / saved
void handle_tone_play(Key key, bool save);
// mode switching
void switch_mode(Mode m);
// handle switch state of dual purpose key
void handle_switch_state(int index);
// handle scale shifting
void handle_change_scale(int index);
// play song from song db
void play_song(int code);

// main setup function
void setup()
{
    // configure LED PWM functionalitites
    ledcSetup(chnl, freq, res);

    // attach the channel to the GPIO to be controlled
    ledcAttachPin(buzzer_pin, chnl);

    Serial.begin(9600);
}

// main execution loop
void loop()
{
    // fetch pressed keys
    keypad.getKeys();
    switch (mode)
    {
        case play: {
            for (int i = 0; i < LIST_MAX; i++)
            {
                Key key = keypad.key[i];
                if (key.stateChanged)
                {
                    if (key.kchar != '0')
                        play_state = idle;

                    if (key.kchar == '0')
                        handle_switch_state(i);
                    else if (key.kchar == '*' || key.kchar == '#')
                        handle_change_scale(i);
                    else if (key.kchar >= '1' && key.kchar <= '9')
                        handle_tone_play(key, false);
                }
            }
        }
        break;

        case recording: {
            for (int i = 0; i < LIST_MAX; i++)
            {
                Key key = keypad.key[i];

                if (key.stateChanged)
                {
                    if (key.kchar != '0')
                        play_state = idle;

                    if (key.kchar >= '1' && key.kchar <= '9')
                        handle_tone_play(key, true);
                    else if (key.kchar == '*' || key.kchar == '#')
                        handle_change_scale(i);
                    else if (key.kchar == '0')
                    {
                        handle_switch_state(0);
                    }
                }
            }
        }
        break;

        case saving: {
            Key key = keypad.key[0];
            if (key.stateChanged && key.kstate == RELEASED)
            {
                keypad.key[0].kstate = IDLE;
                if (key.kchar == '*')
                {
                    Song s = {song_code, song_notes};
                    songs.push_back(s);
                    song_notes.clear();

                    Serial.print("Saved ");
                    Serial.println(song_code);
                    switch_mode(song_wait);
                }
                else if (key.kchar >= '1' && key.kchar <= '9')
                {
                    const int pos = strlen(song_code);
                    if (pos < MAX_CODE_L)
                        song_code[pos] = key.kchar;
                }
                else
                    switch_mode(song_wait);
            }
        }
        break;

        default: { // song modes
            Key key = keypad.key[0];
            if (key.stateChanged && key.kstate == RELEASED)
            {
                keypad.key[0].kstate = IDLE;
                switch (key.kchar)
                {
                    // switch modes
                    case '0':
                        switch_mode(play);
                        break;

                    case '#':
                        if (mode == song_wait)
                        {
                            mode = song_play;
                            has_song = true;
                            play_song(song);
                        }
                        break;

                    case '*':
                        Serial.println("recording");
                        switch_mode(recording);
                        break;

                    default:
                        if (key.kchar >= '1' && key.kchar <= '9')
                        {
                            if (has_song)
                            {
                                song = 0;
                                has_song = false;
                            }
                            song = (song * 10) + key.kchar - '0';
                        }
                        break;
                }
            }
        }
        break;
    }
}

bool is_pressed(char key)
{
    for (int i = 0; i < LIST_MAX; i++)
    {
        if (keypad.key[i].kchar == key)
            return true;
    }
    return false;
}

void set_tone(int t)
{
    // no tone to be played
    if (tone_i == -1 && t == -1)
        return;

    // mute
    if (t == -1)
    {
        tone_i = -1;
        tone = 0;
    }
    // play tone
    else
    {
        tone_i = t - '1';
        if (is_pressed('0'))
        {
            play_state = was_playing;
            tone = semitone_list[scale + t - '1'];
        }
        else
            tone = tone_list[scale + t - '1'];
    }

    // set tone to be played
    ledcWriteTone(chnl, tone);
}

void handle_tone_play(Key key, bool save)
{
    if (key.kstate == PRESSED)
    {
        if (save && (millis() - start > 10))
        {
            if (tone_i != (key.kchar - 'i'))
            {
                const Note note = {tone, millis() - start};
                song_notes.push_back(note);
                // note time of press
                start = millis();
            }
        }

        // play tone
        set_tone(key.kchar);
    }
    else if (key.kstate == RELEASED && tone_i == (key.kchar - '1'))
    {
        if (save)
        {
            const Note note = {tone, millis() - start};
            song_notes.push_back(note);
            start = millis(); // note time of release
        }
        set_tone(-1);
    }
}

void switch_mode(Mode m)
{
    switch (m)
    {
        case recording:
            start = millis();
            break;

        case saving:
            memset(song_code, 0, MAX_CODE_L * sizeof(char));
            break;

        case play:
            play_state = idle;
            scale = C4;
            set_tone(-1);
            for (int i = 0; i < LIST_MAX; i++)
                keypad.key[i].kstate = IDLE;
            Serial.println("Mode: play");
            break;

        case song_wait:
            set_tone(-1);
            song = 0;
            has_song = false;
            for (int i = 0; i < LIST_MAX; i++)
                keypad.key[i].kstate = IDLE;
            break;

        default:
            break;
    }
    mode = m;
}

void handle_switch_state(int index)
{
    if (keypad.key[index].kstate == RELEASED)
    {
        keypad.key[index].kstate = IDLE;
        switch (play_state)
        {
            case was_playing:
                play_state = idle;
                break;

            case idle:
                play_state = switching;
                break;

            case switching:
                if (mode == play)
                    switch_mode(song_wait);
                else if (mode == recording)
                    switch_mode(saving);
                break;
        }
    }
}

void handle_change_scale(int index)
{
    if (keypad.key[index].kstate == RELEASED)
    {
        keypad.key[index].kstate = IDLE;
        if (keypad.key[index].kchar == '#' &&
            ((n_tones - scale) >= (n_keys - 1)))
            scale++;
        else if (keypad.key[index].kchar == '*' && scale > 0)
            scale--;
    }
}

void play_song(int code)
{
    char song[MAX_CODE_L];
    sprintf(song, "%d", code);
    auto it = std::find_if(songs.begin(), songs.end(), [song](const Song& s) {
        return !strcmp(s.code.c_str(), song);
    });
    if (it != songs.end())
    {
        Serial.print("Playing: ");
        Serial.println(song);

        for (auto& n : it->notes)
        {
            ledcWriteTone(chnl, n.tone);
            delay(n.time);
        }
        ledcWriteTone(chnl, 0);
    }
    else
    {
        Serial.print("No song with code ");
        Serial.println(song);
    }
    switch_mode(song_wait);
}
