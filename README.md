# Goa Melody Engine (VST3 / AU)

מחולל MIDI שיושב בתוך ה־DAW: מוטיב קודם, תווים אחר כך. 5 מצבים (Melody, Acid, Arp, Chords → melody, Hook), 34 סולמות + Custom, 8 סגנונות, Generate → Keep → Mutate, Melody DNA מקובץ MIDI, וסינת' Preview פנימי.

הפלאגין מתנגן מסונכרן לשעון ה־DAW, מוציא MIDI החוצה לסינת' שלך, ואפשר לגרור ממנו קליפ MIDI ישר לטראק.

---

## דרך 1: בנייה אוטומטית ב־GitHub (בלי להתקין כלום)

1. פותחים ריפו חדש ב־GitHub ומעלים אליו את כל התיקייה הזו (כולל התיקייה `.github`).
2. בלשונית **Actions** הבנייה רצה לבד (Windows ו־macOS). לוקח בערך 10–15 דקות.
3. בסיום, בתחתית עמוד הריצה, תחת **Artifacts**: מורידים `GoaMelodyEngine-Windows` או `GoaMelodyEngine-macOS`.

## דרך 2: בנייה מקומית

צריך CMake 3.22+ ו־Visual Studio 2022 (Windows) או Xcode (Mac). JUCE יורד אוטומטית.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

התוצרים: `build/GoaMelodyEngine_artefacts/Release/VST3` (ו־`AU`, `Standalone` ב־Mac).

בדיקת המנוע בלבד (בלי JUCE):
```
g++ -std=c++17 -O2 tests/engine_test.cpp Source/Engine.cpp -o engine_test && ./engine_test
```

---

## התקנה

| מערכת | לאן להעתיק |
|---|---|
| Windows VST3 | `C:\Program Files\Common Files\VST3\` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/` |

ב־Mac, הפלאגין לא חתום אצל Apple, אז פעם אחת בטרמינל:
```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Goa Melody Engine.vst3"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Goa Melody Engine.component"
```

---

## שימוש ב־Ableton Live

**הכי פשוט:** טוענים את הפלאגין על טראק MIDI ולוחצים Play. הוא מתנגן בסנכרון עם ה־Built-in sound. כשמשהו מוצא חן, גוררים את **Drag MIDI to a track** לטראק של הסינת'.

**לנגן ישר את Serum / Vital / 303 בלייב:**
Ableton לא מקבל MIDI שיוצא מפלאגין VST3, לכן הפלאגין שולח את התווים לפורט MIDI וירטואלי:

1. **Windows:** מתקינים את **loopMIDI** (חינמי, Tobias Erichsen), פותחים אותו ולוחצים **+** כדי ליצור פורט.
   **Mac:** אין צורך בשום התקנה. בוחרים בפלאגין את **Goa Melody Engine (virtual port)**.
2. בפלאגין, ב־**MIDI out**, בוחרים את הפורט (אם הוא לא מופיע: **Rescan ports**).
3. ב־Ableton: **Settings ← Link, Tempo & MIDI**. בשורת הפורט תחת **Input** מדליקים **Track**. תחת **Output** להשאיר כבוי.
4. בטראק של הסינת': **MIDI From** ← הפורט, **Monitor** ← **In**.
5. בפלאגין מכבים **Built-in sound**.

**FL Studio / Bitwig / Reaper / Cubase:** ה־MIDI יוצא מהפלאגין ישירות לפי הניתוב הרגיל של התוכנה, או דרך **MIDI out** כמו למעלה.

---

## מבנה הקוד

- `Source/Engine.*`: כל ההיגיון המוזיקלי. C++17 טהור, בלי תלות ב־JUCE.
- `Source/PluginProcessor.*`: סנכרון לשעון ה־DAW, תזמון נוטים ברמת הדגימה, slides כנוטים חופפים (בסגנון 303), סינת' Preview, שמירת מצב בפרויקט.
- `Source/PluginEditor.*`: הממשק.
- `tests/engine_test.cpp`: בדיקת עומס של המנוע על כל הסולמות, המצבים והסגנונות.

כל 8 המלודיות, ה־Keep וההגדרות נשמרים בתוך פרויקט ה־DAW.
