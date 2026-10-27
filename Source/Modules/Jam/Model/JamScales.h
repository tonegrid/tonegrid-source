#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
namespace tonegrid::jam
{
struct Scale {juce::String id,name,group,formula,help;std::vector<int> notes;};
inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> definitions{
        {"minor-pentatonic","Minor pentatonic","Essentials","1 b3 4 5 b7","Five notes for blues, rock and many minor-key grooves.",{0,3,5,7,10}},
        {"major-pentatonic","Major pentatonic","Essentials","1 2 3 5 6","Five notes for bright major-key melodies, country and soul.",{0,2,4,7,9}},
        {"minor-blues","Minor blues","Essentials","1 b3 4 b5 5 b7","Minor pentatonic with the passing blue note b5.",{0,3,5,6,7,10}},
        {"major-blues","Major blues","Essentials","1 2 b3 3 5 6","Major pentatonic with a passing minor third.",{0,2,3,4,7,9}},
        {"major","Major (Ionian)","Essentials","1 2 3 4 5 6 7","The major scale. Let the track's chords guide your phrases.",{0,2,4,5,7,9,11}},
        {"minor","Natural minor (Aeolian)","Essentials","1 2 b3 4 5 b6 b7","The natural minor scale for minor-key music.",{0,2,3,5,7,8,10}},
        {"dorian","Dorian","Major modes","1 2 b3 4 5 6 b7","A minor sound with a brighter natural sixth.",{0,2,3,5,7,9,10}},
        {"phrygian","Phrygian","Major modes","1 b2 b3 4 5 b6 b7","A minor sound with a dark flattened second.",{0,1,3,5,7,8,10}},
        {"lydian","Lydian","Major modes","1 2 3 #4 5 6 7","A major sound with a raised fourth.",{0,2,4,6,7,9,11}},
        {"mixolydian","Mixolydian","Major modes","1 2 3 4 5 6 b7","A major sound with a flattened seventh; useful over dominant chords.",{0,2,4,5,7,9,10}},
        {"locrian","Locrian","Major modes","1 b2 b3 4 b5 b6 b7","A diminished sound; useful over half-diminished chords.",{0,1,3,5,6,8,10}},
        {"harmonic-minor","Harmonic minor","Harmonic minor modes","1 2 b3 4 5 b6 7","Natural minor with a raised seventh leading back to the root.",{0,2,3,5,7,8,11}},
        {"locrian-natural6","Locrian natural 6","Harmonic minor modes","1 b2 b3 4 b5 6 b7","Second mode of harmonic minor.",{0,1,3,5,6,9,10}},
        {"ionian-augmented","Ionian augmented","Harmonic minor modes","1 2 3 4 #5 6 7","Third mode of harmonic minor; major with a raised fifth.",{0,2,4,5,8,9,11}},
        {"dorian-sharp4","Dorian #4","Harmonic minor modes","1 2 b3 #4 5 6 b7","Fourth mode of harmonic minor.",{0,2,3,6,7,9,10}},
        {"phrygian-dominant","Phrygian dominant","Harmonic minor modes","1 b2 3 4 5 b6 b7","Fifth mode of harmonic minor, with a major third.",{0,1,4,5,7,8,10}},
        {"lydian-sharp2","Lydian #2","Harmonic minor modes","1 #2 3 #4 5 6 7","Sixth mode of harmonic minor.",{0,3,4,6,7,9,11}},
        {"ultralocrian","Ultralocrian","Harmonic minor modes","1 b2 b3 b4 b5 b6 bb7","Seventh mode of harmonic minor.",{0,1,3,4,6,8,9}},
        {"melodic-minor","Melodic minor (jazz)","Melodic minor modes","1 2 b3 4 5 6 7","Jazz melodic minor: the same notes ascending and descending.",{0,2,3,5,7,9,11}},
        {"dorian-flat2","Dorian b2","Melodic minor modes","1 b2 b3 4 5 6 b7","Second mode of melodic minor.",{0,1,3,5,7,9,10}},
        {"lydian-augmented","Lydian augmented","Melodic minor modes","1 2 3 #4 #5 6 7","Third mode of melodic minor.",{0,2,4,6,8,9,11}},
        {"lydian-dominant","Lydian dominant","Melodic minor modes","1 2 3 #4 5 6 b7","Fourth mode of melodic minor; dominant with a raised fourth.",{0,2,4,6,7,9,10}},
        {"mixolydian-flat6","Mixolydian b6","Melodic minor modes","1 2 3 4 5 b6 b7","Fifth mode of melodic minor.",{0,2,4,5,7,8,10}},
        {"locrian-natural2","Locrian natural 2","Melodic minor modes","1 2 b3 4 b5 b6 b7","Sixth mode of melodic minor; often used over half-diminished chords.",{0,2,3,5,6,8,10}},
        {"altered","Altered (Super Locrian)","Melodic minor modes","1 b2 b3 b4 b5 b6 b7","Seventh mode of melodic minor for altered dominant sounds.",{0,1,3,4,6,8,10}},
        {"whole-tone","Whole tone","Symmetrical & bebop","1 2 3 #4 #5 b7","Six equally spaced whole tones; an augmented sound.",{0,2,4,6,8,10}},
        {"diminished-whole-half","Diminished (whole-half)","Symmetrical & bebop","1 2 b3 4 b5 b6 6 7","Eight notes alternating whole and half steps.",{0,2,3,5,6,8,9,11}},
        {"diminished-half-whole","Diminished (half-whole)","Symmetrical & bebop","1 b2 b3 3 #4 5 6 b7","Eight notes alternating half and whole steps.",{0,1,3,4,6,7,9,10}},
        {"augmented","Augmented","Symmetrical & bebop","1 b3 3 5 #5 7","Six notes alternating minor thirds and semitones.",{0,3,4,7,8,11}},
        {"bebop-dominant","Bebop dominant","Symmetrical & bebop","1 2 3 4 5 6 b7 7","Mixolydian with a passing major seventh.",{0,2,4,5,7,9,10,11}},
        {"bebop-major","Bebop major","Symmetrical & bebop","1 2 3 4 5 #5 6 7","Major with a passing raised fifth.",{0,2,4,5,7,8,9,11}},
        {"harmonic-major","Harmonic major","Other colours","1 2 3 4 5 b6 7","A major scale with a flattened sixth.",{0,2,4,5,7,8,11}},
        {"double-harmonic","Double harmonic major","Other colours","1 b2 3 4 5 b6 7","Also called Byzantine; two augmented-second intervals.",{0,1,4,5,7,8,11}},
        {"hungarian-minor","Hungarian minor","Other colours","1 2 b3 #4 5 b6 7","Harmonic minor with a raised fourth.",{0,2,3,6,7,8,11}},
        {"neapolitan-minor","Neapolitan minor","Other colours","1 b2 b3 4 5 b6 7","Harmonic minor with a flattened second.",{0,1,3,5,7,8,11}},
        {"neapolitan-major","Neapolitan major","Other colours","1 b2 b3 4 5 6 7","Melodic minor with a flattened second.",{0,1,3,5,7,9,11}},
        {"persian","Persian","Other colours","1 b2 3 4 b5 b6 7","A seven-note colour with flattened second, fifth and sixth.",{0,1,4,5,6,8,11}},
        {"hirajoshi","Hirajoshi","Other colours","1 2 b3 5 b6","A five-note scale with wide melodic spaces.",{0,2,3,7,8}},
        {"in-sen","In sen","Other colours","1 b2 4 5 b7","A five-note scale with a flattened second.",{0,1,5,7,10}},
        {"iwato","Iwato","Other colours","1 b2 4 b5 b7","A five-note scale with a flattened second and fifth.",{0,1,5,6,10}},
        {"egyptian","Suspended pentatonic","Other colours","1 2 4 5 b7","A pentatonic colour without a major or minor third.",{0,2,5,7,10}},
        {"chromatic","Chromatic","Other colours","1 b2 2 b3 3 4 b5 5 b6 6 b7 7","All twelve pitch classes; use passing notes with care.",{0,1,2,3,4,5,6,7,8,9,10,11}}
    };return definitions;
}
inline int pitch(int midi){return (midi%12+12)%12;}
inline const juce::StringArray& keyNames(){static const juce::StringArray names{"C","C# / Db","D","D# / Eb","E","F","F# / Gb","G","G# / Ab","A","A# / Bb","B"};return names;}
inline int rootFor(const juce::String& key)
{
    const juce::StringArray sharps{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},flats{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    auto value=key.replace(juce::String::fromUTF8("♯"),"#").replace(juce::String::fromUTF8("♭"),"b");
    for(int i=0;i<12;++i)if(value==sharps[i]||value==flats[i])return i;return -1;
}
inline int scaleIndex(const juce::String& id){for(size_t i=0;i<scales().size();++i)if(scales()[i].id==id)return int(i);return -1;}
inline juce::String tonic(int root){static const juce::StringArray names{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};return names[pitch(root)];}
// Spell notes from scale degrees, so Db major has F (not E#), and F# major has E#.
inline juce::String noteName(int root,int interval,const juce::String& degree)
{
    static const juce::String letters="CDEFGAB";static constexpr int natural[]{0,2,4,5,7,9,11};
    const int start=letters.indexOfChar(tonic(root)[0]);const int step=degree.retainCharacters("1234567").getIntValue()-1;
    if(step<0)return tonic(root+interval);
    const int letter=(start+step)%7;int accidental=pitch(root+interval)-natural[letter];while(accidental>6)accidental-=12;while(accidental < -6)accidental+=12;
    return juce::String::charToString(letters[letter])+juce::String::repeatedString(accidental<0?"b":"#",std::abs(accidental));
}
inline juce::String degreeAt(const Scale& scale,int interval)
{const auto degrees=juce::StringArray::fromTokens(scale.formula," ","");for(size_t i=0;i<scale.notes.size();++i)if(scale.notes[i]==interval)return degrees[int(i)];return {};}
inline constexpr std::array<int,6> openMidi{64,59,55,50,45,40}; // high e to low E
}
