#pragma once

// What the command-line tools print around their own work.
//
// `spirula --help` is the front door: someone who has just installed this and
// types the program's name gets this text, and there is no language picker in
// a terminal to fix it with afterwards. So the top-level usage, the trainer's
// help scaffolding and the lines a training run prints while it works all come
// from here, and follow --lang / SS_LANG like everything else.
//
// What is NOT here, and stays English on purpose:
//
//   - IDENTIFIERS. `--data`, `--help-all`, `3dgs`, `SS_LANG`, a camera model
//     name. They are typed, not read, and a translated flag would not work.
//     The catalog spells them exactly as the parser does.
//   - The per-flag help. That lives in i18n/catalog/TrainFields.h, one entry
//     per row of SS_CONFIG_FIELDS, and `--help` reads it from there -- so a
//     flag's name and its sentence cannot drift apart.
//   - The DEEP DIAGNOSTICS: SS_DUMP_CAMERAS, the meshing pipeline's per-stage
//     numbers, `spirula sam`'s usage, the self-test binaries. Those are read
//     by whoever is debugging the pipeline, and are what a bug report is
//     pasted from.
//
// `spirula sfm` has its own catalog (i18n/catalog/Sfm.h) because it prints
// through a tag column; see src/sfm/core/Log.h.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace cli {

// ===========================================================================
// `spirula --help` -- the one-line summary of each subcommand
// ===========================================================================
//
// The subcommand NAME (gui, sfm, train, sam, mesh) is an identifier and is
// printed verbatim next to these; only the sentence is translated.

SS_MSG(tool_gui,
    EN("the graphical application (the default)"),
    JA("グラフィカルなアプリケーション（既定）"),
    ZH_HANS("图形界面应用（默认）"),
    ZH_HANT("圖形介面應用程式（預設）"),
    KO("그래픽 응용 프로그램(기본값)"),
    DE("die grafische Anwendung (Standard)"),
    FR("l'application graphique (par défaut)"),
    ES("la aplicación gráfica (opción por defecto)"),
    PT("o aplicativo gráfico (padrão)"),
    IT("l'applicazione grafica (predefinita)"),
    NL("de grafische toepassing (standaard)"),
    RU("графическое приложение (по умолчанию)"),
    TR("grafik arayüzlü uygulama (varsayılan)"));
SS_MSG(tool_sfm,
    EN("structure from motion: photos or frames -> cameras"),
    JA("Structure from Motion: 写真やフレームからカメラを求める"),
    ZH_HANS("运动恢复结构：由照片或视频帧求相机"),
    ZH_HANT("運動恢復結構：由照片或影格求相機"),
    KO("Structure from Motion: 사진이나 프레임에서 카메라 구하기"),
    DE("Structure from Motion: Fotos oder Einzelbilder -> Kameras"),
    FR("structure from motion : photos ou images -> caméras"),
    ES("structure from motion: fotos o fotogramas -> cámaras"),
    PT("structure from motion: fotos ou quadros -> câmeras"),
    IT("structure from motion: foto o fotogrammi -> camere"),
    NL("structure from motion: foto's of beelden -> camera's"),
    RU("structure from motion: фотографии или кадры -> камеры"),
    TR("structure from motion: fotoğraf ya da karelerden kameralar"));
SS_MSG(tool_train,
    EN("train a splat model on a dataset"),
    JA("データセットでスプラットモデルを学習する"),
    ZH_HANS("在数据集上训练泼溅模型"),
    ZH_HANT("在資料集上訓練潑濺模型"),
    KO("데이터셋으로 스플랫 모델 학습"),
    DE("ein Splat-Modell auf einem Datensatz trainieren"),
    FR("entraîner un modèle de splats sur un jeu de données"),
    ES("entrenar un modelo de splats con un conjunto de datos"),
    PT("treinar um modelo de splats em um conjunto de dados"),
    IT("addestrare un modello di splat su un set di dati"),
    NL("een splat-model op een dataset trainen"),
    RU("обучить модель сплатов на наборе данных"),
    TR("bir veri kümesinde splat modeli eğitmek"));
SS_MSG(tool_sam,
    EN("segmentation, tracking and frame extraction"),
    JA("領域分割・追跡・フレームの切り出し"),
    ZH_HANS("分割、跟踪与抽帧"),
    ZH_HANT("分割、追蹤與擷取影格"),
    KO("분할, 추적, 프레임 추출"),
    DE("Segmentierung, Verfolgung und Bildentnahme"),
    FR("segmentation, suivi et extraction d'images"),
    ES("segmentación, seguimiento y extracción de fotogramas"),
    PT("segmentação, rastreamento e extração de quadros"),
    IT("segmentazione, tracciamento ed estrazione di fotogrammi"),
    NL("segmentatie, volgen en beelden uitpakken"),
    RU("сегментация, отслеживание и извлечение кадров"),
    TR("bölütleme, izleme ve kare çıkarma"));
SS_MSG(tool_geometry,
    EN("estimate depth and surface normals for a dataset"),
    JA("データセットの深度と面法線を推定する"),
    ZH_HANS("估计数据集的深度与表面法线"),
    ZH_HANT("估計資料集的深度與表面法線"),
    KO("데이터셋의 깊이와 표면 법선을 추정"),
    DE("Tiefe und Oberflächennormalen eines Datensatzes schätzen"),
    FR("estimer la profondeur et les normales d'un jeu de données"),
    ES("estimar la profundidad y las normales de un conjunto de datos"),
    PT("estimar a profundidade e as normais de um conjunto de dados"),
    IT("stimare profondità e normali di un insieme di dati"),
    NL("diepte en oppervlaktenormalen van een dataset schatten"),
    RU("оценить глубину и нормали поверхности набора данных"),
    TR("bir veri kümesinin derinliğini ve yüzey normallerini kestir"));
SS_MSG(tool_mesh,
    EN("extract a mesh from a trained model"),
    JA("学習済みモデルからメッシュを取り出す"),
    ZH_HANS("从训练好的模型提取网格"),
    ZH_HANT("從訓練好的模型擷取網格"),
    KO("학습이 끝난 모델에서 메시 추출"),
    DE("aus einem trainierten Modell ein Netz erzeugen"),
    FR("extraire un maillage d'un modèle entraîné"),
    ES("extraer una malla de un modelo entrenado"),
    PT("extrair uma malha de um modelo treinado"),
    IT("estrarre una mesh da un modello addestrato"),
    NL("een mesh uit een getraind model halen"),
    RU("извлечь меш из обученной модели"),
    TR("eğitilmiş bir modelden ağ çıkarmak"));

// ===========================================================================
// `spirula --help` -- the usage block itself
// ===========================================================================

SS_MSG(usage_open_app,
    EN("open the application"),
    JA("アプリケーションを開く"),
    ZH_HANS("打开应用"),
    ZH_HANT("開啟應用程式"),
    KO("응용 프로그램 열기"),
    DE("die Anwendung öffnen"),
    FR("ouvrir l'application"),
    ES("abrir la aplicación"),
    PT("abrir o aplicativo"),
    IT("apre l'applicazione"),
    NL("de toepassing openen"),
    RU("открыть приложение"),
    TR("uygulamayı açar"));
SS_MSG(usage_open_target,
    EN("open it in the application"),
    JA("それをアプリケーションで開く"),
    ZH_HANS("在应用中打开它"),
    ZH_HANT("在應用程式中開啟它"),
    KO("그것을 응용 프로그램에서 열기"),
    DE("es in der Anwendung öffnen"),
    FR("l'ouvrir dans l'application"),
    ES("abrirlo en la aplicación"),
    PT("abri-lo no aplicativo"),
    IT("lo apre nell'applicazione"),
    NL("het in de toepassing openen"),
    RU("открыть это в приложении"),
    TR("onu uygulamada açar"));
SS_MSG(usage_commands,
    EN("Commands:"),
    JA("コマンド:"),
    ZH_HANS("命令："),
    ZH_HANT("命令："),
    KO("명령:"),
    DE("Befehle:"),
    FR("Commandes :"),
    ES("Comandos:"),
    PT("Comandos:"),
    IT("Comandi:"),
    NL("Opdrachten:"),
    RU("Команды:"),
    TR("Komutlar:"));
SS_MSG(usage_per_command_help,
    EN("`spirula <command> --help` describes one of them."),
    JA("それぞれの詳細は `spirula <command> --help` で見られます。"),
    ZH_HANS("`spirula <command> --help` 会说明其中某一个命令。"),
    ZH_HANT("`spirula <command> --help` 會說明其中某一個命令。"),
    KO("각 명령의 설명은 `spirula <command> --help`로 볼 수 있습니다."),
    DE("`spirula <command> --help` beschreibt einen davon."),
    FR("`spirula <command> --help` décrit l'une d'entre elles."),
    ES("`spirula <command> --help` describe cada uno de ellos."),
    PT("`spirula <command> --help` descreve cada um deles."),
    IT("`spirula <command> --help` descrive uno di questi."),
    NL("`spirula <command> --help` beschrijft er één."),
    RU("`spirula <command> --help` описывает любую из них."),
    TR("`spirula <command> --help` bunlardan birini anlatır."));
SS_MSG(usage_lang,
    EN("Every command also takes --lang <code>, which sets the interface "
       "language (or SS_LANG in the environment):"),
    JA("どのコマンドでも --lang <code> を指定でき、表示言語が変わります"
       "（環境変数 SS_LANG でも同じです）:"),
    ZH_HANS("每个命令都接受 --lang <code>，用来设置界面语言（也可以用环境变量 "
            "SS_LANG）："),
    ZH_HANT("每個命令都接受 --lang <code>，用來設定介面語言（也可以用環境變數 "
            "SS_LANG）："),
    KO("모든 명령은 --lang <code>도 받습니다. 인터페이스 언어를 정합니다"
       "(환경 변수 SS_LANG도 같습니다):"),
    DE("Jeder Befehl nimmt außerdem --lang <code> entgegen und setzt damit die "
       "Sprache der Oberfläche (oder SS_LANG in der Umgebung):"),
    FR("Chaque commande accepte aussi --lang <code>, qui règle la langue de "
       "l'interface (ou SS_LANG dans l'environnement) :"),
    ES("Todos los comandos aceptan además --lang <code>, que fija el idioma de "
       "la interfaz (o SS_LANG en el entorno):"),
    PT("Todo comando também aceita --lang <code>, que define o idioma da "
       "interface (ou SS_LANG no ambiente):"),
    IT("Ogni comando accetta anche --lang <code>, che imposta la lingua "
       "dell'interfaccia (oppure SS_LANG nell'ambiente):"),
    NL("Elke opdracht neemt ook --lang <code>, waarmee je de taal van de "
       "interface instelt (of SS_LANG in de omgeving):"),
    RU("Любая команда принимает также --lang <code>, задающий язык интерфейса "
       "(или переменную окружения SS_LANG):"),
    TR("Her komut ayrıca arayüz dilini belirleyen --lang <code> seçeneğini de "
       "alır (ya da ortamdaki SS_LANG):"));
SS_MSG(err_unknown_command,
    EN("error: unknown command '{0}'"),
    JA("エラー: 不明なコマンド '{0}'"),
    ZH_HANS("错误：未知命令 '{0}'"),
    ZH_HANT("錯誤：未知命令 '{0}'"),
    KO("오류: 알 수 없는 명령 '{0}'"),
    DE("Fehler: unbekannter Befehl '{0}'"),
    FR("erreur : commande inconnue '{0}'"),
    ES("error: comando desconocido '{0}'"),
    PT("erro: comando desconhecido '{0}'"),
    IT("errore: comando sconosciuto '{0}'"),
    NL("fout: onbekende opdracht '{0}'"),
    RU("ошибка: неизвестная команда '{0}'"),
    TR("hata: bilinmeyen komut '{0}'"));
SS_MSG(err_no_gui,
    EN("error: this build has no graphical application (-DSS_BUILD_GUI=OFF)"),
    JA("エラー: このビルドにはグラフィカルなアプリケーションが含まれていません"
       "（-DSS_BUILD_GUI=OFF）"),
    ZH_HANS("错误：这个版本没有图形界面应用（-DSS_BUILD_GUI=OFF）"),
    ZH_HANT("錯誤：這個版本沒有圖形介面應用程式（-DSS_BUILD_GUI=OFF）"),
    KO("오류: 이 빌드에는 그래픽 응용 프로그램이 없습니다(-DSS_BUILD_GUI=OFF)"),
    DE("Fehler: dieser Build enthält keine grafische Anwendung "
       "(-DSS_BUILD_GUI=OFF)"),
    FR("erreur : cette compilation n'a pas d'application graphique "
       "(-DSS_BUILD_GUI=OFF)"),
    ES("error: esta compilación no tiene aplicación gráfica "
       "(-DSS_BUILD_GUI=OFF)"),
    PT("erro: esta compilação não tem aplicativo gráfico (-DSS_BUILD_GUI=OFF)"),
    IT("errore: questa build non ha un'applicazione grafica "
       "(-DSS_BUILD_GUI=OFF)"),
    NL("fout: deze build heeft geen grafische toepassing (-DSS_BUILD_GUI=OFF)"),
    RU("ошибка: в этой сборке нет графического приложения "
       "(-DSS_BUILD_GUI=OFF)"),
    TR("hata: bu derlemede grafik arayüzlü uygulama yok (-DSS_BUILD_GUI=OFF)"));

// ===========================================================================
// The compute-device table, printed by `spirula train` at startup
// ===========================================================================
//
// The device NAME and its type come from the driver and are printed as they
// arrive; only the heading and the note after an unusable row are ours.

SS_MSG(devices_header,
    EN("Devices:"),
    JA("デバイス:"),
    ZH_HANS("设备："),
    ZH_HANT("裝置："),
    KO("장치:"),
    DE("Geräte:"),
    FR("Périphériques :"),
    ES("Dispositivos:"),
    PT("Dispositivos:"),
    IT("Dispositivi:"),
    NL("Apparaten:"),
    RU("Устройства:"),
    TR("Aygıtlar:"));
SS_MSG(device_unusable,
    EN("[missing required features]"),
    JA("[必要な機能がありません]"),
    ZH_HANS("[缺少必需的功能]"),
    ZH_HANT("[缺少必要的功能]"),
    KO("[필요한 기능 없음]"),
    DE("[erforderliche Funktionen fehlen]"),
    FR("[fonctions requises absentes]"),
    ES("[faltan funciones necesarias]"),
    PT("[faltam recursos necessários]"),
    IT("[mancano funzioni necessarie]"),
    NL("[vereiste functies ontbreken]"),
    RU("[нет нужных возможностей]"),
    TR("[gerekli özellikler yok]"));

// ===========================================================================
// `spirula train --help`
// ===========================================================================
//
// The scaffolding only: the per-flag lines come from
// i18n/catalog/TrainFields.h and the preset lines from i18n/catalog/Train.h.

SS_MSG(train_usage,
    EN("usage: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    JA("使い方: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    ZH_HANS("用法：{0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    ZH_HANT("用法：{0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    KO("사용법: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    DE("Aufruf: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    FR("utilisation : {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    ES("uso: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    PT("uso: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    IT("uso: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    NL("gebruik: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    RU("использование: {0} [<preset>] --data <dataset_dir> [--flag value ...]"),
    TR("kullanım: {0} [<preset>] --data <dataset_dir> [--flag value ...]"));
SS_MSG(train_presets_header,
    EN("presets (given as the first argument; default: 3dgs):"),
    JA("プリセット（最初の引数として指定。既定は 3dgs）:"),
    ZH_HANS("预设（作为第一个参数给出；默认为 3dgs）："),
    ZH_HANT("預設（作為第一個引數給出；預設為 3dgs）："),
    KO("프리셋(첫 번째 인수로 지정, 기본값은 3dgs):"),
    DE("Voreinstellungen (als erstes Argument; Standard: 3dgs):"),
    FR("préréglages (donnés en premier argument ; par défaut : 3dgs) :"),
    ES("ajustes (se dan como primer argumento; por defecto: 3dgs):"),
    PT("predefinições (dadas como primeiro argumento; padrão: 3dgs):"),
    IT("preimpostazioni (date come primo argomento; predefinita: 3dgs):"),
    NL("voorinstellingen (als eerste argument; standaard: 3dgs):"),
    RU("пресеты (задаются первым аргументом; по умолчанию 3dgs):"),
    TR("hazır ayarlar (ilk argüman olarak verilir; varsayılan: 3dgs):"));
SS_MSG(train_app_flags_header,
    EN("app flags:"),
    JA("アプリケーションのフラグ:"),
    ZH_HANS("应用级参数："),
    ZH_HANT("應用程式層級參數："),
    KO("응용 프로그램 플래그:"),
    DE("Programm-Schalter:"),
    FR("options de l'application :"),
    ES("opciones de la aplicación:"),
    PT("opções do aplicativo:"),
    IT("opzioni dell'applicazione:"),
    NL("opties van de toepassing:"),
    RU("флаги приложения:"),
    TR("uygulama seçenekleri:"));
SS_MSG(train_device_help,
    EN("Compute device to train on (default: auto). The device list prints at "
       "startup."),
    JA("学習に使う計算デバイスです（既定は自動）。デバイスの一覧は起動時に"
       "表示されます。"),
    ZH_HANS("用于训练的计算设备（默认自动选择）。设备列表会在启动时打印。"),
    ZH_HANT("用於訓練的計算裝置（預設自動選擇）。裝置清單會在啟動時列出。"),
    KO("학습에 쓸 연산 장치입니다(기본값은 자동). 장치 목록은 시작할 때 "
       "출력됩니다."),
    DE("Rechengerät für das Training (Standard: automatisch). Die Geräteliste "
       "wird beim Start ausgegeben."),
    FR("Périphérique de calcul pour l'entraînement (par défaut : automatique). "
       "La liste des périphériques s'affiche au démarrage."),
    ES("Dispositivo de cálculo con el que entrenar (por defecto: automático). "
       "La lista de dispositivos se imprime al arrancar."),
    PT("Dispositivo de cálculo para treinar (padrão: automático). A lista de "
       "dispositivos é impressa na inicialização."),
    IT("Dispositivo di calcolo su cui addestrare (predefinito: automatico). "
       "L'elenco dei dispositivi viene stampato all'avvio."),
    NL("Rekenapparaat om op te trainen (standaard: automatisch). De "
       "apparatenlijst wordt bij het starten afgedrukt."),
    RU("Вычислительное устройство для обучения (по умолчанию автоматически). "
       "Список устройств печатается при запуске."),
    TR("Eğitimin yapılacağı işlem aygıtı (varsayılan: otomatik). Aygıt listesi "
       "başlangıçta yazdırılır."));
SS_MSG(train_flags_header,
    EN("flags ('-' and '_' interchangeable; bools take 0/1; 'none' clears "
       "optional values; defaults shown for the selected preset):"),
    JA("フラグ（'-' と '_' はどちらでも可。真偽値は 0/1。'none' は省略可能な値を"
       "空にします。既定値は選択中のプリセットのものです）:"),
    ZH_HANS("参数（'-' 和 '_' 可互换；布尔值取 0/1；'none' 会清空可选值；下面显示"
            "的默认值对应所选预设）："),
    ZH_HANT("參數（'-' 和 '_' 可互換；布林值取 0/1；'none' 會清空選填值；下面顯示"
            "的預設值對應所選預設）："),
    KO("플래그('-'와 '_'는 서로 바꿔 쓸 수 있고, 불리언은 0/1을 받으며, 'none'은 "
       "선택 값을 비웁니다. 기본값은 선택한 프리셋 기준입니다):"),
    DE("Schalter ('-' und '_' austauschbar; Wahrheitswerte nehmen 0/1; 'none' "
       "leert optionale Werte; die Standardwerte gelten für die gewählte "
       "Voreinstellung):"),
    FR("options ('-' et '_' interchangeables ; les booléens prennent 0/1 ; "
       "'none' vide une valeur facultative ; les valeurs par défaut sont celles "
       "du préréglage choisi) :"),
    ES("opciones ('-' y '_' son intercambiables; los booleanos toman 0/1; "
       "'none' vacía un valor opcional; los valores por defecto son los del "
       "ajuste elegido):"),
    PT("opções ('-' e '_' são intercambiáveis; booleanos aceitam 0/1; 'none' "
       "esvazia um valor opcional; os valores padrão são os da predefinição "
       "escolhida):"),
    IT("opzioni ('-' e '_' sono intercambiabili; i booleani accettano 0/1; "
       "'none' svuota un valore facoltativo; i valori predefiniti sono quelli "
       "della preimpostazione scelta):"),
    NL("opties ('-' en '_' zijn uitwisselbaar; booleans nemen 0/1; 'none' maakt "
       "een optionele waarde leeg; de standaardwaarden horen bij de gekozen "
       "voorinstelling):"),
    RU("флаги ('-' и '_' взаимозаменяемы; логические принимают 0/1; 'none' "
       "очищает необязательное значение; значения по умолчанию показаны для "
       "выбранного пресета):"),
    TR("seçenekler ('-' ile '_' birbirinin yerine geçer; mantıksal değerler 0/1 "
       "alır; 'none' isteğe bağlı bir değeri boşaltır; varsayılanlar seçili "
       "hazır ayara göredir):"));
SS_MSG(train_more_flags,
    EN("More flags, for tuning rather than for getting a first result: {0}. "
       "List them with --help-all"),
    JA("さらに細かい調整用のフラグがあります（最初の結果を出すためのものでは"
       "ありません）: {0}。--help-all で一覧できます"),
    ZH_HANS("还有一些用于微调、而非跑出第一个结果的参数：{0}。用 --help-all 可以"
            "列出它们"),
    ZH_HANT("還有一些用於微調、而非跑出第一個結果的參數：{0}。用 --help-all 可以"
            "列出它們"),
    KO("첫 결과를 얻기보다 세부 조정에 쓰는 플래그가 더 있습니다: {0}. "
       "--help-all로 볼 수 있습니다"),
    DE("Weitere Schalter, zum Feinabstimmen statt zum ersten Ergebnis: {0}. "
       "Mit --help-all auflisten"),
    FR("D'autres options, pour régler finement plutôt que pour obtenir un "
       "premier résultat : {0}. Les lister avec --help-all"),
    ES("Hay más opciones, para afinar más que para obtener un primer "
       "resultado: {0}. Puedes listarlas con --help-all"),
    PT("Há mais opções, para ajuste fino e não para obter um primeiro "
       "resultado: {0}. Liste-as com --help-all"),
    IT("Ci sono altre opzioni, per la messa a punto più che per ottenere un "
       "primo risultato: {0}. Elencale con --help-all"),
    NL("Er zijn meer opties, om fijn af te stemmen in plaats van om een eerste "
       "resultaat te krijgen: {0}. Toon ze met --help-all"),
    RU("Есть ещё флаги -- для тонкой настройки, а не для первого результата: "
       "{0}. Показать их: --help-all"),
    TR("İlk sonucu almaktan çok ince ayar için başka seçenekler de var: {0}. "
       "--help-all ile listeleyin"));

// ===========================================================================
// What a training run says while it works
// ===========================================================================

SS_MSG(resume_target,
    EN("Resuming from: {0}"),
    JA("再開元: {0}"),
    ZH_HANS("从这里继续：{0}"),
    ZH_HANT("從這裡繼續：{0}"),
    KO("이어서 시작할 위치: {0}"),
    DE("Fortsetzung von: {0}"),
    FR("Reprise depuis : {0}"),
    ES("Se continúa desde: {0}"),
    PT("Continuando de: {0}"),
    IT("Ripresa da: {0}"),
    NL("Hervat vanaf: {0}"),
    RU("Продолжение с: {0}"),
    TR("Şuradan devam ediliyor: {0}"));
SS_MSG(viewer_at,
    EN("Viewer at http://0.0.0.0:{0}/ (forward the port for remote boxes: "
       "ssh -L {1}:localhost:{2} <host>)"),
    JA("ビューアは http://0.0.0.0:{0}/ です（リモートのマシンならポートを転送"
       "してください: ssh -L {1}:localhost:{2} <host>）"),
    ZH_HANS("查看器在 http://0.0.0.0:{0}/（远程机器请转发端口："
            "ssh -L {1}:localhost:{2} <host>）"),
    ZH_HANT("檢視器在 http://0.0.0.0:{0}/（遠端機器請轉送連接埠："
            "ssh -L {1}:localhost:{2} <host>）"),
    KO("뷰어 주소는 http://0.0.0.0:{0}/ 입니다(원격 장비라면 포트를 전달하세요: "
       "ssh -L {1}:localhost:{2} <host>)"),
    DE("Betrachter unter http://0.0.0.0:{0}/ (bei entfernten Rechnern den Port "
       "weiterleiten: ssh -L {1}:localhost:{2} <host>)"),
    FR("Visionneuse sur http://0.0.0.0:{0}/ (sur une machine distante, "
       "rediriger le port : ssh -L {1}:localhost:{2} <host>)"),
    ES("Visor en http://0.0.0.0:{0}/ (en máquinas remotas, redirige el puerto: "
       "ssh -L {1}:localhost:{2} <host>)"),
    PT("Visualizador em http://0.0.0.0:{0}/ (em máquinas remotas, encaminhe a "
       "porta: ssh -L {1}:localhost:{2} <host>)"),
    IT("Visualizzatore su http://0.0.0.0:{0}/ (su macchine remote inoltra la "
       "porta: ssh -L {1}:localhost:{2} <host>)"),
    NL("Viewer op http://0.0.0.0:{0}/ (op machines op afstand: stuur de poort "
       "door met ssh -L {1}:localhost:{2} <host>)"),
    RU("Просмотр по адресу http://0.0.0.0:{0}/ (для удалённых машин пробросьте "
       "порт: ssh -L {1}:localhost:{2} <host>)"),
    TR("Görüntüleyici http://0.0.0.0:{0}/ adresinde (uzak makinelerde bağlantı "
       "noktasını yönlendirin: ssh -L {1}:localhost:{2} <host>)"));
// {0} and {2} are already padded to a fixed width by the caller, so the
// numbers stay in a column while the words around them change length.
// {4}|{5} is elapsed|remaining, in the same words as the GUI status strip.
SS_MSG(train_step_line,
    EN("step {0}/{1} ({2}%)  splats {3}  [elapsed {4} | ETA {5}]"),
    JA("ステップ {0}/{1} ({2}%)  スプラット {3}  [経過 {4} | 残り {5}]"),
    ZH_HANS("步 {0}/{1} ({2}%)  泼溅 {3}  [已用 {4} | 剩余 {5}]"),
    ZH_HANT("步 {0}/{1} ({2}%)  潑濺 {3}  [已用 {4} | 剩餘 {5}]"),
    KO("스텝 {0}/{1} ({2}%)  스플랫 {3}  [경과 {4} | 남은 시간 {5}]"),
    DE("Schritt {0}/{1} ({2}%)  Splats {3}  [Laufzeit {4} | Restzeit {5}]"),
    FR("étape {0}/{1} ({2} %)  splats {3}  [écoulé {4} | reste {5}]"),
    ES("paso {0}/{1} ({2}%)  splats {3}  [lleva {4} | faltan {5}]"),
    PT("passo {0}/{1} ({2}%)  splats {3}  [decorrido {4} | faltam {5}]"),
    IT("passo {0}/{1} ({2}%)  splat {3}  [trascorso {4} | mancano {5}]"),
    NL("stap {0}/{1} ({2}%)  splats {3}  [verstreken {4} | nog {5}]"),
    RU("шаг {0}/{1} ({2}%)  сплатов {3}  [прошло {4} | осталось {5}]"),
    TR("adım {0}/{1} (%{2})  splat {3}  [geçen {4} | kalan {5}]"));
SS_MSG(train_done_viewer,
    EN("Training complete. Viewer still running -- press Ctrl-C to exit."),
    JA("学習が完了しました。ビューアはまだ動いています。終了するには Ctrl-C を"
       "押してください。"),
    ZH_HANS("训练完成。查看器仍在运行——按 Ctrl-C 退出。"),
    ZH_HANT("訓練完成。檢視器仍在執行——按 Ctrl-C 結束。"),
    KO("학습이 끝났습니다. 뷰어는 아직 돌고 있습니다. 끝내려면 Ctrl-C를 누르세요."),
    DE("Training abgeschlossen. Der Betrachter läuft weiter -- zum Beenden "
       "Strg-C drücken."),
    FR("Entraînement terminé. La visionneuse tourne toujours -- appuyez sur "
       "Ctrl-C pour quitter."),
    ES("Entrenamiento terminado. El visor sigue funcionando; pulsa Ctrl-C para "
       "salir."),
    PT("Treinamento concluído. O visualizador continua rodando -- pressione "
       "Ctrl-C para sair."),
    IT("Addestramento completato. Il visualizzatore è ancora attivo -- premi "
       "Ctrl-C per uscire."),
    NL("Training klaar. De viewer draait nog -- druk op Ctrl-C om te stoppen."),
    RU("Обучение завершено. Просмотр ещё работает -- нажмите Ctrl-C, чтобы "
       "выйти."),
    TR("Eğitim tamamlandı. Görüntüleyici hâlâ çalışıyor -- çıkmak için Ctrl-C'ye "
       "basın."));
// ===========================================================================
// `spirula mesh`
// ===========================================================================
//
// The meshing pipeline's own running commentary -- "[meshing] uv: packed 41
// charts, 78% efficiency" and the rest -- is a DIAGNOSTIC, stays English, and
// doubles as the marker the GUI's progress bar reads (src/app/gui/MeshRunner.cpp
// keys on those stage words, so translating them would be translating a
// protocol). What is here is the part addressed to the person waiting: the
// advice, the warnings and the failure. The stage NAMES the GUI shows under
// its progress bar are translated too, out of i18n/catalog/Log.h.

SS_MSG(mesh_no_cameras,
    EN("No camera dataset in use: meshing from Gaussian densities only. Pass "
       "--data <dataset_dir> for significantly better surfaces."),
    JA("カメラのデータセットを使っていません。ガウシアンの密度だけからメッシュを"
       "作ります。--data <dataset_dir> を渡すと、面の質が大きく良くなります。"),
    ZH_HANS("没有使用相机数据集：只根据高斯密度生成网格。加上 --data "
            "<dataset_dir> 能让表面好得多。"),
    ZH_HANT("沒有使用相機資料集：只根據高斯密度產生網格。加上 --data "
            "<dataset_dir> 能讓表面好得多。"),
    KO("카메라 데이터셋을 쓰지 않습니다. 가우시안 밀도만으로 메시를 만듭니다. "
       "--data <dataset_dir>를 주면 표면이 훨씬 좋아집니다."),
    DE("Kein Kamera-Datensatz in Gebrauch: das Netz entsteht nur aus den "
       "Gauß-Dichten. Mit --data <dataset_dir> werden die Oberflächen deutlich "
       "besser."),
    FR("Aucun jeu de données de caméras utilisé : le maillage ne vient que des "
       "densités gaussiennes. Passez --data <dataset_dir> pour des surfaces "
       "nettement meilleures."),
    ES("No se usa ningún conjunto de datos de cámaras: la malla sale solo de "
       "las densidades gaussianas. Pasa --data <dataset_dir> para conseguir "
       "superficies bastante mejores."),
    PT("Nenhum conjunto de dados de câmeras em uso: a malha vem só das "
       "densidades gaussianas. Passe --data <dataset_dir> para superfícies "
       "bem melhores."),
    IT("Nessun set di dati di camere in uso: la mesh viene solo dalle densità "
       "gaussiane. Passa --data <dataset_dir> per superfici molto migliori."),
    NL("Geen cameradataset in gebruik: de mesh komt alleen uit de "
       "Gauss-dichtheden. Geef --data <dataset_dir> mee voor duidelijk betere "
       "oppervlakken."),
    RU("Набор данных камер не используется: меш строится только по гауссовым "
       "плотностям. Передайте --data <dataset_dir>, и поверхности будут заметно "
       "лучше."),
    TR("Kamera veri kümesi kullanılmıyor: ağ yalnızca Gauss yoğunluklarından "
       "çıkarılıyor. --data <dataset_dir> verirseniz yüzeyler belirgin biçimde "
       "iyileşir."));
SS_MSG(mesh_mixed_camera_models,
    EN("warning: mixed camera models in dataset; using the first one for all "
       "cameras"),
    JA("警告: データセットにカメラモデルが混在しています。すべてのカメラに"
       "最初のものを使います"),
    ZH_HANS("警告：数据集里混有多种相机模型，将对所有相机使用第一种"),
    ZH_HANT("警告：資料集裡混有多種相機模型，將對所有相機使用第一種"),
    KO("경고: 데이터셋에 카메라 모델이 섞여 있습니다. 모든 카메라에 첫 번째 것을 "
       "씁니다"),
    DE("Warnung: gemischte Kameramodelle im Datensatz; für alle Kameras wird "
       "das erste benutzt"),
    FR("Avertissement : modèles de caméra mélangés dans le jeu de données ; le "
       "premier sera utilisé pour toutes les caméras"),
    ES("Aviso: hay modelos de cámara mezclados en el conjunto de datos; se "
       "usará el primero para todas las cámaras"),
    PT("Aviso: modelos de câmera misturados no conjunto de dados; o primeiro "
       "será usado para todas as câmeras"),
    IT("Avviso: modelli di camera misti nel set di dati; verrà usato il primo "
       "per tutte le camere"),
    NL("Waarschuwing: gemengde cameramodellen in de dataset; het eerste wordt "
       "voor alle camera's gebruikt"),
    RU("Предупреждение: в наборе данных смешаны модели камер; для всех камер "
       "будет взята первая"),
    TR("Uyarı: veri kümesinde karışık kamera modelleri var; hepsi için ilki "
       "kullanılacak"));

SS_MSG(error_line,
    EN("error: {0}"),
    JA("エラー: {0}"),
    ZH_HANS("错误：{0}"),
    ZH_HANT("錯誤：{0}"),
    KO("오류: {0}"),
    DE("Fehler: {0}"),
    FR("erreur : {0}"),
    ES("error: {0}"),
    PT("erro: {0}"),
    IT("errore: {0}"),
    NL("fout: {0}"),
    RU("ошибка: {0}"),
    TR("hata: {0}"));


// ===========================================================================
// `spirula sam` -- what a run says around its own work
// ===========================================================================
// The detection table it writes to stdout is machine-readable output (a TSV a
// script reads) and stays as it is, columns and all. These are the lines
// around it, which a person reads.

SS_MSG(sam_flag_needs_value,
    EN("{0} needs a value"),
    JA("{0} には値が必要です"),
    ZH_HANS("{0} 需要一个值"),
    ZH_HANT("{0} 需要一個值"),
    KO("{0} 에는 값이 필요합니다"),
    DE("{0} braucht einen Wert"),
    FR("{0} attend une valeur"),
    ES("{0} necesita un valor"),
    PT("{0} precisa de um valor"),
    IT("{0} richiede un valore"),
    NL("{0} heeft een waarde nodig"),
    RU("для {0} нужно значение"),
    TR("{0} bir değer ister"));

SS_MSG(sam_unknown_option,
    EN("unknown or malformed option: {0}"),
    JA("知らない、または形式の誤ったオプション: {0}"),
    ZH_HANS("未知或格式有误的选项：{0}"),
    ZH_HANT("未知或格式有誤的選項：{0}"),
    KO("모르거나 형식이 잘못된 옵션: {0}"),
    DE("unbekannte oder fehlerhafte Option: {0}"),
    FR("option inconnue ou mal formée : {0}"),
    ES("opción desconocida o mal formada: {0}"),
    PT("opção desconhecida ou malformada: {0}"),
    IT("opzione sconosciuta o malformata: {0}"),
    NL("onbekende of misvormde optie: {0}"),
    RU("неизвестный или неверно записанный параметр: {0}"),
    TR("bilinmeyen ya da bozuk seçenek: {0}"));

SS_MSG(sam_unexpected_argument,
    EN("unexpected argument: {0}"),
    JA("余分な引数: {0}"),
    ZH_HANS("多余的参数：{0}"),
    ZH_HANT("多餘的參數：{0}"),
    KO("예상하지 못한 인자: {0}"),
    DE("unerwartetes Argument: {0}"),
    FR("argument inattendu : {0}"),
    ES("argumento inesperado: {0}"),
    PT("argumento inesperado: {0}"),
    IT("argomento inatteso: {0}"),
    NL("onverwacht argument: {0}"),
    RU("лишний аргумент: {0}"),
    TR("beklenmeyen argüman: {0}"));

SS_MSG(sam_no_devices,
    EN("no Vulkan devices were found -- is a driver installed?"),
    JA("Vulkan デバイスが見つかりません。ドライバーは入っていますか。"),
    ZH_HANS("没有找到 Vulkan 设备 —— 是否已安装驱动？"),
    ZH_HANT("沒有找到 Vulkan 裝置 —— 是否已安裝驅動程式？"),
    KO("Vulkan 장치를 찾지 못했습니다 -- 드라이버가 설치되어 있습니까?"),
    DE("es wurden keine Vulkan-Geräte gefunden -- ist ein Treiber installiert?"),
    FR("aucun périphérique Vulkan trouvé -- un pilote est-il installé ?"),
    ES("no se encontró ningún dispositivo Vulkan: ¿hay algún controlador "
       "instalado?"),
    PT("nenhum dispositivo Vulkan foi encontrado -- há algum driver instalado?"),
    IT("non è stato trovato alcun dispositivo Vulkan -- c'è un driver "
       "installato?"),
    NL("er zijn geen Vulkan-apparaten gevonden -- is er een stuurprogramma "
       "geïnstalleerd?"),
    RU("устройства Vulkan не найдены -- установлен ли драйвер?"),
    TR("hiç Vulkan aygıtı bulunamadı -- sürücü kurulu mu?"));

SS_MSG(sam_col_index,
    EN("idx"), JA("番号"), ZH_HANS("序号"), ZH_HANT("序號"), KO("번호"),
    DE("Nr."), FR("no"), ES("núm."), PT("núm."), IT("n."), NL("nr."),
    RU("ном."), TR("no"));

SS_MSG(sam_col_name,
    EN("name"), JA("名前"), ZH_HANS("名称"), ZH_HANT("名稱"), KO("이름"),
    DE("Name"), FR("nom"), ES("nombre"), PT("nome"), IT("nome"), NL("naam"),
    RU("название"), TR("ad"));

SS_MSG(sam_col_type,
    EN("type"), JA("種類"), ZH_HANS("类型"), ZH_HANT("類型"), KO("종류"),
    DE("Typ"), FR("type"), ES("tipo"), PT("tipo"), IT("tipo"), NL("type"),
    RU("тип"), TR("tür"));

SS_MSG(sam_col_vram,
    EN("vram"), JA("VRAM"), ZH_HANS("显存"), ZH_HANT("顯示記憶體"), KO("VRAM"),
    DE("VRAM"), FR("VRAM"), ES("VRAM"), PT("VRAM"), IT("VRAM"), NL("VRAM"),
    RU("видеопамять"), TR("VRAM"));

SS_MSG(sam_col_status,
    EN("status"), JA("状態"), ZH_HANS("状态"), ZH_HANT("狀態"), KO("상태"),
    DE("Status"), FR("état"), ES("estado"), PT("estado"), IT("stato"),
    NL("status"), RU("состояние"), TR("durum"));

SS_MSG(sam_status_ok,
    EN("ok"), JA("使用可"), ZH_HANS("可用"), ZH_HANT("可用"), KO("사용 가능"),
    DE("ok"), FR("ok"), ES("correcto"), PT("ok"), IT("ok"), NL("ok"),
    RU("годно"), TR("uygun"));

SS_MSG(sam_wrote_masks,
    EN("wrote masks: {0}, plus an overlay, to {1}"),
    JA("マスク {0} 枚とオーバーレイを {1} に書き出しました"),
    ZH_HANS("已把 {0} 张掩码及一张叠加图写入 {1}"),
    ZH_HANT("已把 {0} 張遮罩及一張疊加圖寫入 {1}"),
    KO("마스크 {0}장과 오버레이를 {1} 에 저장했습니다"),
    DE("geschriebene Masken: {0}, dazu eine Überlagerung, nach {1}"),
    FR("masques écrits : {0}, plus une superposition, dans {1}"),
    ES("máscaras escritas: {0}, más una superposición, en {1}"),
    PT("máscaras escritas: {0}, mais uma sobreposição, em {1}"),
    IT("maschere scritte: {0}, più una sovrapposizione, in {1}"),
    NL("geschreven maskers: {0}, plus een overlay, naar {1}"),
    RU("записано масок: {0}, плюс наложение, в {1}"),
    TR("yazılan maske: {0}, ayrıca bir bindirme, {1} içine"));

SS_MSG(sam_segment_needs,
    EN("segment needs --model and --image"),
    JA("segment には --model と --image が必要です"),
    ZH_HANS("segment 需要 --model 与 --image"),
    ZH_HANT("segment 需要 --model 與 --image"),
    KO("segment 에는 --model 과 --image 가 필요합니다"),
    DE("segment braucht --model und --image"),
    FR("segment a besoin de --model et --image"),
    ES("segment necesita --model e --image"),
    PT("segment precisa de --model e --image"),
    IT("segment richiede --model e --image"),
    NL("segment heeft --model en --image nodig"),
    RU("для segment нужны --model и --image"),
    TR("segment, --model ve --image ister"));

SS_MSG(sam_track_needs,
    EN("track needs --model and --frames <dir>"),
    JA("track には --model と --frames <dir> が必要です"),
    ZH_HANS("track 需要 --model 与 --frames <dir>"),
    ZH_HANT("track 需要 --model 與 --frames <dir>"),
    KO("track 에는 --model 과 --frames <dir> 가 필요합니다"),
    DE("track braucht --model und --frames <dir>"),
    FR("track a besoin de --model et --frames <dir>"),
    ES("track necesita --model y --frames <dir>"),
    PT("track precisa de --model e --frames <dir>"),
    IT("track richiede --model e --frames <dir>"),
    NL("track heeft --model en --frames <dir> nodig"),
    RU("для track нужны --model и --frames <dir>"),
    TR("track, --model ve --frames <dir> ister"));

SS_MSG(sam_no_images_in,
    EN("no images in {0}"),
    JA("{0} に画像がありません"),
    ZH_HANS("{0} 中没有图像"),
    ZH_HANT("{0} 中沒有影像"),
    KO("{0} 에 이미지가 없습니다"),
    DE("keine Bilder in {0}"),
    FR("aucune image dans {0}"),
    ES("no hay imágenes en {0}"),
    PT("não há imagens em {0}"),
    IT("nessuna immagine in {0}"),
    NL("geen beelden in {0}"),
    RU("в {0} нет изображений"),
    TR("{0} içinde görüntü yok"));

SS_MSG(sam_frame_line,
    EN("frame {0}: {1} -- kept {2}%"),
    JA("フレーム {0}: {1} -- 残り {2}%"),
    ZH_HANS("帧 {0}：{1} —— 保留 {2}%"),
    ZH_HANT("影格 {0}：{1} —— 保留 {2}%"),
    KO("프레임 {0}: {1} -- 남김 {2}%"),
    DE("Bild {0}: {1} -- behalten {2} %"),
    FR("image {0} : {1} -- conservé {2} %"),
    ES("fotograma {0}: {1}: se conserva el {2} %"),
    PT("quadro {0}: {1} -- mantido {2}%"),
    IT("fotogramma {0}: {1} -- tenuto {2}%"),
    NL("beeld {0}: {1} -- behouden {2}%"),
    RU("кадр {0}: {1} -- оставлено {2} %"),
    TR("kare {0}: {1} -- korunan %{2}"));

SS_MSG(sam_track_summary,
    EN("frames: {0} in {1} s -- {2} ms per frame (decode {3}, model {4}, "
       "write {5})"),
    JA("フレーム: {0}（{1} 秒）-- 1 フレームあたり {2} ミリ秒"
       "（デコード {3}、モデル {4}、書き出し {5}）"),
    ZH_HANS("帧数：{0}，用时 {1} 秒 —— 每帧 {2} 毫秒（解码 {3}，模型 {4}，写出 {5}）"),
    ZH_HANT("影格數：{0}，用時 {1} 秒 —— 每格 {2} 毫秒（解碼 {3}，模型 {4}，寫出 {5}）"),
    KO("프레임: {0}, {1}초 -- 프레임당 {2} 밀리초(디코딩 {3}, 모델 {4}, 저장 {5})"),
    DE("Einzelbilder: {0} in {1} s -- {2} ms je Bild (Dekodierung {3}, "
       "Modell {4}, Schreiben {5})"),
    FR("images : {0} en {1} s -- {2} ms par image (décodage {3}, modèle {4}, "
       "écriture {5})"),
    ES("fotogramas: {0} en {1} s: {2} ms por fotograma (descodificación {3}, "
       "modelo {4}, escritura {5})"),
    PT("quadros: {0} em {1} s -- {2} ms por quadro (decodificação {3}, "
       "modelo {4}, escrita {5})"),
    IT("fotogrammi: {0} in {1} s -- {2} ms per fotogramma (decodifica {3}, "
       "modello {4}, scrittura {5})"),
    NL("beelden: {0} in {1} s -- {2} ms per beeld (decoderen {3}, model {4}, "
       "schrijven {5})"),
    RU("кадров: {0} за {1} с -- {2} мс на кадр (декодирование {3}, модель {4}, "
       "запись {5})"),
    TR("kare: {0}, {1} sn -- kare başına {2} ms (çözme {3}, model {4}, "
       "yazma {5})"));

SS_MSG(sam_no_video_decoder,
    EN("this build has no in-process video decoder: it is compiled only with "
       "-DSS_ENABLE_PATENTED=ON (see cmake/SsOptions.cmake). Use ffmpeg to "
       "inspect a file or extract frames instead."),
    JA("このビルドにはプロセス内の動画デコーダーがありません。"
       "-DSS_ENABLE_PATENTED=ON でのみ組み込まれます（cmake/SsOptions.cmake 参照）。"
       "ファイルの確認やフレームの取り出しには ffmpeg を使ってください。"),
    ZH_HANS("本版本没有进程内视频解码器：它只在 -DSS_ENABLE_PATENTED=ON 时编译"
            "（见 cmake/SsOptions.cmake）。请改用 ffmpeg 查看文件或提取帧。"),
    ZH_HANT("本版本沒有行程內視訊解碼器：它只在 -DSS_ENABLE_PATENTED=ON 時編譯"
            "（見 cmake/SsOptions.cmake）。請改用 ffmpeg 檢視檔案或擷取影格。"),
    KO("이 빌드에는 프로세스 내 비디오 디코더가 없습니다: -DSS_ENABLE_PATENTED=ON "
       "일 때만 컴파일됩니다(cmake/SsOptions.cmake 참고). 파일 확인이나 프레임 "
       "추출에는 ffmpeg 을 쓰세요."),
    DE("dieser Build hat keinen prozessinternen Videodekoder: er wird nur mit "
       "-DSS_ENABLE_PATENTED=ON übersetzt (siehe cmake/SsOptions.cmake). "
       "Nutzen Sie ffmpeg, um eine Datei zu prüfen oder Einzelbilder zu "
       "entnehmen."),
    FR("cette version n'a pas de décodeur vidéo intégré : il n'est compilé "
       "qu'avec -DSS_ENABLE_PATENTED=ON (voir cmake/SsOptions.cmake). Utilisez "
       "ffmpeg pour inspecter un fichier ou en extraire des images."),
    ES("esta compilación no tiene descodificador de vídeo en el propio proceso: "
       "solo se compila con -DSS_ENABLE_PATENTED=ON (mira "
       "cmake/SsOptions.cmake). Usa ffmpeg para inspeccionar un archivo o "
       "extraer fotogramas."),
    PT("esta compilação não tem decodificador de vídeo no próprio processo: ele "
       "só é compilado com -DSS_ENABLE_PATENTED=ON (veja "
       "cmake/SsOptions.cmake). Use o ffmpeg para inspecionar um arquivo ou "
       "extrair quadros."),
    IT("questa build non ha un decodificatore video nel processo: viene "
       "compilato solo con -DSS_ENABLE_PATENTED=ON (veda "
       "cmake/SsOptions.cmake). Usi ffmpeg per ispezionare un file o estrarne i "
       "fotogrammi."),
    NL("deze build heeft geen videodecoder in het proces zelf: die wordt alleen "
       "met -DSS_ENABLE_PATENTED=ON gecompileerd (zie cmake/SsOptions.cmake). "
       "Gebruik ffmpeg om een bestand te bekijken of er beelden uit te halen."),
    RU("в этой сборке нет внутрипроцессного видеодекодера: он собирается только "
       "с -DSS_ENABLE_PATENTED=ON (см. cmake/SsOptions.cmake). Для просмотра "
       "файла или извлечения кадров используйте ffmpeg."),
    TR("bu derlemede süreç içi video çözücü yok: yalnızca "
       "-DSS_ENABLE_PATENTED=ON ile derlenir (bkz. cmake/SsOptions.cmake). Bir "
       "dosyayı incelemek ya da kare çıkarmak için ffmpeg kullanın."));

SS_MSG(sam_extract_needs_decoder,
    EN("`extract` needs the in-process video decoder, which is compiled only "
       "with -DSS_ENABLE_PATENTED=ON (see cmake/SsOptions.cmake). Extract "
       "frames with ffmpeg and mask them with `{0} track` instead."),
    JA("`extract` にはプロセス内の動画デコーダーが必要ですが、これは "
       "-DSS_ENABLE_PATENTED=ON でのみ組み込まれます（cmake/SsOptions.cmake 参照）。"
       "ffmpeg でフレームを取り出し、`{0} track` でマスクしてください。"),
    ZH_HANS("`extract` 需要进程内视频解码器，而它只在 -DSS_ENABLE_PATENTED=ON 时编译"
            "（见 cmake/SsOptions.cmake）。请用 ffmpeg 提取帧，再用 `{0} track` 遮罩。"),
    ZH_HANT("`extract` 需要行程內視訊解碼器，而它只在 -DSS_ENABLE_PATENTED=ON 時編譯"
            "（見 cmake/SsOptions.cmake）。請用 ffmpeg 擷取影格，再用 `{0} track` 遮罩。"),
    KO("`extract` 에는 프로세스 내 비디오 디코더가 필요한데, 이는 "
       "-DSS_ENABLE_PATENTED=ON 일 때만 컴파일됩니다(cmake/SsOptions.cmake 참고). "
       "ffmpeg 으로 프레임을 뽑고 `{0} track` 으로 마스크하세요."),
    DE("`extract` braucht den prozessinternen Videodekoder, der nur mit "
       "-DSS_ENABLE_PATENTED=ON übersetzt wird (siehe cmake/SsOptions.cmake). "
       "Entnehmen Sie die Einzelbilder mit ffmpeg und maskieren Sie sie mit "
       "`{0} track`."),
    FR("`extract` a besoin du décodeur vidéo intégré, qui n'est compilé qu'avec "
       "-DSS_ENABLE_PATENTED=ON (voir cmake/SsOptions.cmake). Extrayez les "
       "images avec ffmpeg et masquez-les avec `{0} track`."),
    ES("`extract` necesita el descodificador de vídeo del propio proceso, que "
       "solo se compila con -DSS_ENABLE_PATENTED=ON (mira "
       "cmake/SsOptions.cmake). Extrae los fotogramas con ffmpeg y enmascáralos "
       "con `{0} track`."),
    PT("`extract` precisa do decodificador de vídeo no próprio processo, que só "
       "é compilado com -DSS_ENABLE_PATENTED=ON (veja cmake/SsOptions.cmake). "
       "Extraia os quadros com o ffmpeg e mascare-os com `{0} track`."),
    IT("`extract` richiede il decodificatore video nel processo, compilato solo "
       "con -DSS_ENABLE_PATENTED=ON (veda cmake/SsOptions.cmake). Estragga i "
       "fotogrammi con ffmpeg e li mascheri con `{0} track`."),
    NL("`extract` heeft de videodecoder in het proces zelf nodig, die alleen "
       "met -DSS_ENABLE_PATENTED=ON gecompileerd wordt (zie "
       "cmake/SsOptions.cmake). Haal de beelden eruit met ffmpeg en maskeer ze "
       "met `{0} track`."),
    RU("`extract` требует внутрипроцессного видеодекодера, который собирается "
       "только с -DSS_ENABLE_PATENTED=ON (см. cmake/SsOptions.cmake). Извлеките "
       "кадры через ffmpeg и замаскируйте их через `{0} track`."),
    TR("`extract`, süreç içi video çözücüyü ister; o da yalnızca "
       "-DSS_ENABLE_PATENTED=ON ile derlenir (bkz. cmake/SsOptions.cmake). "
       "Kareleri ffmpeg ile çıkarıp `{0} track` ile maskeleyin."));

SS_MSG(sam_video_decode,
    EN("Vulkan video decode: available"),
    JA("Vulkan の動画デコード: 使えます"),
    ZH_HANS("Vulkan 视频解码：可用"),
    ZH_HANT("Vulkan 視訊解碼：可用"),
    KO("Vulkan 비디오 디코딩: 사용 가능"),
    DE("Vulkan-Videodekodierung: verfügbar"),
    FR("décodage vidéo Vulkan : disponible"),
    ES("descodificación de vídeo con Vulkan: disponible"),
    PT("decodificação de vídeo com Vulkan: disponível"),
    IT("decodifica video Vulkan: disponibile"),
    NL("Vulkan-videodecodering: beschikbaar"),
    RU("декодирование видео через Vulkan: доступно"),
    TR("Vulkan video çözme: kullanılabilir"));

SS_MSG(sam_video_decode_why,
    EN("Vulkan video decode: {0}"),
    JA("Vulkan の動画デコード: {0}"),
    ZH_HANS("Vulkan 视频解码：{0}"),
    ZH_HANT("Vulkan 視訊解碼：{0}"),
    KO("Vulkan 비디오 디코딩: {0}"),
    DE("Vulkan-Videodekodierung: {0}"),
    FR("décodage vidéo Vulkan : {0}"),
    ES("descodificación de vídeo con Vulkan: {0}"),
    PT("decodificação de vídeo com Vulkan: {0}"),
    IT("decodifica video Vulkan: {0}"),
    NL("Vulkan-videodecodering: {0}"),
    RU("декодирование видео через Vulkan: {0}"),
    TR("Vulkan video çözme: {0}"));

SS_MSG(sam_wrote_path,
    EN("wrote {0}"),
    JA("{0} を書き出しました"),
    ZH_HANS("已写出 {0}"),
    ZH_HANT("已寫出 {0}"),
    KO("{0} 을(를) 저장했습니다"),
    DE("{0} geschrieben"),
    FR("{0} écrit"),
    ES("se escribió {0}"),
    PT("{0} escrito"),
    IT("scritto {0}"),
    NL("{0} geschreven"),
    RU("записано {0}"),
    TR("{0} yazıldı"));

SS_MSG(sam_decoded,
    EN("decoded frames: {0} in {1} ms ({2} per second)"),
    JA("デコードしたフレーム: {0}（{1} ミリ秒、毎秒 {2}）"),
    ZH_HANS("已解码的帧：{0}，用时 {1} 毫秒（每秒 {2}）"),
    ZH_HANT("已解碼的影格：{0}，用時 {1} 毫秒（每秒 {2}）"),
    KO("디코딩한 프레임: {0}, {1} 밀리초(초당 {2})"),
    DE("dekodierte Einzelbilder: {0} in {1} ms ({2} je Sekunde)"),
    FR("images décodées : {0} en {1} ms ({2} par seconde)"),
    ES("fotogramas descodificados: {0} en {1} ms ({2} por segundo)"),
    PT("quadros decodificados: {0} em {1} ms ({2} por segundo)"),
    IT("fotogrammi decodificati: {0} in {1} ms ({2} al secondo)"),
    NL("gedecodeerde beelden: {0} in {1} ms ({2} per seconde)"),
    RU("декодировано кадров: {0} за {1} мс ({2} в секунду)"),
    TR("çözülen kare: {0}, {1} ms (saniyede {2})"));

SS_MSG(sam_rotate_multiple_of_90,
    EN("--rotate must be a multiple of 90"),
    JA("--rotate は 90 の倍数でなければなりません"),
    ZH_HANS("--rotate 必须是 90 的倍数"),
    ZH_HANT("--rotate 必須是 90 的倍數"),
    KO("--rotate 는 90의 배수여야 합니다"),
    DE("--rotate muss ein Vielfaches von 90 sein"),
    FR("--rotate doit être un multiple de 90"),
    ES("--rotate debe ser múltiplo de 90"),
    PT("--rotate deve ser múltiplo de 90"),
    IT("--rotate deve essere un multiplo di 90"),
    NL("--rotate moet een veelvoud van 90 zijn"),
    RU("--rotate должен быть кратен 90"),
    TR("--rotate 90'ın katı olmalı"));

SS_MSG(sam_mask_needs,
    EN("mask needs a folder of images"),
    JA("mask には画像のフォルダーが必要です"),
    ZH_HANS("mask 需要一个图像文件夹"),
    ZH_HANT("mask 需要一個影像資料夾"),
    KO("mask 에는 이미지 폴더가 필요합니다"),
    DE("mask braucht einen Ordner mit Bildern"),
    FR("mask a besoin d'un dossier d'images"),
    ES("mask necesita una carpeta de imágenes"),
    PT("mask precisa de uma pasta de imagens"),
    IT("mask richiede una cartella di immagini"),
    NL("mask heeft een map met beelden nodig"),
    RU("для mask нужна папка с изображениями"),
    TR("mask, görüntü klasörü ister"));

// One camera's heading: the folder its frames live in, and how many there are.
SS_MSG(sam_mask_camera,
    EN("{0} -- images: {1}"),
    JA("{0} -- 画像: {1}"),
    ZH_HANS("{0} —— 图像：{1}"),
    ZH_HANT("{0} —— 影像：{1}"),
    KO("{0} -- 이미지: {1}"),
    DE("{0} -- Bilder: {1}"),
    FR("{0} -- images : {1}"),
    ES("{0} -- imágenes: {1}"),
    PT("{0} -- imagens: {1}"),
    IT("{0} -- immagini: {1}"),
    NL("{0} -- beelden: {1}"),
    RU("{0} -- изображений: {1}"),
    TR("{0} -- görüntü: {1}"));

SS_MSG(sam_mask_found,
    EN("border: {0} -- keeps {1}% of the frame"),
    JA("枠: {0} -- 画面の {1}% を残します"),
    ZH_HANS("边框：{0} —— 保留画面的 {1}%"),
    ZH_HANT("邊框：{0} —— 保留畫面的 {1}%"),
    KO("테두리: {0} -- 화면의 {1}% 를 남깁니다"),
    DE("Rand: {0} -- behält {1} % des Bildes"),
    FR("bord : {0} -- conserve {1} % de l'image"),
    ES("borde: {0}: conserva el {1} % del fotograma"),
    PT("borda: {0} -- mantém {1}% do quadro"),
    IT("bordo: {0} -- tiene il {1}% del fotogramma"),
    NL("rand: {0} -- behoudt {1}% van het beeld"),
    RU("край: {0} -- остаётся {1} % кадра"),
    TR("kenar: {0} -- karenin %{1} kadarı kalır"));

SS_MSG(sam_mask_no_border,
    EN("no border found in {0}; name one with --shape"),
    JA("{0} では枠が見つかりませんでした。--shape で指定してください"),
    ZH_HANS("在 {0} 中没找到边框；请用 --shape 指定"),
    ZH_HANT("在 {0} 中沒找到邊框；請用 --shape 指定"),
    KO("{0} 에서 테두리를 찾지 못했습니다. --shape 로 지정하세요"),
    DE("in {0} kein Rand gefunden; einen mit --shape angeben"),
    FR("aucun bord trouvé dans {0} ; indiquez-en un avec --shape"),
    ES("no se encontró borde en {0}; indique uno con --shape"),
    PT("nenhuma borda encontrada em {0}; indique uma com --shape"),
    IT("nessun bordo trovato in {0}; ne indichi uno con --shape"),
    NL("geen rand gevonden in {0}; geef er een op met --shape"),
    RU("в {0} край не найден; задайте его через --shape"),
    TR("{0} içinde kenar bulunamadı; --shape ile bildirin"));

SS_MSG(sam_mask_wrote,
    EN("masks written: {0}, in {1}"),
    JA("書き出したマスク: {0}（{1}）"),
    ZH_HANS("已写出掩码：{0}，位于 {1}"),
    ZH_HANT("已寫出遮罩：{0}，位於 {1}"),
    KO("쓴 마스크: {0}, 위치 {1}"),
    DE("geschriebene Masken: {0}, in {1}"),
    FR("masques écrits : {0}, dans {1}"),
    ES("máscaras escritas: {0}, en {1}"),
    PT("máscaras escritas: {0}, em {1}"),
    IT("maschere scritte: {0}, in {1}"),
    NL("geschreven maskers: {0}, in {1}"),
    RU("записано масок: {0}, в {1}"),
    TR("yazılan maske: {0}, {1} içinde"));

SS_MSG(sam_mask_bad_image,
    EN("--mask-image: cannot read '{0}'"),
    JA("--mask-image: '{0}' を読み取れません"),
    ZH_HANS("--mask-image：无法读取 '{0}'"),
    ZH_HANT("--mask-image：無法讀取 '{0}'"),
    KO("--mask-image: '{0}' 을(를) 읽을 수 없습니다"),
    DE("--mask-image: '{0}' ist nicht lesbar"),
    FR("--mask-image : '{0}' est illisible"),
    ES("--mask-image: no se puede leer '{0}'"),
    PT("--mask-image: não dá para ler '{0}'"),
    IT("--mask-image: '{0}' non è leggibile"),
    NL("--mask-image: '{0}' is niet te lezen"),
    RU("--mask-image: не удалось прочитать '{0}'"),
    TR("--mask-image: '{0}' okunamıyor"));

SS_MSG(sam_mask_bad_shape,
    EN("--shape: cannot read '{0}'"),
    JA("--shape: '{0}' を読み取れません"),
    ZH_HANS("--shape：无法解读 '{0}'"),
    ZH_HANT("--shape：無法解讀 '{0}'"),
    KO("--shape: '{0}' 을(를) 읽을 수 없습니다"),
    DE("--shape: '{0}' ist nicht lesbar"),
    FR("--shape : '{0}' est illisible"),
    ES("--shape: no se puede leer '{0}'"),
    PT("--shape: não dá para ler '{0}'"),
    IT("--shape: '{0}' non è leggibile"),
    NL("--shape: '{0}' is niet te lezen"),
    RU("--shape: не удалось прочитать '{0}'"),
    TR("--shape: '{0}' okunamıyor"));


// {0} is the command to type, built by the caller ("spirula sfm map --help").
SS_MSG(usage_try_help,
    EN("Try '{0}' for more information."),
    JA("詳しくは '{0}' を実行してください。"),
    ZH_HANS("更多信息请运行 '{0}'。"),
    ZH_HANT("更多資訊請執行 '{0}'。"),
    KO("자세한 내용은 '{0}' 을(를) 실행하세요."),
    DE("Mehr dazu mit '{0}'."),
    FR("Pour en savoir plus : '{0}'."),
    ES("Para saber más, ejecuta '{0}'."),
    PT("Para saber mais, execute '{0}'."),
    IT("Per saperne di più: '{0}'."),
    NL("Meer weten? Voer '{0}' uit."),
    RU("Подробнее: '{0}'."),
    TR("Ayrıntı için '{0}' çalıştırın."));

SS_MSG(sfm_merge_output_is_input,
    EN("--output {0} is where the input models live; pass --in-place if that is "
       "what you want"),
    JA("--output {0} は入力モデルのある場所です。意図どおりなら --in-place を"
       "渡してください"),
    ZH_HANS("--output {0} 正是输入模型所在之处；若确实要如此，请加 --in-place"),
    ZH_HANT("--output {0} 正是輸入模型所在之處；若確實要如此，請加 --in-place"),
    KO("--output {0} 은(는) 입력 모델이 있는 곳입니다. 그것이 뜻한 바라면 "
       "--in-place 를 주세요"),
    DE("--output {0} ist der Ort, an dem die Eingabemodelle liegen; übergeben "
       "Sie --in-place, wenn das gewollt ist"),
    FR("--output {0} est là où se trouvent les modèles d'entrée ; passez "
       "--in-place si c'est bien ce que vous voulez"),
    ES("--output {0} es donde están los modelos de entrada; pasa --in-place si "
       "es eso lo que quieres"),
    PT("--output {0} é onde estão os modelos de entrada; passe --in-place se é "
       "isso que você quer"),
    IT("--output {0} è dove stanno i modelli di ingresso; passi --in-place se è "
       "questo che vuole"),
    NL("--output {0} is waar de invoermodellen staan; geef --in-place op als "
       "dat de bedoeling is"),
    RU("--output {0} -- это то место, где лежат входные модели; передайте "
       "--in-place, если так и задумано"),
    TR("--output {0} zaten girdi modellerinin bulunduğu yer; istediğiniz buysa "
       "--in-place verin"));

}  // namespace cli
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
