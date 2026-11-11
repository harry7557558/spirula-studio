#pragma once

// The New Dataset screen, the mask preview, and the model licence prompt --
// everything between "here are my photos" and "here is a dataset to train on".
//
// This is where most of the beginner-facing copy lives, so it is also where a
// bad translation costs the most. The messages attached to something
// irreversible or expensive (overwriting a reconstruction, a 700 MB download,
// accepting a licence) are marked with a comment; those get human review in
// every language before shipping, machine translation is not acceptable for
// them.
//
// Same two rules as Gui.h: no sentence built from fragments, no
// plural-sensitive counting.

#include "i18n/BeginCatalog.h"

#include <cstddef>
#include <cstring>

namespace spirula {
namespace i18n {
namespace msg {
namespace dataset {

// ===========================================================================
// Screen chrome
// ===========================================================================

SS_MSG(title_from_video,
    EN("Create Dataset from Video"),
    JA("動画からデータセットを作成"),
    ZH_HANS("从视频创建数据集"),
    ZH_HANT("從影片建立資料集"),
    KO("동영상으로 데이터셋 만들기"),
    DE("Datensatz aus Video erstellen"),
    FR("Créer un jeu de données à partir d'une vidéo"),
    ES("Crear un conjunto de datos a partir de un vídeo"),
    PT("Criar um conjunto de dados a partir de um vídeo"),
    IT("Crea un set di dati da un video"),
    NL("Dataset maken uit video"),
    RU("Создание набора данных из видео"),
    TR("Videodan veri kümesi oluştur"));

SS_MSG(title_from_photos,
    EN("Create Dataset from Photos"),
    JA("写真からデータセットを作成"),
    ZH_HANS("从照片创建数据集"),
    ZH_HANT("從相片建立資料集"),
    KO("사진으로 데이터셋 만들기"),
    DE("Datensatz aus Fotos erstellen"),
    FR("Créer un jeu de données à partir de photos"),
    ES("Crear un conjunto de datos a partir de fotos"),
    PT("Criar um conjunto de dados a partir de fotos"),
    IT("Crea un set di dati da fotografie"),
    NL("Dataset maken uit foto's"),
    RU("Создание набора данных из фотографий"),
    TR("Fotoğraflardan veri kümesi oluştur"));

SS_MSG(section_settings,
    EN("Settings"),      JA("設定"),          ZH_HANS("设置"),     ZH_HANT("設定"),
    KO("설정"),           DE("Einstellungen"), FR("Réglages"),    ES("Ajustes"),
    PT("Configurações"), IT("Impostazioni"), NL("Instellingen"), RU("Настройки"),
    TR("Ayarlar"));

SS_MSG(section_advanced,
    EN("Advanced"),      JA("詳細設定"),      ZH_HANS("高级"),     ZH_HANT("進階"),
    KO("고급"),           DE("Erweitert"),    FR("Avancé"),       ES("Avanzado"),
    PT("Avançado"),      IT("Avanzate"),     NL("Geavanceerd"),  RU("Дополнительно"),
    TR("Gelişmiş"));

SS_MSG(section_color_space,
    EN("Colour Space"),   JA("色空間"),         ZH_HANS("色彩空间"),  ZH_HANT("色彩空間"),
    KO("색 공간"),         DE("Farbraum"),      FR("Espace colorimétrique"),
    ES("Espacio de color"), PT("Espaço de cor"), IT("Spazio colore"),
    NL("Kleurruimte"),    RU("Цветовое пространство"), TR("Renk uzayı"));

SS_MSG(input_gamut,
    EN("Input colour space"), JA("入力の色域"),  ZH_HANS("输入色域"),  ZH_HANT("輸入色域"),
    KO("입력 색역"),          DE("Eingabefarbraum"), FR("Espace des entrées"),
    ES("Espacio de entrada"), PT("Espaço de entrada"), IT("Spazio d'ingresso"),
    NL("Invoerkleurruimte"),  RU("Цветовое пространство входа"),
    TR("Girdi renk uzayı"));

SS_MSG(input_gamut_help,
    EN("Photo color primaries. Reconstruction, AI masks and geometry estimation convert them to sRGB for their models."),
    JA("写真の原色です。再構成、AIマスク、形状推定ではモデル用にsRGBへ変換します。"),
    ZH_HANS("照片的色彩原色。重建、AI 蒙版和几何估计会将其转换为模型使用的 sRGB。"),
    ZH_HANT("照片的色彩原色。重建、AI 遮罩和幾何估計會將其轉換為模型使用的 sRGB。"),
    KO("사진의 색 원색입니다. 재구성, AI 마스크, 기하 추정에서는 모델에 맞게 sRGB로 변환합니다."),
    DE("Farbprimärwerte der Fotos. Rekonstruktion, KI-Masken und Geometrieschätzung wandeln sie für ihre Modelle nach sRGB um."),
    FR("Primaires des photos. Reconstruction, masques IA et estimation géométrique les convertissent en sRGB pour leurs modèles."),
    ES("Primarias de color de las fotos. Reconstrucción, máscaras IA y geometría las convierten a sRGB para sus modelos."),
    PT("Primárias de cor das fotos. Reconstrução, máscaras IA e geometria convertem-nas para sRGB para os seus modelos."),
    IT("Primarie delle foto. Ricostruzione, maschere IA e stima geometrica le convertono in sRGB per i loro modelli."),
    NL("Kleurprimairen van foto’s. Reconstructie, AI-maskers en geometrieschatting zetten ze om naar sRGB voor hun modellen."),
    RU("Цветовые первичные фото. Реконструкция, ИИ-маски и оценка геометрии переводят их в sRGB для своих моделей."),
    TR("Fotoğrafın ana renkleri. Yeniden oluşturma, AI maskeleri ve geometri kestirimi modelleri için sRGB’ye dönüştürür."));

SS_MSG(input_is_linear,
    EN("Input light encoding"),
    JA("入力の光エンコード"),
    ZH_HANS("输入的光编码"),
    ZH_HANT("輸入的光編碼"),
    KO("입력 광 인코딩"),
    DE("Lichtcodierung der Eingabe"),
    FR("Encodage de la lumière"),
    ES("Codificación de la luz de entrada"),
    PT("Codificação da luz de entrada"),
    IT("Codifica della luce in ingresso"),
    NL("Lichtcodering van de invoer"),
    RU("Кодировка света на входе"),
    TR("Girdi ışık kodlaması"));

// Item 0 of BOTH colour-space pickers: an EXR or a TIFF with an ICC profile
// declares its own, and this is what says "do not override it".
SS_MSG(space_from_file,
    EN("From the file"), JA("ファイルから"), ZH_HANS("取自文件"),
    ZH_HANT("取自檔案"), KO("파일에서"), DE("Aus der Datei"),
    FR("D'après le fichier"), ES("Según el archivo"), PT("Conforme o arquivo"),
    IT("Dal file"), NL("Uit het bestand"), RU("Из файла"), TR("Dosyadan"));

SS_MSG(transfer_linear,
    EN("Linear light"), JA("リニア光"), ZH_HANS("线性光"), ZH_HANT("線性光"),
    KO("선형 광"), DE("Lineares Licht"), FR("Lumière linéaire"),
    ES("Luz lineal"), PT("Luz linear"), IT("Luce lineare"),
    NL("Lineair licht"), RU("Линейный свет"), TR("Doğrusal ışık"));

SS_MSG(transfer_display,
    EN("Display-encoded"), JA("表示用エンコード"), ZH_HANS("显示编码"),
    ZH_HANT("顯示編碼"), KO("디스플레이 인코딩"), DE("Anzeigecodiert"),
    FR("Encodé pour l'affichage"), ES("Codificado para pantalla"),
    PT("Codificado para exibição"), IT("Codificato per la visualizzazione"),
    NL("Voor weergave gecodeerd"), RU("С кодировкой для дисплея"),
    TR("Ekran için kodlanmış"));

SS_MSG(input_is_linear_help,
    EN("Whether the pictures hold scene-linear light (EXR, linear 16-bit) or ordinary display-encoded values. Read from an EXR's header or a TIFF's ICC profile unless you set it here."),
    JA("写真がシーンリニアの光（EXR、リニア 16 ビット）か、通常の表示用にエンコードされた値かです。ここで設定しない限り、EXR のヘッダーや TIFF の ICC プロファイルから読み取ります。"),
    ZH_HANS("照片存的是场景线性光（EXR、线性 16 位），还是普通的显示编码数值。除非在此设置，否则取自 EXR 的文件头或 TIFF 的 ICC 配置文件。"),
    ZH_HANT("照片存的是場景線性光（EXR、線性 16 位元），還是普通的顯示編碼數值。除非在此設定，否則取自 EXR 的檔頭或 TIFF 的 ICC 設定檔。"),
    KO("사진이 장면 선형 광(EXR, 선형 16비트)인지 보통의 디스플레이 인코딩 값인지입니다. 여기서 설정하지 않으면 EXR 헤더나 TIFF 의 ICC 프로파일에서 읽습니다."),
    DE("Ob die Bilder szenenlineares Licht (EXR, lineare 16 Bit) oder gewöhnliche anzeigecodierte Werte enthalten.\nWird aus dem Kopf einer EXR oder dem ICC-Profil einer TIFF gelesen, solange Sie es hier nicht setzen."),
    FR("Lumière scène-linéaire (EXR, 16 bits linéaire) ou valeurs encodées pour l’affichage. Détecté via l’en-tête EXR ou le profil ICC TIFF, sauf réglage explicite."),
    ES("Si las fotos guardan luz escena-lineal (EXR, 16 bits lineal) o valores corrientes codificados para pantalla.\nSe lee de la cabecera de un EXR o del perfil ICC de un TIFF mientras no lo fije aquí."),
    PT("Se as fotos guardam luz cena-linear (EXR, 16 bits linear) ou valores comuns codificados para exibição.\nLido do cabeçalho de um EXR ou do perfil ICC de um TIFF enquanto não o definir aqui."),
    IT("Luce lineare di scena (EXR, 16 bit lineare) o valori per lo schermo. Letto da intestazione EXR o profilo ICC TIFF, salvo impostazione esplicita."),
    NL("Of de foto's scène-lineair licht (EXR, lineair 16-bits) bevatten of gewone voor weergave gecodeerde waarden.\nWordt uit de kop van een EXR of het ICC-profiel van een TIFF gelezen zolang u het hier niet instelt."),
    RU("Хранят ли снимки сцен-линейный свет (EXR, линейные 16 бит) или обычные значения с кодировкой для дисплея.\nЧитается из заголовка EXR или ICC-профиля TIFF, пока вы не зададите это здесь."),
    TR("Fotoğrafların sahne-doğrusal ışık (EXR, doğrusal 16 bit) mi yoksa sıradan ekran için kodlanmış değerler mi tuttuğu.\nBurada ayarlamadığınız sürece bir EXR'nin başlığından ya da bir TIFF'in ICC profilinden okunur."));

// What reconstruction, masking and geometry see, never what training reads
// (--image-exposure). Item 2 opens a number of stops.
SS_MSG(input_exposure,
    EN("Exposure for analysis"), JA("解析用の露出"), ZH_HANS("分析用曝光"),
    ZH_HANT("分析用曝光"), KO("분석용 노출"), DE("Belichtung für die Analyse"),
    FR("Exposition pour l'analyse"), ES("Exposición para el análisis"),
    PT("Exposição para a análise"), IT("Esposizione per l'analisi"),
    NL("Belichting voor analyse"), RU("Экспозиция для анализа"),
    TR("Analiz için pozlama"));

SS_MSG(exposure_as_stored,
    EN("As stored"), JA("ファイルのまま"), ZH_HANS("保持原样"), ZH_HANT("保持原樣"),
    KO("저장된 그대로"), DE("Wie gespeichert"), FR("Telle quelle"), ES("Tal cual"),
    PT("Como está"), IT("Così com'è"), NL("Zoals opgeslagen"), RU("Как есть"),
    TR("Olduğu gibi"));

SS_MSG(exposure_auto,
    EN("Auto"), JA("自動"), ZH_HANS("自动"), ZH_HANT("自動"), KO("자동"),
    DE("Automatisch"), FR("Automatique"), ES("Automática"), PT("Automática"),
    IT("Automatica"), NL("Automatisch"), RU("Авто"), TR("Otomatik"));

SS_MSG(exposure_fixed,
    EN("Fixed"), JA("固定"), ZH_HANS("固定"), ZH_HANT("固定"), KO("고정"),
    DE("Fest"), FR("Fixe"), ES("Fija"), PT("Fixa"), IT("Fissa"), NL("Vast"),
    RU("Фиксированная"), TR("Sabit"));

SS_MSG(input_exposure_stops,
    EN("Stops"), JA("段数"), ZH_HANS("档数"), ZH_HANT("檔數"), KO("스톱"),
    DE("Blendenstufen"), FR("Diaphs"), ES("Pasos"), PT("Pontos"), IT("Stop"),
    NL("Stops"), RU("Ступени"), TR("Durak"));

SS_MSG(input_exposure_help,
    EN("Brightens reconstruction, masking and geometry inputs in linear light. Auto lifts dark images. Files, training inputs and point colors stay unchanged."),
    JA("再構成、マスク、形状推定の入力を線形光で明るくします。自動は暗い画像を補正します。ファイル、学習入力、点の色は変わりません。"),
    ZH_HANS("在线性光空间提亮重建、蒙版和几何估计的输入。自动模式提亮暗图。文件、训练输入及点云颜色不变。"),
    ZH_HANT("在線性光空間提亮重建、遮罩和幾何估計的輸入。自動模式提亮暗圖。檔案、訓練輸入及點雲顏色不變。"),
    KO("재구성, 마스크, 기하 추정 입력을 선형 광량으로 밝힙니다. 자동은 어두운 이미지를 보정합니다. 파일, 학습 입력, 점 색은 바뀌지 않습니다."),
    DE("Hellt Eingaben für Rekonstruktion, Masken und Geometrie linear auf. Auto hebt dunkle Bilder an. Dateien, Trainingseingaben und Punktfarben bleiben gleich."),
    FR("Éclaircit en lumière linéaire les entrées de reconstruction, masquage et géométrie. Auto relève les images sombres. Fichiers, entraînement et couleurs restent."),
    ES("Aclara en luz lineal las entradas de reconstrucción, máscaras y geometría. Auto levanta imágenes oscuras. No cambia archivos, entrenamiento ni colores de puntos."),
    PT("Clareia em luz linear as entradas de reconstrução, máscaras e geometria. Auto eleva imagens escuras. Não altera ficheiros, treino nem cores dos pontos."),
    IT("Schiarisce in luce lineare gli input di ricostruzione, maschere e geometria. Auto alza le immagini scure. File, input del training e colori dei punti restano."),
    NL("Verheldert invoer voor reconstructie, maskers en geometrie in lineair licht. Auto licht donkere beelden op. Bestanden, traininginvoer en puntkleuren blijven gelijk."),
    RU("Осветляет в линейном свете входы реконструкции, масок и геометрии. Авто осветляет тёмные снимки. Файлы, входы обучения и цвета точек не меняются."),
    TR("Yeniden oluşturma, maskeleme ve geometri girdilerini doğrusal ışıkta aydınlatır. Otomatik koyu resimleri açar. Dosyalar, eğitim girdileri ve nokta renkleri değişmez."));

SS_MSG(point_color_image_space,
    EN("Point cloud colours in the input colour space"),
    JA("点群の色を入力の色空間で"),
    ZH_HANS("点云颜色使用输入色彩空间"),
    ZH_HANT("點雲顏色使用輸入色彩空間"),
    KO("점 구름 색을 입력 색 공간으로"),
    DE("Punktwolkenfarben im Eingabefarbraum"),
    FR("Couleurs du nuage de points dans l'espace d'entrée"),
    ES("Colores de la nube de puntos en el espacio de entrada"),
    PT("Cores da nuvem de pontos no espaço de entrada"),
    IT("Colori della nuvola di punti nello spazio d'ingresso"),
    NL("Puntenwolkkleuren in de invoerkleurruimte"),
    RU("Цвета облака точек в цветовом пространстве входа"),
    TR("Nokta bulutu renkleri girdi renk uzayında"));

SS_MSG(point_color_image_space_help,
    EN("On writes the sparse point cloud in the photographs' own space, which is where training assumes it by default.\nOff leaves it in sRGB; train it with \"Seed point color space\" set to Rec.709."),
    JA("オンなら疎な点群を写真と同じ空間で書き出します。学習側は既定でそう想定します。オフなら sRGB のままで、学習では「初期点群の色空間」を Rec.709 にしてください。"),
    ZH_HANS("开启时稀疏点云以照片自身的空间写出，这也是训练端的默认假定。关闭则保持 sRGB，训练时请把\"初始点云色彩空间\"设为 Rec.709。"),
    ZH_HANT("開啟時稀疏點雲以照片自身的空間寫出，這也是訓練端的預設假定。關閉則保持 sRGB，訓練時請把「初始點雲色彩空間」設為 Rec.709。"),
    KO("켜면 희소 점 구름을 사진과 같은 공간으로 씁니다. 학습 쪽의 기본 가정도 그렇습니다. 끄면 sRGB로 남으며, 학습 시 \"초기 포인트 색 공간\"을 Rec.709로 설정하십시오."),
    DE("An schreibt die dünne Punktwolke im Raum der Fotos, wovon das Training standardmäßig ausgeht.\nAus lässt sie in sRGB; dann im Training \"Farbraum der Startpunkte\" auf Rec.709 setzen."),
    FR("Activé écrit le nuage épars dans l'espace des photographies, ce que l'entraînement suppose par défaut.\nDésactivé le laisse en sRGB ; entraînez alors avec « Espace colorimétrique des points initiaux » sur Rec.709."),
    ES("Activado: nube dispersa en el espacio de las fotos, como espera el entrenamiento. Desactivado: sRGB; usa «Espacio de color de los puntos iniciales»: Rec.709."),
    PT("Ligado escreve a nuvem esparsa no espaço das fotografias, que é o que o treino supõe por omissão.\nDesligado deixa-a em sRGB; treine então com «Espaço de cor dos pontos iniciais» em Rec.709."),
    IT("Attivo: nuvola sparsa nello spazio delle foto, come atteso dal training. Spento: sRGB; usare «Spazio colore dei punti iniziali»: Rec.709."),
    NL("Aan schrijft de dunne puntenwolk in de ruimte van de foto's, waar de training standaard van uitgaat.\nUit laat haar in sRGB; train dan met \"Kleurruimte van de startpunten\" op Rec.709."),
    RU("Включено пишет разреженное облако в пространстве фотографий -- это и предполагает обучение по умолчанию.\nВыключено оставляет его в sRGB; тогда при обучении задайте «Цветовое пространство начальных точек» Rec.709."),
    TR("Açık, seyrek nokta bulutunu fotoğrafların uzayında yazar; eğitim varsayılan olarak bunu kabul eder.\nKapalı onu sRGB'de bırakır; eğitimde \"Başlangıç noktası renk uzayı\"nı Rec.709 yapın."));

SS_MSG(gamut_rec709,
    EN("sRGB / Rec.709"), JA("sRGB / Rec.709"), ZH_HANS("sRGB / Rec.709"),
    ZH_HANT("sRGB / Rec.709"), KO("sRGB / Rec.709"), DE("sRGB / Rec.709"),
    FR("sRGB / Rec.709"), ES("sRGB / Rec.709"), PT("sRGB / Rec.709"),
    IT("sRGB / Rec.709"), NL("sRGB / Rec.709"), RU("sRGB / Rec.709"),
    TR("sRGB / Rec.709"));

SS_MSG(gamut_aces2065_1,
    EN("ACES2065-1"), JA("ACES2065-1"), ZH_HANS("ACES2065-1"), ZH_HANT("ACES2065-1"),
    KO("ACES2065-1"), DE("ACES2065-1"), FR("ACES2065-1"), ES("ACES2065-1"),
    PT("ACES2065-1"), IT("ACES2065-1"), NL("ACES2065-1"), RU("ACES2065-1"),
    TR("ACES2065-1"));

SS_MSG(gamut_acescg,
    EN("ACEScg"), JA("ACEScg"), ZH_HANS("ACEScg"), ZH_HANT("ACEScg"), KO("ACEScg"),
    DE("ACEScg"), FR("ACEScg"), ES("ACEScg"), PT("ACEScg"), IT("ACEScg"),
    NL("ACEScg"), RU("ACEScg"), TR("ACEScg"));

SS_MSG(gamut_rec2020,
    EN("Rec.2020"), JA("Rec.2020"), ZH_HANS("Rec.2020"), ZH_HANT("Rec.2020"),
    KO("Rec.2020"), DE("Rec.2020"), FR("Rec.2020"), ES("Rec.2020"),
    PT("Rec.2020"), IT("Rec.2020"), NL("Rec.2020"), RU("Rec.2020"),
    TR("Rec.2020"));

SS_MSG(gamut_adobergb,
    EN("Adobe RGB"), JA("Adobe RGB"), ZH_HANS("Adobe RGB"), ZH_HANT("Adobe RGB"),
    KO("Adobe RGB"), DE("Adobe RGB"), FR("Adobe RGB"), ES("Adobe RGB"),
    PT("Adobe RGB"), IT("Adobe RGB"), NL("Adobe RGB"), RU("Adobe RGB"),
    TR("Adobe RGB"));

SS_MSG(gamut_dcip3,
    EN("DCI-P3"), JA("DCI-P3"), ZH_HANS("DCI-P3"), ZH_HANT("DCI-P3"), KO("DCI-P3"),
    DE("DCI-P3"), FR("DCI-P3"), ES("DCI-P3"), PT("DCI-P3"), IT("DCI-P3"),
    NL("DCI-P3"), RU("DCI-P3"), TR("DCI-P3"));

SS_MSG(create_dataset,
    EN("Create Dataset"), JA("データセットを作成"), ZH_HANS("创建数据集"),
    ZH_HANT("建立資料集"), KO("데이터셋 만들기"), DE("Datensatz erstellen"),
    FR("Créer le jeu de données"), ES("Crear el conjunto de datos"),
    PT("Criar o conjunto de dados"), IT("Crea il set di dati"),
    NL("Dataset maken"), RU("Создать набор данных"), TR("Veri kümesini oluştur"));

SS_MSG(train_after_creation,
    EN("Start training after dataset creation"),
    JA("データセット作成後に学習を開始"),
    ZH_HANS("创建数据集后开始训练"),
    ZH_HANT("建立資料集後開始訓練"),
    KO("데이터셋 생성 후 학습 시작"),
    DE("Training nach der Datensatzerstellung starten"),
    FR("Démarrer l'entraînement après la création du jeu de données"),
    ES("Iniciar el entrenamiento tras crear el conjunto de datos"),
    PT("Iniciar o treino após criar o conjunto de dados"),
    IT("Avvia l'addestramento dopo la creazione del set di dati"),
    NL("Training starten na het maken van de dataset"),
    RU("Начать обучение после создания набора данных"),
    TR("Veri kümesi oluşturulduktan sonra eğitimi başlat"));

SS_MSG(pick_input_first,
    EN("pick the input and the output folder first"),
    JA("先に入力と出力フォルダを選んでください"),
    ZH_HANS("请先选好输入和输出文件夹"),
    ZH_HANT("請先選好輸入和輸出資料夾"),
    KO("먼저 입력과 출력 폴더를 고르세요"),
    DE("zuerst die Eingabe und den Ausgabeordner wählen"),
    FR("choisissez d'abord l'entrée et le dossier de sortie"),
    ES("elija primero la entrada y la carpeta de salida"),
    PT("escolha primeiro a entrada e a pasta de saída"),
    IT("scelga prima l'ingresso e la cartella di destinazione"),
    NL("kies eerst de invoer en de uitvoermap"),
    RU("сначала выберите вход и папку результатов"),
    TR("önce girdiyi ve çıktı klasörünü seçin"));

SS_MSG(cancel,
    EN("Cancel"),        JA("キャンセル"),    ZH_HANS("取消"),     ZH_HANT("取消"),
    KO("취소"),           DE("Abbrechen"),    FR("Annuler"),      ES("Cancelar"),
    PT("Cancelar"),      IT("Annulla"),      NL("Annuleren"),    RU("Отмена"),
    TR("İptal"));

// {0} is the stage the runner is on, in English -- it is a diagnostic.
SS_MSG(stage_running,
    EN("{0} ..."),       JA("{0} …"),        ZH_HANS("{0} …"),   ZH_HANT("{0} …"),
    KO("{0} …"),          DE("{0} …"),        FR("{0}…"),         ES("{0}…"),
    PT("{0}…"),          IT("{0}…"),         NL("{0}…"),         RU("{0}…"),
    TR("{0}…"));

// ---------------------------------------------------------------------------
// The steps a run goes through, as the strip above the log names them. Short
// nouns, not the sentences the log uses: this is a row of six.
// ---------------------------------------------------------------------------

SS_MSG(step_frames,
    EN("Frames"),        JA("フレーム"),      ZH_HANS("帧"),       ZH_HANT("影格"),
    KO("프레임"),         DE("Bilder"),       FR("Images"),       ES("Fotogramas"),
    PT("Quadros"),       IT("Fotogrammi"),   NL("Beelden"),      RU("Кадры"),
    TR("Kareler"));

SS_MSG(step_masks,
    EN("Masks"),         JA("マスク"),        ZH_HANS("蒙版"),      ZH_HANT("遮罩"),
    KO("마스크"),         DE("Masken"),       FR("Masques"),      ES("Máscaras"),
    PT("Máscaras"),      IT("Maschere"),     NL("Maskers"),      RU("Маски"),
    TR("Maskeler"));

SS_MSG(step_features,
    EN("Features"),      JA("特徴点"),        ZH_HANS("特征点"),    ZH_HANT("特徵點"),
    KO("특징점"),         DE("Merkmale"),     FR("Points"),       ES("Puntos"),
    PT("Pontos"),        IT("Punti"),        NL("Kenmerken"),    RU("Признаки"),
    TR("Öznitelikler"));

SS_MSG(step_matching,
    EN("Matching"),      JA("照合"),          ZH_HANS("匹配"),      ZH_HANT("比對"),
    KO("정합"),           DE("Zuordnung"),    FR("Appariement"),  ES("Emparejado"),
    PT("Pareamento"),    IT("Confronto"),    NL("Koppelen"),     RU("Сопоставление"),
    TR("Eşleştirme"));

SS_MSG(step_mapping,
    EN("Mapping"),       JA("再構成"),        ZH_HANS("重建"),      ZH_HANT("重建"),
    KO("재구성"),         DE("Rekonstruktion"), FR("Reconstruction"),
    ES("Reconstrucción"), PT("Reconstrução"), IT("Ricostruzione"),
    NL("Reconstructie"), RU("Реконструкция"), TR("Yeniden kurma"));

SS_MSG(step_finishing,
    EN("Finishing"),     JA("仕上げ"),        ZH_HANS("收尾"),      ZH_HANT("收尾"),
    KO("마무리"),         DE("Abschluss"),    FR("Finalisation"), ES("Cierre"),
    PT("Fecho"),         IT("Chiusura"),     NL("Afronden"),     RU("Завершение"),
    TR("Bitiriş"));

SS_MSG(step_locked,
    EN("This step has already started, so what it was told is fixed for this "
       "run. Anything a later step reads can still be changed."),
    JA("この工程はすでに始まっているので、指示は今回の実行では変えられません。"
       "あとの工程が読む設定はまだ変えられます。"),
    ZH_HANS("这一步已经开始，本次运行中它的设置不能再改。后面步骤要读的设置仍然可以改。"),
    ZH_HANT("這一步已經開始，本次執行中它的設定不能再改。後面步驟要讀的設定仍然可以改。"),
    KO("이 단계는 이미 시작해서 이번 실행에서는 설정을 바꿀 수 없습니다. "
       "뒤의 단계가 읽는 설정은 아직 바꿀 수 있습니다."),
    DE("Dieser Schritt läuft bereits, seine Vorgaben stehen für diesen Lauf "
       "fest. Was ein späterer Schritt liest, lässt sich noch ändern."),
    FR("Cette étape a déjà commencé : ses réglages sont figés pour cette "
       "exécution. Ce qu'une étape suivante lit reste modifiable."),
    ES("Este paso ya empezó, así que sus ajustes quedan fijos en esta "
       "ejecución. Lo que lee un paso posterior todavía se puede cambiar."),
    PT("Esta etapa já começou, por isso os ajustes dela ficam fixos nesta "
       "execução. O que uma etapa posterior lê ainda pode mudar."),
    IT("Questo passo è già iniziato, quindi le sue impostazioni sono fissate "
       "per questa esecuzione. Ciò che legge un passo successivo si può ancora "
       "cambiare."),
    NL("Deze stap is al begonnen, dus zijn instellingen liggen vast voor deze "
       "run. Wat een latere stap leest, kan nog veranderen."),
    RU("Этот шаг уже начался, поэтому его настройки закреплены на этот запуск. "
       "То, что читает более поздний шаг, ещё можно изменить."),
    TR("Bu adım çoktan başladı, bu yüzden ayarları bu çalışma için sabit. "
       "Sonraki bir adımın okuduğu ayarlar hâlâ değiştirilebilir."));

SS_MSG(show_run_preview,
    EN("Show what the run is doing"),
    JA("実行中の様子を表示する"),
    ZH_HANS("显示运行过程"),
    ZH_HANT("顯示執行過程"),
    KO("진행 중인 모습 보여주기"),
    DE("Zeigen, woran der Lauf gerade arbeitet"),
    FR("Montrer ce que fait l'exécution"),
    ES("Mostrar lo que está haciendo la ejecución"),
    PT("Mostrar o que a execução está fazendo"),
    IT("Mostra che cosa sta facendo l'esecuzione"),
    NL("Tonen waar de run mee bezig is"),
    RU("Показывать, чем занят запуск"),
    TR("Çalışmanın ne yaptığını göster"));

SS_MSG(show_run_preview_help,
    EN("Shows extracted frames, matches or the growing model for the current step. Excluded mask areas appear red so you can check the prompt."),
    JA("現在の工程の抽出フレーム、対応点、構築中のモデルを表示します。除外領域は赤色で表示され、マスク指示の結果を確認できます。"),
    ZH_HANS("显示当前步骤提取的帧、匹配或构建中的模型。蒙版排除区域显示为红色，方便检查提示是否正确。"),
    ZH_HANT("顯示目前步驟擷取的影格、匹配或建構中的模型。遮罩排除區域顯示為紅色，方便檢查提示是否正確。"),
    KO("현재 단계의 추출 프레임, 매칭 또는 생성 중인 모델을 보여 줍니다. 제외된 마스크 영역은 빨간색으로 표시되어 프롬프트를 확인할 수 있습니다."),
    DE("Zeigt extrahierte Bilder, Zuordnungen oder das entstehende Modell des aktuellen Schritts. Maskierte Ausschlüsse sind rot, um den Prompt zu prüfen."),
    FR("Affiche les images extraites, les correspondances ou le modèle de l’étape en cours. Les zones exclues sont rouges pour vérifier la consigne."),
    ES("Muestra fotogramas extraídos, coincidencias o el modelo del paso actual. Las zonas excluidas aparecen en rojo para comprobar la indicación."),
    PT("Mostra os fotogramas extraídos, correspondências ou o modelo do passo atual. As áreas excluídas aparecem a vermelho para verificar o pedido."),
    IT("Mostra fotogrammi estratti, corrispondenze o il modello del passo attuale. Le aree escluse appaiono rosse per verificare il prompt."),
    NL("Toont uitgepakte beelden, koppelingen of het groeiende model van de huidige stap. Uitgesloten delen zijn rood om de prompt te controleren."),
    RU("Показывает извлечённые кадры, совпадения или строящуюся модель текущего этапа. Исключённые области красные: проверьте результат запроса."),
    TR("Geçerli adımın çıkarılan karelerini, eşleşmelerini veya oluşan modelini gösterir. İstemi denetlemek için dışlanan alanlar kırmızı gösterilir."));

// ---------------------------------------------------------------------------
// What the preview panel shows: frames, the match matrix, the model
// ---------------------------------------------------------------------------

SS_MSG(reel_follow,
    EN("Latest"),        JA("最新"),          ZH_HANS("最新"),      ZH_HANT("最新"),
    KO("최신"),           DE("Neueste"),      FR("Dernière"),     ES("La última"),
    PT("A última"),      IT("L'ultima"),     NL("Nieuwste"),     RU("Последний"),
    TR("En yeni"));

SS_MSG(reel_follow_help,
    EN("Keep up with the step as it works. Turn this off, or drag the slider back, to look at a picture it has already been past -- the run carries on either way."),
    JA("進行に合わせて最新の一枚を表示し続けます。これを切るか、スライダーを戻すと、すでに通り過ぎた画像を見られます。どちらでも処理は止まりません。"),
    ZH_HANS("跟着这一步的进度显示最新的一张。关掉它，或者把滑块往回拖，就能看已经处理过的图像；无论哪种，运行都不会停。"),
    ZH_HANT("跟著這一步的進度顯示最新的一張。關掉它，或者把滑桿往回拖，就能看已經處理過的影像；無論哪種，執行都不會停。"),
    KO("작업이 진행되는 대로 가장 최근 장면을 보여 줍니다. 이것을 끄거나 슬라이더를 뒤로 끌면 이미 지나간 이미지를 볼 수 있고, 어느 쪽이든 실행은 계속됩니다."),
    DE("Zeigt immer das neueste Bild. Ausschalten oder den Regler zurückziehen, um frühere Bilder anzusehen. Der Lauf geht weiter."),
    FR("Montre toujours la dernière image. Décochez ou reculez le curseur pour revoir les précédentes ; le traitement continue."),
    ES("Seguir el paso y mostrar siempre la imagen más reciente. Desactívelo, o arrastre el control hacia atrás, para ver una imagen ya procesada: la ejecución sigue igual."),
    PT("Mostra sempre a imagem mais recente. Desligue ou recue o controlo para rever as anteriores; a execução continua."),
    IT("Segue il passo e mostra sempre l'immagine più recente. Disattivalo, o riporta indietro il cursore, per rivedere un'immagine già elaborata: l'esecuzione prosegue comunque."),
    NL("Loopt mee met de stap en toont steeds het nieuwste beeld. Zet het uit, of sleep de schuif terug, om een al verwerkt beeld te bekijken -- de verwerking gaat hoe dan ook door."),
    RU("Показывать самый свежий кадр по ходу шага. Снимите галочку или перетащите ползунок назад, чтобы посмотреть уже пройденное изображение -- работа при этом не прерывается."),
    TR("Adım ilerledikçe en yeni görüntüyü gösterir. Kapatın ya da kaydırıcıyı geri çekin; böylece çoktan geçilmiş bir görüntüye bakabilirsiniz, çalışma yine de sürer."));

SS_MSG(view_frames,
    EN("Frames"),        JA("フレーム"),      ZH_HANS("画面"),      ZH_HANT("畫面"),
    KO("프레임"),         DE("Bilder"),       FR("Images"),       ES("Fotogramas"),
    PT("Quadros"),       IT("Fotogrammi"),   NL("Beelden"),      RU("Кадры"),
    TR("Kareler"));

SS_MSG(view_masks,
    EN("Masks"),         JA("マスク"),        ZH_HANS("蒙版"),      ZH_HANT("遮罩"),
    KO("마스크"),         DE("Masken"),       FR("Masques"),      ES("Máscaras"),
    PT("Máscaras"),      IT("Maschere"),     NL("Maskers"),      RU("Маски"),
    TR("Maskeler"));

SS_MSG(view_features,
    EN("Features"),      JA("特徴点"),        ZH_HANS("特征点"),    ZH_HANT("特徵點"),
    KO("특징점"),         DE("Merkmale"),     FR("Points"),       ES("Puntos"),
    PT("Pontos"),        IT("Punti"),        NL("Kenmerken"),    RU("Признаки"),
    TR("Öznitelikler"));

SS_MSG(view_matrix,
    EN("Match map"),
    JA("照合マップ"),
    ZH_HANS("匹配图"),
    ZH_HANT("比對圖"),
    KO("정합 지도"),
    DE("Zuordnungskarte"),
    FR("Carte d'appariement"),
    ES("Mapa de emparejado"),
    PT("Mapa de pareamento"),
    IT("Mappa dei confronti"),
    NL("Koppelkaart"),
    RU("Карта сопоставлений"),
    TR("Eşleşme haritası"));

SS_MSG(view_model,
    EN("Model"),         JA("モデル"),        ZH_HANS("模型"),      ZH_HANT("模型"),
    KO("모델"),           DE("Modell"),       FR("Modèle"),       ES("Modelo"),
    PT("Modelo"),        IT("Modello"),      NL("Model"),        RU("Модель"),
    TR("Model"));

SS_MSG(matrix_help,
    EN("Brighter cells mean more surviving matches between images.\n- A walk gives a diagonal; revisiting the start lights the corners.\n- Dark corners can mean an unclosed loop that splits reconstruction."),
    JA("明るいセルほど画像間で多くの一致が残っています。\n- 歩行撮影は対角線、出発点への再訪は隅が明るくなります。\n- 隅が暗いとループ未閉鎖で再構成が分断される場合があります。"),
    ZH_HANS("单元格越亮，两图间保留的匹配越多。\n- 行走拍摄形成对角线，重返起点时角落会亮。\n- 暗角可能表示回环未闭合，导致重建分裂。"),
    ZH_HANT("儲存格越亮，兩圖間保留的匹配越多。\n- 行走拍攝形成對角線，重返起點時角落會亮。\n- 暗角可能表示回環未閉合，導致重建分裂。"),
    KO("밝을수록 이미지 간 살아남은 매칭이 많습니다.\n- 걸으며 촬영하면 대각선, 출발점 재방문은 모서리가 밝아집니다.\n- 어두운 모서리는 루프 미완성으로 재구성이 갈릴 수 있음을 뜻합니다."),
    DE("Hellere Zellen: mehr erhaltene Treffer zwischen Bildern.\n- Ein Weg bildet eine Diagonale; Rückkehr zum Start helle Ecken.\n- Dunkle Ecken können eine offene Schleife und geteilte Rekonstruktion anzeigen."),
    FR("Cases plus claires : plus de correspondances conservées.\n- Une marche trace une diagonale ; le retour éclaire les coins.\n- Coins sombres : boucle peut-être ouverte, reconstruction divisée."),
    ES("Celdas más claras: más coincidencias conservadas entre imágenes.\n- Una caminata da una diagonal; volver al inicio ilumina las esquinas.\n- Esquinas oscuras pueden indicar un bucle abierto que divide la reconstrucción."),
    PT("Células mais claras: mais correspondências mantidas entre imagens.\n- Uma caminhada dá uma diagonal; voltar ao início ilumina os cantos.\n- Cantos escuros podem indicar uma volta aberta que divide a reconstrução."),
    IT("Celle più chiare: più corrispondenze mantenute tra immagini.\n- Un percorso dà una diagonale; tornare all’inizio illumina gli angoli.\n- Angoli scuri possono indicare un anello aperto che divide la ricostruzione."),
    NL("Lichtere cellen: meer behouden koppelingen tussen beelden.\n- Een wandeling geeft een diagonaal; terugkeer verlicht de hoeken.\n- Donkere hoeken kunnen op een open lus en gesplitste reconstructie wijzen."),
    RU("Ярче ячейка — больше сохранённых совпадений снимков.\n- Проход даёт диагональ; возврат к началу освещает углы.\n- Тёмные углы могут означать незамкнутую петлю и разделение реконструкции."),
    TR("Parlak hücreler: resimler arasında daha çok korunmuş eşleşme.\n- Yürüyüş köşegen oluşturur; başlangıca dönüş köşeleri aydınlatır.\n- Karanlık köşeler açık döngü ve bölünmüş model anlamına gelebilir."));

SS_MSG(matrix_cell_pair,
    EN("Images {0} and {1} -- matched points: {2}"),
    JA("画像 {0} と {1} -- 対応した点: {2}"),
    ZH_HANS("图像 {0} 与 {1} —— 匹配上的点：{2}"),
    ZH_HANT("影像 {0} 與 {1} —— 比對上的點：{2}"),
    KO("이미지 {0} 과 {1} -- 맞춰진 점: {2}"),
    DE("Bilder {0} und {1} -- zugeordnete Punkte: {2}"),
    FR("Images {0} et {1} -- points appariés : {2}"),
    ES("Imágenes {0} y {1} -- puntos emparejados: {2}"),
    PT("Imagens {0} e {1} -- pontos pareados: {2}"),
    IT("Immagini {0} e {1} -- punti confrontati: {2}"),
    NL("Beelden {0} en {1} -- gekoppelde punten: {2}"),
    RU("Изображения {0} и {1} -- сопоставленные точки: {2}"),
    TR("Görüntü {0} ile {1} -- eşleşen nokta: {2}"));

SS_MSG(matrix_cell_range,
    EN("Images {0}-{1} and {2}-{3} -- matched points: {4}"),
    JA("画像 {0}-{1} と {2}-{3} -- 対応した点: {4}"),
    ZH_HANS("图像 {0}-{1} 与 {2}-{3} —— 匹配上的点：{4}"),
    ZH_HANT("影像 {0}-{1} 與 {2}-{3} —— 比對上的點：{4}"),
    KO("이미지 {0}-{1} 과 {2}-{3} -- 맞춰진 점: {4}"),
    DE("Bilder {0}-{1} und {2}-{3} -- zugeordnete Punkte: {4}"),
    FR("Images {0}-{1} et {2}-{3} -- points appariés : {4}"),
    ES("Imágenes {0}-{1} y {2}-{3} -- puntos emparejados: {4}"),
    PT("Imagens {0}-{1} e {2}-{3} -- pontos pareados: {4}"),
    IT("Immagini {0}-{1} e {2}-{3} -- punti confrontati: {4}"),
    NL("Beelden {0}-{1} en {2}-{3} -- gekoppelde punten: {4}"),
    RU("Изображения {0}-{1} и {2}-{3} -- сопоставленные точки: {4}"),
    TR("Görüntü {0}-{1} ile {2}-{3} -- eşleşen nokta: {4}"));

SS_MSG(matrix_key_matched,
    EN("Matched"),        JA("対応あり"),      ZH_HANS("已匹配"),     ZH_HANT("已比對"),
    KO("대응됨"),          DE("Zugeordnet"),   FR("Appariées"),    ES("Emparejadas"),
    PT("Pareadas"),      IT("Confrontate"),  NL("Gekoppeld"),    RU("Сопоставлено"),
    TR("Eşleşti"));

SS_MSG(matrix_key_none,
    EN("No match"),
    JA("対応なし"),
    ZH_HANS("无匹配"),
    ZH_HANT("無比對"),
    KO("대응 없음"),
    DE("Keine Übereinstimmung"),
    FR("Aucune correspondance"),
    ES("Sin coincidencias"),
    PT("Sem correspondência"),
    IT("Nessuna corrispondenza"),
    NL("Geen overeenkomst"),
    RU("Совпадений нет"),
    TR("Eşleşme yok"));

SS_MSG(matrix_key_pending,
    EN("Waiting"),       JA("待機中"),        ZH_HANS("等待中"),     ZH_HANT("等待中"),
    KO("대기 중"),        DE("Wartet"),       FR("En attente"),   ES("En espera"),
    PT("Em espera"),     IT("In attesa"),    NL("Wacht"),        RU("В очереди"),
    TR("Bekliyor"));

SS_MSG(matrix_key_skipped,
    EN("Not paired"),
    JA("組み合わせ対象外"),
    ZH_HANS("未配对"),
    ZH_HANT("未配對"),
    KO("짝짓지 않음"),
    DE("Nicht gepaart"),
    FR("Non appairées"),
    ES("No emparejadas"),
    PT("Não pareadas"),
    IT("Non accoppiate"),
    NL("Niet gepaard"),
    RU("Не в парах"),
    TR("Eşleştirilmedi"));

SS_MSG(matrix_pair_hint,
    EN("Point at the map to see the two images behind a cell, the features "
       "found on them and the matches between them."),
    JA("マップの上にカーソルを置くと、そのマスの二枚の画像と、そこで見つかった"
       "特徴点、そして両者の対応が表示されます。"),
    ZH_HANS("把光标放在图上，就能看到该格对应的两张图像、在它们上面找到的特征点，"
            "以及两者之间的匹配。"),
    ZH_HANT("把游標放在圖上，就能看到該格對應的兩張影像、在它們上面找到的特徵點，"
            "以及兩者之間的比對。"),
    KO("지도 위에 커서를 올리면 그 칸에 해당하는 두 이미지와 거기서 찾은 특징점, "
       "그리고 둘 사이의 대응을 볼 수 있습니다."),
    DE("Auf die Karte zeigen, um die beiden Bilder hinter einer Zelle zu sehen, "
       "die darauf gefundenen Merkmale und die Zuordnungen zwischen ihnen."),
    FR("Pointez la carte pour voir les deux images derrière une case, les points "
       "qui y ont été trouvés et les appariements entre eux."),
    ES("Apunte al mapa para ver las dos imágenes que hay tras una celda, los "
       "puntos encontrados en ellas y los emparejamientos entre ambas."),
    PT("Aponte para o mapa para ver as duas imagens por trás de uma célula, os "
       "pontos encontrados nelas e os pareamentos entre as duas."),
    IT("Punta sulla mappa per vedere le due immagini dietro una cella, i punti "
       "trovati su di esse e i confronti fra le due."),
    NL("Wijs de kaart aan om de twee beelden achter een vakje te zien, de "
       "kenmerken die erop gevonden zijn en de koppelingen ertussen."),
    RU("Наведите курсор на карту, чтобы увидеть два изображения за ячейкой, "
       "найденные на них признаки и сопоставления между ними."),
    TR("Bir hücrenin arkasındaki iki görüntüyü, üzerlerinde bulunan öznitelikleri "
       "ve aralarındaki eşleşmeleri görmek için haritanın üzerine gelin."));

SS_MSG(matrix_pair_matches,
    EN("Matches: {0}"),  JA("対応: {0}"),     ZH_HANS("匹配：{0}"),   ZH_HANT("比對：{0}"),
    KO("대응: {0}"),      DE("Zuordnungen: {0}"), FR("Appariements : {0}"),
    ES("Emparejamientos: {0}"), PT("Pareamentos: {0}"), IT("Confronti: {0}"),
    NL("Koppelingen: {0}"), RU("Сопоставлений: {0}"), TR("Eşleşme: {0}"));

SS_MSG(matrix_edit_matches,
    EN("Edit Matches..."),
    JA("照合を編集..."),
    ZH_HANS("编辑匹配..."),
    ZH_HANT("編輯比對..."),
    KO("정합 편집..."),
    DE("Zuordnungen bearbeiten..."),
    FR("Modifier les appariements..."),
    ES("Editar emparejamientos..."),
    PT("Editar pareamentos..."),
    IT("Modifica confronti..."),
    NL("Koppelingen bewerken..."),
    RU("Редактировать сопоставления..."),
    TR("Eşleşmeleri düzenle..."));

SS_MSG(matrix_editor_title,
    EN("Match Matrix Editor"),
    JA("照合行列エディタ"),
    ZH_HANS("匹配矩阵编辑器"),
    ZH_HANT("比對矩陣編輯器"),
    KO("정합 행렬 편집기"),
    DE("Zuordnungsmatrix-Editor"),
    FR("Éditeur de matrice d'appariement"),
    ES("Editor de matriz de emparejamiento"),
    PT("Editor de matriz de pareamento"),
    IT("Editor matrice di confronto"),
    NL("Koppelingsmatrixbewerker"),
    RU("Редактор матрицы сопоставлений"),
    TR("Eşleşme Matrisi Düzenleyicisi"));

SS_MSG(matrix_remove_matches,
    EN("Remove Matches"),
    JA("照合を削除"),
    ZH_HANS("移除匹配"),
    ZH_HANT("移除比對"),
    KO("정합 제거"),
    DE("Zuordnungen entfernen"),
    FR("Supprimer les appariements"),
    ES("Eliminar emparejamientos"),
    PT("Remover pareamentos"),
    IT("Rimuovi confronti"),
    NL("Koppelingen verwijderen"),
    RU("Удалить сопоставления"),
    TR("Eşleşmeleri kaldır"));

SS_MSG(matrix_undo,
    EN("Undo"),
    JA("元に戻す"),
    ZH_HANS("撤销"),
    ZH_HANT("復原"),
    KO("실행 취소"),
    DE("Rückgängig"),
    FR("Annuler"),
    ES("Deshacer"),
    PT("Desfazer"),
    IT("Annulla"),
    NL("Ongedaan maken"),
    RU("Отменить"),
    TR("Geri al"));

SS_MSG(matrix_restore_backup,
    EN("Restore backup"),
    JA("バックアップを復元"),
    ZH_HANS("恢复备份"),
    ZH_HANT("還原備份"),
    KO("백업 복원"),
    DE("Sicherung wiederherstellen"),
    FR("Restaurer la sauvegarde"),
    ES("Restaurar copia de seguridad"),
    PT("Restaurar cópia de segurança"),
    IT("Ripristina backup"),
    NL("Back-up herstellen"),
    RU("Восстановить резервную копию"),
    TR("Yedeği geri yükle"));

SS_MSG(matrix_restore_backup_help,
    EN("Replace matches.bin with matches.bin.bak and clear the undo history. The backup is kept."),
    JA("matches.bin を matches.bin.bak で置き換え、元に戻す履歴を消去します。バックアップは保持されます。"),
    ZH_HANS("用 matches.bin.bak 替换 matches.bin 并清除撤销历史。备份将保留。"),
    ZH_HANT("以 matches.bin.bak 取代 matches.bin 並清除復原記錄。備份會保留。"),
    KO("matches.bin을 matches.bin.bak으로 바꾸고 실행 취소 기록을 지웁니다. 백업은 유지됩니다."),
    DE("matches.bin durch matches.bin.bak ersetzen und den Rückgängig-Verlauf löschen. Die Sicherung bleibt erhalten."),
    FR("Remplacer matches.bin par matches.bin.bak et effacer l’historique d’annulation. La sauvegarde est conservée."),
    ES("Reemplazar matches.bin por matches.bin.bak y borrar el historial de deshacer. La copia se conserva."),
    PT("Substituir matches.bin por matches.bin.bak e limpar o histórico de desfazer. A cópia é mantida."),
    IT("Sostituisce matches.bin con matches.bin.bak e cancella la cronologia di annullamento. Il backup viene conservato."),
    NL("Vervang matches.bin door matches.bin.bak en wis de geschiedenis voor ongedaan maken. De back-up blijft bewaard."),
    RU("Заменить matches.bin файлом matches.bin.bak и очистить историю отмены. Резервная копия сохраняется."),
    TR("matches.bin dosyasını matches.bin.bak ile değiştirir ve geri alma geçmişini temizler. Yedek korunur."));

SS_MSG(matrix_clear_selection,
    EN("Clear Selection"),
    JA("選択を解除"),
    ZH_HANS("清除选择"),
    ZH_HANT("清除選擇"),
    KO("선택 해제"),
    DE("Auswahl aufheben"),
    FR("Désélectionner"),
    ES("Deseleccionar"),
    PT("Limpar seleção"),
    IT("Deseleziona"),
    NL("Selectie wissen"),
    RU("Снять выделение"),
    TR("Seçimi temizle"));

SS_MSG(matrix_show_features,
    EN("Features"),
    JA("特徴点"),
    ZH_HANS("特征点"),
    ZH_HANT("特徵點"),
    KO("특징점"),
    DE("Merkmale"),
    FR("Points d'intérêt"),
    ES("Puntos clave"),
    PT("Pontos-chave"),
    IT("Punti chiave"),
    NL("Kenmerken"),
    RU("Признаки"),
    TR("Öznitelikler"));

SS_MSG(matrix_show_matches,
    EN("Matches"),
    JA("対応線"),
    ZH_HANS("匹配线"),
    ZH_HANT("比對線"),
    KO("대응선"),
    DE("Linien"),
    FR("Correspondances"),
    ES("Líneas de coincidencia"),
    PT("Linhas de pareamento"),
    IT("Linee di confronto"),
    NL("Koppelingslijnen"),
    RU("Линии сопоставления"),
    TR("Eşleşme çizgileri"));

SS_MSG(matrix_reset_view,
    EN("Reset View"),
    JA("表示をリセット"),
    ZH_HANS("重置视图"),
    ZH_HANT("重設檢視"),
    KO("화면 초기화"),
    DE("Ansicht zurücksetzen"),
    FR("Réinitialiser la vue"),
    ES("Restablecer vista"),
    PT("Redefinir visualização"),
    IT("Reimposta vista"),
    NL("Weergave herstellen"),
    RU("Сбросить вид"),
    TR("Görünümü sıfırla"));

SS_MSG(matrix_help_edit,
    EN("{0}\n- Right-click the map to take false matches out by hand."),
    JA("{0}\n- マップを右クリックすると、誤った照合を手で取り除けます。"),
    ZH_HANS("{0}\n- 在图上右键单击，可以手动去掉错误的匹配。"),
    ZH_HANT("{0}\n- 在圖上按右鍵，可以手動去掉錯誤的比對。"),
    KO("{0}\n- 지도를 오른쪽 클릭하면 잘못된 정합을 직접 없앨 수 있습니다."),
    DE("{0}\n- Rechtsklick auf die Karte, um falsche Zuordnungen von Hand zu entfernen."),
    FR("{0}\n- Clic droit sur la carte pour retirer à la main les faux appariements."),
    ES("{0}\n- Haga clic derecho en el mapa para quitar a mano los emparejamientos falsos."),
    PT("{0}\n- Clique com o botão direito no mapa para retirar à mão os pareamentos falsos."),
    IT("{0}\n- Fai clic destro sulla mappa per togliere a mano i confronti sbagliati."),
    NL("{0}\n- Klik met rechts op de kaart om foute koppelingen met de hand te verwijderen."),
    RU("{0}\n- Щёлкните по карте правой кнопкой, чтобы вручную убрать ложные сопоставления."),
    TR("{0}\n- Yanlış eşleşmeleri elle kaldırmak için haritaya sağ tıklayın."));

SS_MSG(matrix_editor_help,
    EN("Removing pairs rewrites matches.bin; the first edit keeps the original as matches.bin.bak.\n- The next run then reconstructs again from the edited matches.\n- A run that matches again -- because a matching setting changed -- replaces them."),
    JA("ペアを取り除くと matches.bin が書き換わります。 最初の編集のときに元のファイルを matches.bin.bak として残します。\n次の実行では、編集した照合から再構成をやり直します。 照合の設定を変えて照合をやり直すと、編集は置き換えられます。"),
    ZH_HANS("去掉图像对会改写 matches.bin；第一次编辑时，原文件保留为 matches.bin.bak。下一次运行会用编辑后的匹配重新重建。如果改了匹配设置而重新匹配，编辑结果会被替换。"),
    ZH_HANT("去掉影像對會改寫 matches.bin；第一次編輯時，原檔案保留為 matches.bin.bak。下一次執行會用編輯後的比對重新重建。如果改了比對設定而重新比對，編輯結果會被取代。"),
    KO("쌍 삭제는 matches.bin을 바꾸며 최초 원본은 matches.bin.bak에 보관합니다. 다음 실행은 편집본으로 재구성합니다. 설정 변경으로 다시 매칭하면 편집본을 대체합니다."),
    DE("Entfernte Paare schreiben matches.bin neu; die erste Änderung behält das Original als matches.bin.bak.\n- Der nächste Lauf rekonstruiert dann aus den bearbeiteten Zuordnungen neu.\n- Ein Lauf, der neu zuordnet -- weil sich eine Zuordnungseinstellung geändert hat --, ersetzt sie."),
    FR("Retirer des paires réécrit matches.bin ; la première modification garde l'original sous matches.bin.bak.\n- Le passage suivant reconstruit alors à partir des appariements modifiés.\n- Un passage qui apparie de nouveau -- parce qu'un réglage d'appariement a changé -- les remplace."),
    ES("Quitar pares reescribe matches.bin; la primera edición guarda el original como matches.bin.bak.\n- La siguiente ejecución reconstruye entonces a partir de los emparejamientos editados.\n- Una ejecución que vuelve a emparejar -- porque cambió un ajuste de emparejamiento -- los reemplaza."),
    PT("Retirar pares reescreve matches.bin; a primeira edição guarda o original como matches.bin.bak.\n- A próxima execução reconstrói então a partir dos pareamentos editados.\n- Uma execução que pareia de novo -- porque uma definição de pareamento mudou -- os substitui."),
    IT("Togliere coppie riscrive matches.bin; la prima modifica conserva l'originale come matches.bin.bak.\n- L'esecuzione successiva ricostruisce allora dai confronti modificati.\n- Un'esecuzione che confronta di nuovo -- perché è cambiata un'impostazione di confronto -- li sostituisce."),
    NL("Paren verwijderen herschrijft matches.bin; de eerste bewerking bewaart het origineel als matches.bin.bak.\n- De volgende run reconstrueert dan opnieuw uit de bewerkte koppelingen.\n- Een run die opnieuw koppelt -- omdat een koppelinstelling is veranderd -- vervangt ze."),
    RU("Удаление пар перезаписывает matches.bin; при первой правке оригинал сохраняется как matches.bin.bak.\n- Следующий запуск реконструирует заново по исправленным сопоставлениям.\n- Запуск, который сопоставляет заново -- потому что изменилась настройка сопоставления, -- заменит их."),
    TR("Çift kaldırmak matches.bin dosyasını yeniden yazar; ilk düzenleme özgün dosyayı matches.bin.bak olarak saklar.\n- Sonraki çalıştırma düzenlenmiş eşleşmelerden yeniden kurar.\n- Eşleştirmeyi yeniden yapan bir çalıştırma -- bir eşleştirme ayarı değiştiği için -- bunları değiştirir."));

SS_MSG(matrix_selection_counts,
    EN("Pairs: {0}   Matches: {1}"),
    JA("ペア: {0}   対応: {1}"),
    ZH_HANS("图像对：{0}   匹配：{1}"),
    ZH_HANT("影像對：{0}   比對：{1}"),
    KO("쌍: {0}   대응: {1}"),
    DE("Paare: {0}   Zuordnungen: {1}"),
    FR("Paires : {0}   Appariements : {1}"),
    ES("Pares: {0}   Emparejamientos: {1}"),
    PT("Pares: {0}   Pareamentos: {1}"),
    IT("Coppie: {0}   Confronti: {1}"),
    NL("Paren: {0}   Koppelingen: {1}"),
    RU("Пар: {0}   Сопоставлений: {1}"),
    TR("Çift: {0}   Eşleşme: {1}"));

SS_MSG(matrix_removed,
    EN("Removed -- pairs: {0}, matches: {1}"),
    JA("取り除いたもの -- ペア: {0}、対応: {1}"),
    ZH_HANS("已去掉 —— 图像对：{0}，匹配：{1}"),
    ZH_HANT("已去掉 —— 影像對：{0}，比對：{1}"),
    KO("없앤 것 -- 쌍: {0}, 대응: {1}"),
    DE("Entfernt -- Paare: {0}, Zuordnungen: {1}"),
    FR("Retiré -- paires : {0}, appariements : {1}"),
    ES("Quitado -- pares: {0}, emparejamientos: {1}"),
    PT("Retirado -- pares: {0}, pareamentos: {1}"),
    IT("Tolto -- coppie: {0}, confronti: {1}"),
    NL("Verwijderd -- paren: {0}, koppelingen: {1}"),
    RU("Удалено -- пар: {0}, сопоставлений: {1}"),
    TR("Kaldırıldı -- çift: {0}, eşleşme: {1}"));

SS_MSG(matrix_restored,
    EN("Restored -- pairs: {0}, matches: {1}"),
    JA("元に戻したもの -- ペア: {0}、対応: {1}"),
    ZH_HANS("已恢复 —— 图像对：{0}，匹配：{1}"),
    ZH_HANT("已恢復 —— 影像對：{0}，比對：{1}"),
    KO("되돌린 것 -- 쌍: {0}, 대응: {1}"),
    DE("Wiederhergestellt -- Paare: {0}, Zuordnungen: {1}"),
    FR("Rétabli -- paires : {0}, appariements : {1}"),
    ES("Restaurado -- pares: {0}, emparejamientos: {1}"),
    PT("Restaurado -- pares: {0}, pareamentos: {1}"),
    IT("Ripristinato -- coppie: {0}, confronti: {1}"),
    NL("Hersteld -- paren: {0}, koppelingen: {1}"),
    RU("Восстановлено -- пар: {0}, сопоставлений: {1}"),
    TR("Geri yüklendi -- çift: {0}, eşleşme: {1}"));

SS_MSG(matrix_edit_failed,
    EN("The matches were not changed: {0}"),
    JA("照合は変更されていません: {0}"),
    ZH_HANS("匹配没有改动：{0}"),
    ZH_HANT("比對沒有改動：{0}"),
    KO("정합은 바뀌지 않았습니다: {0}"),
    DE("Die Zuordnungen wurden nicht geändert: {0}"),
    FR("Les appariements n'ont pas été modifiés : {0}"),
    ES("Los emparejamientos no se cambiaron: {0}"),
    PT("Os pareamentos não foram alterados: {0}"),
    IT("I confronti non sono stati modificati: {0}"),
    NL("De koppelingen zijn niet gewijzigd: {0}"),
    RU("Сопоставления не изменены: {0}"),
    TR("Eşleşmeler değiştirilmedi: {0}"));

SS_MSG(matrix_file_changed,
    EN("matches.bin was rewritten by another run; there is nothing to undo."),
    JA("matches.bin は別の実行で書き換えられました。元に戻せるものはありません。"),
    ZH_HANS("matches.bin 已被另一次运行改写，没有可以撤销的操作。"),
    ZH_HANT("matches.bin 已被另一次執行改寫，沒有可以復原的操作。"),
    KO("matches.bin 이 다른 실행에서 다시 쓰였습니다. 실행 취소할 것이 없습니다."),
    DE("matches.bin wurde von einem anderen Lauf neu geschrieben; es gibt nichts rückgängig zu machen."),
    FR("matches.bin a été réécrit par un autre passage ; il n'y a rien à annuler."),
    ES("Otra ejecución reescribió matches.bin; no hay nada que deshacer."),
    PT("Outra execução reescreveu matches.bin; não há nada para desfazer."),
    IT("Un'altra esecuzione ha riscritto matches.bin; non c'è nulla da annullare."),
    NL("matches.bin is door een andere run herschreven; er valt niets ongedaan te maken."),
    RU("matches.bin перезаписан другим запуском; отменять нечего."),
    TR("matches.bin başka bir çalıştırma tarafından yeniden yazıldı; geri alınacak bir şey yok."));

SS_MSG(matrix_close,
    EN("Close"),
    JA("閉じる"),
    ZH_HANS("关闭"),
    ZH_HANT("關閉"),
    KO("닫기"),
    DE("Schließen"),
    FR("Fermer"),
    ES("Cerrar"),
    PT("Fechar"),
    IT("Chiudi"),
    NL("Sluiten"),
    RU("Закрыть"),
    TR("Kapat"));

SS_MSG(model_live_counts,
    EN("Placed cameras: {0} of {1}   Points: {2}"),
    JA("配置できたカメラ: {0} / {1}   点: {2}"),
    ZH_HANS("已定位相机：{0} / {1}   点：{2}"),
    ZH_HANT("已定位相機：{0} / {1}   點：{2}"),
    KO("자리를 잡은 카메라: {0} / {1}   점: {2}"),
    DE("Platzierte Kameras: {0} von {1}   Punkte: {2}"),
    FR("Caméras placées : {0} sur {1}   Points : {2}"),
    ES("Cámaras situadas: {0} de {1}   Puntos: {2}"),
    PT("Câmeras posicionadas: {0} de {1}   Pontos: {2}"),
    IT("Fotocamere collocate: {0} su {1}   Punti: {2}"),
    NL("Geplaatste camera's: {0} van {1}   Punten: {2}"),
    RU("Размещено камер: {0} из {1}   Точек: {2}"),
    TR("Yerleşen kamera: {0} / {1}   Nokta: {2}"));

SS_MSG(pano360_unsupported,
    EN("360 layout not recognised"),
    JA("360 の並びが不明です"),
    ZH_HANS("无法识别的 360 排布"),
    ZH_HANT("無法辨識的 360 排布"),
    KO("알 수 없는 360 배치"),
    DE("360-Anordnung nicht erkannt"),
    FR("disposition 360 non reconnue"),
    ES("disposición 360 no reconocida"),
    PT("disposição 360 não reconhecida"),
    IT("disposizione 360 non riconosciuta"),
    NL("360-indeling niet herkend"),
    RU("раскладка 360 не распознана"),
    TR("360 yerleşimi tanınmadı"));

SS_MSG(pano360_unsupported_help,
    EN("Unsupported 360° track layout. Read as two ordinary lenses instead; reconstruction is unlikely to succeed."),
    JA("未対応の360度トラック配置です。通常の2レンズとして読むため、再構成は成功しにくくなります。"),
    ZH_HANS("不支持此 360° 轨道布局。改按两个普通镜头读取，重建很可能失败。"),
    ZH_HANT("不支援此 360° 軌道配置。改按兩個普通鏡頭讀取，重建很可能失敗。"),
    KO("지원하지 않는 360° 트랙 배치입니다. 일반 렌즈 두 개로 읽으므로 재구성 성공 가능성이 낮습니다."),
    DE("Nicht unterstütztes 360°-Spurlayout. Wird als zwei gewöhnliche Objektive gelesen; Rekonstruktion gelingt wahrscheinlich nicht."),
    FR("Disposition des pistes 360° non prise en charge. Lues comme deux objectifs ordinaires ; la reconstruction risque d’échouer."),
    ES("Distribución de pistas 360° no compatible. Se leen como dos lentes normales; es probable que falle la reconstrucción."),
    PT("Disposição de pistas 360° não suportada. Lidas como duas lentes normais; é provável que a reconstrução falhe."),
    IT("Tracce 360° non supportate. Lette come due lenti normali; la ricostruzione probabilmente fallirà."),
    NL("Niet-ondersteunde 360°-spoorindeling. Gelezen als twee gewone lenzen; reconstructie lukt waarschijnlijk niet."),
    RU("Неподдерживаемая раскладка треков 360°. Читается как два обычных объектива; реконструкция вряд ли удастся."),
    TR("Desteklenmeyen 360° iz düzeni. İki normal mercek olarak okunur; yeniden oluşturma muhtemelen başarısız olur."));

SS_MSG(view_motion,
    EN("Motion"),
    JA("動き"),
    ZH_HANS("运动"),
    ZH_HANT("運動"),
    KO("움직임"),
    DE("Bewegung"),
    FR("Mouvement"),
    ES("Movimiento"),
    PT("Movimento"),
    IT("Movimento"),
    NL("Beweging"),
    RU("Движение"),
    TR("Hareket"));

SS_MSG(view_sharpness,
    EN("Sharpness"),
    JA("シャープさ"),
    ZH_HANS("清晰度"),
    ZH_HANT("清晰度"),
    KO("선명도"),
    DE("Schärfe"),
    FR("Netteté"),
    ES("Nitidez"),
    PT("Nitidez"),
    IT("Nitidezza"),
    NL("Scherpte"),
    RU("Резкость"),
    TR("Keskinlik"));

SS_MSG(sharpness_preview,
    EN("This is what these settings would keep, not what the dataset holds. Applying them sorts the frames again; the list beside the button says what else that redoes."),
    JA("これは今の設定で残るフレームで、データセットの現状ではありません。適用するとフレームを仕分け直します。ほかに何をやり直すかは、ボタン横の一覧に出ます。"),
    ZH_HANS("这是按当前设置会保留的帧，而不是数据集现有的内容。应用后会重新整理帧；还会重做什么，见按钮旁的列表。"),
    ZH_HANT("這是依目前設定會保留的影格，而不是資料集現有的內容。套用後會重新整理影格；還會重做什麼，請看按鈕旁的清單。"),
    KO("지금 설정으로 남게 될 프레임이며, 데이터셋의 현재 상태가 아닙니다. 적용하면 프레임을 다시 정리합니다. 그 밖에 무엇을 다시 하는지는 버튼 옆 목록에 나옵니다."),
    DE("Vorschau dessen, was diese Einstellungen behalten würden. Anwenden sortiert die Bilder neu; weitere wiederholte Schritte stehen neben der Schaltfläche."),
    FR("Voici ce que ces réglages garderaient, pas ce que contient le jeu de données. Les appliquer trie de nouveau les images ; la liste à côté du bouton dit ce qui est refait d’autre."),
    ES("Esto es lo que conservarían estos ajustes, no lo que contiene el conjunto de datos.\nAl aplicarlos se vuelven a ordenar los fotogramas; la lista junto al botón dice qué más se rehace."),
    PT("Isto é o que estas configurações manteriam, não o que o conjunto de dados contém. Aplicá-las organiza os quadros de novo; a lista ao lado do botão diz o que mais é refeito."),
    IT("Questo è ciò che queste impostazioni terrebbero, non ciò che il set di dati contiene.\nApplicandole i fotogrammi vengono riordinati; l’elenco accanto al pulsante dice cos’altro viene rifatto."),
    NL("Dit is wat deze instellingen zouden behouden, niet wat de dataset bevat. Toepassen sorteert de beelden opnieuw; de lijst naast de knop zegt wat er verder opnieuw wordt gedaan."),
    RU("Это то, что оставили бы эти настройки, а не то, что сейчас в наборе данных. При применении кадры сортируются заново; что ещё будет переделано, показано в списке рядом с кнопкой."),
    TR("Bu, bu ayarların tutacağı karelerdir; veri kümesinde şu an olanlar değil.\nUygulandığında kareler yeniden ayıklanır; başka neyin yeniden yapılacağını düğmenin yanındaki liste söyler."));

SS_MSG(sharpness_apply,
    EN("Apply to the dataset"),
    JA("データセットに適用"),
    ZH_HANS("应用到数据集"),
    ZH_HANT("套用到資料集"),
    KO("데이터셋에 적용"),
    DE("Auf den Datensatz anwenden"),
    FR("Appliquer au jeu de données"),
    ES("Aplicar al conjunto de datos"),
    PT("Aplicar ao conjunto de dados"),
    IT("Applica al set di dati"),
    NL("Toepassen op de dataset"),
    RU("Применить к набору данных"),
    TR("Veri kümesine uygula"));

SS_MSG(sharpness_view_help,
    EN("Green is sharp, red is twice the blur threshold; rejected frames are dimmed. Adjust settings, click to inspect, Ctrl+scroll to resize."),
    JA("緑は鮮明、赤はぼけ閾値の2倍で、除外フレームは暗く表示します。設定を調整し、クリックで確認、Ctrl＋スクロールで拡縮します。"),
    ZH_HANS("绿色表示清晰，红色表示模糊达到阈值两倍，被排除帧变暗。调整设置、点击查看，Ctrl+滚轮缩放。"),
    ZH_HANT("綠色表示清晰，紅色表示模糊達到閾值兩倍，被排除影格變暗。調整設定、點擊查看，Ctrl+滾輪縮放。"),
    KO("초록은 선명, 빨강은 흐림 기준의 두 배, 제외 프레임은 흐리게 표시됩니다. 설정 조정, 클릭으로 확인, Ctrl+스크롤로 크기를 바꿉니다."),
    DE("Grün: scharf; Rot: doppelte Unschärfeschwelle; Ausschlüsse gedimmt. Einstellungen ändern, per Klick prüfen, Ctrl+Scroll ändert die Bildgröße."),
    FR("Vert : net ; rouge : deux fois le seuil ; rejets assombris. Ajuster les réglages, cliquer pour inspecter, Ctrl+défilement redimensionne."),
    ES("Verde: nítido; rojo: doble del umbral; descartes atenuados. Ajusta valores, haz clic para ver, Ctrl+rueda cambia el tamaño."),
    PT("Verde: nítido; vermelho: dobro do limiar; rejeitados escurecidos. Ajuste valores, clique para ver, Ctrl+roda muda o tamanho."),
    IT("Verde: nitido; rosso: doppio della soglia; scarti attenuati. Regolare i valori, clic per vedere, Ctrl+rotella cambia dimensione."),
    NL("Groen: scherp; rood: dubbele drempel; afgewezen beelden gedimd. Wijzig instellingen, klik voor inspectie, Ctrl+scroll verandert de grootte."),
    RU("Зелёный — резко, красный — двойной порог, отклонённые затемнены. Меняйте настройки, кликайте для просмотра; Ctrl+прокрутка меняет размер."),
    TR("Yeşil net, kırmızı bulanıklık eşiğinin iki katı; elenenler soluktur. Ayarları değiştirin, incelemek için tıklayın, Ctrl+kaydırma boyutlandırır."));

SS_MSG(sharpness_counts,
    EN("{0}: kept {1}, blurry {2}, too similar {3}"),
    JA("{0}: 残す {1}、ぶれ {2}、似すぎ {3}"),
    ZH_HANS("{0}：保留 {1}，模糊 {2}，过于相似 {3}"),
    ZH_HANT("{0}：保留 {1}，模糊 {2}，過於相似 {3}"),
    KO("{0}: 유지 {1}, 흐림 {2}, 너무 비슷함 {3}"),
    DE("{0}: behalten {1}, unscharf {2}, zu ähnlich {3}"),
    FR("{0} : gardées {1}, floues {2}, trop semblables {3}"),
    ES("{0}: conservados {1}, borrosos {2}, demasiado parecidos {3}"),
    PT("{0}: mantidos {1}, borrados {2}, parecidos demais {3}"),
    IT("{0}: tenuti {1}, sfocati {2}, troppo simili {3}"),
    NL("{0}: behouden {1}, onscherp {2}, te gelijkend {3}"),
    RU("{0}: оставлено {1}, размытых {2}, слишком похожих {3}"),
    TR("{0}: tutulan {1}, bulanık {2}, fazla benzer {3}"));

SS_MSG(pick_kept,
    EN("Frame {0}, kept: blurred about {1} px"),
    JA("フレーム {0}、残す: ぶれ約 {1} px"),
    ZH_HANS("帧 {0}，保留：模糊约 {1} px"),
    ZH_HANT("影格 {0}，保留：模糊約 {1} px"),
    KO("프레임 {0}, 유지: 흐림 약 {1} px"),
    DE("Bild {0}, behalten: etwa {1} px unscharf"),
    FR("Image {0}, gardée : flou d’environ {1} px"),
    ES("Fotograma {0}, conservado: desenfoque de unos {1} px"),
    PT("Quadro {0}, mantido: borrão de cerca de {1} px"),
    IT("Fotogramma {0}, tenuto: sfocatura di circa {1} px"),
    NL("Beeld {0}, behouden: ongeveer {1} px onscherp"),
    RU("Кадр {0}, оставлен: размытие около {1} px"),
    TR("Kare {0}, tutuldu: yaklaşık {1} px bulanık"));

SS_MSG(pick_blurry,
    EN("Frame {0}, skipped as blurry: blurred about {1} px"),
    JA("フレーム {0}、ぶれのため除外: ぶれ約 {1} px"),
    ZH_HANS("帧 {0}，因模糊跳过：模糊约 {1} px"),
    ZH_HANT("影格 {0}，因模糊略過：模糊約 {1} px"),
    KO("프레임 {0}, 흐려서 건너뜀: 흐림 약 {1} px"),
    DE("Bild {0}, als unscharf übersprungen: etwa {1} px unscharf"),
    FR("Image {0}, ignorée car floue : flou d’environ {1} px"),
    ES("Fotograma {0}, omitido por borroso: desenfoque de unos {1} px"),
    PT("Quadro {0}, ignorado por estar borrado: borrão de cerca de {1} px"),
    IT("Fotogramma {0}, saltato perché sfocato: sfocatura di circa {1} px"),
    NL("Beeld {0}, overgeslagen als onscherp: ongeveer {1} px onscherp"),
    RU("Кадр {0}, пропущен как размытый: размытие около {1} px"),
    TR("Kare {0}, bulanık olduğu için atlandı: yaklaşık {1} px bulanık"));

SS_MSG(pick_similar,
    EN("Frame {0}, skipped as too similar to the last kept frame "
       "(fingerprint bits that differ, of 64: {1})"),
    JA("フレーム {0}、似すぎのため除外 (直前に残したフレームと違う指紋ビッ"
       "ト、64 中: {1})"),
    ZH_HANS("帧 {0}，过于相似，跳过（与上一个保留帧不同的指纹位，64 位中："
            "{1}）"),
    ZH_HANT("影格 {0}，過於相似，略過（與上一個保留的影格不同的指紋位元，64 位"
            "元中：{1}）"),
    KO("프레임 {0}, 너무 비슷해 건너뜀 (마지막으로 남긴 프레임과 다"
       "른 지문 비트, 64비트 중: {1})"),
    DE("Bild {0}, als zu ähnlich übersprungen (Bits des Fingerabdrucks, "
       "die sich vom zuletzt behaltenen Bild unterscheiden, von 64: {1})"),
    FR("Image {0}, ignorée car trop semblable à la dernière image gardée "
       "(bits de l’empreinte qui diffèrent, sur 64 : {1})"),
    ES("Fotograma {0}, omitido por parecerse demasiado al último "
       "conservado (bits de la huella que difieren, de 64: {1})"),
    PT("Quadro {0}, ignorado por ser parecido demais com o último mantido "
       "(bits da impressão digital que diferem, de 64: {1})"),
    IT("Fotogramma {0}, saltato perché troppo simile all’ultimo tenuto "
       "(bit dell’impronta che differiscono, su 64: {1})"),
    NL("Beeld {0}, overgeslagen als te gelijkend op het laatst behouden "
       "beeld (vingerafdrukbits die verschillen, van 64: {1})"),
    RU("Кадр {0}, пропущен как слишком похожий на последний оставленный "
       "(отличается бит отпечатка, из 64: {1})"),
    TR("Kare {0}, son tutulan kareye fazla benzediği için atlandı (farklı "
       "parmak izi biti, 64 bitten: {1})"));

SS_MSG(pick_unmeasured,
    EN("Frame {0}: its blur was not measured"),
    JA("フレーム {0}: ぶれは測っていません"),
    ZH_HANS("帧 {0}：未测量模糊"),
    ZH_HANT("影格 {0}：未測量模糊"),
    KO("프레임 {0}: 흐림을 측정하지 않았습니다"),
    DE("Bild {0}: Unschärfe wurde nicht gemessen"),
    FR("Image {0} : son flou n’a pas été mesuré"),
    ES("Fotograma {0}: no se midió su desenfoque"),
    PT("Quadro {0}: o borrão não foi medido"),
    IT("Fotogramma {0}: la sfocatura non è stata misurata"),
    NL("Beeld {0}: onscherpte is niet gemeten"),
    RU("Кадр {0}: размытие не измерялось"),
    TR("Kare {0}: bulanıklığı ölçülmedi"));

SS_MSG(review_continue,
    EN("Continue with these frames"),
    JA("これらのフレームで続行"),
    ZH_HANS("用这些帧继续"),
    ZH_HANT("用這些影格繼續"),
    KO("이 프레임으로 계속"),
    DE("Mit diesen Bildern fortfahren"),
    FR("Continuer avec ces images"),
    ES("Continuar con estos fotogramas"),
    PT("Continuar com estes quadros"),
    IT("Continua con questi fotogrammi"),
    NL("Doorgaan met deze beelden"),
    RU("Продолжить с этими кадрами"),
    TR("Bu karelerle devam et"));

SS_MSG(review_hint,
    EN("Frames are ready and the run is paused. Review thresholds and rejected frames, then continue to masks, reconstruction and depth."),
    JA("フレームの準備ができ、停止中です。閾値と除外フレームを確認し、続行するとマスク・再構成・深度処理に進みます。"),
    ZH_HANS("帧已就绪，运行已暂停。检查阈值和排除帧后继续，开始蒙版、重建及深度处理。"),
    ZH_HANT("影格已就緒，執行已暫停。檢查閾值和排除影格後繼續，開始遮罩、重建及深度處理。"),
    KO("프레임이 준비되어 실행이 멈췄습니다. 기준과 제외 프레임을 검토한 뒤 계속하면 마스크·재구성·깊이 처리가 시작됩니다."),
    DE("Bilder sind bereit, der Lauf pausiert. Schwellen und aussortierte Bilder prüfen; dann mit Masken, Rekonstruktion und Tiefe fortfahren."),
    FR("Images prêtes, traitement en pause. Vérifiez seuils et rejets, puis continuez vers masques, reconstruction et profondeur."),
    ES("Fotogramas listos, ejecución en pausa. Revisa umbrales y descartes, y continúa con máscaras, reconstrucción y profundidad."),
    PT("Fotogramas prontos, execução em pausa. Reveja limiares e rejeitados, depois continue com máscaras, reconstrução e profundidade."),
    IT("Fotogrammi pronti, esecuzione in pausa. Rivedere soglie e scarti, poi continuare con maschere, ricostruzione e profondità."),
    NL("Beelden klaar, run gepauzeerd. Bekijk drempels en afgewezen beelden; ga dan door met maskers, reconstructie en diepte."),
    RU("Кадры готовы, запуск на паузе. Проверьте пороги и отклонённые кадры, затем продолжите маски, реконструкцию и глубину."),
    TR("Kareler hazır, işlem duraklatıldı. Eşikleri ve elenen kareleri inceleyip maskeleme, yeniden oluşturma ve derinliğe devam edin."));

SS_MSG(pick_back,
    EN("Back to all frames"),
    JA("すべてのフレームに戻る"),
    ZH_HANS("返回全部帧"),
    ZH_HANT("返回全部影格"),
    KO("모든 프레임으로 돌아가기"),
    DE("Zurück zu allen Bildern"),
    FR("Retour à toutes les images"),
    ES("Volver a todos los fotogramas"),
    PT("Voltar a todos os quadros"),
    IT("Torna a tutti i fotogrammi"),
    NL("Terug naar alle beelden"),
    RU("Назад ко всем кадрам"),
    TR("Tüm karelere dön"));

SS_MSG(pick_no_file,
    EN("The full frame is not on disk; this is its thumbnail."),
    JA("フレーム本体はディスクにありません。これはサムネイルです。"),
    ZH_HANS("完整的帧不在磁盘上；这是它的缩略图。"),
    ZH_HANT("完整的影格不在磁碟上；這是它的縮圖。"),
    KO("원본 프레임이 디스크에 없습니다. 이것은 미리보기입니다."),
    DE("Das volle Bild liegt nicht auf der Festplatte; dies ist seine "
       "Vorschau."),
    FR("L’image complète n’est pas sur le disque ; ceci est sa vignette."),
    ES("El fotograma completo no está en el disco; esta es su miniatura."),
    PT("O quadro completo não está no disco; esta é a miniatura."),
    IT("Il fotogramma intero non è sul disco; questa è la sua miniatura."),
    NL("Het volledige beeld staat niet op schijf; dit is de miniatuur."),
    RU("Полного кадра на диске нет; это его миниатюра."),
    TR("Karenin tamamı diskte yok; bu onun küçük resmi."));

SS_MSG(pick_view_help,
    EN("Scroll to zoom, drag to move, double-click for 1:1 or to fit. The "
       "arrow keys step to the next frame and Esc goes back."),
    JA("スクロールで拡大縮小、ドラッグで移動、ダブルクリックで等倍または全体表示"
       "。矢印キーで次のフレームへ、Esc で戻ります。"),
    ZH_HANS("滚动缩放，拖动平移，双击切换 1:1 或适应窗口。方向键切换到下一帧，E"
            "sc 返回。"),
    ZH_HANT("捲動縮放，拖曳平移，按兩下切換 1:1 或符合視窗。方向鍵切換到下一個影"
            "格，Esc 返回。"),
    KO("스크롤로 확대/축소, 끌어서 이동, 두 번 클릭하면 1:1 또는 "
       "화면 맞춤. 화살표 키로 다음 프레임으로 가고 Esc로 돌아갑니다"
       "."),
    DE("Scrollen zoomt, Ziehen verschiebt, Doppelklick zeigt 1:1 oder "
       "passt ein. Die Pfeiltasten gehen zum nächsten Bild, Esc geht "
       "zurück."),
    FR("Faites défiler pour zoomer, glissez pour déplacer, double-cliquez "
       "pour 1:1 ou pour ajuster. Les flèches passent à l’image suivante "
       "et Échap revient en arrière."),
    ES("Desplace para ampliar, arrastre para mover, doble clic para 1:1 o "
       "para ajustar. Las flechas pasan al siguiente fotograma y Esc "
       "vuelve atrás."),
    PT("Role para ampliar, arraste para mover, clique duas vezes para 1:1 "
       "ou para ajustar. As setas passam ao próximo quadro e Esc volta."),
    IT("Scorri per ingrandire, trascina per spostare, doppio clic per 1:1 "
       "o per adattare. I tasti freccia passano al fotogramma successivo "
       "ed Esc torna indietro."),
    NL("Scrollen om te zoomen, slepen om te verplaatsen, dubbelklikken "
       "voor 1:1 of passend. De pijltoetsen gaan naar het volgende beeld "
       "en Esc gaat terug."),
    RU("Прокрутка — масштаб, перетаскивание — сдвиг, двойной щелчок — 1:1 "
       "или вписать. Стрелки переходят к следующему кадру, Esc возвращает "
       "назад."),
    TR("Yakınlaştırmak için kaydırın, taşımak için sürükleyin, 1:1 veya "
       "sığdırmak için çift tıklayın. Ok tuşları sonraki kareye geçer, Esc"
       " geri döner."));

SS_MSG(blur_off_hint,
    EN("Blur is measured only while blurry frames are being skipped. Choose that under Skip blurry frames and create the dataset again to see it for each frame."),
    JA("ぶれは、ぶれたフレームを除外しているときだけ測ります。「ぶれたフレームを除外」で選び、データセットを作り直すと、フレームごとに表示されます。"),
    ZH_HANS("只有在跳过模糊帧时才会测量模糊。请在“跳过模糊帧”中选择，然后重新创建数据集，即可看到每一帧的模糊程度。"),
    ZH_HANT("只有在略過模糊影格時才會測量模糊。請在「略過模糊影格」中選擇，然後重新建立資料集，即可看到每個影格的模糊程度。"),
    KO("흐림은 흐린 프레임을 건너뛸 때만 측정합니다. ‘흐린 프레임 건너뛰기’에서 선택하고 데이터셋을 다시 만들면 프레임마다 볼 수 있습니다."),
    DE("Unschärfe wird nur gemessen, solange unscharfe Bilder übersprungen werden.\nUnter „Unscharfe Bilder überspringen“ wählen und den Datensatz neu erstellen, um sie für jedes Bild zu sehen."),
    FR("Le flou n’est mesuré que lorsque les images floues sont ignorées. Choisissez-le sous « Ignorer les images floues » et recréez le jeu de données pour le voir pour chaque image."),
    ES("El desenfoque solo se mide mientras se omiten los fotogramas borrosos. Elíjalo en «Omitir fotogramas borrosos» y vuelva a crear el conjunto de datos para verlo en cada fotograma."),
    PT("O borrão só é medido enquanto os quadros borrados estão sendo ignorados.\nEscolha isso em “Ignorar quadros borrados” e crie o conjunto de dados de novo para vê-lo em cada quadro."),
    IT("La sfocatura viene misurata solo mentre si saltano i fotogrammi sfocati.\nSceglilo in «Salta i fotogrammi sfocati» e crea di nuovo il set di dati per vederla su ogni fotogramma."),
    NL("Onscherpte wordt alleen gemeten zolang onscherpe beelden worden overgeslagen.\nKies dat onder ‘Onscherpe beelden overslaan’ en maak de dataset opnieuw om het voor elk beeld te zien."),
    RU("Размытие измеряется при пропуске размытых кадров. Включите это в «Пропускать размытые кадры» и создайте набор заново, чтобы увидеть оценки кадров."),
    TR("Bulanıklık yalnızca bulanık kareler atlanırken ölçülür. Bunu “Bulanık kareleri atla” altında seçin ve her kare için görmek üzere veri kümesini yeniden oluşturun."));

SS_MSG(scan_picture_size,
    EN("Picture size"),
    JA("画像サイズ"),
    ZH_HANS("图片大小"),
    ZH_HANT("圖片大小"),
    KO("그림 크기"),
    DE("Bildgröße"),
    FR("Taille des images"),
    ES("Tamaño de imagen"),
    PT("Tamanho da imagem"),
    IT("Dimensione immagini"),
    NL("Beeldgrootte"),
    RU("Размер картинок"),
    TR("Resim boyutu"));

SS_MSG(scan_photos,
    EN("{0} photographs, nothing to measure"),
    JA("写真 {0} 枚、測るものはありません"),
    ZH_HANS("{0} 张照片，没有运动可测"),
    ZH_HANT("{0} 張照片，沒有運動可測"),
    KO("사진 {0}장, 잴 것이 없습니다"),
    DE("{0} Fotos, nichts zu messen"),
    FR("{0} photographies, rien à mesurer"),
    ES("{0} fotografías, nada que medir"),
    PT("{0} fotografias, nada a medir"),
    IT("{0} fotografie, niente da misurare"),
    NL("{0} foto's, niets te meten"),
    RU("{0} фотографий, измерять нечего"),
    TR("{0} fotoğraf, ölçecek bir şey yok"));

SS_MSG(scan_kept_frames,
    EN("{0} frames kept"),
    JA("{0} フレームを残しました"),
    ZH_HANS("保留了 {0} 帧"),
    ZH_HANT("保留了 {0} 影格"),
    KO("{0}개 프레임을 남겼습니다"),
    DE("{0} Einzelbilder behalten"),
    FR("{0} images conservées"),
    ES("{0} fotogramas conservados"),
    PT("{0} quadros mantidos"),
    IT("{0} fotogrammi tenuti"),
    NL("{0} beelden gehouden"),
    RU("оставлено кадров: {0}"),
    TR("{0} kare tutuldu"));

SS_MSG(scan_show_pictures,
    EN("Show the frames"),
    JA("フレームを表示"),
    ZH_HANS("显示帧"),
    ZH_HANT("顯示影格"),
    KO("프레임 보기"),
    DE("Bilder anzeigen"),
    FR("Afficher les images"),
    ES("Mostrar los fotogramas"),
    PT("Mostrar os quadros"),
    IT("Mostra i fotogrammi"),
    NL("Beelden tonen"),
    RU("Показывать кадры"),
    TR("Kareleri göster"));

SS_MSG(frame_spacing_help,
    EN("Left to right is the length of the capture. A tall bar is a stretch "
       "where the view changed fast and more frames were kept."),
    JA("左から右が撮影の長さです。棒が高いところは視界の変化が速く、"
       "多くのフレームを残した区間です。"),
    ZH_HANS("从左到右是整段素材的长度。柱子高的地方视野变化快，留下的帧也多。"),
    ZH_HANT("從左到右是整段素材的長度。柱子高的地方視野變化快，留下的影格也多。"),
    KO("왼쪽에서 오른쪽이 촬영 전체 길이입니다. 막대가 높은 구간은 화면이 빨리 "
       "바뀌어 프레임을 더 남긴 곳입니다."),
    DE("Links nach rechts ist die Länge der Aufnahme. Ein hoher Balken ist ein "
       "Stück, in dem sich das Bild schnell änderte und mehr Bilder blieben."),
    FR("De gauche à droite, la durée de la prise. Une barre haute est un "
       "passage où la vue changeait vite et où plus d'images ont été gardées."),
    ES("De izquierda a derecha, la duración de la toma. Una barra alta es un "
       "tramo donde la vista cambiaba deprisa y se guardaron más fotogramas."),
    PT("Da esquerda para a direita, a duração da captura. Uma barra alta é um "
       "trecho onde a vista mudava depressa e ficaram mais quadros."),
    IT("Da sinistra a destra, la durata della ripresa. Una barra alta è un "
       "tratto in cui la vista cambiava in fretta e sono rimasti più fotogrammi."),
    NL("Van links naar rechts is de lengte van de opname. Een hoge balk is een "
       "stuk waar het beeld snel veranderde en meer beelden zijn gehouden."),
    RU("Слева направо — длительность съёмки. Высокий столбик — участок, где "
       "вид менялся быстро и кадров осталось больше."),
    TR("Soldan sağa çekimin uzunluğu. Yüksek çubuk, görüntünün hızlı değiştiği "
       "ve daha çok kare tutulan bir bölüm."));

SS_MSG(model_waiting,
    EN("Nothing placed yet -- the first two views have to agree before there is "
       "anything to draw."),
    JA("まだ何も配置されていません。最初の二枚が一致するまで描くものがありません。"),
    ZH_HANS("还没有定位任何相机 —— 要等最初两张视图对上，才有东西可画。"),
    ZH_HANT("還沒有定位任何相機 —— 要等最初兩張視圖對上，才有東西可畫。"),
    KO("아직 자리를 잡은 것이 없습니다. 처음 두 장이 맞아야 그릴 것이 생깁니다."),
    DE("Noch nichts platziert -- erst müssen sich die ersten beiden Ansichten "
       "einig werden, bevor es etwas zu zeichnen gibt."),
    FR("Rien de placé pour l'instant : il faut que les deux premières vues "
       "s'accordent avant qu'il y ait quelque chose à dessiner."),
    ES("Todavía no hay nada situado: las dos primeras vistas tienen que "
       "coincidir antes de que haya algo que dibujar."),
    PT("Ainda nada posicionado -- as duas primeiras vistas precisam concordar "
       "antes de haver algo para desenhar."),
    IT("Ancora niente collocato: le prime due viste devono trovarsi d'accordo "
       "prima che ci sia qualcosa da disegnare."),
    NL("Nog niets geplaatst -- de eerste twee aanzichten moeten het eens worden "
       "voordat er iets te tekenen valt."),
    RU("Пока ничего не размещено -- сначала должны сойтись первые два вида, и "
       "только тогда появится что рисовать."),
    TR("Henüz hiçbir şey yerleşmedi -- çizilecek bir şey olması için önce ilk "
       "iki görüntünün anlaşması gerekiyor."));

// ---------------------------------------------------------------------------
// Re-doing one step of a run that is already on disk
// ---------------------------------------------------------------------------

SS_MSG(rerun_section,
    EN("Re-do one step"),
    JA("一つの工程だけやり直す"),
    ZH_HANS("只重做一步"),
    ZH_HANT("只重做一步"),
    KO("한 단계만 다시 하기"),
    DE("Einen Schritt wiederholen"),
    FR("Refaire une seule étape"),
    ES("Rehacer un solo paso"),
    PT("Refazer uma etapa"),
    IT("Rifare un solo passo"),
    NL("Eén stap opnieuw doen"),
    RU("Переделать один шаг"),
    TR("Tek bir adımı yenile"));

SS_MSG(rerun_section_help,
    EN("This folder already holds part of a run. Pick a step to throw away and do again; everything before it is kept.\nA bad mask prompt costs the masking pass, not the extraction as well."),
    JA("このフォルダーには前回の途中結果が残っています。 やり直す工程を選んでください。 その前の結果はそのまま使います。\nマスクの指定を間違えても、やり直すのはマスクだけで、フレームの書き出しからにはなりません。"),
    ZH_HANS("这个文件夹里已经有上次运行的一部分结果。选一步丢掉重做，它之前的都保留。蒙版提示词写错了，只需重做蒙版，不必连抽帧一起重来。"),
    ZH_HANT("這個資料夾裡已經有上次執行的一部分結果。選一步丟掉重做，它之前的都保留。遮罩提示詞寫錯了，只需重做遮罩，不必連抽格一起重來。"),
    KO("기존 결과 중 다시 실행할 단계를 고르세요. 이전 단계는 유지됩니다. 마스크 문구를 바꿔도 마스크만 다시 만들며 프레임은 다시 추출하지 않습니다."),
    DE("In diesem Ordner liegt schon ein Teil eines Laufs.\n- Wählen Sie den Schritt, der verworfen und neu gemacht wird; alles davor bleibt.\n- Ein schlechter Masken-Text kostet den Maskendurchgang, nicht auch die Bildausgabe."),
    FR("Ce dossier contient déjà une partie d'une exécution.\n- Choisissez l'étape à jeter et à refaire ; tout ce qui précède est conservé.\n- Une mauvaise description de masque coûte la passe de masquage, pas aussi l'extraction."),
    ES("Esta carpeta ya guarda parte de una ejecución.\n- Elige el paso que se tira y se rehace; todo lo anterior se conserva.\n- Una descripción de máscara equivocada cuesta la pasada de máscaras, no también la extracción."),
    PT("Esta pasta já guarda parte de uma execução. Escolha a etapa a jogar fora e refazer; tudo antes dela fica.\nUma descrição de máscara errada custa a passagem de máscaras, não também a extração."),
    IT("Questa cartella contiene già parte di un'esecuzione.\n- Scegli il passo da buttare e rifare; tutto ciò che viene prima resta.\n- Una descrizione di maschera sbagliata costa la passata di mascheratura, non anche l'estrazione."),
    NL("In deze map staat al een deel van een run.\n- Kies de stap die wordt weggegooid en opnieuw gedaan; alles ervoor blijft staan.\n- Een verkeerde maskeromschrijving kost de maskeerronde, niet ook het uitpakken."),
    RU("В этой папке уже лежит часть запуска.\n- Выберите шаг, который надо выбросить и сделать заново; всё до него остаётся.\n- Неудачный запрос для маски стоит прохода масок, а не ещё и извлечения кадров."),
    TR("Bu klasörde bir çalışmanın bir bölümü zaten duruyor. Atılıp yeniden yapılacak adımı seçin; ondan öncesi kalır.\nKötü bir maske metni yalnız maskeleme geçişine mal olur, kare çıkarmaya da değil."));

SS_MSG(rerun_frames,
    EN("Frames again"),
    JA("フレームからやり直す"),
    ZH_HANS("重新抽帧"),
    ZH_HANT("重新抽格"),
    KO("프레임부터 다시"),
    DE("Bilder neu"),
    FR("Refaire les images"),
    ES("Rehacer los fotogramas"),
    PT("Refazer os quadros"),
    IT("Rifai i fotogrammi"),
    NL("Beelden opnieuw"),
    RU("Кадры заново"),
    TR("Kareler yeniden"));

SS_MSG(rerun_masks,
    EN("Masks again"),
    JA("マスクからやり直す"),
    ZH_HANS("重做蒙版"),
    ZH_HANT("重做遮罩"),
    KO("마스크부터 다시"),
    DE("Masken neu"),
    FR("Refaire les masques"),
    ES("Rehacer las máscaras"),
    PT("Refazer as máscaras"),
    IT("Rifai le maschere"),
    NL("Maskers opnieuw"),
    RU("Маски заново"),
    TR("Maskeler yeniden"));

SS_MSG(rerun_model,
    EN("Reconstruction again"),
    JA("再構成からやり直す"),
    ZH_HANS("重新重建"),
    ZH_HANT("重新重建"),
    KO("재구성부터 다시"),
    DE("Rekonstruktion neu"),
    FR("Refaire la reconstruction"),
    ES("Rehacer la reconstrucción"),
    PT("Refazer a reconstrução"),
    IT("Rifai la ricostruzione"),
    NL("Reconstructie opnieuw"),
    RU("Реконструкция заново"),
    TR("Yeniden kurma yeniden"));

// ---------------------------------------------------------------------------
// Starting the project over
// ---------------------------------------------------------------------------

SS_MSG(reset_section,
    EN("Start over"),
    JA("最初からやり直す"), ZH_HANS("从头开始"), ZH_HANT("從頭開始"),
    KO("처음부터 다시"),   DE("Von vorn anfangen"),
    FR("Repartir de zéro"), ES("Empezar de nuevo"),
    PT("Começar do zero"),  IT("Ricominciare da capo"),
    NL("Opnieuw beginnen"), RU("Начать заново"),
    TR("Baştan başla"));

SS_MSG(reset_section_help,
    EN("Throw away what this project has made, or put the options back where "
       "they started. The photos and videos you picked are never touched."),
    JA("このプロジェクトが作ったものを捨てるか、設定を最初の状態に戻します。"
       "選んだ写真や動画には手を触れません。"),
    ZH_HANS("丢掉这个项目已经生成的东西，或者把设置恢复成最初的样子。"
            "你选的照片和视频不会被动。"),
    ZH_HANT("丟掉這個專案已經產生的東西，或者把設定恢復成最初的樣子。"
            "你選的照片和影片不會被動。"),
    KO("이 프로젝트가 만든 것을 버리거나, 설정을 처음 상태로 되돌립니다. "
       "고른 사진과 영상에는 손대지 않습니다."),
    DE("Werfen Sie weg, was dieses Projekt erzeugt hat, oder setzen Sie die "
       "Optionen auf den Anfang zurück. Die gewählten Fotos und Videos bleiben "
       "unangetastet."),
    FR("Jetez ce que ce projet a produit, ou remettez les réglages à leur "
       "point de départ. Les photos et vidéos choisies ne sont jamais "
       "touchées."),
    ES("Tira lo que este proyecto ha generado, o devuelve las opciones a su "
       "punto de partida. Las fotos y los vídeos que elegiste no se tocan "
       "nunca."),
    PT("Deite fora o que este projeto produziu, ou reponha as opções no ponto "
       "de partida. As fotos e os vídeos que escolheu nunca são tocados."),
    IT("Butta via ciò che questo progetto ha prodotto, o riporta le opzioni al "
       "punto di partenza. Le foto e i video che hai scelto non vengono mai "
       "toccati."),
    NL("Gooi weg wat dit project heeft gemaakt, of zet de opties terug op hun "
       "beginstand. De gekozen foto's en video's worden nooit aangeraakt."),
    RU("Выбросьте то, что сделал этот проект, или верните настройки к "
       "исходным. Выбранные фотографии и видео никогда не трогаются."),
    TR("Bu projenin ürettiğini atın ya da seçenekleri başlangıç durumuna "
       "döndürün. Seçtiğiniz fotoğraflara ve videolara hiç dokunulmaz."));

SS_MSG(clear_project,
    EN("Clear this project's data"),
    JA("このプロジェクトのデータを消す"),
    ZH_HANS("清除本项目的数据"), ZH_HANT("清除本專案的資料"),
    KO("이 프로젝트의 데이터 지우기"),
    DE("Daten dieses Projekts löschen"),
    FR("Effacer les données de ce projet"),
    ES("Borrar los datos de este proyecto"),
    PT("Apagar os dados deste projeto"),
    IT("Cancella i dati di questo progetto"),
    NL("Gegevens van dit project wissen"),
    RU("Удалить данные этого проекта"),
    TR("Bu projenin verilerini sil"));

SS_MSG(clear_project_help,
    EN("Deletes all generated output: frames, masks, features, reconstruction, depth and normals. The next run starts over."),
    JA("生成されたフレーム、マスク、特徴点、再構成、深度、法線をすべて削除します。次回は最初から実行します。"),
    ZH_HANS("删除全部生成结果：帧、蒙版、特征、重建、深度及法线。下次运行从头开始。"),
    ZH_HANT("刪除全部產生結果：影格、遮罩、特徵、重建、深度及法線。下次執行從頭開始。"),
    KO("생성된 프레임·마스크·특징·재구성·깊이·법선을 모두 삭제합니다. 다음 실행은 처음부터 시작합니다."),
    DE("Löscht alle erzeugten Ergebnisse: Bilder, Masken, Merkmale, Rekonstruktion, Tiefe und Normalen. Der nächste Lauf beginnt von vorn."),
    FR("Supprime tous les résultats générés : images, masques, caractéristiques, reconstruction, profondeur et normales. Le prochain traitement repart de zéro."),
    ES("Borra todos los resultados generados: fotogramas, máscaras, características, reconstrucción, profundidad y normales. La próxima ejecución empieza de cero."),
    PT("Apaga todos os resultados gerados: fotogramas, máscaras, características, reconstrução, profundidade e normais. A próxima execução começa do zero."),
    IT("Elimina tutti i risultati generati: fotogrammi, maschere, caratteristiche, ricostruzione, profondità e normali. Il prossimo avvio riparte da zero."),
    NL("Verwijdert alle gemaakte uitvoer: beelden, maskers, kenmerken, reconstructie, diepte en normalen. De volgende run begint opnieuw."),
    RU("Удаляет все результаты: кадры, маски, признаки, реконструкцию, глубину и нормали. Следующий запуск начнётся заново."),
    TR("Üretilen tüm kare, maske, özellik, model, derinlik ve normalleri siler. Sonraki çalıştırma baştan başlar."));

SS_MSG(clear_project_title,
    EN("Clear the output folder"),
    JA("出力フォルダを空にする"),
    ZH_HANS("清空输出文件夹"), ZH_HANT("清空輸出資料夾"),
    KO("출력 폴더 비우기"),
    DE("Ausgabeordner leeren"),
    FR("Vider le dossier de sortie"),
    ES("Vaciar la carpeta de salida"),
    PT("Esvaziar a pasta de saída"),
    IT("Svuota la cartella di uscita"),
    NL("Uitvoermap leegmaken"),
    RU("Очистить папку вывода"),
    TR("Çıktı klasörünü boşalt"));

SS_MSG(clear_project_confirm,
    EN("These are deleted from {0}. The photos and videos you picked are not "
       "touched."),
    JA("{0} から次のものを削除します。選んだ写真や動画には手を触れません。"),
    ZH_HANS("将从 {0} 中删除下面这些。你选的照片和视频不会被动。"),
    ZH_HANT("將從 {0} 中刪除下面這些。你選的照片和影片不會被動。"),
    KO("{0} 에서 다음을 지웁니다. 고른 사진과 영상에는 손대지 않습니다."),
    DE("Aus {0} wird Folgendes gelöscht. Die gewählten Fotos und Videos "
       "bleiben unangetastet."),
    FR("Ceci est supprimé de {0}. Les photos et vidéos choisies ne sont pas "
       "touchées."),
    ES("Esto se borra de {0}. Las fotos y los vídeos que elegiste no se "
       "tocan."),
    PT("Isto é apagado de {0}. As fotos e os vídeos que escolheu não são "
       "tocados."),
    IT("Questo viene cancellato da {0}. Le foto e i video che hai scelto non "
       "vengono toccati."),
    NL("Dit wordt verwijderd uit {0}. De gekozen foto's en video's worden niet "
       "aangeraakt."),
    RU("Это будет удалено из {0}. Выбранные фотографии и видео не трогаются."),
    TR("Şunlar {0} içinden silinir. Seçtiğiniz fotoğraflara ve videolara "
       "dokunulmaz."));

SS_MSG(clear_project_button,
    EN("Delete"),
    JA("削除"),            ZH_HANS("删除"),      ZH_HANT("刪除"),
    KO("삭제"),            DE("Löschen"),
    FR("Supprimer"),       ES("Borrar"),
    PT("Apagar"),          IT("Cancella"),
    NL("Verwijderen"),     RU("Удалить"),
    TR("Sil"));

SS_MSG(clear_project_done,
    EN("Cleared the output folder: {0}"),
    JA("出力フォルダを空にしました: {0}"),
    ZH_HANS("已清空输出文件夹：{0}"), ZH_HANT("已清空輸出資料夾：{0}"),
    KO("출력 폴더를 비웠습니다: {0}"),
    DE("Ausgabeordner geleert: {0}"),
    FR("Dossier de sortie vidé : {0}"),
    ES("Carpeta de salida vaciada: {0}"),
    PT("Pasta de saída esvaziada: {0}"),
    IT("Cartella di uscita svuotata: {0}"),
    NL("Uitvoermap leeggemaakt: {0}"),
    RU("Папка вывода очищена: {0}"),
    TR("Çıktı klasörü boşaltıldı: {0}"));

SS_MSG(clear_project_failed,
    EN("Could not delete {0}: {1}"),
    JA("{0} を削除できませんでした: {1}"),
    ZH_HANS("无法删除 {0}：{1}"), ZH_HANT("無法刪除 {0}：{1}"),
    KO("{0} 을 지우지 못했습니다: {1}"),
    DE("{0} konnte nicht gelöscht werden: {1}"),
    FR("Impossible de supprimer {0} : {1}"),
    ES("No se pudo borrar {0}: {1}"),
    PT("Não foi possível apagar {0}: {1}"),
    IT("Impossibile cancellare {0}: {1}"),
    NL("Kon {0} niet verwijderen: {1}"),
    RU("Не удалось удалить {0}: {1}"),
    TR("{0} silinemedi: {1}"));

SS_MSG(reset_options,
    EN("Reset the options"),
    JA("設定を初期値に戻す"),
    ZH_HANS("把设置恢复默认"), ZH_HANT("把設定恢復預設"),
    KO("설정을 기본값으로"),
    DE("Optionen zurücksetzen"),
    FR("Réinitialiser les réglages"),
    ES("Restablecer las opciones"),
    PT("Repor as opções"),
    IT("Riporta le opzioni ai valori iniziali"),
    NL("Opties terugzetten"),
    RU("Сбросить настройки"),
    TR("Seçenekleri sıfırla"));

SS_MSG(reset_options_help,
    EN("Puts the reconstruction and the depth-and-normals options back to what a freshly picked input would have given them.\nThe inputs, the output folder and the mask prompt stay as they are."),
    JA("再構成と、深度・法線の設定を、入力を選び直したときの値に戻します。入力、出力フォルダ、マスクの指定はそのままです。"),
    ZH_HANS("把重建以及深度与法线的设置，恢复成刚选好输入时的值。输入、输出文件夹和蒙版提示词保持不变。"),
    ZH_HANT("把重建以及深度與法線的設定，恢復成剛選好輸入時的值。輸入、輸出資料夾和遮罩提示詞保持不變。"),
    KO("재구성과 깊이·법선 설정을 입력을 새로 골랐을 때의 값으로 되돌립니다. 입력, 출력 폴더, 마스크 문구는 그대로 둡니다."),
    DE("Setzt Rekonstruktion, Tiefe und Normalen auf die Vorgaben für die gewählte Eingabe zurück. Eingaben, Ausgabeordner und Masken-Text bleiben."),
    FR("Rétablit les réglages de reconstruction, profondeur et normales pour l’entrée choisie. Entrées, dossier de sortie et consigne de masque restent."),
    ES("Restablece reconstrucción, profundidad y normales a los valores de la entrada elegida. Mantiene las entradas, carpeta de salida e indicación de máscara."),
    PT("Repõe as opções de reconstrução e de profundidade e normais no que uma entrada acabada de escolher lhes teria dado. As entradas, a pasta de saída e a descrição de máscara ficam."),
    IT("Ripristina ricostruzione, profondità e normali ai valori dell’ingresso scelto. Mantiene ingressi, cartella di uscita e prompt della maschera."),
    NL("Zet de opties voor reconstructie en voor diepte en normalen terug op wat een net gekozen invoer ze zou hebben gegeven. De invoer, de uitvoermap en de maskeromschrijving blijven."),
    RU("Возвращает настройки реконструкции, глубины и нормалей к тем, что дал бы только что выбранный источник. Источники, папка вывода и запрос для маски остаются."),
    TR("Yeniden kurma ile derinlik ve normal seçeneklerini, yeni seçilmiş bir girdinin vereceği değerlere döndürür. Girdiler, çıktı klasörü ve maske metni kalır."));

SS_MSG(reset_options_done,
    EN("The options are back to their defaults."),
    JA("設定を初期値に戻しました。"),
    ZH_HANS("设置已恢复默认。"), ZH_HANT("設定已恢復預設。"),
    KO("설정을 기본값으로 되돌렸습니다."),
    DE("Die Optionen stehen wieder auf ihren Vorgaben."),
    FR("Les réglages sont revenus à leurs valeurs par défaut."),
    ES("Las opciones han vuelto a sus valores por defecto."),
    PT("As opções voltaram aos valores por omissão."),
    IT("Le opzioni sono tornate ai valori predefiniti."),
    NL("De opties staan weer op hun standaardwaarden."),
    RU("Настройки вернулись к значениям по умолчанию."),
    TR("Seçenekler varsayılan değerlerine döndü."));

SS_MSG(open_in_trainer,
    EN("Open in Trainer"),
    JA("トレーナーで開く"),
    ZH_HANS("在训练器中打开"),
    ZH_HANT("在訓練器中開啟"),
    KO("트레이너에서 열기"),
    DE("Im Trainer öffnen"),
    FR("Ouvrir dans l'atelier"),
    ES("Abrir en el entrenador"),
    PT("Abrir no treinador"),
    IT("Apri nell'addestratore"),
    NL("Openen in de trainer"),
    RU("Открыть в тренажёре"),
    TR("Eğiticide aç"));

SS_MSG(done_at,
    EN("Done: {0}"),     JA("完了: {0}"),     ZH_HANS("完成：{0}"), ZH_HANT("完成：{0}"),
    KO("완료: {0}"),      DE("Fertig: {0}"),  FR("Terminé : {0}"), ES("Listo: {0}"),
    PT("Pronto: {0}"),   IT("Fatto: {0}"),   NL("Klaar: {0}"),   RU("Готово: {0}"),
    TR("Bitti: {0}"));

SS_MSG(failed,
    EN("Failed: {0}"),   JA("失敗: {0}"),     ZH_HANS("失败：{0}"), ZH_HANT("失敗：{0}"),
    KO("실패: {0}"),      DE("Fehlgeschlagen: {0}"), FR("Échec : {0}"),
    ES("Error: {0}"),    PT("Falhou: {0}"),  IT("Non riuscito: {0}"),
    NL("Mislukt: {0}"),  RU("Ошибка: {0}"),  TR("Başarısız: {0}"));

SS_MSG(cancelled,
    EN("Cancelled."),    JA("中止しました。"), ZH_HANS("已取消。"),  ZH_HANT("已取消。"),
    KO("취소했습니다."),  DE("Abgebrochen."), FR("Annulé."),      ES("Cancelado."),
    PT("Cancelado."),    IT("Annullato."),   NL("Geannuleerd."), RU("Отменено."),
    TR("İptal edildi."));

SS_MSG(partial_reconstruction,
    EN("Only part of the capture reconstructed -- it will train, but expect gaps. More overlap between shots, or a higher quality setting, usually fixes it."),
    JA("撮影の一部しか再構成できませんでした。学習は行えますが、欠けが出ます。撮影どうしの重なりを増やすか、品質設定を上げると直ることが多いです。"),
    ZH_HANS("只重建出了拍摄内容的一部分——仍然可以训练，但会有缺口。增加拍摄之间的重叠，或者提高质量设置，通常就能解决。"),
    ZH_HANT("只重建出了拍攝內容的一部分——仍然可以訓練，但會有缺口。增加拍攝之間的重疊，或者提高品質設定，通常就能解決。"),
    KO("촬영분의 일부만 재구성되었습니다. 학습은 되지만 빈 곳이 생깁니다. 촬영끼리 더 많이 겹치게 하거나 품질 설정을 올리면 대개 해결됩니다."),
    DE("Nur ein Teil der Aufnahme wurde rekonstruiert -- trainieren lässt es sich, aber mit Lücken. Mehr Überlappung zwischen den Aufnahmen oder eine höhere Qualitätsstufe hilft meist."),
    FR("Seule une partie de la prise a été reconstruite : elle s'entraînera, mais avec des trous.\nPlus de recouvrement entre les prises, ou un réglage de qualité plus élevé, corrige généralement le problème."),
    ES("Solo se reconstruyó parte de la captura: se puede entrenar, pero habrá huecos. Más solapamiento entre tomas, o una calidad más alta, suele arreglarlo."),
    PT("Só parte da captura foi reconstruída: dá para treinar, mas haverá falhas. Mais sobreposição entre as tomadas, ou uma qualidade mais alta, costuma resolver."),
    IT("È stata ricostruita solo una parte della ripresa: si può addestrare, ma con dei vuoti. Più sovrapposizione tra gli scatti, o una qualità più alta, di solito risolve."),
    NL("Slechts een deel van de opname is gereconstrueerd -- trainen kan, maar met gaten. Meer overlap tussen de opnamen, of een hogere kwaliteitsinstelling, helpt meestal."),
    RU("Восстановилась лишь часть съёмки — обучать можно, но с пробелами. Обычно помогает большее перекрытие между кадрами или более высокая настройка качества."),
    TR("Çekimin yalnızca bir bölümü yeniden oluşturuldu; eğitilebilir ama boşluklar bekleyin. Kareler arasında daha çok örtüşme veya yüksek kalite genelde düzeltir."));

SS_MSG(not_metric_reconstruction,
    EN("The model reconstructed, but its GPS scale could not be fitted -- it is in its own units, not metres. The log line above says which check refused it."),
    JA("モデルは再構成できましたが、GPS による寸法を当てはめられませんでした。メートルではなく独自の単位のままです。どの検査で退けられたかは上のログ行にあります。"),
    ZH_HANS("模型重建成功，但没能拟合出 GPS 尺度——它仍是自身单位，而不是米。上面的日志行说明是哪一项检查拒绝了它。"),
    ZH_HANT("模型重建成功，但沒能擬合出 GPS 尺度——它仍是自身單位，而不是公尺。上面的日誌行說明是哪一項檢查拒絕了它。"),
    KO("모델은 재구성되었지만 GPS 로 크기를 맞추지 못했습니다. 미터가 아니라 자체 단위 그대로입니다. 어떤 검사에서 막혔는지는 위 기록 줄에 있습니다."),
    DE("Rekonstruiert, aber ohne GPS-Maßstab: eigene Einheiten statt Meter. Die Protokollzeile oben nennt die fehlgeschlagene Prüfung."),
    FR("Modèle reconstruit sans échelle GPS : unités propres, pas des mètres. Le journal ci-dessus indique le contrôle ayant échoué."),
    ES("Modelo reconstruido sin escala GPS: usa unidades propias, no metros. El registro de arriba indica qué comprobación falló."),
    PT("Modelo reconstruído sem escala GPS: usa unidades próprias, não metros. O registo acima indica a verificação que falhou."),
    IT("Il modello è stato ricostruito, ma la sua scala da GPS non si è potuta stimare: è nelle sue unità, non in metri. La riga di log qui sopra dice quale controllo l'ha rifiutata."),
    NL("Het model is gereconstrueerd, maar de GPS-schaal kon niet worden gefit -- het staat in eigen eenheden, niet in meters. De logregel hierboven noemt de controle die het weigerde."),
    RU("Модель восстановлена, но масштаб по GPS подобрать не удалось — она в своих единицах, а не в метрах. В строке журнала выше сказано, какая проверка его отклонила."),
    TR("Model yeniden oluşturuldu ama GPS ölçeği oturtulamadı -- metre değil, kendi biriminde. Yukarıdaki günlük satırı hangi denetimin reddettiğini söyler."));

// ===========================================================================
// Inputs and output folder
// ===========================================================================

SS_MSG(browse,
    EN("Browse..."),     JA("参照…"),         ZH_HANS("浏览…"),    ZH_HANT("瀏覽…"),
    KO("찾아보기…"),      DE("Durchsuchen …"), FR("Parcourir…"),  ES("Examinar…"),
    PT("Procurar…"),     IT("Sfoglia…"),     NL("Bladeren…"),    RU("Обзор…"),
    TR("Gözat…"));

SS_MSG(remove,
    EN("Remove"),        JA("削除"),          ZH_HANS("移除"),     ZH_HANT("移除"),
    KO("제거"),           DE("Entfernen"),    FR("Retirer"),      ES("Quitar"),
    PT("Remover"),       IT("Rimuovi"),      NL("Verwijderen"),  RU("Убрать"),
    TR("Kaldır"));

SS_MSG(add_video,
    EN("Add video..."),  JA("動画を追加…"),   ZH_HANS("添加视频…"), ZH_HANT("新增影片…"),
    KO("동영상 추가…"),   DE("Video hinzufügen …"), FR("Ajouter une vidéo…"),
    ES("Añadir un vídeo…"), PT("Adicionar um vídeo…"), IT("Aggiungi un video…"),
    NL("Video toevoegen…"), RU("Добавить видео…"), TR("Video ekle…"));

SS_MSG(add_video_help,
    EN("Adds clips to one scene, each with its own frames folder and camera. Select several files in the dialog or drop them here."),
    JA("各クリップを同じシーンに追加し、フレームフォルダとカメラを個別に使います。ダイアログで複数選択するか、ここにドロップしてください。"),
    ZH_HANS("将视频添加到同一场景，各自使用帧文件夹和相机。可在对话框中多选文件，或拖到此处。"),
    ZH_HANT("將影片新增至同一場景，各自使用影格資料夾和相機。可在對話框中多選檔案，或拖到此處。"),
    KO("클립들을 한 장면에 추가하며 각각 프레임 폴더와 카메라를 사용합니다. 대화 상자에서 여러 파일을 고르거나 여기에 놓으세요."),
    DE("Fügt Clips zu einer Szene hinzu, jeweils mit eigenem Bildordner und eigener Kamera. Mehrere Dateien im Dialog wählen oder hier ablegen."),
    FR("Ajoute des clips à une même scène, chacun avec son dossier et sa caméra. Sélectionnez plusieurs fichiers ou déposez-les ici."),
    ES("Añade clips a una escena, cada uno con su carpeta de fotogramas y cámara. Selecciona varios archivos o arrástralos aquí."),
    PT("Adiciona clipes à mesma cena, cada um com pasta de fotogramas e câmara próprias. Selecione vários ficheiros ou largue-os aqui."),
    IT("Aggiunge clip a una scena, ciascuna con cartella fotogrammi e camera proprie. Selezionare più file o trascinarli qui."),
    NL("Voegt clips toe aan één scène, elk met een eigen beeldmap en camera. Selecteer meerdere bestanden of sleep ze hierheen."),
    RU("Добавляет клипы в одну сцену, каждому — своя папка кадров и камера. Выберите несколько файлов или перетащите их сюда."),
    TR("Klipleri tek sahneye ekler; her birinin kare klasörü ve kamerası ayrıdır. Birden fazla dosya seçin veya buraya bırakın."));

SS_MSG(add_photos,
    EN("Add photos..."), JA("写真を追加…"),   ZH_HANS("添加照片…"), ZH_HANT("新增相片…"),
    KO("사진 추가…"),     DE("Fotos hinzufügen …"), FR("Ajouter des photos…"),
    ES("Añadir fotos…"), PT("Adicionar fotos…"), IT("Aggiungi fotografie…"),
    NL("Foto's toevoegen…"), RU("Добавить фотографии…"), TR("Fotoğraf ekle…"));

SS_MSG(add_photos_help,
    EN("Adds a photo folder. A sole input is read in place; with other inputs, its images are linked into the dataset."),
    JA("写真フォルダを追加します。入力が1つならその場で読み、複数ならデータセット内に画像をリンクします。"),
    ZH_HANS("添加照片文件夹。唯一输入直接读取原位置；有其他输入时，将照片链接到数据集。"),
    ZH_HANT("新增照片資料夾。唯一輸入直接讀取原位置；有其他輸入時，將照片連結到資料集。"),
    KO("사진 폴더를 추가합니다. 단독 입력이면 원래 위치에서 읽고, 다른 입력과 함께라면 데이터셋에 이미지를 연결합니다."),
    DE("Fügt einen Fotoordner hinzu. Allein wird er direkt gelesen; mit weiteren Eingaben werden die Bilder in den Datensatz verlinkt."),
    FR("Ajoute un dossier photo. Seul, il est lu sur place ; avec d’autres entrées, ses images sont liées au jeu de données."),
    ES("Añade una carpeta de fotos. Sola se lee donde está; con otras entradas, sus imágenes se enlazan al dataset."),
    PT("Adiciona uma pasta de fotos. Sozinha é lida no local; com outras entradas, as imagens são ligadas ao conjunto de dados."),
    IT("Aggiunge una cartella foto. Da sola è letta sul posto; con altri ingressi, le immagini sono collegate nel dataset."),
    NL("Voegt een fotomap toe. Alleen wordt die ter plaatse gelezen; met andere invoer worden beelden in de dataset gelinkt."),
    RU("Добавляет папку фото. Единственный вход читается на месте; с другими входами снимки связываются с набором данных."),
    TR("Fotoğraf klasörü ekler. Tek girdiyse yerinde okunur; başka girdilerle birlikteyse resimler veri kümesine bağlanır."));

SS_MSG(add_dataset,
    EN("Add dataset..."), JA("データセットを追加…"), ZH_HANS("添加数据集…"),
    ZH_HANT("新增資料集…"), KO("데이터셋 추가…"),
    DE("Datensatz hinzufügen …"), FR("Ajouter un jeu de données…"),
    ES("Añadir un conjunto de datos…"), PT("Adicionar um conjunto de dados…"),
    IT("Aggiungi un set di dati…"), NL("Dataset toevoegen…"),
    RU("Добавить набор данных…"), TR("Veri kümesi ekle…"));

SS_MSG(add_dataset_help,
    EN("Adds an existing COLMAP, Nerfstudio, Metashape or Spirula reconstruction. Reuses its folder and cameras; adds masks, depth and normals."),
    JA("COLMAP、Nerfstudio、Metashape、Spirulaの既存再構成を追加します。フォルダとカメラを再利用し、マスク・深度・法線を追加します。"),
    ZH_HANS("添加已有的 COLMAP、Nerfstudio、Metashape 或 Spirula 重建。复用其文件夹和相机，添加蒙版、深度及法线。"),
    ZH_HANT("新增已有的 COLMAP、Nerfstudio、Metashape 或 Spirula 重建。重用其資料夾和相機，新增遮罩、深度及法線。"),
    KO("기존 COLMAP, Nerfstudio, Metashape 또는 Spirula 재구성을 추가합니다. 폴더와 카메라를 재사용하고 마스크·깊이·법선을 추가합니다."),
    DE("Fügt eine COLMAP-, Nerfstudio-, Metashape- oder Spirula-Rekonstruktion hinzu. Nutzt Ordner und Kameras weiter; ergänzt Masken, Tiefe und Normalen."),
    FR("Ajoute une reconstruction COLMAP, Nerfstudio, Metashape ou Spirula. Réutilise dossier et caméras ; ajoute masques, profondeur et normales."),
    ES("Añade una reconstrucción COLMAP, Nerfstudio, Metashape o Spirula. Reutiliza carpeta y cámaras; añade máscaras, profundidad y normales."),
    PT("Adiciona uma reconstrução COLMAP, Nerfstudio, Metashape ou Spirula. Reutiliza pasta e câmaras; acrescenta máscaras, profundidade e normais."),
    IT("Aggiunge una ricostruzione COLMAP, Nerfstudio, Metashape o Spirula. Riusa cartella e camere; aggiunge maschere, profondità e normali."),
    NL("Voegt een COLMAP-, Nerfstudio-, Metashape- of Spirula-reconstructie toe. Hergebruikt map en camera’s; voegt maskers, diepte en normalen toe."),
    RU("Добавляет реконструкцию COLMAP, Nerfstudio, Metashape или Spirula. Использует её папку и камеры; добавляет маски, глубину и нормали."),
    TR("Mevcut COLMAP, Nerfstudio, Metashape veya Spirula modeli ekler. Klasör ve kameraları kullanır; maske, derinlik ve normal ekler."));

SS_MSG(no_input_yet,
    EN("no input picked yet"),
    JA("入力がまだ選ばれていません"),
    ZH_HANS("还没有选择输入"),
    ZH_HANT("還沒有選擇輸入"),
    KO("아직 입력을 고르지 않았습니다"),
    DE("noch keine Eingabe gewählt"),
    FR("aucune entrée choisie pour l'instant"),
    ES("aún no se ha elegido ninguna entrada"),
    PT("nenhuma entrada escolhida ainda"),
    IT("nessun ingresso scelto finora"),
    NL("nog geen invoer gekozen"),
    RU("вход ещё не выбран"),
    TR("henüz girdi seçilmedi"));

SS_MSG(kind_video_file,
    EN("video file"),    JA("動画ファイル"),   ZH_HANS("视频文件"),  ZH_HANT("影片檔"),
    KO("동영상 파일"),    DE("Videodatei"),   FR("fichier vidéo"), ES("archivo de vídeo"),
    PT("arquivo de vídeo"), IT("file video"), NL("videobestand"),
    RU("видеофайл"),     TR("video dosyası"));

SS_MSG(kind_photo_folder,
    EN("photo folder"),  JA("写真フォルダ"),   ZH_HANS("照片文件夹"), ZH_HANT("相片資料夾"),
    KO("사진 폴더"),      DE("Fotoordner"),   FR("dossier de photos"),
    ES("carpeta de fotos"), PT("pasta de fotos"), IT("cartella di fotografie"),
    NL("fotomap"),       RU("папка с фотографиями"), TR("fotoğraf klasörü"));

SS_MSG(kind_video_file_masks,
    EN("video file + masks"),
    JA("動画ファイル＋マスク"),
    ZH_HANS("视频文件 + 蒙版"),
    ZH_HANT("影片檔 + 遮罩"),
    KO("동영상 파일 + 마스크"),
    DE("Videodatei + Masken"),
    FR("fichier vidéo + masques"),
    ES("archivo de vídeo + máscaras"),
    PT("arquivo de vídeo + máscaras"),
    IT("file video + maschere"),
    NL("videobestand + maskers"),
    RU("видеофайл + маски"),
    TR("video dosyası + maskeler"));

SS_MSG(kind_photo_folder_masks,
    EN("photo folder + masks"),
    JA("写真フォルダ＋マスク"),
    ZH_HANS("照片文件夹 + 蒙版"),
    ZH_HANT("相片資料夾 + 遮罩"),
    KO("사진 폴더 + 마스크"),
    DE("Fotoordner + Masken"),
    FR("dossier de photos + masques"),
    ES("carpeta de fotos + máscaras"),
    PT("pasta de fotos + máscaras"),
    IT("cartella di fotografie + maschere"),
    NL("fotomap + maskers"),
    RU("папка с фотографиями + маски"),
    TR("fotoğraf klasörü + maskeler"));

// {0} is a folder name under images/.
SS_MSG(row_video_to,
    EN("video -> images/{0}"),       JA("動画 → images/{0}"),
    ZH_HANS("视频 → images/{0}"),     ZH_HANT("影片 → images/{0}"),
    KO("동영상 → images/{0}"),        DE("Video -> images/{0}"),
    FR("vidéo -> images/{0}"),       ES("vídeo -> images/{0}"),
    PT("vídeo -> images/{0}"),       IT("video -> images/{0}"),
    NL("video -> images/{0}"),       RU("видео -> images/{0}"),
    TR("video -> images/{0}"));

SS_MSG(row_photos_to,
    EN("photos -> images/{0}"),      JA("写真 → images/{0}"),
    ZH_HANS("照片 → images/{0}"),     ZH_HANT("相片 → images/{0}"),
    KO("사진 → images/{0}"),          DE("Fotos -> images/{0}"),
    FR("photos -> images/{0}"),      ES("fotos -> images/{0}"),
    PT("fotos -> images/{0}"),       IT("fotografie -> images/{0}"),
    NL("foto's -> images/{0}"),      RU("фотографии -> images/{0}"),
    TR("fotoğraflar -> images/{0}"));

SS_MSG(row_video_masks_to,
    EN("video + masks -> images/{0}"),   JA("動画＋マスク → images/{0}"),
    ZH_HANS("视频 + 蒙版 → images/{0}"),  ZH_HANT("影片 + 遮罩 → images/{0}"),
    KO("동영상 + 마스크 → images/{0}"),   DE("Video + Masken -> images/{0}"),
    FR("vidéo + masques -> images/{0}"), ES("vídeo + máscaras -> images/{0}"),
    PT("vídeo + máscaras -> images/{0}"), IT("video + maschere -> images/{0}"),
    NL("video + maskers -> images/{0}"), RU("видео + маски -> images/{0}"),
    TR("video + maskeler -> images/{0}"));

SS_MSG(row_photos_masks_to,
    EN("photos + masks -> images/{0}"),  JA("写真＋マスク → images/{0}"),
    ZH_HANS("照片 + 蒙版 → images/{0}"),  ZH_HANT("相片 + 遮罩 → images/{0}"),
    KO("사진 + 마스크 → images/{0}"),     DE("Fotos + Masken -> images/{0}"),
    FR("photos + masques -> images/{0}"), ES("fotos + máscaras -> images/{0}"),
    PT("fotos + máscaras -> images/{0}"), IT("fotografie + maschere -> images/{0}"),
    NL("foto's + maskers -> images/{0}"), RU("фотографии + маски -> images/{0}"),
    TR("fotoğraflar + maskeler -> images/{0}"));

SS_MSG(existing_masks_tooltip,
    EN("Uses existing masks without segmentation: {0}"),
    JA("既存マスクを使い、分割処理はしません：{0}"),
    ZH_HANS("使用已有蒙版，不再分割：{0}"),
    ZH_HANT("使用已有遮罩，不再分割：{0}"),
    KO("기존 마스크를 사용하며 분할은 하지 않습니다: {0}"),
    DE("Nutzt vorhandene Masken ohne Segmentierung: {0}"),
    FR("Utilise les masques existants sans segmentation : {0}"),
    ES("Usa máscaras existentes sin segmentar: {0}"),
    PT("Usa máscaras existentes sem segmentar: {0}"),
    IT("Usa le maschere esistenti senza segmentare: {0}"),
    NL("Gebruikt bestaande maskers zonder segmentatie: {0}"),
    RU("Использует готовые маски без сегментации: {0}"),
    TR("Bölütleme yapmadan mevcut maskeleri kullanır: {0}"));

SS_MSG(output_folder,
    EN("output dataset folder"),
    JA("出力先のデータセットフォルダ"),
    ZH_HANS("输出数据集文件夹"),
    ZH_HANT("輸出資料集資料夾"),
    KO("출력 데이터셋 폴더"),
    DE("Ausgabeordner des Datensatzes"),
    FR("dossier du jeu de données de sortie"),
    ES("carpeta del conjunto de datos de salida"),
    PT("pasta do conjunto de dados de saída"),
    IT("cartella del set di dati in uscita"),
    NL("uitvoermap van de dataset"),
    RU("папка выходного набора данных"),
    TR("çıktı veri kümesi klasörü"));

SS_MSG(resume_previous,
    EN("Resume previous run"),
    JA("前回の実行を再開する"),
    ZH_HANS("继续上次的运行"),
    ZH_HANT("繼續上次的執行"),
    KO("이전 실행 이어서 하기"),
    DE("Vorherigen Lauf fortsetzen"),
    FR("Reprendre l'exécution précédente"),
    ES("Reanudar la ejecución anterior"),
    PT("Retomar a execução anterior"),
    IT("Riprendi l'esecuzione precedente"),
    NL("Vorige run hervatten"),
    RU("Продолжить прошлый запуск"),
    TR("Önceki çalıştırmayı sürdür"));

SS_MSG(resume_previous_help,
    EN("Reuses completed frames, masks, features and matches; runs only missing work.\nOff requires a fresh output folder. Nothing is deleted automatically."),
    JA("完了したフレーム、マスク、特徴点、照合を再利用し、未完了の処理だけ実行します。\nオフでは新しい出力フォルダが必要です。自動削除はしません。"),
    ZH_HANS("复用已完成的帧、蒙版、特征及匹配，只运行缺失的工作。\n关闭则需全新输出文件夹，不会自动删除内容。"),
    ZH_HANT("重用已完成的影格、遮罩、特徵及匹配，只執行缺失的工作。\n關閉則需全新輸出資料夾，不會自動刪除內容。"),
    KO("완료된 프레임·마스크·특징·매칭을 재사용하고 누락된 작업만 실행합니다。\n끄면 새 출력 폴더가 필요합니다. 자동 삭제는 하지 않습니다。"),
    DE("Nutzt fertige Bilder, Masken, Merkmale und Zuordnungen; führt nur Fehlendes aus.\nAus erfordert einen frischen Ausgabeordner. Nichts wird automatisch gelöscht."),
    FR("Réutilise images, masques, caractéristiques et paires terminés ; fait seulement ce qui manque.\nDésactivé exige un dossier de sortie neuf. Aucune suppression automatique."),
    ES("Reutiliza fotogramas, máscaras, características y pares terminados; solo hace lo pendiente.\nDesactivado exige una carpeta de salida nueva. No borra nada automáticamente."),
    PT("Reutiliza fotogramas, máscaras, características e pares prontos; só faz o que falta.\nDesligado exige uma pasta de saída nova. Nada é apagado automaticamente."),
    IT("Riusa fotogrammi, maschere, caratteristiche e coppie completati; esegue solo ciò che manca.\nSpento richiede una cartella di uscita nuova. Nessuna eliminazione automatica."),
    NL("Hergebruikt voltooide beelden, maskers, kenmerken en paren; doet alleen wat ontbreekt.\nUit vereist een nieuwe uitvoermap. Niets wordt automatisch verwijderd."),
    RU("Использует готовые кадры, маски, признаки и пары; делает лишь недостающее.\nВыкл. требует новую выходную папку. Ничто не удаляется автоматически."),
    TR("Hazır kare, maske, özellik ve çiftleri kullanır; yalnız eksik işleri yapar.\nKapalıyken yeni çıktı klasörü gerekir. Hiçbir şey otomatik silinmez."));

SS_MSG(unfinished_run_detected,
    EN("(unfinished run detected in this folder)"),
    JA("（このフォルダに未完了の実行があります）"),
    ZH_HANS("（在这个文件夹里发现了未完成的运行）"),
    ZH_HANT("（在這個資料夾裡發現了未完成的執行）"),
    KO("(이 폴더에서 끝나지 않은 실행을 찾았습니다)"),
    DE("(unvollständiger Lauf in diesem Ordner gefunden)"),
    FR("(exécution inachevée trouvée dans ce dossier)"),
    ES("(se encontró una ejecución sin terminar en esta carpeta)"),
    PT("(execução inacabada encontrada nesta pasta)"),
    IT("(trovata un'esecuzione incompleta in questa cartella)"),
    NL("(onafgemaakte run in deze map gevonden)"),
    RU("(в этой папке найден незавершённый запуск)"),
    TR("(bu klasörde yarım kalmış bir çalıştırma bulundu)"));

// IRREVERSIBLE -- human review in every language.
// ===========================================================================
// Reconstruction engine
// ===========================================================================

SS_MSG(reconstruction,
    EN("Reconstruction:"), JA("再構成:"),   ZH_HANS("重建："),   ZH_HANT("重建："),
    KO("재구성:"),         DE("Rekonstruktion:"), FR("Reconstruction :"),
    ES("Reconstrucción:"), PT("Reconstrução:"), IT("Ricostruzione:"),
    NL("Reconstructie:"), RU("Реконструкция:"), TR("Yeniden oluşturma:"));

SS_MSG(engine_builtin,
    EN("Built-in (GPU)"), JA("内蔵（GPU）"),  ZH_HANS("内置（GPU）"), ZH_HANT("內建（GPU）"),
    KO("내장(GPU)"),      DE("Eingebaut (GPU)"), FR("Intégrée (GPU)"),
    ES("Integrada (GPU)"), PT("Integrada (GPU)"), IT("Integrata (GPU)"),
    NL("Ingebouwd (GPU)"), RU("Встроенная (GPU)"), TR("Yerleşik (GPU)"));

SS_MSG(engine_builtin_help,
    EN("This program's own structure-from-motion. Nothing to install, runs on "
       "the GPU."),
    JA("このプログラム自身の Structure from Motion です。インストール不要で、"
       "GPU 上で動きます。"),
    ZH_HANS("本程序自带的运动恢复结构。无需安装，在 GPU 上运行。"),
    ZH_HANT("本程式自帶的運動恢復結構。無需安裝，在 GPU 上執行。"),
    KO("이 프로그램에 들어 있는 Structure from Motion입니다. 따로 설치할 것 "
       "없이 GPU에서 돌아갑니다."),
    DE("Die eigene Structure-from-Motion dieses Programms. Nichts zu "
       "installieren, läuft auf der GPU."),
    FR("Le structure-from-motion intégré à ce programme. Rien à installer, "
       "tourne sur le GPU."),
    ES("El structure-from-motion propio de este programa. Nada que instalar, "
       "funciona en la GPU."),
    PT("O structure-from-motion do próprio programa. Nada a instalar, roda na "
       "GPU."),
    IT("Lo structure-from-motion integrato in questo programma. Niente da "
       "installare, gira sulla GPU."),
    NL("De eigen structure-from-motion van dit programma. Niets te "
       "installeren, draait op de GPU."),
    RU("Собственная реализация structure-from-motion в этой программе. Ставить "
       "ничего не нужно, работает на GPU."),
    TR("Bu programın kendi structure-from-motion'ı. Kurulacak bir şey yok, "
       "GPU üzerinde çalışır."));

SS_MSG(engine_colmap,
    EN("COLMAP (installed separately)"),
    JA("COLMAP（別途インストール）"),
    ZH_HANS("COLMAP（需另行安装）"),
    ZH_HANT("COLMAP（需另行安裝）"),
    KO("COLMAP(따로 설치)"),
    DE("COLMAP (separat installiert)"),
    FR("COLMAP (installé séparément)"),
    ES("COLMAP (instalado aparte)"),
    PT("COLMAP (instalado à parte)"),
    IT("COLMAP (installato a parte)"),
    NL("COLMAP (apart geïnstalleerd)"),
    RU("COLMAP (устанавливается отдельно)"),
    TR("COLMAP (ayrıca kurulur)"));

SS_MSG(engine_colmap_help,
    EN("Drive an external COLMAP instead. Worth having for comparison."),
    JA("代わりに外部の COLMAP を動かします。比較用として役立ちます。"),
    ZH_HANS("改为驱动外部的 COLMAP。留着它可以做对比。"),
    ZH_HANT("改為驅動外部的 COLMAP。留著它可以做對比。"),
    KO("대신 외부 COLMAP을 실행합니다. 비교용으로 쓸모가 있습니다."),
    DE("Stattdessen ein externes COLMAP steuern. Nützlich zum Vergleich."),
    FR("Piloter un COLMAP externe à la place. Utile pour comparer."),
    ES("Controlar un COLMAP externo en su lugar. Útil para comparar."),
    PT("Controlar um COLMAP externo em vez disso. Útil para comparar."),
    IT("Pilotare invece un COLMAP esterno. Utile per confrontare."),
    NL("In plaats daarvan een externe COLMAP aansturen. Nuttig om te "
       "vergelijken."),
    RU("Вместо этого запускать внешний COLMAP. Пригодится для сравнения."),
    TR("Onun yerine harici bir COLMAP çalıştırın. Karşılaştırma için işe "
       "yarar."));

// ===========================================================================
// The basics
// ===========================================================================

SS_MSG(quality,
    EN("Quality"),       JA("品質"),          ZH_HANS("质量"),     ZH_HANT("品質"),
    KO("품질"),           DE("Qualität"),     FR("Qualité"),      ES("Calidad"),
    PT("Qualidade"),     IT("Qualità"),      NL("Kwaliteit"),    RU("Качество"),
    TR("Kalite"));

SS_MSG(quality_fast,
    EN("Fast"),          JA("高速"),          ZH_HANS("快速"),     ZH_HANT("快速"),
    KO("빠름"),           DE("Schnell"),      FR("Rapide"),       ES("Rápida"),
    PT("Rápida"),        IT("Veloce"),       NL("Snel"),         RU("Быстро"),
    TR("Hızlı"));

SS_MSG(quality_balanced,
    EN("Balanced"),      JA("バランス"),      ZH_HANS("均衡"),     ZH_HANT("均衡"),
    KO("균형"),           DE("Ausgewogen"),   FR("Équilibrée"),   ES("Equilibrada"),
    PT("Equilibrada"),   IT("Bilanciata"),   NL("Gebalanceerd"), RU("Сбалансированно"),
    TR("Dengeli"));

SS_MSG(quality_high_recommended,
    EN("High (recommended)"),
    JA("高（推奨）"),     ZH_HANS("高（推荐）"), ZH_HANT("高（建議）"),
    KO("높음(권장)"),     DE("Hoch (empfohlen)"), FR("Élevée (recommandée)"),
    ES("Alta (recomendada)"), PT("Alta (recomendada)"), IT("Alta (consigliata)"),
    NL("Hoog (aanbevolen)"), RU("Высокое (рекомендуется)"), TR("Yüksek (önerilen)"));

SS_MSG(quality_maximum,
    EN("Maximum"),       JA("最大"),          ZH_HANS("最高"),     ZH_HANT("最高"),
    KO("최대"),           DE("Maximal"),      FR("Maximale"),     ES("Máxima"),
    PT("Máxima"),        IT("Massima"),      NL("Maximaal"),     RU("Максимальное"),
    TR("En yüksek"));

SS_MSG(quality_high,
    EN("High quality"),  JA("高品質"),        ZH_HANS("高质量"),   ZH_HANT("高品質"),
    KO("고품질"),         DE("Hohe Qualität"), FR("Haute qualité"),
    ES("Alta calidad"),  PT("Alta qualidade"), IT("Alta qualità"),
    NL("Hoge kwaliteit"), RU("Высокое качество"), TR("Yüksek kalite"));

SS_MSG(quality_help_builtin,
    EN("Working resolution, how many features are found per image, and how many image pairs are compared. Higher finds more cameras in difficult scenes and takes longer."),
    JA("作業解像度、1枚あたりに検出する特徴の数、比較する画像ペアの数をまとめて決めます。上げるほど難しいシーンでもカメラが見つかりますが、時間は長くなります。"),
    ZH_HANS("工作分辨率、每张图像找多少特征，以及比较多少对图像。调高在困难场景里能找到更多相机，但更耗时。"),
    ZH_HANT("工作解析度、每張影像找多少特徵，以及比較多少對影像。調高在困難場景裡能找到更多相機，但更耗時。"),
    KO("작업 해상도, 이미지당 찾는 특징점 수, 비교하는 이미지 쌍의 수를 함께 정합니다. 높일수록 어려운 장면에서도 카메라를 더 많이 찾지만 시간이 더 걸립니다."),
    DE("Arbeitsauflösung, wie viele Merkmale je Bild gefunden werden und wie viele Bildpaare verglichen werden. Höher findet in schwierigen Szenen mehr Kameras und dauert länger."),
    FR("Résolution de travail, nombre de points caractéristiques par image et nombre de paires d'images comparées.\nPlus haut trouve plus de caméras dans les scènes difficiles et prend plus de temps."),
    ES("Resolución de trabajo, cuántas características se buscan por imagen y cuántos pares de imágenes se comparan. Más alto encuentra más cámaras en escenas difíciles y tarda más."),
    PT("Define resolução de trabalho, características por imagem e pares comparados. Maior encontra mais câmaras em cenas difíceis e demora mais."),
    IT("Risoluzione di lavoro, quante caratteristiche si cercano per immagine e quante coppie di immagini si confrontano.\nPiù alto trova più fotocamere nelle scene difficili e richiede più tempo."),
    NL("Werkresolutie, hoeveel kenmerken per beeld worden gevonden en hoeveel beeldparen worden vergeleken. Hoger vindt meer camera's in lastige scènes en duurt langer."),
    RU("Рабочее разрешение, сколько особых точек ищется на снимке и сколько пар снимков сравнивается. Выше — больше найденных камер в сложных сценах и дольше."),
    TR("Çalışma çözünürlüğü, görüntü başına kaç öznitelik bulunacağı ve kaç görüntü çiftinin karşılaştırılacağı. Yüksek olan zor sahnelerde daha çok kamera bulur ve daha uzun sürer."));

SS_MSG(quality_help_colmap,
    EN("Feature count used for matching (4k / 8k / 16k). Higher finds more cameras in difficult scenes but matching is O(n^2) in feature count."),
    JA("マッチングに使う特徴の数です（4k / 8k / 16k）。増やすほど難しいシーンでもカメラが見つかりますが、マッチングの計算量は特徴数の2乗です。"),
    ZH_HANS("用于匹配的特征数量（4k / 8k / 16k）。调高在困难场景里能找到更多相机，但匹配的复杂度是特征数的平方。"),
    ZH_HANT("用於配對的特徵數量（4k / 8k / 16k）。調高在困難場景裡能找到更多相機，但配對的複雜度是特徵數的平方。"),
    KO("매칭에 쓰는 특징점 수입니다(4k / 8k / 16k). 높이면 어려운 장면에서 카메라를 더 찾지만 매칭 비용은 특징점 수의 제곱입니다."),
    DE("Zahl der Merkmale für die Zuordnung (4k / 8k / 16k). Höher findet in schwierigen Szenen mehr Kameras, doch die Zuordnung wächst quadratisch mit der Merkmalszahl."),
    FR("Caractéristiques pour l’appariement (4k / 8k / 16k). Plus haut trouve plus de caméras difficiles, mais le coût est quadratique en nombre de points."),
    ES("Características para emparejar (4k / 8k / 16k). Más encuentra más cámaras en escenas difíciles, pero el coste crece al cuadrado con las características."),
    PT("Número de características usadas na correspondência (4k / 8k / 16k). Mais alto encontra mais câmeras em cenas difíceis, mas a correspondência é O(n²) no número de características."),
    IT("Numero di caratteristiche usate per la corrispondenza (4k / 8k / 16k).\nPiù alto trova più fotocamere nelle scene difficili, ma la corrispondenza è O(n²) nel numero di caratteristiche."),
    NL("Aantal kenmerken voor het matchen (4k / 8k / 16k). Hoger vindt meer camera's in lastige scènes, maar matchen is O(n²) in het aantal kenmerken."),
    RU("Число особых точек для сопоставления (4k / 8k / 16k). Больше — больше найденных камер в сложных сценах, но сопоставление растёт как квадрат числа точек."),
    TR("Eşleştirmede kullanılan öznitelik sayısı (4k / 8k / 16k). Yüksek olan zor sahnelerde daha çok kamera bulur ama eşleştirme öznitelik sayısına göre O(n²)'dir."));

SS_MSG(camera_lens,
    EN("Camera / lens"), JA("カメラ／レンズ"), ZH_HANS("相机／镜头"), ZH_HANT("相機／鏡頭"),
    KO("카메라 / 렌즈"), DE("Kamera / Objektiv"), FR("Caméra / objectif"),
    ES("Cámara / objetivo"), PT("Câmera / lente"), IT("Fotocamera / obiettivo"),
    NL("Camera / lens"), RU("Камера / объектив"), TR("Kamera / objektif"));

SS_MSG(camera_lens_help,
    EN("OpenCV suits most cameras. Use a fisheye model for fisheye or 360 rigs; this is not detected automatically. Pinhole requires undistorted images."),
    JA("通常はOpenCVを使います。魚眼や360度リグには魚眼モデルを選んでください。自動検出はしません。ピンホールは歪み補正済み画像用です。"),
    ZH_HANS("多数相机适用 OpenCV。鱼眼或 360 度相机组须选鱼眼模型，不会自动识别。针孔模型仅适用于已去畸变图像。"),
    ZH_HANT("多數相機適用 OpenCV。魚眼或 360 度相機組須選魚眼模型，不會自動識別。針孔模型僅適用於已去畸變影像。"),
    KO("대부분의 카메라는 OpenCV가 적합합니다. 어안이나 360 리그는 어안 모델을 선택하세요. 자동 감지되지 않습니다. 핀홀은 왜곡 보정된 이미지용입니다."),
    DE("OpenCV passt meist. Für Fischaugen oder 360°-Rigs ein Fischaugenmodell wählen; keine automatische Erkennung. Pinhole braucht entzerrte Bilder."),
    FR("OpenCV convient généralement. Choisir un modèle fisheye pour les fisheyes ou rigs 360° : pas de détection automatique. Pinhole exige des images corrigées."),
    ES("OpenCV sirve para casi todas. Usa un modelo ojo de pez para lentes así o rigs 360°; no se detecta solo. Pinhole exige imágenes sin distorsión."),
    PT("OpenCV serve para quase todas. Use um modelo olho de peixe para essas lentes ou rigs 360°; não há deteção automática. Pinhole exige imagens corrigidas."),
    IT("OpenCV va bene di solito. Per fisheye o rig 360° scegliere un modello fisheye; non è rilevato automaticamente. Pinhole richiede immagini senza distorsione."),
    NL("OpenCV past meestal. Kies voor fisheye of 360°-rigs een fisheyemodel; dit wordt niet automatisch herkend. Pinhole vereist beelden zonder vervorming."),
    RU("OpenCV подходит большинству камер. Для рыбьего глаза и 360° выберите модель fisheye: автоопределения нет. Pinhole требует снимки без дисторсии."),
    TR("OpenCV çoğu kameraya uyar. Balıkgözü veya 360° düzeneklerde balıkgözü modeli seçin; otomatik algılanmaz. Pinhole, bozulması giderilmiş görüntüler ister."));

SS_MSG(colmap_one_lens_warning,
    EN("COLMAP fits this one lens model to every input. Switch to the "
       "built-in reconstruction to give each input its own."),
    JA("COLMAP はこの1つのレンズモデルをすべての入力に当てはめます。入力ごとに"
       "別のモデルを使うには、内蔵の再構成に切り替えてください。"),
    ZH_HANS("COLMAP 会把这一个镜头模型套用到所有输入上。想让每个输入各用各的，"
            "请切换到内置重建。"),
    ZH_HANT("COLMAP 會把這一個鏡頭模型套用到所有輸入上。想讓每個輸入各用各的，"
            "請切換到內建重建。"),
    KO("COLMAP은 이 렌즈 모델 하나를 모든 입력에 적용합니다. 입력마다 따로 "
       "쓰려면 내장 재구성으로 바꾸세요."),
    DE("COLMAP legt dieses eine Objektivmodell über jede Eingabe. Für ein "
       "eigenes Modell je Eingabe zur eingebauten Rekonstruktion wechseln."),
    FR("COLMAP applique ce seul modèle d'objectif à toutes les entrées. "
       "Passez à la reconstruction intégrée pour en donner un à chacune."),
    ES("COLMAP aplica este único modelo de objetivo a todas las entradas. "
       "Cambie a la reconstrucción integrada para dar uno propio a cada una."),
    PT("O COLMAP aplica este único modelo de lente a todas as entradas. Mude "
       "para a reconstrução integrada para dar um a cada uma."),
    IT("COLMAP applica questo unico modello di obiettivo a tutti gli "
       "ingressi. Passi alla ricostruzione integrata per darne uno a "
       "ciascuno."),
    NL("COLMAP past dit ene lensmodel op elke invoer toe. Schakel over op de "
       "ingebouwde reconstructie om elke invoer een eigen model te geven."),
    RU("COLMAP применяет одну эту модель объектива ко всем входам. Чтобы у "
       "каждого входа была своя, переключитесь на встроенную реконструкцию."),
    TR("COLMAP bu tek objektif modelini bütün girdilere uygular. Her girdiye "
       "kendi modelini vermek için yerleşik yeniden oluşturmaya geçin."));

SS_MSG(camera_lens_per_input,
    EN("Camera / lens per input"),
    JA("入力ごとのカメラ／レンズ"),
    ZH_HANS("每个输入的相机／镜头"),
    ZH_HANT("每個輸入的相機／鏡頭"),
    KO("입력별 카메라 / 렌즈"),
    DE("Kamera / Objektiv je Eingabe"),
    FR("Caméra / objectif par entrée"),
    ES("Cámara / objetivo por entrada"),
    PT("Câmera / lente por entrada"),
    IT("Fotocamera / obiettivo per ingresso"),
    NL("Camera / lens per invoer"),
    RU("Камера / объектив для каждого входа"),
    TR("Girdi başına kamera / objektif"));

SS_MSG(camera_lens_per_input_help,
    EN("Each input has its own lens model and focal length. A 360 file’s two lens tracks share this row, but reconstruct as separate cameras."),
    JA("入力ごとにレンズモデルと焦点距離を指定します。360度ファイルの2つのレンズはこの行を共有しますが、別々のカメラとして再構成します。"),
    ZH_HANS("每个输入可设独立镜头模型和焦距。360 文件的双镜头轨道共用此行，但作为独立相机重建。"),
    ZH_HANT("每個輸入可設獨立鏡頭模型和焦距。360 檔案的雙鏡頭軌道共用此列，但作為獨立相機重建。"),
    KO("입력마다 렌즈 모델과 초점 거리를 정합니다. 360 파일의 두 렌즈 트랙은 이 행을 공유하지만 별도 카메라로 재구성됩니다."),
    DE("Jede Eingabe hat Objektivmodell und Brennweite. Die zwei Objektivspuren einer 360°-Datei teilen diese Zeile, werden aber getrennte Kameras."),
    FR("Chaque entrée a son modèle d’objectif et sa focale. Les deux pistes d’un fichier 360° partagent cette ligne, mais restent deux caméras."),
    ES("Cada entrada tiene modelo y focal propios. Las dos pistas de lentes de un archivo 360° comparten fila, pero se reconstruyen como cámaras separadas."),
    PT("Cada entrada tem modelo e focal próprios. As duas pistas de lentes de um ficheiro 360° partilham a linha, mas são reconstruídas como câmaras separadas."),
    IT("Ogni ingresso ha modello e focale propri. Le due tracce ottiche di un file 360° condividono la riga, ma sono ricostruite come camere distinte."),
    NL("Elke invoer heeft een eigen lensmodel en brandpuntsafstand. Twee lenssporen van een 360°-bestand delen deze rij, maar worden aparte camera’s."),
    RU("Каждый вход имеет свою модель объектива и фокус. Два трека линз файла 360° делят эту строку, но реконструируются как отдельные камеры."),
    TR("Her girdinin mercek modeli ve odağı ayrıdır. 360° dosyasının iki mercek izi bu satırı paylaşır, ancak ayrı kameralar olarak oluşturulur."));

SS_MSG(focal_x_width,
    EN("x width"),       JA("×幅"),          ZH_HANS("× 宽度"),   ZH_HANT("× 寬度"),
    KO("× 너비"),         DE("× Breite"),     FR("× largeur"),    ES("× ancho"),
    PT("× largura"),     IT("× larghezza"),  NL("× breedte"),    RU("× ширина"),
    TR("× genişlik"));

SS_MSG(focal_x_width_help,
    EN("Starting focal length: fx = fy = factor × image width. 0 uses EXIF or an image-size estimate. Set it for fisheye lenses if reconstruction fails to start."),
    JA("初期焦点距離：fx = fy = 係数 × 画像幅。0はEXIFか画像サイズから推定します。魚眼で再構成を開始できない場合に設定してください。"),
    ZH_HANS("初始焦距：fx = fy = 系数 × 图像宽度。0 使用 EXIF 或按图像尺寸估计。鱼眼镜头无法开始重建时可手动设置。"),
    ZH_HANT("初始焦距：fx = fy = 係數 × 影像寬度。0 使用 EXIF 或按影像尺寸估計。魚眼鏡頭無法開始重建時可手動設定。"),
    KO("초기 초점 거리: fx = fy = 계수 × 이미지 너비. 0은 EXIF나 이미지 크기로 추정합니다. 어안 렌즈에서 재구성이 시작되지 않으면 설정하세요."),
    DE("Anfangsbrennweite: fx = fy = Faktor × Bildbreite. 0 nutzt EXIF oder schätzt aus der Bildgröße. Bei Startproblemen mit Fischaugen einstellen."),
    FR("Focale initiale : fx = fy = facteur × largeur. 0 utilise EXIF ou estime selon la taille. À régler si une reconstruction fisheye ne démarre pas."),
    ES("Focal inicial: fx = fy = factor × ancho de imagen. 0 usa EXIF o estima según el tamaño. Ajústala si la reconstrucción con ojo de pez no arranca."),
    PT("Focal inicial: fx = fy = fator × largura da imagem. 0 usa EXIF ou estima pelo tamanho. Ajuste se a reconstrução com olho de peixe não arrancar."),
    IT("Focale iniziale: fx = fy = fattore × larghezza immagine. 0 usa EXIF o una stima dalla dimensione. Impostarla se il fisheye non avvia la ricostruzione."),
    NL("Beginbrandpuntsafstand: fx = fy = factor × beeldbreedte. 0 gebruikt EXIF of een schatting uit de beeldgrootte. Stel in als fisheye-reconstructie niet start."),
    RU("Начальный фокус: fx = fy = коэффициент × ширина снимка. 0 берёт EXIF или оценку по размеру. Задайте, если реконструкция с рыбьим глазом не начинается."),
    TR("Başlangıç odağı: fx = fy = katsayı × görüntü genişliği. 0, EXIF veya boyut tahmini kullanır. Balıkgözünde yeniden oluşturma başlamıyorsa ayarlayın."));

SS_MSG(camera_sharing,
    EN("Camera sharing"), JA("カメラの共有"),  ZH_HANS("相机共享"),  ZH_HANT("相機共用"),
    KO("카메라 공유"),    DE("Kamera teilen"), FR("Partage de caméra"),
    ES("Cámara compartida"), PT("Compartilhamento de câmera"),
    IT("Condivisione fotocamera"), NL("Camera delen"), RU("Общая камера"),
    TR("Kamera paylaşımı"));

SS_MSG(camera_sharing_one,
    EN("One shared camera"),
    JA("共有カメラ1台"), ZH_HANS("共用一台相机"), ZH_HANT("共用一台相機"),
    KO("공유 카메라 하나"), DE("Eine gemeinsame Kamera"), FR("Une caméra partagée"),
    ES("Una cámara compartida"), PT("Uma câmera compartilhada"),
    IT("Una fotocamera condivisa"), NL("Één gedeelde camera"),
    RU("Одна общая камера"), TR("Tek ortak kamera"));

SS_MSG(camera_sharing_folder,
    EN("One camera per folder"),
    JA("フォルダごとに1台"), ZH_HANS("每个文件夹一台相机"), ZH_HANT("每個資料夾一台相機"),
    KO("폴더마다 카메라 하나"), DE("Eine Kamera je Ordner"),
    FR("Une caméra par dossier"), ES("Una cámara por carpeta"),
    PT("Uma câmera por pasta"), IT("Una fotocamera per cartella"),
    NL("Één camera per map"), RU("По камере на папку"),
    TR("Klasör başına bir kamera"));

SS_MSG(camera_sharing_image,
    EN("One camera per image"),
    JA("画像ごとに1台"), ZH_HANS("每张图像一台相机"), ZH_HANT("每張影像一台相機"),
    KO("이미지마다 카메라 하나"), DE("Eine Kamera je Bild"),
    FR("Une caméra par image"), ES("Una cámara por imagen"),
    PT("Uma câmera por imagem"), IT("Una fotocamera per immagine"),
    NL("Één camera per beeld"), RU("По камере на снимок"),
    TR("Görüntü başına bir kamera"));

SS_MSG(camera_sharing_help,
    EN("- Shared: one camera at one zoom.\n- Per folder: one camera per folder; automatic for multi-track 360 video.\n- Per image: changing zoom or focus."),
    JA("- 共通：同じカメラとズーム。\n- フォルダ別：各フォルダに1台。複数トラックの360度動画では自動。\n- 画像別：ズームやフォーカスが変化。"),
    ZH_HANS("- 共享：同一相机、固定变焦。\n- 每文件夹：每个文件夹一个相机；多轨 360 视频自动采用。\n- 每图像：变焦或对焦有变化。"),
    ZH_HANT("- 共用：同一相機、固定變焦。\n- 每資料夾：每個資料夾一個相機；多軌 360 影片自動採用。\n- 每影像：變焦或對焦有變化。"),
    KO("- 공유: 같은 카메라와 줌.\n- 폴더별: 폴더마다 카메라 하나. 다중 트랙 360 영상은 자동.\n- 이미지별: 줌이나 초점이 변할 때."),
    DE("- Geteilt: eine Kamera, ein Zoom.\n- Pro Ordner: eine Kamera je Ordner; automatisch bei Mehrspur-360°-Video.\n- Pro Bild: wechselnder Zoom oder Fokus."),
    FR("- Partagé : une caméra, un zoom.\n- Par dossier : une caméra par dossier ; automatique en vidéo 360° multipiste.\n- Par image : zoom ou mise au point variable."),
    ES("- Compartida: una cámara y un zoom.\n- Por carpeta: una cámara por carpeta; automático en vídeo 360° multipista.\n- Por imagen: zoom o enfoque variable."),
    PT("- Partilhada: uma câmara e um zoom.\n- Por pasta: uma câmara por pasta; automático em vídeo 360° com várias pistas.\n- Por imagem: zoom ou foco variável."),
    IT("- Condivisa: una camera e uno zoom.\n- Per cartella: una camera per cartella; automatico per video 360° multitraccia.\n- Per immagine: zoom o fuoco variabile."),
    NL("- Gedeeld: één camera en zoom.\n- Per map: één camera per map; automatisch voor 360°-video met meerdere sporen.\n- Per beeld: wisselende zoom of focus."),
    RU("- Общие: одна камера и зум.\n- По папкам: камера на папку; автоматически для многотрекового видео 360°.\n- По кадрам: меняющийся зум или фокус."),
    TR("- Paylaşılan: tek kamera ve yakınlaştırma.\n- Klasör başına: bir kamera; çok izli 360° videoda otomatik.\n- Resim başına: değişen yakınlaştırma veya odak."));

SS_MSG(image_matching,
    EN("Image matching"), JA("画像のマッチング"), ZH_HANS("图像匹配"),  ZH_HANT("影像配對"),
    KO("이미지 매칭"),    DE("Bildzuordnung"), FR("Appariement d'images"),
    ES("Emparejamiento de imágenes"), PT("Correspondência de imagens"),
    IT("Corrispondenza tra immagini"), NL("Beeldmatching"),
    RU("Сопоставление снимков"), TR("Görüntü eşleştirme"));

SS_MSG(matching_automatic,
    EN("Automatic"),     JA("自動"),          ZH_HANS("自动"),     ZH_HANT("自動"),
    KO("자동"),           DE("Automatisch"),  FR("Automatique"),  ES("Automático"),
    PT("Automático"),    IT("Automatico"),   NL("Automatisch"),  RU("Автоматически"),
    TR("Otomatik"));

SS_MSG(matching_every_pair,
    EN("Every pair (best, slowest)"),
    JA("すべての組み合わせ（最良・最遅）"),
    ZH_HANS("所有配对（最好、最慢）"),
    ZH_HANT("所有配對（最好、最慢）"),
    KO("모든 쌍(가장 좋고 가장 느림)"),
    DE("Jedes Paar (bestes, langsamstes)"),
    FR("Toutes les paires (meilleur, plus lent)"),
    ES("Todos los pares (mejor, más lento)"),
    PT("Todos os pares (melhor, mais lento)"),
    IT("Tutte le coppie (migliore, più lento)"),
    NL("Elk paar (beste, traagste)"),
    RU("Все пары (лучше всего, медленнее всего)"),
    TR("Her çift (en iyi, en yavaş)"));

SS_MSG(matching_neighbours,
    EN("Neighbouring frames (video)"),
    JA("隣り合うフレーム（動画）"),
    ZH_HANS("相邻帧（视频）"),
    ZH_HANT("相鄰影格（影片）"),
    KO("이웃 프레임(동영상)"),
    DE("Benachbarte Bilder (Video)"),
    FR("Images voisines (vidéo)"),
    ES("Fotogramas vecinos (vídeo)"),
    PT("Quadros vizinhos (vídeo)"),
    IT("Fotogrammi vicini (video)"),
    NL("Naburige beelden (video)"),
    RU("Соседние кадры (видео)"),
    TR("Komşu kareler (video)"));

SS_MSG(matching_gpu_preselect,
    EN("GPU pre-selection (large captures)"),
    JA("GPU による事前選択（大規模な撮影）"),
    ZH_HANS("GPU 预筛选（大规模拍摄）"),
    ZH_HANT("GPU 預篩選（大規模拍攝）"),
    KO("GPU 사전 선별(대규모 촬영)"),
    DE("GPU-Vorauswahl (große Aufnahmen)"),
    FR("Présélection GPU (grandes prises)"),
    ES("Preselección en GPU (capturas grandes)"),
    PT("Pré-seleção na GPU (capturas grandes)"),
    IT("Preselezione su GPU (riprese grandi)"),
    NL("GPU-voorselectie (grote opnamen)"),
    RU("Предварительный отбор на GPU (крупные съёмки)"),
    TR("GPU ön seçimi (büyük çekimler)"));

SS_MSG(matching_help_builtin,
    EN("Image-pair strategy. Automatic: neighbors for short video, every pair below 100 images, GPU pre-selection for larger sets."),
    JA("画像ペアの方式です。自動は短い動画で隣接、100枚未満で全ペア、それ以上でGPU事前選択を使います。"),
    ZH_HANS("图像对策略。自动：短视频匹配相邻帧，少于 100 张匹配所有对，更大数据集用 GPU 预选。"),
    ZH_HANT("影像對策略。自動：短影片匹配相鄰影格，少於 100 張匹配所有對，更大資料集用 GPU 預選。"),
    KO("이미지 쌍 전략입니다. 자동: 짧은 영상은 이웃, 100장 미만은 모든 쌍, 더 큰 데이터셋은 GPU 사전 선택을 씁니다."),
    DE("Bildpaarstrategie. Automatisch: Nachbarn bei kurzen Videos, alle Paare unter 100 Bildern, sonst GPU-Vorauswahl."),
    FR("Stratégie de paires. Automatique : voisins pour les vidéos courtes, toutes les paires sous 100 images, puis présélection GPU."),
    ES("Estrategia de pares. Automático: vecinos en vídeos cortos, todos los pares con menos de 100 imágenes, luego preselección GPU."),
    PT("Estratégia de pares. Automático: vizinhos em vídeos curtos, todos os pares abaixo de 100 imagens, depois pré-seleção GPU."),
    IT("Strategia delle coppie. Automatico: vicini per video brevi, tutte le coppie sotto 100 immagini, poi preselezione GPU."),
    NL("Beeldpaarstrategie. Automatisch: buren bij korte video, alle paren onder 100 beelden, anders GPU-voorselectie."),
    RU("Стратегия пар. Авто: соседи для короткого видео, все пары до 100 снимков, затем предварительный отбор GPU."),
    TR("Resim çifti stratejisi. Otomatik: kısa videoda komşular, 100’den az resimde tüm çiftler, daha büyük kümelerde GPU ön seçimi."));

SS_MSG(matching_exhaustive,
    EN("Exhaustive"),    JA("総当たり"),      ZH_HANS("穷举"),     ZH_HANT("窮舉"),
    KO("전수"),           DE("Vollständig"),  FR("Exhaustif"),    ES("Exhaustivo"),
    PT("Exaustivo"),     IT("Esaustivo"),    NL("Uitputtend"),   RU("Полный перебор"),
    TR("Kapsamlı"));

SS_MSG(matching_sequential,
    EN("Sequential"),    JA("逐次"),          ZH_HANS("顺序"),     ZH_HANT("循序"),
    KO("순차"),           DE("Sequenziell"),  FR("Séquentiel"),   ES("Secuencial"),
    PT("Sequencial"),    IT("Sequenziale"),  NL("Sequentieel"),  RU("Последовательное"),
    TR("Sıralı"));

SS_MSG(matching_vocab_tree,
    EN("Vocabulary tree"), JA("ボキャブラリツリー"), ZH_HANS("词汇树"), ZH_HANT("詞彙樹"),
    KO("어휘 트리"),      DE("Vokabularbaum"), FR("Arbre de vocabulaire"),
    ES("Árbol de vocabulario"), PT("Árvore de vocabulário"),
    IT("Albero di vocabolario"), NL("Vocabulaireboom"),
    RU("Словарное дерево"), TR("Sözcük ağacı"));

SS_MSG(matching_help_colmap,
    EN("How image pairs are matched. Exhaustive tries every pair (best quality, fine up to a few hundred images).\nSequential matches temporal neighbors (video). Vocabulary tree scales to thousands of unordered photos."),
    JA("画像ペアのマッチング方法です。\n- 総当たりはすべての組み合わせを試します（品質は最良で、数百枚までなら問題ありません）。\n- 逐次は時間的に隣り合うフレームを照合します（動画向け）。 ボキャブラリツリーは順不同の数千枚規模まで対応できます。"),
    ZH_HANS("图像配对的匹配方式。穷举会试遍所有配对（质量最好，几百张以内没问题）。顺序匹配时间上相邻的帧（视频用）。词汇树可以扩展到数千张无序照片。"),
    ZH_HANT("影像配對的比對方式。窮舉會試遍所有配對（品質最好，幾百張以內沒問題）。循序比對時間上相鄰的影格（影片用）。詞彙樹可以擴展到數千張無序相片。"),
    KO("전수: 모든 쌍을 매칭해 품질이 높고 수백 장까지 적합합니다. 순차: 동영상의 이웃 프레임. 어휘 트리: 순서 없는 수천 장에 적합합니다."),
    DE("Wie Bildpaare zugeordnet werden.\n- Vollständig probiert jedes Paar (beste Qualität, bis zu einigen hundert Bildern unproblematisch).\n- Sequenziell ordnet zeitliche Nachbarn zu (Video). Der Vokabularbaum skaliert auf Tausende ungeordneter Fotos."),
    FR("Comment les paires d'images sont appariées.\n- Exhaustif essaie chaque paire (meilleure qualité, sans problème jusqu'à quelques centaines d'images).\n- Séquentiel apparie les voisins temporels (vidéo). L'arbre de vocabulaire monte à des milliers de photos sans ordre."),
    ES("Cómo se emparejan los pares de imágenes.\n- Exhaustivo prueba todos los pares (mejor calidad, sin problema hasta unos cientos de imágenes).\n- Secuencial empareja vecinos temporales (vídeo). El árbol de vocabulario escala a miles de fotos sin orden."),
    PT("Como os pares de imagens são correspondidos.\n- Exaustivo tenta todos os pares (melhor qualidade, tranquilo até algumas centenas de imagens).\n- Sequencial casa vizinhos temporais (vídeo). A árvore de vocabulário escala para milhares de fotos sem ordem."),
    IT("Come vengono messe in corrispondenza le coppie di immagini.\n- Esaustivo prova ogni coppia (qualità migliore, tranquillo fino a qualche centinaio di immagini).\n- Sequenziale abbina i vicini temporali (video). L'albero di vocabolario arriva a migliaia di foto senza ordine."),
    NL("Hoe beeldparen worden gematcht.\n- Uitputtend probeert elk paar (beste kwaliteit, tot enkele honderden beelden prima).\n- Sequentieel matcht tijdelijke buren (video). De vocabulaireboom schaalt naar duizenden ongeordende foto's."),
    RU("Как сопоставляются пары снимков.\n- Полный перебор пробует все пары (лучшее качество, до нескольких сотен снимков вполне нормально).\n- Последовательное сопоставляет соседей по времени (видео). Словарное дерево тянет тысячи неупорядоченных фотографий."),
    TR("Görüntü çiftlerinin nasıl eşleştirileceği.\n- Kapsamlı her çifti dener (en iyi kalite, birkaç yüz görüntüye kadar sorunsuz).\n- Sıralı zamansal komşuları eşleştirir (video). Sözcük ağacı sırasız binlerce fotoğrafa ölçeklenir."));

SS_MSG(loop_closure,
    EN("Loop closure detection"),
    JA("ループ閉じ込みの検出"),
    ZH_HANS("回环检测"),
    ZH_HANT("迴環偵測"),
    KO("루프 클로저 검출"),
    DE("Schleifenschluss erkennen"),
    FR("Détection de fermeture de boucle"),
    ES("Detección de cierre de bucle"),
    PT("Detecção de fechamento de laço"),
    IT("Rilevamento della chiusura del giro"),
    NL("Lusdetectie"),
    RU("Обнаружение замыкания петли"),
    TR("Çevrim kapanışı algılama"));

SS_MSG(loop_closure_help_colmap,
    EN("Matches visually similar non-neighbor frames with a vocabulary tree to close loops on revisits. SIFT only."),
    JA("語彙木で似た非隣接フレームを照合し、同じ場所への再訪でループを閉じます。SIFT専用です。"),
    ZH_HANS("通过词汇树匹配外观相似的非相邻帧，在重访时闭合回环。仅支持 SIFT。"),
    ZH_HANT("透過詞彙樹匹配外觀相似的非相鄰影格，在重訪時閉合回環。僅支援 SIFT。"),
    KO("어휘 트리로 비슷한 비인접 프레임을 매칭해 재방문 시 루프를 닫습니다. SIFT 전용입니다."),
    DE("Gleicht ähnliche, nicht benachbarte Bilder per Vokabularbaum ab, um Schleifen bei Wiederbesuchen zu schließen. Nur SIFT."),
    FR("Apparie les images similaires non voisines par arbre de vocabulaire pour fermer les boucles lors des revisites. SIFT uniquement."),
    ES("Empareja fotogramas similares no vecinos con un árbol de vocabulario para cerrar bucles al volver a un lugar. Solo SIFT."),
    PT("Combina fotogramas semelhantes não vizinhos com uma árvore de vocabulário para fechar voltas ao revisitar locais. Só SIFT."),
    IT("Abbina fotogrammi simili non vicini con un albero del vocabolario per chiudere anelli nelle rivisite. Solo SIFT."),
    NL("Koppelt gelijkende niet-naburige beelden via een vocabulaireboom om lussen bij terugkeer te sluiten. Alleen SIFT."),
    RU("Сопоставляет похожие несоседние кадры деревом слов, замыкая петли при возврате. Только SIFT."),
    TR("Sözlük ağacıyla benzer ama komşu olmayan kareleri eşleştirip aynı yere dönüşte döngüleri kapatır. Yalnızca SIFT."));

SS_MSG(loop_closure_help_builtin,
    EN("Matches similar non-neighbor frames to close loops. On by default, roughly doubles matching time. Automatic only uses it below 100 frames."),
    JA("似た非隣接フレームを照合してループを閉じます。既定で有効、照合時間は約2倍です。自動では100フレーム未満でのみ使います。"),
    ZH_HANS("匹配相似的非相邻帧以闭合回环。默认开启，匹配耗时约翻倍。自动模式仅在少于 100 帧时使用。"),
    ZH_HANT("匹配相似的非相鄰影格以閉合回環。預設開啟，匹配耗時約翻倍。自動模式僅在少於 100 影格時使用。"),
    KO("비슷한 비인접 프레임을 매칭해 루프를 닫습니다. 기본으로 켜며 매칭 시간은 약 두 배입니다. 자동은 100프레임 미만에서만 씁니다."),
    DE("Gleicht ähnliche Nicht-Nachbarn ab, um Schleifen zu schließen. Standardmäßig an, etwa doppelte Zuordnungszeit. Automatisch nur unter 100 Bildern."),
    FR("Apparie les images similaires non voisines pour fermer les boucles. Activé par défaut, temps d’appariement ~doublé. Automatique sous 100 images seulement."),
    ES("Empareja fotogramas similares no vecinos para cerrar bucles. Activado por defecto, tarda ~el doble. Automático solo con menos de 100 fotogramas."),
    PT("Combina fotogramas semelhantes não vizinhos para fechar voltas. Ligado por padrão, demora ~o dobro. Automático só abaixo de 100 fotogramas."),
    IT("Abbina fotogrammi simili non vicini per chiudere anelli. Attivo per default, tempo circa doppio. Automatico solo sotto 100 fotogrammi."),
    NL("Koppelt gelijkende niet-buren om lussen te sluiten. Standaard aan, ongeveer dubbele koppeltijd. Automatisch alleen onder 100 beelden."),
    RU("Сопоставляет похожие несоседние кадры для замыкания петель. По умолчанию включено, время ~вдвое. Авто лишь до 100 кадров."),
    TR("Döngü kapatmak için benzer komşu olmayan kareleri eşleştirir. Varsayılan açık, süre yaklaşık iki katıdır. Otomatik yalnız 100 kare altında kullanır."));

SS_MSG(frames_per_second_help,
    EN("How many frames to keep per second of video.\n- 1-3 is right for a slow walkthrough; more only helps if the camera moved fast.\n- A video with a rate of its own uses that instead. 0 keeps every frame."),
    JA("動画1秒あたり何フレーム残すかです。\n- ゆっくり歩いて撮ったなら 1〜3 が適切で、それ以上が効くのはカメラが速く動いたときだけです。\n- 個別の値を入れた動画はそちらに従います。 0 にするとすべてのフレームを残します。"),
    ZH_HANS("每秒视频保留多少帧。慢慢走着拍的话 1-3 就合适；更高只有在相机移动很快时才有用。单独设了帧率的视频按各自的来。设为 0 则保留每一帧。"),
    ZH_HANT("每秒影片保留多少影格。慢慢走著拍的話 1-3 就合適；更高只有在相機移動很快時才有用。單獨設了影格率的影片按各自的來。設為 0 則保留每一影格。"),
    KO("초당 남길 프레임 수입니다. 느린 보행 촬영은 1~3, 빠른 이동은 더 높게 설정하세요. 동영상별 설정이 우선합니다. 0은 모든 프레임을 남깁니다."),
    DE("Wie viele Bilder je Sekunde Video behalten werden.\n- 1-3 passt für einen langsamen Rundgang; mehr hilft nur, wenn die Kamera schnell bewegt wurde.\n- Ein Video mit eigener Rate nimmt seine eigene. 0 behält jedes Bild."),
    FR("Combien d'images conserver par seconde de vidéo.\n- 1 à 3 convient à une déambulation lente ; davantage n'aide que si la caméra bougeait vite.\n- Une vidéo ayant son propre débit garde le sien. 0 conserve toutes les images."),
    ES("Cuántos fotogramas conservar por segundo de vídeo.\n- De 1 a 3 va bien para un recorrido lento; más solo ayuda si la cámara se movía rápido.\n- Un vídeo con su propia tasa usa la suya. 0 conserva todos los fotogramas."),
    PT("Quantos quadros manter por segundo de vídeo.\n- De 1 a 3 serve para um percurso lento; mais só ajuda se a câmera se moveu rápido.\n- Um vídeo com taxa própria usa a dele. 0 mantém todos os quadros."),
    IT("Quanti fotogrammi tenere per ogni secondo di video.\n- Da 1 a 3 va bene per una camminata lenta; di più serve solo se la fotocamera si muoveva in fretta.\n- Un video con una frequenza propria usa la sua. 0 tiene tutti i fotogrammi."),
    NL("Hoeveel beelden per seconde video bewaard blijven.\n- 1-3 past bij een rustige rondgang; meer helpt alleen als de camera snel bewoog.\n- Een video met een eigen tempo houdt dat van zichzelf. 0 bewaart elk beeld."),
    RU("Сколько кадров оставлять на секунду видео.\n- 1-3 подходит для неторопливого обхода; больше помогает, только если камера двигалась быстро.\n- Видео со своей частотой берёт свою. 0 оставляет все кадры."),
    TR("Videonun her saniyesinden kaç karenin tutulacağı.\n- Yavaş bir gezinti için 1-3 uygundur; daha fazlası yalnızca kamera hızlı hareket ettiyse işe yarar.\n- Kendi hızı olan video kendininkini kullanır. 0 her kareyi tutar."));

SS_MSG(frames_per_second_help_adaptive,
    EN("Average frames per second, spaced by view changes. Per-video rates override this. 0 keeps every frame and disables motion spacing."),
    JA("視点の変化に応じて配置する平均フレーム数です。動画ごとの値を優先します。0は全フレームを残し、動きによる間隔調整を無効にします。"),
    ZH_HANS("按视图变化分配的平均每秒帧数。各视频的帧率优先。0 保留所有帧，不再按运动调整间隔。"),
    ZH_HANT("按視圖變化分配的平均每秒影格數。各影片的影格率優先。0 保留所有影格，不再按運動調整間隔。"),
    KO("시점 변화에 따라 배치하는 초당 평균 프레임 수입니다. 동영상별 설정이 우선합니다. 0은 모두 남겨 움직임 간격 조정을 끕니다."),
    DE("Mittlere Bilder pro Sekunde, nach Ansichtsänderung verteilt. Videowerte gehen vor. 0 behält alle Bilder und deaktiviert Bewegungsabstände."),
    FR("Images par seconde en moyenne, espacées selon les changements de vue. Les valeurs par vidéo priment. 0 garde tout, sans espacement par mouvement."),
    ES("Fotogramas por segundo de media, según cambios de vista. La tasa por vídeo tiene prioridad. 0 conserva todos, sin espaciado por movimiento."),
    PT("Fotogramas por segundo em média, conforme mudanças de vista. A taxa por vídeo tem prioridade. 0 guarda todos, sem espaçamento por movimento."),
    IT("Fotogrammi medi al secondo, distribuiti per variazione di vista. Prevale il valore per video. 0 tiene tutto, senza spaziatura per movimento."),
    NL("Gemiddelde beelden per seconde, verdeeld naar beeldverandering. Videowaarden gaan voor. 0 bewaart alles zonder spreiding op beweging."),
    RU("Среднее число кадров в секунду с учётом смены вида. Значение для видео приоритетнее. 0 сохраняет все кадры без интервалов по движению."),
    TR("Görüş değişimine göre aralıklı, saniyelik ortalama kare sayısı. Video ayarları önceliklidir. 0 hepsini tutar ve hareket aralığını kapatır."));

SS_MSG(video_lut_none,
    EN("No LUT"), JA("LUT なし"), ZH_HANS("无 LUT"), ZH_HANT("無 LUT"),
    KO("LUT 없음"), DE("Keine LUT"), FR("Aucune LUT"), ES("Sin LUT"),
    PT("Sem LUT"), IT("Nessuna LUT"), NL("Geen LUT"), RU("Без LUT"),
    TR("LUT yok"));

SS_MSG(video_lut_choose,
    EN("Choose a .cube file..."), JA(".cube ファイルを選択..."),
    ZH_HANS("选择 .cube 文件…"), ZH_HANT("選擇 .cube 檔案…"),
    KO(".cube 파일 선택..."), DE(".cube-Datei wählen..."),
    FR("Choisir un fichier .cube..."), ES("Elegir un archivo .cube..."),
    PT("Escolher um arquivo .cube..."), IT("Scegli un file .cube..."),
    NL("Een .cube-bestand kiezen..."), RU("Выбрать файл .cube..."),
    TR(".cube dosyası seç..."));

SS_MSG(video_lut_pick_title,
    EN("Choose a colour LUT (.cube)"), JA("カラー LUT（.cube）を選択"),
    ZH_HANS("选择颜色 LUT（.cube）"), ZH_HANT("選擇色彩 LUT（.cube）"),
    KO("색상 LUT(.cube) 선택"), DE("Farb-LUT (.cube) wählen"),
    FR("Choisir une LUT couleur (.cube)"), ES("Elegir una LUT de color (.cube)"),
    PT("Escolher uma LUT de cor (.cube)"), IT("Scegli una LUT colore (.cube)"),
    NL("Een kleur-LUT (.cube) kiezen"), RU("Выбрать цветовую LUT (.cube)"),
    TR("Renk LUT'u seç (.cube)"));

SS_MSG(video_lut_auto,
    EN("Automatic"), JA("自動"), ZH_HANS("自动"), ZH_HANT("自動"), KO("자동"),
    DE("Automatisch"), FR("Automatique"), ES("Automático"), PT("Automático"),
    IT("Automatico"), NL("Automatisch"), RU("Автоматически"), TR("Otomatik"));

SS_MSG(video_lut_auto_help,
    EN("The LUT the clip's own metadata names, when it was shot in a log profile "
       "Spirula knows; otherwise the choice made on a row above, or none."),
    JA("Spirula が知っているログプロファイルで撮影されたクリップなら、そのメタデータが"
       "示す LUT。そうでなければ上の行での選択、それもなければ LUT なし。"),
    ZH_HANS("片段若以 Spirula 认识的 Log 配置拍摄，就用其元数据所指的 LUT；否则沿用"
            "上面某一行的选择，都没有则不套用。"),
    ZH_HANT("片段若以 Spirula 認得的 Log 設定檔拍攝，就用其中繼資料所指的 LUT；否則"
            "沿用上面某一行的選擇，都沒有則不套用。"),
    KO("Spirula가 아는 로그 프로필로 촬영된 클립이면 메타데이터가 가리키는 LUT, "
       "아니면 위 행에서 고른 것, 그것도 없으면 LUT 없음."),
    DE("Die LUT, die die Metadaten des Clips nennen, wenn er in einem Log-Profil "
       "gedreht wurde, das Spirula kennt; sonst die Wahl einer Zeile darüber, oder keine."),
    FR("La LUT que nomment les métadonnées du clip, s'il a été tourné dans un profil "
       "log que Spirula connaît ; sinon le choix fait sur une ligne au-dessus, ou aucune."),
    ES("La LUT que indican los metadatos del clip, si se grabó en un perfil log que "
       "Spirula conoce; si no, la elegida en una fila de arriba, o ninguna."),
    PT("A LUT indicada pelos metadados do clipe, se foi gravado num perfil log que o "
       "Spirula conhece; senão, a escolhida numa linha acima, ou nenhuma."),
    IT("La LUT indicata dai metadati del clip, se è stato girato in un profilo log "
       "che Spirula conosce; altrimenti la scelta fatta in una riga sopra, o nessuna."),
    NL("De LUT die de metadata van de clip noemt, als die is opgenomen in een "
       "log-profiel dat Spirula kent; anders de keuze van een rij erboven, of geen."),
    RU("LUT, указанная в метаданных клипа, если он снят в известном Spirula "
       "лог-профиле; иначе выбор из строки выше или никакой."),
    TR("Klip Spirula'nın bildiği bir log profiliyle çekildiyse meta verisinin "
       "belirttiği LUT; değilse yukarıdaki bir satırda yapılan seçim, o da yoksa hiçbiri."));

SS_MSG(video_lut_help,
    EN("Bakes a log-to-Rec.709 3D LUT into extracted frames.\nBuilt-in approximations or a .cube file. Now: {0}"),
    JA("抽出フレームにlog→Rec.709の3D LUTを適用します。\n内蔵の近似LUTか.cubeファイルを使います。現在：{0}"),
    ZH_HANS("将 log→Rec.709 的 3D LUT 烘焙到提取帧。\n使用内置近似 LUT 或 .cube 文件。当前：{0}"),
    ZH_HANT("將 log→Rec.709 的 3D LUT 烘焙到擷取影格。\n使用內建近似 LUT 或 .cube 檔案。目前：{0}"),
    KO("추출 프레임에 log→Rec.709 3D LUT를 적용합니다.\n내장 근사 LUT 또는 .cube 파일입니다. 현재: {0}"),
    DE("Bäckt eine Log-zu-Rec.709-3D-LUT in extrahierte Bilder ein.\nEingebaute Näherungen oder .cube-Datei. Aktuell: {0}"),
    FR("Applique une LUT 3D log vers Rec.709 aux images extraites.\nApproximations intégrées ou fichier .cube. Actuel : {0}"),
    ES("Aplica una LUT 3D de log a Rec.709 a los fotogramas extraídos.\nAproximaciones integradas o archivo .cube. Actual: {0}"),
    PT("Aplica uma LUT 3D de log a Rec.709 aos fotogramas extraídos.\nAproximações integradas ou ficheiro .cube. Atual: {0}"),
    IT("Applica una LUT 3D da log a Rec.709 ai fotogrammi estratti.\nApprossimazioni integrate o file .cube. Attuale: {0}"),
    NL("Past een log-naar-Rec.709-3D-LUT toe op uitgepakte beelden.\nIngebouwde benaderingen of .cube-bestand. Nu: {0}"),
    RU("Применяет 3D LUT log→Rec.709 к извлечённым кадрам.\nВстроенные приближения или файл .cube. Сейчас: {0}"),
    TR("Çıkarılan karelere log→Rec.709 3D LUT uygular.\nYerleşik yaklaşımlar veya .cube dosyası. Şimdi: {0}"));

SS_MSG(video_fps_this_one_help,
    EN("Frames per second for this video alone. \"^\" is following the video "
       "above it; type that rate back in to go back to following. "
       "0 keeps every frame of it."),
    JA("この動画だけの毎秒フレーム数です。「^」は上の動画に従っている印で、"
       "上と同じ値を入れ直すとまた従います。"
       "0 にするとこの動画のすべてのフレームを残します。"),
    ZH_HANS("仅用于这个视频的每秒帧数。「^」表示跟随上面那个视频；改回上面的值"
            "就重新跟随。"
            "设为 0 则保留它的每一帧。"),
    ZH_HANT("僅用於這個影片的每秒影格數。「^」表示跟隨上面那個影片；改回上面的"
            "值就重新跟隨。"
            "設為 0 則保留它的每一影格。"),
    KO("이 동영상에만 적용되는 초당 프레임 수입니다. \"^\"는 위 동영상을 따르고 "
       "있다는 뜻이며, 그 값을 다시 입력하면 다시 따릅니다. "
       "0이면 이 동영상의 모든 프레임을 남깁니다."),
    DE("Bilder je Sekunde nur für dieses Video. \"^\" heißt, es folgt dem Video "
       "darüber; die Rate wieder eintragen, und es folgt erneut. "
       "0 behält jedes seiner Bilder."),
    FR("Images par seconde pour cette vidéo seule. « ^ » signifie qu'elle suit "
       "la vidéo au-dessus ; retapez ce débit pour qu'elle la suive à nouveau. "
       "0 en conserve toutes les images."),
    ES("Fotogramas por segundo solo para este vídeo. «^» es que sigue al vídeo "
       "de arriba; vuelve a escribir esa tasa para que lo siga otra vez. "
       "0 conserva todos sus fotogramas."),
    PT("Quadros por segundo só para este vídeo. \"^\" é seguir o vídeo acima; "
       "escreva essa taxa outra vez para voltar a segui-lo. "
       "0 mantém todos os quadros dele."),
    IT("Fotogrammi al secondo solo per questo video. \"^\" vuol dire che segue "
       "il video qui sopra; riscrivi quella frequenza e torna a seguirlo. "
       "0 ne tiene tutti i fotogrammi."),
    NL("Beelden per seconde alleen voor deze video. \"^\" is de video hierboven "
       "volgen; typ dat tempo terug om weer te volgen. "
       "0 bewaart er elk beeld van."),
    RU("Кадров в секунду только для этого видео. «^» значит, что оно следует за "
       "видео выше; введите ту же частоту, чтобы снова следовать. "
       "0 оставляет все его кадры."),
    TR("Yalnızca bu video için saniyedeki kare sayısı. \"^\", üstündeki videoyu "
       "izlediği anlamına gelir; o hızı yeniden yazınca yine izler. "
       "0 onun her karesini tutar."));

SS_MSG(adaptive_fps,
    EN("Adapt the rate to the motion"),
    JA("動きに合わせてレートを変える"),
    ZH_HANS("按运动调整帧率"),
    ZH_HANT("依運動調整影格率"),
    KO("움직임에 맞춰 속도 조절"),
    DE("Rate an die Bewegung anpassen"),
    FR("Adapter le débit au mouvement"),
    ES("Adaptar la tasa al movimiento"),
    PT("Adaptar a taxa ao movimento"),
    IT("Adatta la frequenza al movimento"),
    NL("Tempo aanpassen aan de beweging"),
    RU("Подстраивать частоту под движение"),
    TR("Hızı harekete göre ayarla"));

SS_MSG(adaptive_fps_help,
    EN("Keeps more frames for fast or close motion, fewer for rotation or distant views. The rate becomes an average. Adds one video pass."),
    JA("速い移動や近景では多く、回転や遠景では少なくフレームを残します。指定レートは平均になります。動画を1回追加で読みます。"),
    ZH_HANS("快速移动或近景多留帧，原地旋转或远景少留帧。指定帧率作为平均值；增加一次视频扫描。"),
    ZH_HANT("快速移動或近景多留影格，原地旋轉或遠景少留影格。指定影格率作為平均值；增加一次影片掃描。"),
    KO("빠른 이동·가까운 장면은 더 많이, 회전·먼 장면은 적게 남깁니다. 지정 속도는 평균이 됩니다. 동영상 처리가 한 번 추가됩니다."),
    DE("Mehr Bilder bei schneller oder naher Bewegung, weniger bei Drehung oder Fernsicht. Die Rate wird zum Mittelwert. Ein zusätzlicher Videodurchlauf."),
    FR("Garde plus d’images en mouvement rapide ou proche, moins en rotation ou au loin. Le débit devient une moyenne. Ajoute une passe vidéo."),
    ES("Conserva más con movimiento rápido o cercano, menos con giros o vistas lejanas. La tasa pasa a ser media. Añade una pasada de vídeo."),
    PT("Guarda mais com movimento rápido ou próximo, menos com rotação ou vistas distantes. A taxa passa a ser média. Acrescenta uma passagem pelo vídeo."),
    IT("Più fotogrammi per movimenti rapidi o ravvicinati, meno per rotazioni o viste lontane. La frequenza diventa media. Aggiunge un passaggio video."),
    NL("Meer beelden bij snelle of nabije beweging, minder bij draaien of verre uitzichten. De snelheid wordt gemiddeld. Eén extra videodoorgang."),
    RU("Больше кадров при быстром движении или вблизи, меньше при поворотах или вдали. Частота становится средней. Добавляет проход по видео."),
    TR("Hızlı veya yakın harekette daha çok, dönüş veya uzak sahnelerde daha az kare tutar. Hız ortalama olur. Bir video geçişi ekler."));

SS_MSG(adaptive_fps_every_frame,
    EN("Every video keeps every frame (0 fps), so adapting the rate to the "
       "motion has no effect."),
    JA("どの動画もすべてのフレームを残す設定（0 fps）なので、動きに合わせた"
       "レート調整は効きません。"),
    ZH_HANS("所有视频都保留每一帧（0 fps），按运动调整帧率不会起作用。"),
    ZH_HANT("所有影片都保留每一影格（0 fps），依運動調整影格率不會起作用。"),
    KO("모든 동영상이 모든 프레임을 남기므로(0 fps) 움직임에 맞춘 속도 조절은 "
       "효과가 없습니다."),
    DE("Jedes Video behält jedes Bild (0 fps), die Anpassung an die Bewegung "
       "wirkt daher nicht."),
    FR("Chaque vidéo conserve toutes ses images (0 fps) : adapter le débit au "
       "mouvement n'a donc aucun effet."),
    ES("Todos los vídeos conservan todos sus fotogramas (0 fps), así que "
       "adaptar la tasa al movimiento no tiene efecto."),
    PT("Todos os vídeos mantêm todos os quadros (0 fps), então adaptar a taxa "
       "ao movimento não tem efeito."),
    IT("Ogni video tiene tutti i fotogrammi (0 fps), quindi adattare la "
       "frequenza al movimento non ha effetto."),
    NL("Elke video bewaart elk beeld (0 fps), dus het tempo aanpassen aan de "
       "beweging doet niets."),
    RU("Каждое видео оставляет все кадры (0 fps), поэтому подстройка частоты "
       "под движение ни на что не влияет."),
    TR("Her video her kareyi tutuyor (0 fps), bu yüzden hızı harekete göre "
       "ayarlamanın etkisi yok."));

SS_MSG(adaptive_range,
    EN("Spread"),
    JA("振れ幅"),
    ZH_HANS("浮动范围"),
    ZH_HANT("浮動範圍"),
    KO("변동 폭"),
    DE("Spanne"),
    FR("Amplitude"),
    ES("Margen"),
    PT("Margem"),
    IT("Escursione"),
    NL("Spreiding"),
    RU("Разброс"),
    TR("Aralık"));

SS_MSG(adaptive_range_help,
    EN("How far the rate may stray from the average, either way. 4 lets it "
       "run between a quarter of it and four times it."),
    JA("レートが平均からどこまで離れてよいかです。4 なら平均の 1/4 から 4 倍まで"
       "振れます。"),
    ZH_HANS("帧率相对平均值的上下浮动倍数。设为 4 表示可在平均值的 1/4 到 4 倍之间。"),
    ZH_HANT("影格率相對平均值的上下浮動倍數。設為 4 表示可在平均值的 1/4 到 4 倍之間。"),
    KO("속도가 평균에서 얼마나 벗어날 수 있는지입니다. 4면 평균의 1/4에서 4배 "
       "사이를 오갑니다."),
    DE("Wie weit die Rate nach beiden Seiten vom Durchschnitt abweichen darf. "
       "Bei 4 reicht sie von einem Viertel bis zum Vierfachen."),
    FR("De combien le débit peut s'écarter de la moyenne, dans les deux sens. "
       "4 le laisse aller du quart au quadruple."),
    ES("Cuánto puede alejarse la tasa del promedio, en ambos sentidos. Con 4 "
       "va de la cuarta parte al cuádruple."),
    PT("Quanto a taxa pode afastar-se da média, nos dois sentidos. Com 4 vai "
       "de um quarto ao quádruplo."),
    IT("Di quanto la frequenza può scostarsi dalla media, in entrambi i sensi. "
       "Con 4 va da un quarto al quadruplo."),
    NL("Hoever het tempo van het gemiddelde mag afwijken, beide kanten op. "
       "Bij 4 loopt het van een kwart tot vier keer."),
    RU("Насколько частота может отходить от средней в обе стороны. При 4 она "
       "идёт от четверти до четырёхкратной."),
    TR("Hızın ortalamadan iki yöne de ne kadar sapabileceği. 4 olunca dörtte "
       "birinden dört katına kadar gider."));

SS_MSG(pano360_output,
    EN("Unwrap into (360 video)"),
    JA("展開先（360 動画）"),
    ZH_HANS("展开为（360 视频）"),
    ZH_HANT("展開為（360 影片）"),
    KO("펼칠 형식(360 영상)"),
    DE("Entfalten zu (360-Video)"),
    FR("Déplier en (vidéo 360)"),
    ES("Desplegar en (vídeo 360)"),
    PT("Desdobrar em (vídeo 360)"),
    IT("Sviluppa in (video 360)"),
    NL("Uitvouwen naar (360-video)"),
    RU("Развернуть в (видео 360)"),
    TR("Şuna aç (360 video)"));

SS_MSG(pano360_output_faces,
    EN("Perspective views"),
    JA("透視投影のビュー"),
    ZH_HANS("透视视角"),
    ZH_HANT("透視視角"),
    KO("원근 뷰"),
    DE("Perspektivische Ansichten"),
    FR("Vues en perspective"),
    ES("Vistas en perspectiva"),
    PT("Vistas em perspectiva"),
    IT("Viste prospettiche"),
    NL("Perspectiefaanzichten"),
    RU("Перспективные виды"),
    TR("Perspektif görünümler"));

SS_MSG(pano360_output_equirect,
    EN("Equirectangular panorama"),
    JA("正距円筒パノラマ"),
    ZH_HANS("等距柱状全景"),
    ZH_HANT("等距柱狀全景"),
    KO("등장방형 파노라마"),
    DE("Äquirektanguläres Panorama"),
    FR("Panorama équirectangulaire"),
    ES("Panorama equirectangular"),
    PT("Panorama equirretangular"),
    IT("Panorama equirettangolare"),
    NL("Equirectangulair panorama"),
    RU("Равнопромежуточная панорама"),
    TR("Eş dikdörtgen panorama"));

SS_MSG(pano360_output_help,
    EN("Choose how to extract dual-lens 360° video.\n- Perspective: 10 pinhole views per frame, five per lens. Each stays on one side of the parallax seam.\n- Panorama: one image per frame across both lenses. The seam remains, and reconstruction downsamples it like other photos."),
    JA("透視投影のビューは以降の処理が想定している形です。このファイルはスティッチされておらず、2 つのレンズは視差のある継ぎ目で接します。そこで全天球をレンズごとに 5 枚、計 10 枚へ切り分け、どのビューも継ぎ目をまたぎません。\n- 結果として各ビューが本物のピンホールカメラになります。\n- パノラマは 1 フレーム 1 枚に保てますが、2 つのレンズを継ぎ目ごと 1 台のカメラに押し込み、復元は他の写真と同じ作業解像度まで縮小します。"),
    ZH_HANS("透视视角是后续流程真正为之设计的形式。该文件未做拼接：两只镜头在一条带视差的拼缝处相接，因此球面被切成十个视角、每镜头五个，没有一个跨过拼缝——每个视角都是真正的针孔相机。\n全景每帧只有一张图，却把两只镜头连同拼缝塞进同一台相机，而且重建会把它缩小到与普通照片相同的工作分辨率。"),
    ZH_HANT("透視視角是後續流程真正為之設計的形式。該檔案未做拼接：兩顆鏡頭在一條帶視差的拼縫處相接，因此球面被切成十個視角、每鏡頭五個，沒有一個跨過拼縫——每個視角都是真正的針孔相機。\n全景每格只有一張圖，卻把兩顆鏡頭連同拼縫塞進同一台相機，而且重建會把它縮小到與一般照片相同的工作解析度。"),
    KO("원근 뷰는 이후 단계가 전제로 하는 형식입니다. 이 파일은 스티칭되어 있지 않아 두 렌즈가 시차가 있는 이음매에서 만납니다.\n- 그래서 구를 렌즈당 5장씩 모두 10개의 뷰로 자르며 어느 뷰도 이음매를 가로지르지 않습니다.\n- 그 결과 각 뷰가 진짜 핀홀 카메라가 됩니다. 파노라마는 프레임당 한 장으로 유지되지만 두 렌즈를 이음매째 한 카메라에 담고, 재구성이 다른 사진과 같은 작업 해상도로 줄입니다."),
    DE("Perspektivische Ansichten sind das, worauf der Rest der Kette ausgelegt ist. Die Datei ist nicht zusammengesetzt:\n- Ihre beiden Objektive treffen sich an einer Naht mit echter Parallaxe, also wird die Kugel in zehn Ansichten geschnitten, fünf je Objektiv, von denen keine sie kreuzt -- jede ist dann eine echte Lochkamera.\n- Ein Panorama hält jede Aufnahme in einem Bild, steckt aber beide Objektive samt Naht in eine Kamera, und die Rekonstruktion verkleinert es auf dieselbe Arbeitsgröße wie jedes andere Foto."),
    FR("Les vues en perspective sont ce pour quoi la suite du traitement est faite. Le fichier n'est pas assemblé :\n- ses deux objectifs se rejoignent sur une couture traversée par une vraie parallaxe, donc la sphère est découpée en dix vues, cinq par objectif, dont aucune ne la traverse -- chacune est alors un vrai sténopé.\n- Un panorama garde une image par prise, mais il met les deux objectifs dans une seule caméra, couture comprise, et la reconstruction le réduit à la même taille de travail que n'importe quelle photo."),
    ES("Las vistas en perspectiva son aquello para lo que está hecho el resto del proceso. El archivo no está unido:\n- sus dos objetivos se encuentran en una costura con paralaje real, así que la esfera se corta en diez vistas, cinco por objetivo, y ninguna la cruza;\n- cada una es entonces una cámara estenopeica de verdad.\n- Un panorama guarda una imagen por toma, pero mete los dos objetivos en una sola cámara, costura incluida, y la reconstrucción lo reduce al mismo tamaño de trabajo que cualquier foto."),
    PT("As vistas em perspectiva são aquilo para que o resto do processo foi feito. O arquivo não é costurado: suas duas lentes se encontram numa emenda com paralaxe real, então a esfera é cortada em dez vistas, cinco por lente, e nenhuma a atravessa -- cada uma é então uma câmera pinhole de verdade.\nUm panorama guarda uma imagem por quadro, mas põe as duas lentes numa só câmera, emenda inclusive, e a reconstrução o reduz ao mesmo tamanho de trabalho de qualquer foto."),
    IT("Le viste prospettiche sono ciò per cui è fatto il resto della catena. Il file non è cucito: i suoi due obiettivi si incontrano su una cucitura attraversata da una parallasse reale, quindi la sfera viene tagliata in dieci viste, cinque per obiettivo, e nessuna la attraversa: ognuna è allora una vera camera a foro stenopeico.\nUn panorama tiene un'immagine per fotogramma, ma mette entrambi gli obiettivi in una sola fotocamera, cucitura compresa, e la ricostruzione la riduce alla stessa dimensione di lavoro di ogni altra foto."),
    NL("Perspectiefaanzichten zijn waar de rest van de keten op gebouwd is. Het bestand is niet aaneengezet:\n- de twee lenzen komen samen op een naad met echte parallax, dus wordt de bol in tien aanzichten gesneden, vijf per lens, waarvan er geen enkele die naad kruist -- elk is dan een echte gaatjescamera.\n- Een panorama houdt één beeld per opname, maar stopt beide lenzen mét naad in één camera, en de reconstructie verkleint het tot dezelfde werkmaat als elke andere foto."),
    RU("Перспективные виды — то, подо что сделана остальная часть обработки. Файл не сшит: два его объектива сходятся на шве с настоящим параллаксом, поэтому сфера режется на десять видов, по пять на объектив, и ни один шов не пересекает — каждый вид тогда настоящая камера-обскура.\nПанорама оставляет один снимок на кадр, но помещает оба объектива вместе со швом в одну камеру, а реконструкция уменьшает её до того же рабочего размера, что и любое фото."),
    TR("Perspektif görünümler, işlem zincirinin geri kalanının tasarlandığı biçimdir. Dosya birleştirilmemiştir:\n- iki objektifi gerçek paralaksın olduğu bir dikişte buluşur, bu yüzden küre objektif başına beş olmak üzere on görünüme bölünür ve hiçbiri dikişi kesmez -- böylece her biri gerçek bir iğne deliği kamerasıdır.\n- Panorama kare başına tek görüntü tutar, ama iki objektifi dikişiyle birlikte tek kameraya koyar ve geri çatım onu da her fotoğrafla aynı çalışma boyutuna küçültür."));
SS_MSG(pano360_size,
    EN("View size (px, 360 video)"),
    JA("ビューの大きさ（px、360 動画）"),
    ZH_HANS("视角尺寸（像素，360 视频）"),
    ZH_HANT("視角尺寸（像素，360 影片）"),
    KO("뷰 크기(px, 360 영상)"),
    DE("Ansichtsgröße (px, 360-Video)"),
    FR("Taille des vues (px, vidéo 360)"),
    ES("Tamaño de vista (px, vídeo 360)"),
    PT("Tamanho da vista (px, vídeo 360)"),
    IT("Dimensione della vista (px, video 360)"),
    NL("Grootte van aanzicht (px, 360-video)"),
    RU("Размер вида (пикс., видео 360)"),
    TR("Görünüm boyutu (px, 360 video)"));

SS_MSG(pano360_size_help,
    EN("Output size of the extracted views or panorama. Leave at 0 to choose from the input."),
    JA("抽出するビューやパノラマの出力サイズです。0なら入力に合わせて自動で決まります。"),
    ZH_HANS("提取视图或全景图的输出尺寸。保留 0 可根据输入自动选择。"),
    ZH_HANT("擷取視圖或全景圖的輸出尺寸。保留 0 可依輸入自動選擇。"),
    KO("추출한 뷰 또는 파노라마의 출력 크기입니다. 0이면 입력에 맞춰 자동으로 정합니다."),
    DE("Ausgabegröße der Ansichten oder des Panoramas. 0 wählt sie passend zur Quelle."),
    FR("Taille de sortie des vues ou du panorama. Laissez 0 pour l’adapter à la source."),
    ES("Tamaño de salida de las vistas o del panorama. Deja 0 para elegirlo según el origen."),
    PT("Tamanho de saída das vistas ou do panorama. Deixe 0 para escolher a partir da origem."),
    IT("Dimensione di uscita delle viste o del panorama. Lascia 0 per sceglierla dalla sorgente."),
    NL("Uitvoergrootte van de aanzichten of het panorama. Laat 0 staan om de invoer te volgen."),
    RU("Размер извлечённых видов или панорамы. При 0 размер выбирается по источнику."),
    TR("Çıkarılan görünümlerin veya panoramanın boyutu. Girdiye göre seçmek için 0 bırakın."));
SS_MSG(pano360_colmap_warning,
    EN("COLMAP has no spherical camera. Choose perspective views, or "
       "reconstruct with the built-in engine."),
    JA("COLMAP には球面カメラがありません。透視投影のビューを選ぶか、内蔵エンジンで"
       "復元してください。"),
    ZH_HANS("COLMAP 没有球面相机模型。请改用透视视角，或改用内置引擎重建。"),
    ZH_HANT("COLMAP 沒有球面相機模型。請改用透視視角，或改用內建引擎重建。"),
    KO("COLMAP에는 구면 카메라가 없습니다. 원근 뷰를 고르거나 내장 엔진으로 "
       "재구성하세요."),
    DE("COLMAP kennt keine sphärische Kamera. Wähle perspektivische Ansichten "
       "oder rekonstruiere mit der eingebauten Engine."),
    FR("COLMAP n'a pas de caméra sphérique. Choisissez des vues en perspective, "
       "ou reconstruisez avec le moteur intégré."),
    ES("COLMAP no tiene cámara esférica. Elige vistas en perspectiva o "
       "reconstruye con el motor integrado."),
    PT("O COLMAP não tem câmera esférica. Escolha vistas em perspectiva ou "
       "reconstrua com o motor integrado."),
    IT("COLMAP non ha una fotocamera sferica. Scegli le viste prospettiche "
       "oppure ricostruisci con il motore integrato."),
    NL("COLMAP kent geen sferische camera. Kies perspectiefaanzichten of "
       "reconstrueer met de ingebouwde engine."),
    RU("В COLMAP нет сферической камеры. Выберите перспективные виды или "
       "стройте встроенным движком."),
    TR("COLMAP'te küresel kamera yok. Perspektif görünümleri seçin ya da "
       "yerleşik motorla geri çatın."));

SS_MSG(skip_blurry,
    EN("Skip blurry frames"),
    JA("ぶれたフレームを除外"),
    ZH_HANS("跳过模糊帧"),
    ZH_HANT("略過模糊影格"),
    KO("흐린 프레임 건너뛰기"),
    DE("Unscharfe Bilder überspringen"),
    FR("Ignorer les images floues"),
    ES("Omitir fotogramas borrosos"),
    PT("Ignorar quadros borrados"),
    IT("Salta i fotogrammi sfocati"),
    NL("Onscherpe beelden overslaan"),
    RU("Пропускать размытые кадры"),
    TR("Bulanık kareleri atla"));

SS_MSG(skip_blurry_help,
    EN("Skips frames exceeding the blur threshold, measured from edge width in each frame. Works for videos, image sequences and photos."),
    JA("各フレームの輪郭幅から測ったぼけが閾値を超えるものを除外します。動画、連番画像、写真に対応します。"),
    ZH_HANS("根据每帧边缘宽度测量模糊，跳过超过阈值的帧。适用于视频、图像序列及照片。"),
    ZH_HANT("根據每影格邊緣寬度測量模糊，跳過超過閾值的影格。適用於影片、影像序列及照片。"),
    KO("각 프레임의 가장자리 폭으로 흐림을 측정해 기준 초과 프레임을 제외합니다. 영상·이미지 시퀀스·사진에 적용됩니다."),
    DE("Überspringt Bilder über der Unschärfeschwelle, gemessen an der Kantenbreite je Bild. Für Videos, Bildfolgen und Fotos."),
    FR("Écarte les images au-delà du seuil de flou, mesuré par largeur des contours de chaque image. Pour vidéos, séquences et photos."),
    ES("Omite fotogramas que superan el umbral de desenfoque, medido por ancho de bordes en cada imagen. Para vídeos, secuencias y fotos."),
    PT("Salta imagens acima do limiar de desfoque, medido pela largura dos contornos de cada uma. Para vídeos, sequências e fotos."),
    IT("Scarta immagini oltre la soglia di sfocatura, misurata dalla larghezza dei bordi di ciascuna. Per video, sequenze e foto."),
    NL("Slaat beelden boven de onscherptedrempel over, gemeten via randbreedte per beeld. Voor video’s, beeldreeksen en foto’s."),
    RU("Пропускает кадры выше порога размытия, измеренного по ширине краёв каждого кадра. Для видео, последовательностей и фото."),
    TR("Her karenin kenar genişliğinden ölçülen bulanıklığı eşik üstündeyse atlar. Video, resim dizisi ve fotoğraflar için."));

SS_MSG(blur_review,
    EN("Set by hand (the run pauses)"),
    JA("手動で設定（処理を一時停止）"),
    ZH_HANS("手动设置（运行会暂停）"),
    ZH_HANT("手動設定（執行會暫停）"),
    KO("직접 설정(실행이 일시 중지됨)"),
    DE("Von Hand einstellen (der Lauf pausiert)"),
    FR("Régler à la main (le traitement se met en pause)"),
    ES("Ajustar a mano (el proceso se pausa)"),
    PT("Ajustar à mão (a execução pausa)"),
    IT("Imposta a mano (l’esecuzione va in pausa)"),
    NL("Met de hand instellen (de run pauzeert)"),
    RU("Задать вручную (запуск приостановится)"),
    TR("Elle ayarla (çalışma duraklar)"));

SS_MSG(blur_review_help,
    EN("Pauses after extracting frames for threshold review in Sharpness. Continue starts masking, reconstruction and depth."),
    JA("フレーム抽出後に停止し、鮮明度画面で閾値を確認します。「続行」でマスク・再構成・深度処理を開始します。"),
    ZH_HANS("提取帧后暂停，在清晰度视图中检查阈值。继续后开始蒙版、重建及深度处理。"),
    ZH_HANT("擷取影格後暫停，在清晰度檢視中檢查閾值。繼續後開始遮罩、重建及深度處理。"),
    KO("프레임 추출 후 선명도 화면에서 기준을 검토하도록 멈춥니다. 계속하면 마스크·재구성·깊이 처리가 시작됩니다."),
    DE("Pausiert nach der Bildextraktion zur Schwellenprüfung in Schärfe. Fortsetzen startet Masken, Rekonstruktion und Tiefe."),
    FR("Pause après extraction pour vérifier le seuil dans Netteté. Continuer lance masquage, reconstruction et profondeur."),
    ES("Pausa tras extraer fotogramas para revisar el umbral en Nitidez. Continuar inicia máscaras, reconstrucción y profundidad."),
    PT("Pausa após extrair fotogramas para rever o limiar em Nitidez. Continuar inicia máscaras, reconstrução e profundidade."),
    IT("Pausa dopo l’estrazione per rivedere la soglia in Nitidezza. Continua avvia maschere, ricostruzione e profondità."),
    NL("Pauzeert na beeldextractie om de drempel in Scherpte te bekijken. Doorgaan start maskers, reconstructie en diepte."),
    RU("Пауза после извлечения кадров для проверки порога в «Резкость». Продолжение запускает маски, реконструкцию и глубину."),
    TR("Kare çıkarma sonrası Netlik ekranında eşiği incelemek için durur. Devam, maskeleme, yeniden oluşturma ve derinliği başlatır."));

SS_MSG(blur_every_frame,
    EN("A video that keeps every frame (0 fps) is not sorted: nothing is "
       "skipped from it."),
    JA("すべてのフレームを残す動画（0 fps）は仕分けしません。そこからは何も"
       "除外されません。"),
    ZH_HANS("保留每一帧的视频（0 fps）不做整理：不会从中跳过任何帧。"),
    ZH_HANT("保留每個影格的視訊（0 fps）不做整理：不會從中略過任何影格。"),
    KO("모든 프레임을 남기는 동영상(0 fps)은 정리하지 않습니다. 거"
       "기서는 아무것도 건너뛰지 않습니다."),
    DE("Ein Video, das jedes Bild behält (0 fps), wird nicht sortiert: "
       "daraus wird nichts übersprungen."),
    FR("Une vidéo qui garde toutes ses images (0 fps) n’est pas triée : "
       "rien n’y est ignoré."),
    ES("Un vídeo que conserva todos los fotogramas (0 fps) no se ordena: "
       "no se omite nada de él."),
    PT("Um vídeo que mantém todos os quadros (0 fps) não é organizado: "
       "nada dele é ignorado."),
    IT("Un video che tiene tutti i fotogrammi (0 fps) non viene "
       "riordinato: non ne viene saltato nessuno."),
    NL("Een video die elk beeld behoudt (0 fps) wordt niet gesorteerd: er "
       "wordt niets uit overgeslagen."),
    RU("Видео, из которого берётся каждый кадр (0 fps), не сортируется: из"
       " него ничего не пропускается."),
    TR("Her kareyi tutan bir video (0 fps) ayıklanmaz: ondan hiçbir kare "
       "atlanmaz."));

SS_MSG(plan_sort,
    EN("Sort: leave the blurry ones out, keep what is built"),
    JA("仕分け: ぶれたものを外し、作成済みのものは残す"),
    ZH_HANS("整理：剔除模糊帧，保留已生成的内容"),
    ZH_HANT("整理：剔除模糊影格，保留已建立的內容"),
    KO("정리: 흐린 프레임은 빼고, 이미 만든 것은 유지"),
    DE("Sortieren: unscharfe Bilder auslassen, Erstelltes behalten"),
    FR("Trier : écarter les images floues, garder ce qui est construit"),
    ES("Ordenar: apartar los borrosos, conservar lo ya construido"),
    PT("Organizar: deixar de fora os borrados, manter o que já foi feito"),
    IT("Riordina: scarta gli sfocati, tieni ciò che è già costruito"),
    NL("Sorteren: onscherpe beelden weglaten, het gebouwde behouden"),
    RU("Сортировка: убрать размытые, готовое оставить"),
    TR("Ayıkla: bulanıkları çıkar, kurulmuş olanı koru"));

SS_MSG(blur_kept_note,
    EN("Not applied: this run keeps the frames the dataset already has.\nUntick \"Keep the existing frames and reconstruction\" to sort them again."),
    JA("適用されません: 今回はデータセットに今あるフレームを残します。\n仕分け直すには「いまあるフレームと再構成を残す」のチェックを外してください。"),
    ZH_HANS("未应用：本次运行保留数据集已有的帧。\n要重新整理，请取消勾选“保留现有的帧和重建”。"),
    ZH_HANT("未套用：本次執行保留資料集已有的影格。\n要重新整理，請取消勾選「保留現有的影格和重建」。"),
    KO("적용되지 않음: 이번 실행은 데이터셋에 이미 있는 프레임을 그대로 둡니다. 다시 정리하려면 ‘지금 있는 프레임과 재구성 남겨 두기’를 해제하세요."),
    DE("Nicht angewendet: dieser Lauf behält die Bilder, die der Datensatz schon hat. „Vorhandene Bilder und Rekonstruktion behalten“ abwählen, um sie neu zu sortieren."),
    FR("Non appliqué : ce traitement garde les images que le jeu de données a déjà.\nDécochez « Garder les images et la reconstruction existantes » pour les trier de nouveau."),
    ES("No aplicado: este proceso conserva los fotogramas que el conjunto de datos ya tiene.\nDesmarque «Conservar los fotogramas y la reconstrucción existentes» para volver a ordenarlos."),
    PT("Não aplicado: esta execução mantém os quadros que o conjunto de dados já tem.\nDesmarque “Manter os quadros e a reconstrução existentes” para organizá-los de novo."),
    IT("Non applicato: questa esecuzione tiene i fotogrammi che il set di dati ha già.\nDeseleziona «Tenere i fotogrammi e la ricostruzione esistenti» per riordinarli."),
    NL("Niet toegepast: deze run behoudt de beelden die de dataset al heeft.\nVink ‘Bestaande beelden en reconstructie behouden’ uit om ze opnieuw te sorteren."),
    RU("Не применено: при этом запуске остаются кадры, которые уже есть в наборе данных.\nСнимите флажок «Оставить имеющиеся кадры и реконструкцию», чтобы отсортировать их заново."),
    TR("Uygulanmadı: bu çalışma veri kümesinde zaten bulunan kareleri korur.\nYeniden ayıklamak için “Mevcut kareleri ve yeniden kurmayı koru” işaretini kaldırın."));

SS_MSG(blur_review_note,
    EN("The run will pause after the frames, in the Sharpness view, until "
       "you press Continue."),
    JA("フレームの取り出し後、「シャープさ」ビューで処理が一時停止し、「これらの"
       "フレームで続行」を押すまで待ちます。"),
    ZH_HANS("提取帧之后，运行会在“清晰度”视图中暂停，直到你按下“用这些帧继续”。"),
    ZH_HANT("擷取影格之後，執行會在「清晰度」檢視中暫停，直到你按下「用這些影格繼續」"
            "。"),
    KO("프레임을 뽑은 뒤 ‘선명도’ 보기에서 실행이 멈추고, ‘이 프레임"
       "으로 계속’을 누를 때까지 기다립니다."),
    DE("Der Lauf hält nach den Bildern in der Ansicht „Schärfe“ an, bis "
       "„Mit diesen Bildern fortfahren“ gedrückt wird."),
    FR("Le traitement se mettra en pause après les images, dans la vue « "
       "Netteté », jusqu’à ce que vous appuyiez sur « Continuer avec ces "
       "images »."),
    ES("El proceso se pausará después de los fotogramas, en la vista "
       "«Nitidez», hasta que pulse «Continuar con estos fotogramas»."),
    PT("A execução vai pausar depois dos quadros, na vista “Nitidez”, até "
       "você pressionar “Continuar com estes quadros”."),
    IT("L’esecuzione andrà in pausa dopo i fotogrammi, nella vista "
       "«Nitidezza», finché non premi «Continua con questi fotogrammi»."),
    NL("De run pauzeert na de beelden, in de weergave ‘Scherpte’, totdat "
       "je op ‘Doorgaan met deze beelden’ drukt."),
    RU("Запуск приостановится после кадров, в виде «Резкость», пока вы не "
       "нажмёте «Продолжить с этими кадрами»."),
    TR("Çalışma karelerden sonra “Keskinlik” görünümünde duraklar ve siz "
       "“Bu karelerle devam et” düğmesine basana kadar bekler."));

SS_MSG(blur_review_no_pause,
    EN("Only the built-in engine pauses, and never in a batch: here the "
       "threshold is applied as it stands."),
    JA("一時停止するのは内蔵エンジンだけで、バッチでは停止しません。ここではしき"
       "い値がそのまま適用されます。"),
    ZH_HANS("只有内置引擎会暂停，批处理中从不暂停：这里会直接按当前阈值处理。"),
    ZH_HANT("只有內建引擎會暫停，批次處理中從不暫停：這裡會直接依目前的閾值處理。"),
    KO("내장 엔진만 멈추며, 일괄 처리에서는 멈추지 않습니다. 여기서는 "
       "임계값이 그대로 적용됩니다."),
    DE("Nur die eingebaute Engine hält an, und nie in einem Stapel: hier "
       "wird die Schwelle so angewendet, wie sie steht."),
    FR("Seul le moteur intégré se met en pause, et jamais dans un lot : "
       "ici le seuil est appliqué tel quel."),
    ES("Solo el motor integrado se pausa, y nunca en un lote: aquí el "
       "umbral se aplica tal como está."),
    PT("Só o motor integrado pausa, e nunca em um lote: aqui o limiar é "
       "aplicado como está."),
    IT("Solo il motore integrato va in pausa, e mai in un batch: qui la "
       "soglia viene applicata così com’è."),
    NL("Alleen de ingebouwde engine pauzeert, en nooit in een batch: hier "
       "wordt de drempel toegepast zoals die staat."),
    RU("Приостанавливается только встроенный движок, и никогда в пакете: "
       "здесь порог применяется как есть."),
    TR("Yalnızca yerleşik motor duraklar, toplu işte ise hiç duraklamaz: "
       "burada eşik olduğu gibi uygulanır."));

SS_MSG(keep_skipped,
    EN("Keep the skipped frames"),
    JA("除外したフレームを残す"),
    ZH_HANS("保留被跳过的帧"),
    ZH_HANT("保留被略過的影格"),
    KO("건너뛴 프레임 보관"),
    DE("Übersprungene Bilder aufbewahren"),
    FR("Conserver les images ignorées"),
    ES("Conservar los fotogramas omitidos"),
    PT("Guardar os quadros ignorados"),
    IT("Conserva i fotogrammi saltati"),
    NL("Overgeslagen beelden bewaren"),
    RU("Сохранять пропущенные кадры"),
    TR("Atlanan kareleri sakla"));

SS_MSG(keep_skipped_help,
    EN("Keeps rejected frames in images_skipped. Off deletes them; relaxing the threshold then requires extracting them again. Existing images and imported photos are kept."),
    JA("除外フレームをimages_skippedに残します。オフでは削除され、閾値を緩めると再抽出が必要です。既存の画像と取り込んだ写真は残します。"),
    ZH_HANS("将被筛掉的帧保存在 images_skipped。关闭会删除它们，放宽阈值后需重新提取。已有图像及导入的照片始终保留。"),
    ZH_HANT("將被篩掉的影格保存在 images_skipped。關閉會刪除它們，放寬閾值後需重新擷取。已有影像及匯入的照片始終保留。"),
    KO("제외된 프레임을 images_skipped에 보관합니다. 끄면 삭제되며, 기준을 완화하면 다시 추출해야 합니다. 기존 이미지와 가져온 사진은 보관됩니다."),
    DE("Behält aussortierte Bilder in images_skipped. Aus löscht sie; ein lockerer Grenzwert erfordert dann erneute Extraktion. Bestehende Bilder und importierte Fotos bleiben."),
    FR("Garde les vues rejetées dans images_skipped. Désactivé, les supprime : un seuil plus souple impose une nouvelle extraction. Images existantes et photos importées restent."),
    ES("Guarda los descartes en images_skipped. Desactivado los borra; bajar el umbral exige volver a extraerlos. Se guardan imágenes existentes y fotos importadas."),
    PT("Guarda os rejeitados em images_skipped. Desligado elimina-os; aliviar o limiar exige nova extração. Imagens existentes e fotos importadas ficam."),
    IT("Conserva gli scarti in images_skipped. Disattivato li elimina: una soglia meno severa richiede una nuova estrazione. Immagini esistenti e foto importate restano."),
    NL("Bewaart afgewezen beelden in images_skipped. Uit verwijdert ze; een soepelere drempel vereist dan opnieuw extraheren. Bestaande beelden en ingevoerde foto’s blijven."),
    RU("Хранит отклонённые кадры в images_skipped. Выкл. удаляет их: при смягчении порога нужна повторная выгрузка. Имеющиеся снимки и добавленные фото сохраняются."),
    TR("Elenen kareleri images_skipped içinde saklar. Kapalıyken silinir; eşiği gevşetmek yeniden çıkarmayı gerektirir. Mevcut resimler ve aktarılan fotoğraflar korunur."));

SS_MSG(blur_px,
    EN("Blur threshold"),
    JA("ぶれのしきい値"),
    ZH_HANS("模糊阈值"),
    ZH_HANT("模糊閾值"),
    KO("흐림 임계값"),
    DE("Unschärfeschwelle"),
    FR("Seuil de flou"),
    ES("Umbral de desenfoque"),
    PT("Limiar de borrão"),
    IT("Soglia di sfocatura"),
    NL("Onscherptedrempel"),
    RU("Порог размытия"),
    TR("Bulanıklık eşiği"));

SS_MSG(blur_px_help,
    EN("Skips frames whose excess edge blur exceeds this many pixels, relative to the capture’s sharpest frames. Lower skips more."),
    JA("撮影中の最も鮮明なフレームを基準に、輪郭のぼけがこのピクセル数を超えるフレームを除外します。小さい値ほど多く除外します。"),
    ZH_HANS("以本次拍摄最清晰的帧为基准，跳过边缘模糊超出此像素数的帧。值越低，跳过越多。"),
    ZH_HANT("以本次拍攝最清晰的影格為基準，跳過邊緣模糊超出此像素數的影格。值越低，跳過越多。"),
    KO("촬영의 가장 선명한 프레임보다 가장자리 흐림이 이 픽셀 수를 넘는 프레임을 제외합니다. 낮을수록 더 많이 제외합니다."),
    DE("Überspringt Bilder, deren Kantenunschärfe die schärfsten Bilder der Aufnahme um so viele Pixel übersteigt. Niedriger überspringt mehr."),
    FR("Écarte les images dont le flou des contours dépasse d’autant de pixels les plus nettes de la capture. Plus bas écarte davantage."),
    ES("Omite imágenes cuyo desenfoque de bordes supera en estos píxeles al de las más nítidas de la captura. Más bajo omite más."),
    PT("Salta imagens cujo desfoque dos contornos excede em tantos píxeis o das mais nítidas da captura. Mais baixo salta mais."),
    IT("Scarta immagini con bordi sfocati oltre questi pixel rispetto alle più nitide della ripresa. Più basso scarta di più."),
    NL("Slaat beelden over waarvan de randonscherpte zoveel pixels groter is dan bij de scherpste opnamebeelden. Lager slaat meer over."),
    RU("Пропускает кадры с размытием краёв на столько пикселей больше, чем у самых резких кадров съёмки. Меньше — больше пропусков."),
    TR("Kenar bulanıklığı çekimin en net karelerinden bu kadar piksel fazla olanları atlar. Düşük değer daha çok atlar."));

SS_MSG(blur_neighbour,
    EN("Keep a neighbour of every skipped frame"),
    JA("除外した各フレームの隣を残す"),
    ZH_HANS("为每个被跳过的帧保留相邻帧"),
    ZH_HANT("為每個被略過的影格保留相鄰影格"),
    KO("건너뛴 프레임마다 이웃 프레임 유지"),
    DE("Zu jedem übersprungenen Bild einen Nachbarn behalten"),
    FR("Garder une voisine de chaque image ignorée"),
    ES("Conservar un vecino de cada fotograma omitido"),
    PT("Manter um vizinho de cada quadro ignorado"),
    IT("Tieni un vicino di ogni fotogramma saltato"),
    NL("Van elk overgeslagen beeld een buur behouden"),
    RU("Оставлять соседа каждого пропущенного кадра"),
    TR("Atlanan her karenin bir komşusunu tut"));

SS_MSG(blur_neighbour_help,
    EN("Never skip more than two frames in a row, so every skipped frame "
       "still has a kept one beside it and a shaky stretch is thinned "
       "rather than cut out."),
    JA("連続して 3 枚以上は除外しません。除外した各フレームの隣には必ず残した"
       "フレームがあり、ぶれた区間は切り捨てられずに間引かれます。"),
    ZH_HANS("连续跳过不超过两帧，这样每个被跳过的帧旁边仍有保留的帧，抖动的片段只是被"
            "抽稀而不是被整段剪掉。"),
    ZH_HANT("連續略過不超過兩個影格，這樣每個被略過的影格旁邊仍有保留的影格，晃動的片"
            "段只是被抽稀而不是被整段剪掉。"),
    KO("연속으로 두 장 넘게는 건너뛰지 않습니다. 그래서 건너뛴 프레임 "
       "옆에는 늘 남긴 프레임이 있고, 흔들린 구간은 통째로 잘리지 않고"
       " 솎아집니다."),
    DE("Nie mehr als zwei Bilder in Folge überspringen, sodass neben jedem"
       " übersprungenen Bild noch ein behaltenes steht und eine "
       "verwackelte Strecke ausgedünnt statt herausgeschnitten wird."),
    FR("Ne jamais ignorer plus de deux images de suite : chaque image "
       "ignorée garde ainsi une voisine conservée, et un passage tremblé "
       "est éclairci plutôt que coupé."),
    ES("No omitir nunca más de dos fotogramas seguidos, de modo que cada "
       "fotograma omitido tenga uno conservado al lado y un tramo movido "
       "se aclare en vez de cortarse."),
    PT("Nunca ignorar mais de dois quadros seguidos, para que cada quadro "
       "ignorado ainda tenha um mantido ao lado e um trecho tremido seja "
       "rareado em vez de cortado."),
    IT("Non saltare mai più di due fotogrammi di seguito, così ogni "
       "fotogramma saltato ne ha accanto uno tenuto e un tratto mosso "
       "viene sfoltito anziché tagliato via."),
    NL("Nooit meer dan twee beelden achter elkaar overslaan, zodat elk "
       "overgeslagen beeld nog een behouden buur heeft en een schokkerig "
       "stuk wordt uitgedund in plaats van weggeknipt."),
    RU("Никогда не пропускать более двух кадров подряд, чтобы рядом с "
       "каждым пропущенным кадром оставался сохранённый, а дрожащий "
       "участок прореживался, а не вырезался."),
    TR("Art arda ikiden fazla kareyi asla atlamaz; böylece atlanan her "
       "karenin yanında tutulan bir kare kalır ve sarsıntılı bir bölüm "
       "kesilip atılmak yerine seyreltilir."));

SS_MSG(similar_bits,
    EN("Skip similar frames"),
    JA("似たフレームを除外"),
    ZH_HANS("跳过相似帧"),
    ZH_HANT("略過相似影格"),
    KO("비슷한 프레임 건너뛰기"),
    DE("Ähnliche Bilder überspringen"),
    FR("Ignorer les images semblables"),
    ES("Omitir fotogramas parecidos"),
    PT("Ignorar quadros parecidos"),
    IT("Salta i fotogrammi simili"),
    NL("Gelijkende beelden overslaan"),
    RU("Пропускать похожие кадры"),
    TR("Benzer kareleri atla"));

SS_MSG(similar_bits_help,
    EN("Skip a frame that looks too much like the last one kept, as when the camera stood still.\n- Each frame gets a 64-bit fingerprint; this is how many bits must differ.\n- 0 keeps every frame, a higher value skips more."),
    JA("直前に残したフレームに似すぎたフレームを除外します。 カメラが止まっていたときなどです。\n- 各フレームに 64 ビットの指紋を付け、ここで何ビット違えば別のフレームとみなすかを決めます。\n- 0 ならすべて残し、大きくするほど多く除外します。"),
    ZH_HANS("跳过与上一个保留帧过于相似的帧，例如相机静止时。每一帧都有一个 64 位指纹；这里设置必须有多少位不同。0 表示保留所有帧，数值越高跳过越多。"),
    ZH_HANT("略過與上一個保留影格過於相似的影格，例如相機靜止時。每個影格都有一個 64 位元指紋；這裡設定必須有多少位元不同。0 表示保留所有影格，數值越高略過越多。"),
    KO("마지막 보관 프레임과 너무 비슷한 프레임을 건너뜁니다. 64비트 지문에서 달라야 할 비트 수입니다. 0은 모두 보관하며 값이 클수록 더 건너뜁니다."),
    DE("Überspringt ein Bild, das dem zuletzt behaltenen zu sehr gleicht, etwa wenn die Kamera stillstand.\n- Jedes Bild bekommt einen 64-Bit-Fingerabdruck; hier steht, wie viele Bits sich unterscheiden müssen.\n- 0 behält jedes Bild, ein höherer Wert überspringt mehr."),
    FR("Ignore une image qui ressemble trop à la dernière gardée, comme quand la caméra était immobile.\n- Chaque image reçoit une empreinte de 64 bits ; ce réglage dit combien de bits doivent différer.\n- 0 garde toutes les images, une valeur plus haute en ignore davantage."),
    ES("Omite el fotograma que se parece demasiado al último conservado, como cuando la cámara estaba quieta.\n- Cada fotograma recibe una huella de 64 bits; aquí se indica cuántos bits deben diferir.\n- 0 conserva todos los fotogramas, un valor más alto omite más."),
    PT("Ignora o quadro que se parece demais com o último mantido, como quando a câmera estava parada.\n- Cada quadro recebe uma impressão digital de 64 bits; aqui se define quantos bits devem diferir.\n- 0 mantém todos os quadros, um valor mais alto ignora mais."),
    IT("Salta il fotogramma troppo simile all’ultimo tenuto, come quando la fotocamera era ferma.\n- Ogni fotogramma riceve un’impronta a 64 bit; qui si indica quanti bit devono differire.\n- 0 tiene tutti i fotogrammi, un valore più alto ne salta di più."),
    NL("Slaat een beeld over dat te veel lijkt op het laatst behouden beeld, zoals wanneer de camera stilstond.\n- Elk beeld krijgt een 64-bits vingerafdruk; hier staat hoeveel bits moeten verschillen.\n- 0 behoudt elk beeld, een hogere waarde slaat meer over."),
    RU("Пропускает кадр, слишком похожий на последний оставленный, например когда камера стояла на месте.\n- У каждого кадра есть 64-битный отпечаток; здесь задаётся, сколько бит должно отличаться.\n- 0 оставляет все кадры, большее значение пропускает больше."),
    TR("Son tutulan kareye fazla benzeyen kareyi atlar; örneğin kamera sabit durduğunda.\n- Her kare 64 bitlik bir parmak izi alır; burada kaç bitin farklı olması gerektiği belirlenir.\n- 0 her kareyi tutar, daha yüksek bir değer daha çok kare atlar."));

SS_MSG(sharpness_window,
    EN("Sharpness window"),
    JA("シャープさの判定窓"),
    ZH_HANS("清晰度窗口"),
    ZH_HANT("清晰度視窗"),
    KO("선명도 창"),
    DE("Schärfefenster"),
    FR("Fenêtre de netteté"),
    ES("Ventana de nitidez"),
    PT("Janela de nitidez"),
    IT("Finestra di nitidezza"),
    NL("Scherptevenster"),
    RU("Окно резкости"),
    TR("Keskinlik penceresi"));

SS_MSG(sharpness_window_help,
    EN("Look at this many candidate frames for each one kept and keep the "
       "least motion-blurred. 1 turns the selection off."),
    JA("1枚残すごとにこの数だけ候補フレームを見て、いちばんブレの少ないものを"
       "残します。1 にすると選別しません。"),
    ZH_HANS("每保留一帧就看这么多候选帧，从中挑运动模糊最少的。设为 1 就不做筛选。"),
    ZH_HANT("每保留一影格就看這麼多候選影格，從中挑運動模糊最少的。設為 1 就不做篩選。"),
    KO("한 장을 남길 때마다 이만큼의 후보 프레임을 보고 흔들림이 가장 적은 것을 "
       "남깁니다. 1이면 선별하지 않습니다."),
    DE("Für jedes behaltene Bild so viele Kandidaten ansehen und das am "
       "wenigsten verwackelte nehmen. 1 schaltet die Auswahl ab."),
    FR("Examiner ce nombre d'images candidates pour chaque image conservée et "
       "garder la moins floue. 1 désactive la sélection."),
    ES("Mirar esta cantidad de fotogramas candidatos por cada uno conservado "
       "y quedarse con el menos movido. 1 desactiva la selección."),
    PT("Olhar esta quantidade de quadros candidatos para cada um mantido e "
       "ficar com o menos tremido. 1 desliga a seleção."),
    IT("Guardare questo numero di fotogrammi candidati per ognuno tenuto e "
       "conservare il meno mosso. 1 disattiva la selezione."),
    NL("Zoveel kandidaatbeelden bekijken voor elk bewaard beeld en het minst "
       "bewogen exemplaar houden. 1 zet de selectie uit."),
    RU("Просматривать столько кадров-кандидатов на каждый оставленный и брать "
       "наименее смазанный. 1 отключает отбор."),
    TR("Tutulan her kare için bu kadar aday kareye bakıp en az bulanık olanı "
       "tutar. 1, seçimi kapatır."));

// {0} is a build-configuration note from the backend, in English.
SS_MSG(build_note,
    EN("Note: {0}."),    JA("注意: {0}。"),   ZH_HANS("注意：{0}。"), ZH_HANT("注意：{0}。"),
    KO("참고: {0}."),     DE("Hinweis: {0}."), FR("Remarque : {0}."),
    ES("Nota: {0}."),    PT("Observação: {0}."), IT("Nota: {0}."),
    NL("Let op: {0}."),  RU("Примечание: {0}."), TR("Not: {0}."));


// ===========================================================================
// Masking
// ===========================================================================

SS_MSG(masks_found_all,
    EN("Masks were found beside the images; they are used as they are."),
    JA("画像の隣にマスクが見つかりました。そのまま使われます。"),
    ZH_HANS("在图像旁边找到了蒙版，将按原样使用。"),
    ZH_HANT("在影像旁邊找到了遮罩，將按原樣使用。"),
    KO("이미지 옆에서 마스크를 찾았습니다. 그대로 사용합니다."),
    DE("Neben den Bildern wurden Masken gefunden; sie werden unverändert "
       "genutzt."),
    FR("Des masques ont été trouvés à côté des images ; ils sont utilisés tels "
       "quels."),
    ES("Se encontraron máscaras junto a las imágenes; se usan tal cual."),
    PT("Foram encontradas máscaras ao lado das imagens; elas são usadas como "
       "estão."),
    IT("Accanto alle immagini sono state trovate delle maschere; vengono usate "
       "così come sono."),
    NL("Naast de beelden zijn maskers gevonden; ze worden gebruikt zoals ze "
       "zijn."),
    RU("Рядом со снимками найдены маски; они используются как есть."),
    TR("Görüntülerin yanında maskeler bulundu; oldukları gibi kullanılıyor."));

// {0} inputs with masks, {1} inputs in total.
SS_MSG(masks_found_some,
    EN("{0} of the {1} inputs came with masks; those are used as they are."),
    JA("{1} 件の入力のうち {0} 件にマスクが付いていました。それらはそのまま"
       "使われます。"),
    ZH_HANS("{1} 个输入中有 {0} 个自带蒙版；这些会按原样使用。"),
    ZH_HANT("{1} 個輸入中有 {0} 個自帶遮罩；這些會按原樣使用。"),
    KO("입력 {1}개 가운데 {0}개에 마스크가 딸려 있습니다. 그것들은 그대로 "
       "사용합니다."),
    DE("{0} der {1} Eingaben brachten Masken mit; diese werden unverändert "
       "genutzt."),
    FR("{0} des {1} entrées sont fournies avec des masques ; ceux-là sont "
       "utilisés tels quels."),
    ES("{0} de las {1} entradas traían máscaras; esas se usan tal cual."),
    PT("{0} das {1} entradas vieram com máscaras; essas são usadas como estão."),
    IT("{0} dei {1} ingressi hanno già delle maschere; quelle vengono usate "
       "così come sono."),
    NL("{0} van de {1} invoeren kwamen met maskers; die worden gebruikt zoals "
       "ze zijn."),
    RU("Маски были у {0} из {1} входов; они используются как есть."),
    TR("{1} girdiden {0} tanesi maskeyle geldi; onlar oldukları gibi "
       "kullanılıyor."));

SS_MSG(mask_enable,
    EN("Remove moving or unwanted objects"),
    JA("動くものや不要なものを取り除く"),
    ZH_HANS("移除移动或不需要的物体"),
    ZH_HANT("移除移動或不需要的物體"),
    KO("움직이거나 원하지 않는 물체 제거"),
    DE("Bewegte oder unerwünschte Objekte entfernen"),
    FR("Retirer les objets mobiles ou indésirables"),
    ES("Quitar objetos en movimiento o no deseados"),
    PT("Remover objetos em movimento ou indesejados"),
    IT("Rimuovere oggetti in movimento o indesiderati"),
    NL("Bewegende of ongewenste objecten verwijderen"),
    RU("Убрать движущиеся или лишние объекты"),
    TR("Hareketli veya istenmeyen nesneleri kaldır"));

SS_MSG(mask_enable_help,
    EN("Describe objects to exclude, such as people or shadows. Masks exclude them from camera solving and training. Existing input masks are kept."),
    JA("人物や影など除外する物を記述します。マスクはカメラ推定と学習の両方から除外します。入力に既存のマスクがあれば保持します。"),
    ZH_HANS("描述要排除的对象，如行人或阴影。蒙版将其从相机求解和训练中排除。输入已有的蒙版会保留。"),
    ZH_HANT("描述要排除的對象，如行人或陰影。遮罩將其從相機求解和訓練中排除。輸入已有的遮罩會保留。"),
    KO("사람이나 그림자 등 제외할 대상을 설명하세요. 마스크는 카메라 추정과 학습 모두에서 대상을 제외합니다. 입력의 기존 마스크는 유지됩니다."),
    DE("Zu entfernende Objekte beschreiben, etwa Menschen oder Schatten. Masken schließen sie von Kameralösung und Training aus. Eingabemasken bleiben erhalten."),
    FR("Décrivez les objets à exclure, comme les personnes ou les ombres. Les masques les excluent du calcul des caméras et de l’entraînement. Ceux fournis restent."),
    ES("Describe qué excluir, como personas o sombras. Las máscaras lo excluyen del cálculo de cámaras y del entrenamiento. Se conservan las máscaras de entrada."),
    PT("Descreva o que excluir, como pessoas ou sombras. As máscaras excluem do cálculo das câmaras e do treino. As máscaras de entrada são mantidas."),
    IT("Descrivere cosa escludere, come persone o ombre. Le maschere escludono dal calcolo delle camere e dal training. Le maschere già presenti restano."),
    NL("Beschrijf wat weg moet, zoals mensen of schaduwen. Maskers sluiten dit uit van cameraberekening en training. Bestaande invoermaskers blijven behouden."),
    RU("Опишите, что исключить, например людей или тени. Маски исключают это из расчёта камер и обучения. Маски входных данных сохраняются."),
    TR("İnsanlar veya gölgeler gibi dışlanacak şeyleri tanımlayın. Maskeler bunları kamera çözümü ve eğitimden çıkarır. Girdideki mevcut maskeler korunur."));

SS_MSG(mask_objects_need_segmentation,
    EN("Removing objects needs a build with the segmentation module, and this "
       "one has none. Removing fixed areas of the frame still works."),
    JA("物体を取り除くには分割モジュール入りのビルドが必要ですが、このビルドには"
       "入っていません。画面の決まった位置を取り除くことはできます。"),
    ZH_HANS("移除物体需要包含分割模块的版本，而此版本没有。去掉画面中固定的区域"
            "仍然可用。"),
    ZH_HANT("移除物體需要包含分割模組的版本，而此版本沒有。去掉畫面中固定的區域"
            "仍然可用。"),
    KO("물체를 제거하려면 분할 모듈이 포함된 빌드가 필요한데, 이 빌드에는 "
       "없습니다. 화면에서 늘 같은 자리를 없애는 기능은 그대로 쓸 수 있습니다."),
    DE("Objekte entfernen braucht einen Build mit Segmentierungsmodul, und "
       "dieser hat keines. Feste Bereiche des Bildes lassen sich weiterhin "
       "entfernen."),
    FR("Retirer des objets demande une version avec le module de segmentation, "
       "et celle-ci ne l'a pas. Retirer les zones fixes de l'image reste "
       "possible."),
    ES("Quitar objetos requiere una compilación con el módulo de segmentación, "
       "y esta no lo tiene. Quitar las zonas fijas del fotograma sigue "
       "funcionando."),
    PT("Remover objetos exige uma versão com o módulo de segmentação, e esta "
       "não o tem. Tirar as áreas fixas do quadro continua funcionando."),
    IT("Rimuovere oggetti richiede una build con il modulo di segmentazione, e "
       "questa non ce l'ha. Togliere le zone fisse del fotogramma funziona "
       "comunque."),
    NL("Objecten verwijderen vraagt een build met de segmentatiemodule, en deze "
       "heeft er geen. Vaste gebieden van het beeld weghalen werkt nog steeds."),
    RU("Чтобы убирать объекты, нужна сборка с модулем сегментации, а в этой его "
       "нет. Убирать постоянные участки кадра по-прежнему можно."),
    TR("Nesneleri kaldırmak için bölütleme modülü içeren bir sürüm gerekir; bu "
       "sürümde yok. Karenin sabit alanlarını çıkarmak yine de çalışır."));

SS_MSG(mask_model,
    EN("Model"),         JA("モデル"),        ZH_HANS("模型"),     ZH_HANT("模型"),
    KO("모델"),           DE("Modell"),       FR("Modèle"),       ES("Modelo"),
    PT("Modelo"),        IT("Modello"),      NL("Model"),        RU("Модель"),
    TR("Model"));

// The second combo under a SAM 2.1 pick: the model that reads its words.
SS_MSG(mask_text_detector,
    EN("Text detector"),
    JA("テキスト検出器"),
    ZH_HANS("文字检测器"),
    ZH_HANT("文字偵測器"),
    KO("텍스트 검출기"),
    DE("Textdetektor"),
    FR("Détecteur de texte"),
    ES("Detector de texto"),
    PT("Detector de texto"),
    IT("Rilevatore di testo"),
    NL("Tekstdetector"),
    RU("Текстовый детектор"),
    TR("Metin algılayıcı"));

SS_MSG(mask_text_detector_help,
    EN("SAM 2.1 cannot read words, so a second model finds what the text prompt "
       "names and SAM 2.1 cuts it out. Clicks still go straight to SAM 2.1."),
    JA("SAM 2.1 は単語を読めないため、別のモデルがテキストのプロンプトの指す"
       "ものを見つけ、SAM 2.1 がそれを切り抜きます。クリックはこれまでどおり"
       " SAM 2.1 に直接渡ります。"),
    ZH_HANS("SAM 2.1 读不懂文字，所以由另一个模型找出文字提示所指的目标，再由 S"
            "AM 2.1 把它分割出来。点击仍然直接交给 SAM 2.1。"),
    ZH_HANT("SAM 2.1 讀不懂文字，所以由另一個模型找出文字提示所指的目標，再由 S"
            "AM 2.1 把它分割出來。點擊仍然直接交給 SAM 2.1。"),
    KO("SAM 2.1은 단어를 읽지 못하므로, 다른 모델이 텍스트 프롬프트가 가리키는 것을 찾고 SAM 2.1이 그것을 잘라 냅니다. "
       "클릭은 여전히 SAM 2.1로 바로 갑니다."),
    DE("SAM 2.1 kann keine Wörter lesen. Deshalb findet ein zweites Modell, was "
       "die Texteingabe nennt, und SAM 2.1 schneidet es aus. Klicks gehen "
       "weiterhin direkt an SAM 2.1."),
    FR("SAM 2.1 ne lit pas les mots : un second modèle trouve ce que nomme "
       "l'invite textuelle, et SAM 2.1 le découpe. Les clics vont toujours "
       "directement à SAM 2.1."),
    ES("SAM 2.1 no sabe leer palabras, así que un segundo modelo encuentra lo "
       "que nombra la indicación de texto y SAM 2.1 lo recorta. Los clics siguen "
       "yendo directamente a SAM 2.1."),
    PT("O SAM 2.1 não lê palavras, então um segundo modelo encontra o que o "
       "comando de texto nomeia e o SAM 2.1 o recorta. Os cliques continuam indo "
       "direto para o SAM 2.1."),
    IT("SAM 2.1 non sa leggere le parole: un secondo modello trova ciò che il "
       "testo nomina e SAM 2.1 lo ritaglia. I clic vanno sempre direttamente a "
       "SAM 2.1."),
    NL("SAM 2.1 kan geen woorden lezen, dus een tweede model vindt wat de "
       "tekstprompt noemt en SAM 2.1 knipt het uit. Klikken gaan nog steeds "
       "rechtstreeks naar SAM 2.1."),
    RU("SAM 2.1 не читает слова, поэтому вторая модель находит то, что названо в "
       "текстовом запросе, а SAM 2.1 вырезает это. Щелчки по-прежнему идут прямо "
       "в SAM 2.1."),
    TR("SAM 2.1 kelime okuyamaz; bu yüzden ikinci bir model metin isteminin "
       "adını verdiği şeyi bulur, SAM 2.1 de onu keser. Tıklamalar yine doğrudan "
       "SAM 2.1'e gider."));

SS_MSG(mask_model_needs_download,
    EN("{0}  (download)"),
    JA("{0}（ダウンロード）"),
    ZH_HANS("{0}（需下载）"),
    ZH_HANT("{0}（需下載）"),
    KO("{0}(내려받기)"),
    DE("{0}  (Download)"),
    FR("{0}  (à télécharger)"),
    ES("{0}  (descarga)"),
    PT("{0}  (baixar)"),
    IT("{0}  (da scaricare)"),
    NL("{0}  (downloaden)"),
    RU("{0}  (загрузка)"),
    TR("{0}  (indirilecek)"));

SS_MSG(mask_get_model,
    EN("Get the model"), JA("モデルを取得"),   ZH_HANS("获取模型"),  ZH_HANT("取得模型"),
    KO("모델 받기"),      DE("Modell holen"), FR("Obtenir le modèle"),
    ES("Obtener el modelo"), PT("Obter o modelo"), IT("Ottieni il modello"),
    NL("Model ophalen"), RU("Получить модель"), TR("Modeli getir"));

SS_MSG(mask_model_first,
    EN("the masking model has not been downloaded yet"),
    JA("マスク用のモデルがまだダウンロードされていません"),
    ZH_HANS("还没有下载遮罩用的模型"),
    ZH_HANT("還沒有下載遮罩用的模型"),
    KO("마스킹 모델을 아직 내려받지 않았습니다"),
    DE("das Maskierungsmodell ist noch nicht heruntergeladen"),
    FR("le modèle de masquage n'est pas encore téléchargé"),
    ES("el modelo de enmascarado todavía no está descargado"),
    PT("o modelo de máscara ainda não foi baixado"),
    IT("il modello per le maschere non è ancora stato scaricato"),
    NL("het maskeermodel is nog niet gedownload"),
    RU("модель для масок ещё не загружена"),
    TR("maskeleme modeli henüz indirilmedi"));

SS_MSG(mask_one_time_download,
    EN("one-time download, kept for next time"),
    JA("初回だけのダウンロードで、次回以降は再利用します"),
    ZH_HANS("只需下载一次，之后会一直保留"),
    ZH_HANT("只需下載一次，之後會一直保留"),
    KO("한 번만 내려받고 다음부터는 그대로 씁니다"),
    DE("einmaliger Download, bleibt für das nächste Mal erhalten"),
    FR("téléchargement unique, conservé pour la prochaine fois"),
    ES("descarga única, se conserva para la próxima vez"),
    PT("download único, guardado para a próxima vez"),
    IT("scaricamento una tantum, resta per la prossima volta"),
    NL("eenmalige download, blijft bewaard voor de volgende keer"),
    RU("загрузка один раз, дальше используется сохранённая"),
    TR("bir kez indirilir, bir dahaki sefere saklanır"));

SS_MSG(stop,
    EN("Stop"),          JA("停止"),          ZH_HANS("停止"),     ZH_HANT("停止"),
    KO("중지"),           DE("Stopp"),        FR("Arrêter"),      ES("Detener"),
    PT("Parar"),         IT("Ferma"),        NL("Stoppen"),      RU("Стоп"),
    TR("Durdur"));

SS_MSG(mask_model_ready,
    EN("Model ready."),  JA("モデルの準備ができました。"), ZH_HANS("模型已就绪。"),
    ZH_HANT("模型已就緒。"), KO("모델이 준비되었습니다."), DE("Modell bereit."),
    FR("Modèle prêt."),  ES("Modelo listo."), PT("Modelo pronto."),
    IT("Modello pronto."), NL("Model gereed."), RU("Модель готова."),
    TR("Model hazır."));

SS_MSG(mask_try,
    EN("Try the mask..."),
    JA("マスクを試す…"),  ZH_HANS("试一下蒙版…"), ZH_HANT("試一下遮罩…"),
    KO("마스크 시험해 보기…"), DE("Maske ausprobieren …"), FR("Essayer le masque…"),
    ES("Probar la máscara…"), PT("Testar a máscara…"), IT("Prova la maschera…"),
    NL("Masker uitproberen…"), RU("Проверить маску…"), TR("Maskeyi dene…"));

SS_MSG(mask_try_help,
    EN("Run the prompt on one real frame and see exactly what would be cut "
       "out, before committing to the whole capture."),
    JA("撮影全体に適用する前に、実際のフレーム1枚でプロンプトを試し、何が"
       "取り除かれるのかを確かめられます。"),
    ZH_HANS("在对整段拍摄下手之前，先在一张真实帧上跑一遍提示词，看清到底会剔除什么。"),
    ZH_HANT("在對整段拍攝下手之前，先在一張真實影格上跑一遍提示詞，看清到底會剔除什麼。"),
    KO("전체 촬영에 적용하기 전에, 실제 프레임 한 장에 프롬프트를 돌려 무엇이 "
       "잘려 나가는지 정확히 확인합니다."),
    DE("Die Eingabe an einem echten Bild ausprobieren und genau sehen, was "
       "wegfiele, bevor man sich für die ganze Aufnahme entscheidet."),
    FR("Essayer l'invite sur une vraie image et voir exactement ce qui serait "
       "retiré, avant de l'appliquer à toute la prise."),
    ES("Probar la indicación en un fotograma real y ver exactamente qué se "
       "eliminaría, antes de aplicarlo a toda la captura."),
    PT("Testar o comando num quadro real e ver exatamente o que seria "
       "removido, antes de aplicar à captura inteira."),
    IT("Provare il testo su un fotogramma reale e vedere esattamente che cosa "
       "verrebbe tolto, prima di applicarlo a tutta la ripresa."),
    NL("De prompt op één echt beeld draaien en precies zien wat eruit zou "
       "gaan, voordat je de hele opname vastlegt."),
    RU("Проверить запрос на одном настоящем кадре и увидеть, что именно будет "
       "вырезано, прежде чем применять ко всей съёмке."),
    TR("Bütün çekime uygulamadan önce istemi gerçek bir kare üzerinde "
       "çalıştırıp neyin çıkarılacağını tam olarak görün."));

SS_MSG(mask_on_input,
    EN("on input"),      JA("対象の入力"),    ZH_HANS("在输入"),   ZH_HANT("在輸入"),
    KO("대상 입력"),      DE("an Eingabe"),   FR("sur l'entrée"), ES("en la entrada"),
    PT("na entrada"),    IT("sull'ingresso"), NL("op invoer"),   RU("на входе"),
    TR("girdi üzerinde"));

SS_MSG(mask_on_input_help,
    EN("Which input \"Try the mask\" runs on. The text prompt applies to every "
       "input; clicked objects only to this one."),
    JA("「マスクを試す」をどの入力で実行するかです。テキストのプロンプトは"
       "すべての入力に適用されますが、クリックした物体はこの入力だけに"
       "適用されます。"),
    ZH_HANS("“试一下蒙版”在哪个输入上运行。文本提示对所有输入生效；点选的物体"
            "只对这一个输入生效。"),
    ZH_HANT("「試一下遮罩」在哪個輸入上執行。文字提示對所有輸入生效；點選的物體"
            "只對這一個輸入生效。"),
    KO("[마스크 시험해 보기]를 어느 입력에서 실행할지입니다. 텍스트 프롬프트는 "
       "모든 입력에 적용되지만, 클릭한 물체는 이 입력에만 적용됩니다."),
    DE("An welcher Eingabe „Maske ausprobieren“ läuft. Der Textbefehl gilt für "
       "jede Eingabe; angeklickte Objekte nur für diese."),
    FR("Sur quelle entrée « Essayer le masque » s'exécute. L'invite textuelle "
       "vaut pour toutes les entrées ; les objets cliqués seulement pour "
       "celle-ci."),
    ES("En qué entrada se ejecuta «Probar la máscara». La indicación de texto "
       "vale para todas las entradas; los objetos marcados, solo para esta."),
    PT("Em qual entrada “Testar a máscara” roda. O comando de texto vale para "
       "todas as entradas; os objetos clicados só para esta."),
    IT("Su quale ingresso viene eseguito «Prova la maschera». Il testo vale "
       "per tutti gli ingressi; gli oggetti cliccati solo per questo."),
    NL("Op welke invoer ‘Masker uitproberen’ draait. De tekstprompt geldt voor "
       "elke invoer; aangeklikte objecten alleen voor deze."),
    RU("На каком входе выполняется «Проверить маску». Текстовый запрос "
       "действует на все входы, а отмеченные объекты — только на этот."),
    TR("“Maskeyi dene”nin hangi girdide çalışacağı. Metin istemi bütün "
       "girdilere uygulanır; tıklanan nesneler yalnızca buna."));

SS_MSG(mask_no_text_prompts,
    EN("This model has no text understanding -- use \"Try the mask\" and click "
       "the object instead."),
    JA("このモデルはテキストを理解しません。「マスクを試す」から対象を"
       "クリックして指定してください。"),
    ZH_HANS("这个模型不理解文字——请改用“试一下蒙版”，直接点选目标物体。"),
    ZH_HANT("這個模型不理解文字——請改用「試一下遮罩」，直接點選目標物體。"),
    KO("이 모델은 텍스트를 이해하지 못합니다. [마스크 시험해 보기]에서 물체를 "
       "직접 클릭하세요."),
    DE("Dieses Modell versteht keinen Text -- stattdessen „Maske ausprobieren“ "
       "öffnen und das Objekt anklicken."),
    FR("Ce modèle ne comprend pas le texte : utilisez plutôt « Essayer le "
       "masque » et cliquez sur l'objet."),
    ES("Este modelo no entiende texto: use «Probar la máscara» y haga clic en "
       "el objeto."),
    PT("Este modelo não entende texto -- use “Testar a máscara” e clique no "
       "objeto."),
    IT("Questo modello non capisce il testo: usi «Prova la maschera» e clicchi "
       "sull'oggetto."),
    NL("Dit model begrijpt geen tekst -- gebruik ‘Masker uitproberen’ en klik "
       "het object aan."),
    RU("Эта модель не понимает текст — откройте «Проверить маску» и укажите "
       "объект щелчком."),
    TR("Bu model metni anlamaz -- bunun yerine “Maskeyi dene”yi açıp nesneye "
       "tıklayın."));

// {0} is a count, labelled rather than inflected.
SS_MSG(mask_clicked_objects,
    EN("Clicked objects: {0}. They are tracked through the capture."),
    JA("クリックした物体: {0} 件。撮影全体を通して追跡されます。"),
    ZH_HANS("已点选的物体：{0} 个。它们会在整段拍摄中被跟踪。"),
    ZH_HANT("已點選的物體：{0} 個。它們會在整段拍攝中被追蹤。"),
    KO("클릭한 물체: {0}개. 촬영 전체에 걸쳐 추적됩니다."),
    DE("Angeklickte Objekte: {0}. Sie werden durch die Aufnahme hindurch "
       "verfolgt."),
    FR("Objets cliqués : {0}. Ils sont suivis tout au long de la prise."),
    ES("Objetos marcados: {0}. Se siguen a lo largo de toda la captura."),
    PT("Objetos clicados: {0}. Eles são rastreados por toda a captura."),
    IT("Oggetti cliccati: {0}. Vengono seguiti per tutta la ripresa."),
    NL("Aangeklikte objecten: {0}. Ze worden door de hele opname gevolgd."),
    RU("Отмечено объектов: {0}. Они отслеживаются по всей съёмке."),
    TR("Tıklanan nesneler: {0}. Çekim boyunca izlenirler."));

SS_MSG(mask_forget_clicks,
    EN("Forget them"),   JA("忘れる"),        ZH_HANS("忘掉它们"),  ZH_HANT("忘掉它們"),
    KO("잊기"),           DE("Verwerfen"),    FR("Les oublier"),  ES("Olvidarlos"),
    PT("Esquecê-los"),   IT("Dimenticali"),  NL("Vergeten"),     RU("Забыть их"),
    TR("Unut"));

SS_MSG(mask_forget_clicks_help,
    EN("Objects clicked in “Try the mask” are tracked from that frame using video memory. No need to click every frame."),
    JA("「マスクを試す」でクリックした対象を、そのフレームから動画メモリで追跡します。毎フレームのクリックは不要です。"),
    ZH_HANS("“试用蒙版”中点击的对象从该帧起使用视频记忆追踪，无需每帧点击。"),
    ZH_HANT("「試用遮罩」中點擊的對象從該影格起使用影片記憶追蹤，無需每影格點擊。"),
    KO("“마스크 시험”에서 클릭한 객체를 해당 프레임부터 영상 메모리로 추적합니다. 매 프레임 클릭할 필요가 없습니다."),
    DE("In „Maske testen“ angeklickte Objekte werden ab diesem Bild per Videogedächtnis verfolgt. Nicht jedes Bild muss angeklickt werden."),
    FR("Les objets cliqués dans « Essayer le masque » sont suivis dès cette image par mémoire vidéo. Inutile de cliquer chaque image."),
    ES("Los objetos marcados en «Probar máscara» se siguen desde ese fotograma con memoria de vídeo. No hace falta marcar cada fotograma."),
    PT("Objetos clicados em «Experimentar máscara» são seguidos desse fotograma em diante com memória de vídeo. Não é preciso clicar cada um."),
    IT("Gli oggetti cliccati in «Prova maschera» sono seguiti da quel fotogramma con memoria video. Non occorre cliccare ogni fotogramma."),
    NL("Objecten uit ‘Masker proberen’ worden vanaf het aangeklikte beeld gevolgd met videogeheugen. Klikken op elk beeld is niet nodig."),
    RU("Объекты, отмеченные в «Проба маски», отслеживаются с этого кадра видеопамятью. Кликать каждый кадр не нужно."),
    TR("“Maskeyi dene” içinde tıklanan nesneler o kareden itibaren video belleğiyle izlenir. Her karede tıklamak gerekmez."));

// {0} is the input the clicks were drawn on.
SS_MSG(mask_inputs_need_clicks,
    EN("They prompt only the input they were drawn on. Still without one: {0}. "
       "Click the object there too, or add a text prompt."),
    JA("クリックした対象は、それを描いた入力にしか効きません。まだないのは "
       "{0} です。そちらでも対象をクリックするか、文字のプロンプトを追加して"
       "ください。"),
    ZH_HANS("点选的对象只对标注它的那个输入有效。还没有的是：{0}。请在那里也"
            "点选一次，或者加上文字提示。"),
    ZH_HANT("點選的對象只對標註它的那個輸入有效。還沒有的是：{0}。請在那裡也"
            "點選一次，或者加上文字提示。"),
    KO("클릭한 대상은 그것을 그린 입력에만 적용됩니다. 아직 없는 입력: {0}. "
       "거기서도 대상을 클릭하거나 텍스트 프롬프트를 추가하세요."),
    DE("Sie gelten nur für die Eingabe, auf der sie eingezeichnet wurden. Noch "
       "ohne: {0}. Klicken Sie das Objekt auch dort an, oder ergänzen Sie einen "
       "Texthinweis."),
    FR("Ils ne valent que pour l'entrée sur laquelle ils ont été tracés. "
       "Toujours sans : {0}. Cliquez-y aussi l'objet, ou ajoutez une invite "
       "textuelle."),
    ES("Solo valen para la entrada en la que se marcaron. Aún sin ninguno: {0}. "
       "Marque el objeto ahí también, o añada una indicación de texto."),
    PT("Só valem para a entrada em que foram marcados. Ainda sem: {0}. Marque o "
       "objeto ali também, ou acrescente um comando de texto."),
    IT("Valgono solo per l'ingresso su cui sono stati tracciati. Ancora senza: "
       "{0}. Clicchi l'oggetto anche lì, oppure aggiunga un testo."),
    NL("Ze gelden alleen voor de invoer waarop ze zijn gezet. Nog zonder: {0}. "
       "Klik het object daar ook aan, of voeg een tekstprompt toe."),
    RU("Они действуют только на том входе, где их указали. Пока без них: {0}. "
       "Отметьте объект и там или добавьте текстовый запрос."),
    TR("Yalnızca işaretlendikleri girdi için geçerlidirler. Hâlâ olmayan: {0}. "
       "Nesneyi orada da tıklayın ya da bir metin istemi ekleyin."));

SS_MSG(mask_pick_input_first,
    EN("Pick the photos or video first."),
    JA("先に写真か動画を選んでください。"),
    ZH_HANS("请先选择照片或视频。"),
    ZH_HANT("請先選擇相片或影片。"),
    KO("먼저 사진이나 동영상을 고르세요."),
    DE("Zuerst die Fotos oder das Video wählen."),
    FR("Choisissez d'abord les photos ou la vidéo."),
    ES("Elija primero las fotos o el vídeo."),
    PT("Escolha primeiro as fotos ou o vídeo."),
    IT("Scelga prima le fotografie o il video."),
    NL("Kies eerst de foto's of de video."),
    RU("Сначала выберите фотографии или видео."),
    TR("Önce fotoğrafları veya videoyu seçin."));

SS_MSG(mask_remove_named,
    EN("Remove what I name"),
    JA("指定したものを取り除く"),
    ZH_HANS("移除我指名的东西"),
    ZH_HANT("移除我指名的東西"),
    KO("내가 말한 것을 제거"),
    DE("Entfernen, was ich nenne"),
    FR("Retirer ce que je nomme"),
    ES("Quitar lo que yo nombre"),
    PT("Remover o que eu nomear"),
    IT("Rimuovere ciò che indico"),
    NL("Verwijderen wat ik noem"),
    RU("Убрать то, что я назову"),
    TR("Adını verdiğimi kaldır"));

SS_MSG(mask_keep_named,
    EN("Keep only what I name"),
    JA("指定したものだけを残す"),
    ZH_HANS("只保留我指名的东西"),
    ZH_HANT("只保留我指名的東西"),
    KO("내가 말한 것만 남기기"),
    DE("Nur behalten, was ich nenne"),
    FR("Ne garder que ce que je nomme"),
    ES("Conservar solo lo que yo nombre"),
    PT("Manter só o que eu nomear"),
    IT("Tenere solo ciò che indico"),
    NL("Alleen houden wat ik noem"),
    RU("Оставить только то, что я назову"),
    TR("Yalnızca adını verdiğimi tut"));

SS_MSG(mask_polarity_help,
    EN("\"Remove\" is for distractors. \"Keep only\" is for capturing a single "
       "object, where everything around it should be ignored."),
    JA("「取り除く」は邪魔物向けです。「だけを残す」は単体の被写体を撮るとき、"
       "まわりのすべてを無視したい場合に使います。"),
    ZH_HANS("“移除”用于干扰物。“只保留”用于拍摄单个物体，此时周围的一切都该被忽略。"),
    ZH_HANT("「移除」用於干擾物。「只保留」用於拍攝單個物體，此時周圍的一切都該被忽略。"),
    KO("'제거'는 방해물용입니다. '만 남기기'는 물체 하나를 촬영할 때, 그 주위의 "
       "모든 것을 무시하고 싶을 때 씁니다."),
    DE("„Entfernen“ ist für Störendes. „Nur behalten“ ist für die Aufnahme "
       "eines einzelnen Objekts, bei der alles ringsum ignoriert werden soll."),
    FR("« Retirer » sert pour les gêneurs. « Ne garder que » sert à "
       "photographier un objet unique, où tout ce qui l'entoure doit être "
       "ignoré."),
    ES("«Quitar» es para elementos molestos. «Conservar solo» es para capturar "
       "un único objeto, cuando todo lo que lo rodea debe ignorarse."),
    PT("“Remover” é para elementos indesejados. “Manter só” é para capturar um "
       "único objeto, quando tudo à volta deve ser ignorado."),
    IT("«Rimuovere» serve per gli elementi di disturbo. «Tenere solo» serve a "
       "riprendere un singolo oggetto, quando tutto ciò che lo circonda va "
       "ignorato."),
    NL("‘Verwijderen’ is voor stoorelementen. ‘Alleen houden’ is voor het "
       "opnemen van één object, waarbij alles eromheen genegeerd moet worden."),
    RU("«Убрать» — для помех. «Оставить только» — для съёмки одного предмета, "
       "когда всё вокруг него нужно игнорировать."),
    TR("“Kaldır” istenmeyen ögeler içindir. “Yalnızca tut” tek bir nesneyi "
       "çekerken, çevresindeki her şeyin yok sayılması gerektiğinde "
       "kullanılır."));

SS_MSG(mask_what_to_keep,
    EN("What to keep"),  JA("残すもの"),      ZH_HANS("要保留什么"), ZH_HANT("要保留什麼"),
    KO("남길 것"),        DE("Was bleiben soll"), FR("Ce qu'il faut garder"),
    ES("Qué conservar"), PT("O que manter"), IT("Che cosa tenere"),
    NL("Wat te houden"), RU("Что оставить"), TR("Ne tutulacak"));

SS_MSG(mask_what_to_remove,
    EN("What to remove"), JA("取り除くもの"),  ZH_HANS("要移除什么"), ZH_HANT("要移除什麼"),
    KO("제거할 것"),      DE("Was weg soll"), FR("Ce qu'il faut retirer"),
    ES("Qué quitar"),    PT("O que remover"), IT("Che cosa rimuovere"),
    NL("Wat te verwijderen"), RU("Что убрать"), TR("Ne kaldırılacak"));

SS_MSG(mask_prompt_help_keep,
    EN("Plain words, separated by semicolons. Everything NOT matching them is "
       "cut out of the reconstruction."),
    JA("ふつうの言葉をセミコロンで区切って書きます。それに当てはまらないものは"
       "すべて再構成から取り除かれます。"),
    ZH_HANS("用平常的词语，以分号分隔。凡是不匹配的都会从重建中剔除。"),
    ZH_HANT("用平常的詞語，以分號分隔。凡是不符合的都會從重建中剔除。"),
    KO("평범한 낱말을 세미콜론으로 구분해 적으세요. 거기 해당하지 않는 것은 모두 "
       "재구성에서 제외됩니다."),
    DE("Einfache Wörter, durch Semikolon getrennt. Alles, was NICHT darauf "
       "passt, fällt aus der Rekonstruktion heraus."),
    FR("Des mots ordinaires, séparés par des points-virgules. Tout ce qui n'y "
       "correspond PAS est retiré de la reconstruction."),
    ES("Palabras corrientes, separadas por punto y coma. Todo lo que NO "
       "coincida se elimina de la reconstrucción."),
    PT("Palavras simples, separadas por ponto e vírgula. Tudo o que NÃO "
       "corresponder é retirado da reconstrução."),
    IT("Parole comuni, separate da punto e virgola. Tutto ciò che NON "
       "corrisponde viene tolto dalla ricostruzione."),
    NL("Gewone woorden, gescheiden door puntkomma's. Alles wat er NIET bij "
       "past valt uit de reconstructie."),
    RU("Обычные слова через точку с запятой. Всё, что им НЕ соответствует, "
       "исключается из реконструкции."),
    TR("Noktalı virgülle ayrılmış sıradan sözcükler. Bunlara uymayan her şey "
       "yeniden oluşturmadan çıkarılır."));

SS_MSG(mask_prompt_help_remove,
    EN("Plain words, separated by semicolons. Everything matching them is cut "
       "out of the reconstruction."),
    JA("ふつうの言葉をセミコロンで区切って書きます。当てはまるものは再構成から"
       "取り除かれます。"),
    ZH_HANS("用平常的词语，以分号分隔。凡是匹配的都会从重建中剔除。"),
    ZH_HANT("用平常的詞語，以分號分隔。凡是符合的都會從重建中剔除。"),
    KO("평범한 낱말을 세미콜론으로 구분해 적으세요. 거기 해당하는 것은 재구성에서 "
       "제외됩니다."),
    DE("Einfache Wörter, durch Semikolon getrennt. Alles, was darauf passt, "
       "fällt aus der Rekonstruktion heraus."),
    FR("Des mots ordinaires, séparés par des points-virgules. Tout ce qui y "
       "correspond est retiré de la reconstruction."),
    ES("Palabras corrientes, separadas por punto y coma. Todo lo que coincida "
       "se elimina de la reconstrucción."),
    PT("Palavras simples, separadas por ponto e vírgula. Tudo o que "
       "corresponder é retirado da reconstrução."),
    IT("Parole comuni, separate da punto e virgola. Tutto ciò che corrisponde "
       "viene tolto dalla ricostruzione."),
    NL("Gewone woorden, gescheiden door puntkomma's. Alles wat erbij past valt "
       "uit de reconstructie."),
    RU("Обычные слова через точку с запятой. Всё, что им соответствует, "
       "исключается из реконструкции."),
    TR("Noktalı virgülle ayrılmış sıradan sözcükler. Bunlara uyan her şey "
       "yeniden oluşturmadan çıkarılır."));

SS_MSG(mask_but_remove,
    EN("...but remove"), JA("…ただし取り除く"), ZH_HANS("…但要移除"), ZH_HANT("…但要移除"),
    KO("…단, 제거할 것"), DE("… aber entfernen"), FR("… mais retirer"),
    ES("… pero quitar"), PT("… mas remover"),  IT("… ma rimuovere"),
    NL("… maar verwijderen"), RU("…но убрать"), TR("…ama kaldır"));

SS_MSG(mask_but_keep,
    EN("...but keep"),   JA("…ただし残す"),   ZH_HANS("…但要保留"), ZH_HANT("…但要保留"),
    KO("…단, 남길 것"),   DE("… aber behalten"), FR("… mais garder"),
    ES("… pero conservar"), PT("… mas manter"), IT("… ma tenere"),
    NL("… maar houden"), RU("…но оставить"), TR("…ama tut"));

SS_MSG(mask_negative_help_keep,
    EN("Exceptions that go even though they match the line above. Optional."),
    JA("上の行に当てはまっても取り除きたい例外です。省略できます。"),
    ZH_HANS("即使符合上一行也要剔除的例外。可以留空。"),
    ZH_HANT("即使符合上一行也要剔除的例外。可以留空。"),
    KO("위 줄에 해당하더라도 제거할 예외입니다. 비워 둬도 됩니다."),
    DE("Ausnahmen, die trotz Treffer in der Zeile darüber wegfallen. "
       "Optional."),
    FR("Exceptions qui partent quand même bien qu'elles correspondent à la "
       "ligne du dessus. Facultatif."),
    ES("Excepciones que se van aunque coincidan con la línea de arriba. "
       "Opcional."),
    PT("Exceções que saem mesmo correspondendo à linha acima. Opcional."),
    IT("Eccezioni che vanno via pur corrispondendo alla riga sopra. "
       "Facoltativo."),
    NL("Uitzonderingen die toch weggaan hoewel ze bij de regel hierboven "
       "passen. Optioneel."),
    RU("Исключения, которые всё же убираются, хотя и подходят под строку выше. "
       "Необязательно."),
    TR("Yukarıdaki satıra uysa bile yine de çıkarılacak istisnalar. İsteğe "
       "bağlı."));

SS_MSG(mask_negative_help_remove,
    EN("Exceptions that stay even though they match the line above. "
       "Optional."),
    JA("上の行に当てはまっても残したい例外です。省略できます。"),
    ZH_HANS("即使符合上一行也要保留的例外。可以留空。"),
    ZH_HANT("即使符合上一行也要保留的例外。可以留空。"),
    KO("위 줄에 해당하더라도 남겨 둘 예외입니다. 비워 둬도 됩니다."),
    DE("Ausnahmen, die trotz Treffer in der Zeile darüber bleiben. Optional."),
    FR("Exceptions qui restent bien qu'elles correspondent à la ligne du "
       "dessus. Facultatif."),
    ES("Excepciones que se quedan aunque coincidan con la línea de arriba. "
       "Opcional."),
    PT("Exceções que ficam mesmo correspondendo à linha acima. Opcional."),
    IT("Eccezioni che restano pur corrispondendo alla riga sopra. "
       "Facoltativo."),
    NL("Uitzonderingen die blijven hoewel ze bij de regel hierboven passen. "
       "Optioneel."),
    RU("Исключения, которые остаются, хотя и подходят под строку выше. "
       "Необязательно."),
    TR("Yukarıdaki satıra uysa bile kalacak istisnalar. İsteğe bağlı."));

SS_MSG(mask_features_only,
    EN("Hide from the reconstruction only"),
    JA("再構成からだけ隠す"),
    ZH_HANS("只在重建时避开"),
    ZH_HANT("只在重建時避開"),
    KO("재구성에서만 빼기"),
    DE("Nur vor der Rekonstruktion verbergen"),
    FR("Cacher à la reconstruction seulement"),
    ES("Ocultar solo a la reconstrucción"),
    PT("Esconder só da reconstrução"),
    IT("Nascondere solo alla ricostruzione"),
    NL("Alleen voor de reconstructie verbergen"),
    RU("Скрывать только от реконструкции"),
    TR("Yalnızca yeniden kurmadan gizle"));

SS_MSG(mask_features_only_help,
    EN("Excludes areas such as sky from camera features, not training. Writes feature_masks/ beside masks/. Preview shows amber hatching."),
    JA("空などをカメラ特徴点から除外し、学習には使います。masks/横のfeature_masks/に書きます。プレビューでは琥珀色の斜線です。"),
    ZH_HANS("从相机特征中排除天空等区域，训练仍使用。写入 masks/ 旁的 feature_masks/。预览显示为琥珀色斜线。"),
    ZH_HANT("從相機特徵中排除天空等區域，訓練仍使用。寫入 masks/ 旁的 feature_masks/。預覽顯示為琥珀色斜線。"),
    KO("하늘 등을 카메라 특징에서만 제외하고 학습에는 사용합니다. masks/ 옆 feature_masks/에 기록하며 미리보기는 황색 빗금입니다."),
    DE("Schließt etwa Himmel von Kameramerkmalen aus, nicht vom Training. Schreibt feature_masks/ neben masks/. Vorschau: bernsteinfarbene Schraffur."),
    FR("Exclut le ciel des caractéristiques caméra, pas de l’entraînement. Écrit feature_masks/ près de masks/. Aperçu : hachures ambrées."),
    ES("Excluye el cielo de características de cámara, no del entrenamiento. Escribe feature_masks/ junto a masks/. Vista previa: rayado ámbar."),
    PT("Exclui o céu das características da câmara, não do treino. Grava feature_masks/ junto a masks/. Prévia: hachuras âmbar."),
    IT("Esclude il cielo dalle caratteristiche delle camere, non dal training. Scrive feature_masks/ accanto a masks/. Anteprima: tratteggio ambra."),
    NL("Sluit o.a. lucht uit van camerakenmerken, niet van training. Schrijft feature_masks/ naast masks/. Voorbeeld: amberkleurige arcering."),
    RU("Исключает небо из признаков камер, но не обучения. Пишет feature_masks/ рядом с masks/. В предпросмотре — янтарная штриховка."),
    TR("Gökyüzünü kamera özelliklerinden çıkarır, eğitimden değil. masks/ yanına feature_masks/ yazar. Önizleme: kehribar tarama."));

SS_MSG(mask_advanced,
    EN("Advanced masking"),
    JA("マスクの詳細設定"),
    ZH_HANS("蒙版高级设置"),
    ZH_HANT("遮罩進階設定"),
    KO("마스킹 고급 설정"),
    DE("Erweiterte Maskierung"),
    FR("Masquage avancé"),
    ES("Enmascarado avanzado"),
    PT("Mascaramento avançado"),
    IT("Mascheratura avanzata"),
    NL("Geavanceerd maskeren"),
    RU("Дополнительно о масках"),
    TR("Gelişmiş maskeleme"));

SS_MSG(mask_threshold,
    EN("Detection threshold"),
    JA("検出のしきい値"),
    ZH_HANS("检测阈值"),
    ZH_HANT("偵測門檻"),
    KO("검출 임계값"),
    DE("Erkennungsschwelle"),
    FR("Seuil de détection"),
    ES("Umbral de detección"),
    PT("Limiar de detecção"),
    IT("Soglia di rilevamento"),
    NL("Detectiedrempel"),
    RU("Порог обнаружения"),
    TR("Algılama eşiği"));

SS_MSG(mask_threshold_help,
    EN("Detection confidence threshold. Lower catches hidden objects but adds false masks; higher keeps only confident matches."),
    JA("検出の確信度です。低いと隠れた対象も拾いますが誤マスクが増え、高いと確実な一致のみ残します。"),
    ZH_HANS("检测置信度阈值。低值能捕捉被遮挡的对象，但增加误蒙版；高值只保留可靠匹配。"),
    ZH_HANT("偵測信心度閾值。低值能捕捉被遮擋的對象，但增加誤遮罩；高值只保留可靠匹配。"),
    KO("검출 신뢰도 기준입니다. 낮으면 가려진 객체도 찾지만 오검출 마스크가 늘고, 높으면 확실한 매칭만 남습니다."),
    DE("Erkennungssicherheit. Niedriger findet verdeckte Objekte, maskiert aber auch falsch; höher behält nur sichere Treffer."),
    FR("Seuil de confiance. Plus bas trouve les objets cachés mais ajoute de faux masques ; plus haut garde les détections sûres."),
    ES("Umbral de confianza. Menor detecta objetos ocultos pero añade máscaras falsas; mayor conserva solo coincidencias seguras."),
    PT("Limiar de confiança. Menor encontra objetos ocultos mas cria máscaras falsas; maior guarda apenas correspondências seguras."),
    IT("Soglia di confidenza. Più bassa trova oggetti nascosti ma crea maschere false; più alta tiene solo risultati sicuri."),
    NL("Detectiezekerheid. Lager vindt verborgen objecten maar maakt ook foute maskers; hoger bewaart alleen zekere treffers."),
    RU("Порог уверенности. Ниже находит скрытые объекты, но добавляет ложные маски; выше сохраняет лишь надёжные совпадения."),
    TR("Tespit güven eşiği. Düşük değer gizli nesneleri bulur ama yanlış maskeler ekler; yüksek değer yalnız kesin eşleşmeleri tutar."));

SS_MSG(mask_nms,
    EN("Overlap threshold"),
    JA("重なりのしきい値"),
    ZH_HANS("重叠阈值"),
    ZH_HANT("重疊門檻"),
    KO("겹침 임계값"),
    DE("Überlappungsschwelle"),
    FR("Seuil de recouvrement"),
    ES("Umbral de solapamiento"),
    PT("Limiar de sobreposição"),
    IT("Soglia di sovrapposizione"),
    NL("Overlapdrempel"),
    RU("Порог перекрытия"),
    TR("Örtüşme eşiği"));

SS_MSG(mask_nms_help,
    EN("Keeps only the stronger detection when same-phrase masks overlap above this threshold. Raise it if nearby people are missed."),
    JA("同じ語句の検出がこの閾値以上重なると、強い方だけを残します。近くに並ぶ人物を見落とす場合は上げてください。"),
    ZH_HANS("同一提示词的检测重叠超过此阈值时，仅保留更强的检测。遗漏靠近的人时可调高。"),
    ZH_HANT("同一提示詞的偵測重疊超過此閾值時，僅保留較強的偵測。遺漏靠近的人時可調高。"),
    KO("같은 문구의 검출이 이 기준보다 많이 겹치면 더 강한 것만 남깁니다. 가까이 있는 사람이 누락되면 높이세요."),
    DE("Behält bei überlappenden Erkennungen derselben Phrase nur die stärkere. Schwelle erhöhen, wenn nahe beieinander stehende Menschen fehlen."),
    FR("Ne garde que la détection la plus forte si celles d’une même phrase se chevauchent au-delà du seuil. Augmenter si des personnes proches manquent."),
    ES("Conserva solo la detección más fuerte si las de una misma frase superan este solape. Aumenta si faltan personas cercanas entre sí."),
    PT("Guarda só a deteção mais forte se as da mesma frase excederem esta sobreposição. Aumente se faltarem pessoas próximas umas das outras."),
    IT("Tiene solo la rilevazione più forte se quelle della stessa frase superano questa sovrapposizione. Alzare se mancano persone vicine tra loro."),
    NL("Behoudt alleen de sterkste detectie als dezelfde zin boven deze drempel overlapt. Verhoog als mensen dicht bij elkaar ontbreken."),
    RU("Оставляет сильнейшее обнаружение одной фразы при перекрытии выше порога. Увеличьте, если пропускаются стоящие рядом люди."),
    TR("Aynı ifadenin tespitleri bu eşikten fazla örtüşürse güçlü olanı tutar. Yakın duran insanlar kaçırılıyorsa artırın."));

SS_MSG(mask_dilate_remove,
    EN("Extra margin around what's removed"),
    JA("消すものの周りの余白"),
    ZH_HANS("移除对象周围的额外边距"),
    ZH_HANT("移除對象周圍的額外邊距"),
    KO("지울 대상 주변 여백"),
    DE("Zusätzlicher Rand um das Entfernte"),
    FR("Marge autour de ce qui est retiré"),
    ES("Margen alrededor de lo que se quita"),
    PT("Margem à volta do que é removido"),
    IT("Margine attorno a ciò che viene rimosso"),
    NL("Extra marge rond wat wordt verwijderd"),
    RU("Отступ вокруг удаляемого"),
    TR("Kaldırılanın çevresinde ek pay"));

SS_MSG(mask_dilate_keep,
    EN("Trim in from the edge of what's kept"),
    JA("残すものの縁を内側へ削る"),
    ZH_HANS("从保留对象的边缘向内收"),
    ZH_HANT("從保留對象的邊緣向內收"),
    KO("남길 대상의 가장자리를 안쪽으로 깎기"),
    DE("Vom Rand des Behaltenen nach innen abtragen"),
    FR("Rogner vers l'intérieur du bord de ce qui est gardé"),
    ES("Recortar hacia dentro desde el borde de lo que se conserva"),
    PT("Aparar para dentro a partir da borda do que é mantido"),
    IT("Rifilare verso l'interno dal bordo di ciò che viene mantenuto"),
    NL("Vanaf de rand van wat behouden blijft naar binnen bijsnijden"),
    RU("Срезать внутрь от края сохраняемого"),
    TR("Korunanın kenarından içeri doğru kırp"));

SS_MSG(mask_shrink_help,
    EN("Trims kept objects by a percentage of their size to remove background rims. 0% keeps the outline; frame edges stay unchanged."),
    JA("残す対象を大きさに対する割合で縮め、背景の縁を除きます。0%は元の輪郭を保ち、画像の端は削りません。"),
    ZH_HANS("按对象自身尺寸的百分比向内收缩，去掉残留的背景边缘。0% 保持原轮廓；画面边缘不收缩。"),
    ZH_HANT("按對象自身尺寸的百分比向內收縮，去掉殘留的背景邊緣。0% 保持原輪廓；畫面邊緣不收縮。"),
    KO("남길 대상을 크기의 일정 비율만큼 줄여 배경 테두리를 제거합니다. 0%는 원래 윤곽을 유지하며, 화면 가장자리는 줄이지 않습니다."),
    DE("Verkleinert behaltene Objekte prozentual, um Hintergrundsäume zu entfernen. 0% erhält die Kontur; Bildränder bleiben unverändert."),
    FR("Réduit les objets conservés en proportion de leur taille pour retirer les liserés de fond. 0% garde le contour ; les bords du cadre restent inchangés."),
    ES("Reduce los objetos conservados según su tamaño para quitar bordes de fondo. 0% mantiene el contorno; los bordes del encuadre no cambian."),
    PT("Reduz os objetos mantidos em proporção ao seu tamanho para remover bordas de fundo. 0% mantém o contorno; as bordas do quadro não mudam."),
    IT("Riduce gli oggetti mantenuti in proporzione alla loro dimensione per eliminare bordi di sfondo. 0% conserva il contorno; i bordi del fotogramma non cambiano."),
    NL("Verkleint behouden objecten met een percentage van hun grootte om achtergrondrandjes te verwijderen. 0% behoudt de omtrek; beeldranden blijven gelijk."),
    RU("Уменьшает сохраняемые объекты на процент от их размера, убирая кайму фона. 0% сохраняет контур; края кадра не меняются."),
    TR("Arka plan kenarlarını gidermek için korunan nesneleri boyutlarının yüzdesi kadar küçültür. %0 sınırı korur; kare kenarları değişmez."));

SS_MSG(mask_dilate_help,
    EN("Expands excluded objects by a percentage of their size to remove color fringes. 0% keeps the original outline."),
    JA("除外する対象を大きさに対する割合で広げ、色の縁が残るのを防ぎます。0%は元の輪郭を保ちます。"),
    ZH_HANS("按对象自身尺寸的百分比向外扩张排除区域，去掉残留的色边。0% 保持原轮廓。"),
    ZH_HANT("按對象自身尺寸的百分比向外擴張排除區域，去掉殘留的色邊。0% 保持原輪廓。"),
    KO("제외할 대상을 크기의 일정 비율만큼 넓혀 색 테두리가 남지 않게 합니다. 0%는 원래 윤곽을 유지합니다."),
    DE("Vergrößert ausgeschlossene Objekte prozentual, um Farbsäume zu entfernen. 0% erhält die ursprüngliche Kontur."),
    FR("Agrandit les objets exclus en proportion de leur taille pour retirer les franges colorées. 0% garde le contour initial."),
    ES("Amplía los objetos excluidos según su tamaño para eliminar halos de color. 0% mantiene el contorno original."),
    PT("Amplia os objetos excluídos em proporção ao seu tamanho para remover halos de cor. 0% mantém o contorno original."),
    IT("Espande gli oggetti esclusi in proporzione alla loro dimensione per eliminare aloni di colore. 0% conserva il contorno originale."),
    NL("Vergroot uitgesloten objecten met een percentage van hun grootte om kleurrandjes te verwijderen. 0% behoudt de oorspronkelijke omtrek."),
    RU("Расширяет исключаемые объекты на процент от их размера, убирая цветную кайму. 0% сохраняет исходный контур."),
    TR("Renk saçaklarını gidermek için dışlanan nesneleri boyutlarının yüzdesi kadar genişletir. %0 özgün sınırı korur."));

SS_MSG(mask_max_size,
    EN("Maximum image size"),
    JA("画像の最大サイズ"),
    ZH_HANS("图像最大尺寸"),
    ZH_HANT("影像最大尺寸"),
    KO("이미지 최대 크기"),
    DE("Maximale Bildgröße"),
    FR("Taille d'image maximale"),
    ES("Tamaño máximo de imagen"),
    PT("Tamanho máximo da imagem"),
    IT("Dimensione massima dell'immagine"),
    NL("Maximale beeldgrootte"),
    RU("Максимальный размер изображения"),
    TR("En büyük görüntü boyutu"));

SS_MSG(mask_max_size_help,
    EN("Longest side used for masking. Smaller is faster but coarsens edges; output masks keep the original resolution. 0 uses source size."),
    JA("マスク処理時の長辺です。小さい値は速い一方、輪郭が粗くなります。出力は元の解像度です。0は元サイズを使います。"),
    ZH_HANS("蒙版处理时的最长边。越小越快，但边缘较粗；输出蒙版保持原分辨率。0 使用原始尺寸。"),
    ZH_HANT("遮罩處理時的最長邊。越小越快，但邊緣較粗；輸出遮罩保持原解析度。0 使用原始尺寸。"),
    KO("마스크 처리의 긴 변입니다. 작을수록 빠르지만 가장자리가 거칠어집니다. 출력은 원본 해상도이며 0은 원본 크기입니다."),
    DE("Längste Seite beim Maskieren. Kleiner ist schneller, aber gröber am Rand; Ausgabemasken behalten die Originalauflösung. 0 nutzt die Quellgröße."),
    FR("Côté long du masquage. Plus petit est plus rapide mais moins précis aux bords ; sortie à la résolution d’origine. 0 utilise la taille source."),
    ES("Lado largo para máscaras. Menor es más rápido pero empeora los bordes; la salida conserva la resolución original. 0 usa el tamaño fuente."),
    PT("Lado maior para máscaras. Menor é mais rápido mas piora os contornos; a saída mantém a resolução original. 0 usa o tamanho original."),
    IT("Lato massimo per le maschere. Minore è più rapido ma rende i bordi grossolani; uscita alla risoluzione originale. 0 usa la dimensione sorgente."),
    NL("Langste zijde voor maskers. Kleiner is sneller maar geeft grovere randen; uitvoer behoudt de originele resolutie. 0 gebruikt bronformaat."),
    RU("Длинная сторона для масок. Меньше — быстрее, но края грубее; выход сохраняет исходное разрешение. 0 использует исходный размер."),
    TR("Maskelemede uzun kenar. Küçük değer hızlı ama kenarları kabadır; çıktı özgün çözünürlüğü korur. 0 kaynak boyutunu kullanır."));

SS_MSG(mask_memory,
    EN("Track objects across frames"),
    JA("フレームをまたいで物体を追跡する"),
    ZH_HANS("跨帧跟踪物体"),
    ZH_HANT("跨影格追蹤物體"),
    KO("프레임을 넘어 물체 추적"),
    DE("Objekte über Bilder hinweg verfolgen"),
    FR("Suivre les objets d'une image à l'autre"),
    ES("Seguir los objetos entre fotogramas"),
    PT("Seguir os objetos entre quadros"),
    IT("Seguire gli oggetti tra i fotogrammi"),
    NL("Objecten over beelden heen volgen"),
    RU("Отслеживать объекты между кадрами"),
    TR("Nesneleri kareler boyunca izle"));

SS_MSG(mask_memory_help,
    EN("Tracks objects across frames, including brief disappearances. Adds a pass per object per frame. Clicked objects always use tracking."),
    JA("対象を一時的な見失いも含めてフレーム間で追跡します。各フレームの対象ごとに追加処理が必要です。クリック指定の対象は常に追跡します。"),
    ZH_HANS("跨帧追踪对象，可应对短暂消失。每帧每个对象增加一次处理。点击指定的对象始终使用追踪。"),
    ZH_HANT("跨影格追蹤對象，可應對短暫消失。每影格每個對象增加一次處理。點擊指定的對象始終使用追蹤。"),
    KO("잠시 사라지는 대상도 프레임 간 추적합니다. 매 프레임 객체마다 추가 처리가 필요합니다. 클릭한 객체는 항상 추적합니다."),
    DE("Verfolgt Objekte über Bilder hinweg, auch bei kurzem Verschwinden. Ein Zusatzdurchlauf je Objekt und Bild. Angeklickte Objekte werden immer verfolgt."),
    FR("Suit les objets entre images, même après une brève disparition. Ajoute une passe par objet et image. Les objets cliqués sont toujours suivis."),
    ES("Sigue objetos entre fotogramas, aunque desaparezcan brevemente. Añade una pasada por objeto y fotograma. Los objetos marcados por clic siempre se siguen."),
    PT("Segue objetos entre fotogramas, mesmo quando somem brevemente. Acrescenta uma passagem por objeto e fotograma. Objetos clicados são sempre seguidos."),
    IT("Segue gli oggetti tra fotogrammi, anche dopo brevi scomparse. Aggiunge un passaggio per oggetto e fotogramma. Gli oggetti cliccati sono sempre seguiti."),
    NL("Volgt objecten tussen beelden, ook als ze kort verdwijnen. Voegt per object per beeld een extra doorgang toe. Aangeklikte objecten worden altijd gevolgd."),
    RU("Отслеживает объекты между кадрами, даже при кратком исчезновении. Дополнительный проход на объект и кадр. Объекты с кликами отслеживаются всегда."),
    TR("Kısa süre kaybolsalar da nesneleri kareler arasında izler. Her nesne ve kare için ek geçiş gerekir. Tıklanan nesneler her zaman izlenir."));

SS_MSG(mask_detect_every,
    EN("Detect every N frames"),
    JA("検出する間隔（フレーム）"),
    ZH_HANS("每隔几帧检测一次"),
    ZH_HANT("每隔幾格偵測一次"),
    KO("몇 프레임마다 검출"),
    DE("Nur jedes N-te Bild erkennen"),
    FR("Détecter une image sur N"),
    ES("Detectar cada N fotogramas"),
    PT("Detectar a cada N quadros"),
    IT("Rilevare ogni N fotogrammi"),
    NL("Elk N-de beeld detecteren"),
    RU("Обнаруживать раз в N кадров"),
    TR("N karede bir algıla"));

SS_MSG(mask_detect_every_help,
    EN("Look for new objects only every Nth frame and let the memory carry the ones already found in between.\nBigger is faster and slower to notice something that walks into the shot. 1 = every frame."),
    JA("新しい物体を探すのは N フレームに 1 回だけにして、その間は見つけ済みのものをメモリで持ち越します。\n大きくすると速くなりますが、途中で入ってきたものが見つかるのは遅くなります。 1 で毎フレーム検出します。"),
    ZH_HANS("只每隔 N 帧找一次新物体，中间已经找到的靠记忆带过去。调大更快，但中途走进画面的东西会晚一些才被发现。填 1 表示每帧都检测。"),
    ZH_HANT("只每隔 N 格找一次新物體，中間已經找到的靠記憶帶過去。調大更快，但中途走進畫面的東西會晚一些才被發現。填 1 表示每格都偵測。"),
    KO("새 물체는 N 프레임마다 한 번만 찾고, 그 사이에는 이미 찾은 것을 메모리로 이어 갑니다. 크게 잡으면 빨라지지만 도중에 들어온 것을 늦게 알아차립니다. 1이면 매 프레임 검출합니다."),
    DE("Sucht nur jedes N-te Bild nach neuen Objekten; dazwischen wird verfolgt. Größer ist schneller, erkennt neue Objekte aber später. 1 = jedes Bild."),
    FR("Ne chercher de nouveaux objets qu'une image sur N ; entre-temps, la mémoire porte ceux déjà trouvés.\nPlus grand est plus rapide et remarque plus tard ce qui entre dans le champ. 1 = chaque image."),
    ES("Buscar objetos nuevos solo cada N fotogramas; entre medias, la memoria lleva los ya encontrados.\nMás grande es más rápido y tarda más en notar lo que entra en cuadro. 1 = cada fotograma."),
    PT("Procurar objetos novos só a cada N quadros; no intervalo, a memória carrega os já encontrados.\nMaior é mais rápido e demora mais a notar o que entra em cena. 1 = todos os quadros."),
    IT("Cercare nuovi oggetti solo ogni N fotogrammi; nel mezzo la memoria porta quelli già trovati. Più grande è più veloce e nota più tardi ciò che entra in campo. 1 = ogni fotogramma."),
    NL("Alleen elk N-de beeld op nieuwe objecten doorzoeken; daartussen draagt het geheugen de al gevonden objecten.\nGroter is sneller en merkt later op wat het beeld in loopt. 1 = elk beeld."),
    RU("Искать новые объекты лишь раз в N кадров, а между ними вести уже найденные памятью. Больше -- быстрее, но позже заметит то, что вошло в кадр. 1 -- каждый кадр."),
    TR("Yeni nesneleri yalnızca N karede bir ara; arada bulunmuş olanları bellek taşır. Büyütmek hızlandırır ama kareye gireni daha geç fark eder. 1 = her kare."));

SS_MSG(mask_memory_frames,
    EN("Memory frames"),
    JA("記憶するフレーム数"),
    ZH_HANS("记忆帧数"),
    ZH_HANT("記憶影格數"),
    KO("기억할 프레임 수"),
    DE("Gedächtnisbilder"),
    FR("Images en mémoire"),
    ES("Fotogramas de memoria"),
    PT("Quadros de memória"),
    IT("Fotogrammi di memoria"),
    NL("Geheugenbeelden"),
    RU("Кадров в памяти"),
    TR("Bellekteki kare sayısı"));

SS_MSG(mask_memory_frames_help,
    EN("Past frames remembered per object. 2–3 is faster than the usual 7 but loses objects hidden longer. 0 uses the model default."),
    JA("対象ごとに記憶する過去フレーム数です。2〜3は通常の7より高速ですが、長く隠れた対象を失います。0はモデルの既定値です。"),
    ZH_HANS("每个对象记忆的历史帧数。2–3 比通常的 7 更快，但会丢失遮挡更久的对象。0 使用模型默认值。"),
    ZH_HANT("每個對象記憶的歷史影格數。2–3 比通常的 7 更快，但會丟失遮擋更久的對象。0 使用模型預設值。"),
    KO("객체가 기억할 과거 프레임 수입니다. 2~3은 보통의 7보다 빠르지만 오래 가려진 객체를 놓칩니다. 0은 모델 기본값입니다."),
    DE("Vergangene Bilder je Objekt im Gedächtnis. 2–3 ist schneller als meist 7, verliert aber länger verdeckte Objekte. 0 nutzt die Modellvorgabe."),
    FR("Images passées mémorisées par objet. 2–3 est plus rapide que 7, mais perd les objets cachés plus longtemps. 0 utilise le défaut du modèle."),
    ES("Fotogramas pasados recordados por objeto. 2–3 es más rápido que los 7 habituales, pero pierde objetos ocultos más tiempo. 0 usa el valor del modelo."),
    PT("Fotogramas passados lembrados por objeto. 2–3 é mais rápido que os 7 habituais, mas perde objetos ocultos por mais tempo. 0 usa o valor do modelo."),
    IT("Fotogrammi passati ricordati per oggetto. 2–3 è più rapido dei soliti 7, ma perde oggetti nascosti più a lungo. 0 usa il valore del modello."),
    NL("Onthouden eerdere beelden per object. 2–3 is sneller dan de gebruikelijke 7, maar verliest langer verborgen objecten. 0 gebruikt de modelstandaard."),
    RU("Прошлые кадры в памяти объекта. 2–3 быстрее обычных 7, но теряет дольше скрытые объекты. 0 использует значение модели."),
    TR("Nesne başına hatırlanan geçmiş kareler. 2–3, normal 7’den hızlıdır ama uzun gizlenen nesneleri kaybeder. 0 model varsayılanıdır."));

SS_MSG(use_found_masks,
    EN("Use the masks found next to the photos"),
    JA("写真のとなりで見つかったマスクを使う"),
    ZH_HANS("使用照片旁边找到的蒙版"),
    ZH_HANT("使用照片旁邊找到的遮罩"),
    KO("사진 옆에서 찾은 마스크 사용"),
    DE("Neben den Fotos gefundene Masken verwenden"),
    FR("Utiliser les masques trouvés à côté des photos"),
    ES("Usar las máscaras encontradas junto a las fotos"),
    PT("Usar as máscaras encontradas ao lado das fotos"),
    IT("Usare le maschere trovate accanto alle foto"),
    NL("De maskers gebruiken die naast de foto's staan"),
    RU("Использовать маски, найденные рядом с фотографиями"),
    TR("Fotoğrafların yanında bulunan maskeleri kullan"));

SS_MSG(use_found_masks_help,
    EN("Automatically uses masks/ beside or inside the photo folder. Disable if those masks belong to other views or a different experiment."),
    JA("写真フォルダ内や隣のmasks/を自動使用します。別の視点や実験用のマスクなら無効にしてください。"),
    ZH_HANS("自动使用照片文件夹内或旁边的 masks/。若这些蒙版属于其他视角或不同实验，请关闭。"),
    ZH_HANT("自動使用照片資料夾內或旁邊的 masks/。若這些遮罩屬於其他視角或不同實驗，請關閉。"),
    KO("사진 폴더 안이나 옆의 masks/를 자동 사용합니다. 다른 뷰나 실험용 마스크라면 끄세요."),
    DE("Nutzt automatisch masks/ im oder neben dem Fotoordner. Deaktivieren, wenn die Masken zu anderen Ansichten oder Experimenten gehören."),
    FR("Utilise automatiquement masks/ dans ou près du dossier photo. Désactiver si ces masques sont pour d’autres vues ou expériences."),
    ES("Usa automáticamente masks/ dentro o junto a la carpeta de fotos. Desactiva si esas máscaras son de otras vistas o experimentos."),
    PT("Usa automaticamente masks/ dentro ou junto à pasta de fotos. Desligue se essas máscaras são de outras vistas ou experiências."),
    IT("Usa automaticamente masks/ dentro o accanto alla cartella foto. Disattivare se le maschere sono per altre viste o esperimenti."),
    NL("Gebruikt automatisch masks/ in of naast de fotomap. Uitschakelen als de maskers bij andere beelden of experimenten horen."),
    RU("Автоматически использует masks/ внутри или рядом с папкой фото. Отключите, если маски относятся к другим видам или экспериментам."),
    TR("Fotoğraf klasörü içi veya yanındaki masks/ otomatik kullanılır. Maskeler başka görünümlere ya da deneye aitse kapatın."));

// ---------------------------------------------------------------------------
// Lens models offered by the built-in reconstruction (SfmRunner.h).
//
// The model NAMES -- OpenCV, Kannala-Brandt, thin prism -- are the names of
// the distortion models themselves and stay put; what is translated is the
// parenthetical that says which one to pick.
// ---------------------------------------------------------------------------

SS_MSG(lens_opencv,
    EN("OpenCV"),
    JA("OpenCV"),        ZH_HANS("OpenCV"),   ZH_HANT("OpenCV"),  KO("OpenCV"),
    DE("OpenCV"),        FR("OpenCV"),        ES("OpenCV"),       PT("OpenCV"),
    IT("OpenCV"),        NL("OpenCV"),        RU("OpenCV"),       TR("OpenCV"));

SS_MSG(lens_opencv_help,
    EN("The usual choice: an ordinary lens, corrected for barrel and pincushion distortion.\nPhones, compacts, SLRs, and action cameras that are not in a fisheye mode."),
    JA("ふつうはこれです。 通常のレンズを、樽型・糸巻き型のゆがみを含めて補正します。\nスマートフォン、コンパクト、一眼、魚眼モードでないアクションカメラ向けです。"),
    ZH_HANS("一般就选它：普通镜头，校正桶形和枕形畸变。\n手机、卡片机、单反，以及没有开鱼眼模式的运动相机。"),
    ZH_HANT("一般就選它：普通鏡頭，校正桶形和枕形變形。\n手機、輕便相機、單眼，以及沒有開魚眼模式的運動相機。"),
    KO("보통은 이것입니다. 일반 렌즈를 통형·실패형 왜곡까지 보정합니다. 휴대폰, 콤팩트, DSLR, 어안 모드가 아닌 액션캠에 씁니다."),
    DE("Die übliche Wahl: ein gewöhnliches Objektiv, korrigiert um tonnen- und kissenförmige Verzeichnung.\nTelefone, Kompakte, Spiegelreflex und Actionkameras, die nicht im Fischaugenmodus sind."),
    FR("Le choix habituel : un objectif ordinaire, corrigé de la distorsion en barillet et en coussinet.\nTéléphones, compacts, reflex et caméras d'action qui ne sont pas en mode fisheye."),
    ES("La opción habitual: un objetivo normal, corregido de distorsión de barril y de cojín.\nTeléfonos, compactas, réflex y cámaras de acción que no estén en modo ojo de pez."),
    PT("A escolha habitual: uma lente comum, corrigida de distorção de barril e de almofada.\nTelefones, compactas, reflex e câmeras de ação que não estejam em modo olho de peixe."),
    IT("La scelta abituale: un obiettivo comune, corretto dalla distorsione a barile e a cuscinetto.\nTelefoni, compatte, reflex e action cam che non siano in modalità fisheye."),
    NL("De gewone keuze: een normaal objectief, gecorrigeerd voor ton- en kussenvormige vertekening.\nTelefoons, compacts, spiegelreflex en actiecamera's die niet in een fisheye-stand staan."),
    RU("Обычный выбор: обыкновенный объектив с поправкой на бочкообразную и подушкообразную дисторсию.\nТелефоны, компакты, зеркальные камеры и экшн-камеры не в режиме «рыбий глаз»."),
    TR("Alışılmış seçim: sıradan bir objektif, fıçı ve yastık bozulmasına göre düzeltilir.\nTelefonlar, kompaktlar, SLR'ler ve balıkgözü kipinde olmayan aksiyon kameraları."));

SS_MSG(lens_pinhole,
    EN("Pinhole"),
    JA("ピンホール"),     ZH_HANS("针孔"),      ZH_HANT("針孔"),
    KO("핀홀"),           DE("Lochkamera"),   FR("Sténopé"),      ES("Estenopeica"),
    PT("Estenopeica"),   IT("Stenopeica"),   NL("Gaatjescamera"),
    RU("Точечная камера"), TR("İğne deliği"));

SS_MSG(lens_pinhole_help,
    EN("An ideal lens with no distortion at all and a separate focal length "
       "for each axis. For photographs that have already been undistorted."),
    JA("ゆがみのない理想レンズで、焦点距離は縦横それぞれ持ちます。すでに"
       "ゆがみ補正済みの写真向けです。"),
    ZH_HANS("完全没有畸变的理想镜头，横竖各有一个焦距。用于已经做过畸变校正的"
            "照片。"),
    ZH_HANT("完全沒有變形的理想鏡頭，橫豎各有一個焦距。用於已經做過變形校正的"
            "相片。"),
    KO("왜곡이 전혀 없는 이상적인 렌즈로, 초점거리를 축마다 따로 가집니다. "
       "이미 왜곡을 편 사진에 씁니다."),
    DE("Ein ideales Objektiv ganz ohne Verzeichnung, mit eigener Brennweite "
       "je Achse. Für Aufnahmen, die schon entzerrt sind."),
    FR("Un objectif idéal, sans aucune distorsion, avec une focale par axe. "
       "Pour des photographies déjà redressées."),
    ES("Un objetivo ideal, sin distorsión alguna, con una focal por eje. Para "
       "fotografías que ya se han corregido."),
    PT("Uma lente ideal, sem distorção nenhuma, com uma distância focal por "
       "eixo. Para fotografias que já foram corrigidas."),
    IT("Un obiettivo ideale, senza alcuna distorsione, con una focale per "
       "asse. Per fotografie già corrette."),
    NL("Een ideaal objectief zonder enige vertekening, met een eigen "
       "brandpuntsafstand per as. Voor foto's die al ontvertekend zijn."),
    RU("Идеальный объектив совсем без дисторсии, с отдельным фокусным "
       "расстоянием по каждой оси. Для уже исправленных снимков."),
    TR("Hiç bozulması olmayan ideal bir objektif; her eksen için ayrı odak "
       "uzaklığı. Bozulması zaten giderilmiş fotoğraflar için."));

SS_MSG(lens_simple_pinhole,
    EN("Simple pinhole"),
    JA("単純ピンホール"),  ZH_HANS("简单针孔"),  ZH_HANT("簡單針孔"),
    KO("간단 핀홀"),
    DE("Einfache Lochkamera"),
    FR("Sténopé simple"),
    ES("Estenopeica simple"),
    PT("Estenopeica simples"),
    IT("Stenopeica semplice"),
    NL("Eenvoudige gaatjescamera"),
    RU("Простая точечная камера"),
    TR("Basit iğne deliği"));

SS_MSG(lens_simple_pinhole_help,
    EN("The same ideal lens with one focal length for both axes. For "
       "already-undistorted photographs from a camera with square pixels."),
    JA("同じ理想レンズで、焦点距離は縦横共通の1つです。画素が正方形の"
       "カメラの、ゆがみ補正済みの写真向けです。"),
    ZH_HANS("同样是理想镜头，但横竖共用一个焦距。用于像素为正方形的相机、"
            "且已做过畸变校正的照片。"),
    ZH_HANT("同樣是理想鏡頭，但橫豎共用一個焦距。用於像素為正方形的相機、"
            "且已做過變形校正的相片。"),
    KO("같은 이상적인 렌즈지만 초점거리를 두 축이 함께 씁니다. 화소가 정사각형인 "
       "카메라의, 왜곡을 이미 편 사진에 씁니다."),
    DE("Dasselbe ideale Objektiv mit einer Brennweite für beide Achsen. Für "
       "schon entzerrte Aufnahmen einer Kamera mit quadratischen Pixeln."),
    FR("Le même objectif idéal avec une seule focale pour les deux axes. Pour "
       "des photographies déjà redressées, d'un appareil à pixels carrés."),
    ES("El mismo objetivo ideal con una sola focal para ambos ejes. Para "
       "fotografías ya corregidas de una cámara de píxeles cuadrados."),
    PT("A mesma lente ideal com uma só distância focal para os dois eixos. "
       "Para fotografias já corrigidas de uma câmera de pixels quadrados."),
    IT("Lo stesso obiettivo ideale con una sola focale per entrambi gli assi. "
       "Per fotografie già corrette, da una fotocamera a pixel quadrati."),
    NL("Hetzelfde ideale objectief met één brandpuntsafstand voor beide "
       "assen. Voor al ontvertekende foto's van een camera met vierkante "
       "pixels."),
    RU("Тот же идеальный объектив с одним фокусным расстоянием на обе оси. "
       "Для уже исправленных снимков камеры с квадратными пикселями."),
    TR("Aynı ideal objektif, iki eksen için tek odak uzaklığıyla. Kare "
       "pikselli bir kameranın, bozulması zaten giderilmiş fotoğrafları "
       "için."));

SS_MSG(lens_radial,
    EN("Radial"),
    JA("放射方向のみ"),
    ZH_HANS("仅径向畸变"),
    ZH_HANT("僅徑向變形"),
    KO("방사 왜곡만"),
    DE("Radial"),
    FR("Radiale"),
    ES("Radial"),
    PT("Radial"),
    IT("Radiale"),
    NL("Radiaal"),
    RU("Радиальная"),
    TR("Işınsal"));

SS_MSG(lens_radial_help,
    EN("Radial distortion only, with no tangential terms. Fewer numbers to "
       "fit than OpenCV, which helps when there are few photographs or they "
       "overlap little."),
    JA("放射方向のゆがみだけを扱い、接線方向の項はありません。OpenCV より"
       "求めるべき数が少ないので、写真が少ない・重なりが乏しいときに向きます。"),
    ZH_HANS("只处理径向畸变，没有切向项。要拟合的参数比 OpenCV 少，照片少或"
            "重叠不多时更稳。"),
    ZH_HANT("只處理徑向變形，沒有切向項。要擬合的參數比 OpenCV 少，相片少或"
            "重疊不多時更穩。"),
    KO("방사 왜곡만 다루고 접선 항은 없습니다. OpenCV보다 맞출 값이 적어 사진이 "
       "적거나 겹침이 부족할 때 알맞습니다."),
    DE("Nur radiale Verzeichnung, ohne tangentiale Glieder. Weniger zu "
       "schätzende Zahlen als bei OpenCV, was bei wenigen oder kaum "
       "überlappenden Aufnahmen hilft."),
    FR("Distorsion radiale seule, sans termes tangentiels. Moins de nombres à "
       "ajuster qu'OpenCV, ce qui aide quand les photographies sont peu "
       "nombreuses ou se recouvrent peu."),
    ES("Solo distorsión radial, sin términos tangenciales. Menos números que "
       "ajustar que con OpenCV, lo que ayuda cuando hay pocas fotografías o "
       "se solapan poco."),
    PT("Só distorção radial, sem termos tangenciais. Menos números a ajustar "
       "que o OpenCV, o que ajuda quando há poucas fotografias ou elas se "
       "sobrepõem pouco."),
    IT("Solo distorsione radiale, senza termini tangenziali. Meno numeri da "
       "stimare rispetto a OpenCV, il che aiuta quando le fotografie sono "
       "poche o si sovrappongono poco."),
    NL("Alleen radiale vertekening, zonder tangentiële termen. Minder te "
       "schatten getallen dan OpenCV, wat helpt bij weinig of nauwelijks "
       "overlappende foto's."),
    RU("Только радиальная дисторсия, без тангенциальных членов. Подбирать "
       "нужно меньше чисел, чем в OpenCV, — это выручает, когда снимков мало "
       "или они плохо перекрываются."),
    TR("Yalnızca ışınsal bozulma, teğetsel terimler olmadan. OpenCV'ye göre "
       "daha az sayı oturtulur; fotoğraf az olduğunda veya az örtüştüğünde "
       "işe yarar."));

SS_MSG(lens_full_opencv,
    EN("Full OpenCV"),
    JA("OpenCV（全パラメータ）"),
    ZH_HANS("OpenCV（完整参数）"),
    ZH_HANT("OpenCV（完整參數）"),
    KO("OpenCV(전체 계수)"),
    DE("OpenCV (vollständig)"),
    FR("OpenCV complet"),
    ES("OpenCV completo"),
    PT("OpenCV completo"),
    IT("OpenCV completo"),
    NL("Volledige OpenCV"),
    RU("OpenCV (полная модель)"),
    TR("Tam OpenCV"));

SS_MSG(lens_full_opencv_help,
    EN("OpenCV with its whole set of distortion terms. Worth it only for a "
       "strongly distorted wide lens photographed often enough to pin all of "
       "them down."),
    JA("OpenCV のゆがみ項をすべて使います。ゆがみの大きい広角レンズを、"
       "すべての項を決められるだけ多く撮った場合にだけ意味があります。"),
    ZH_HANS("使用 OpenCV 的全部畸变项。只有畸变很大的广角镜头、并且拍得足够多能"
            "定住所有参数时才值得。"),
    ZH_HANT("使用 OpenCV 的全部變形項。只有變形很大的廣角鏡頭、並且拍得足夠多能"
            "定住所有參數時才值得。"),
    KO("OpenCV의 왜곡 항을 모두 씁니다. 왜곡이 큰 광각 렌즈를, 모든 항을 정할 "
       "만큼 충분히 찍었을 때에만 쓸모가 있습니다."),
    DE("OpenCV mit dem vollen Satz an Verzeichnungsgliedern. Lohnt sich nur "
       "bei einem stark verzeichnenden Weitwinkel, das oft genug fotografiert "
       "wurde, um alle festzulegen."),
    FR("OpenCV avec l'ensemble de ses termes de distorsion. N'en vaut la "
       "peine que pour un grand-angle très déformant, photographié assez "
       "souvent pour tous les fixer."),
    ES("OpenCV con todos sus términos de distorsión. Solo compensa con un "
       "gran angular muy distorsionado, fotografiado las veces suficientes "
       "para fijarlos todos."),
    PT("OpenCV com todo o seu conjunto de termos de distorção. Só compensa "
       "com uma grande-angular muito distorcida, fotografada o bastante para "
       "fixar todos eles."),
    IT("OpenCV con tutti i suoi termini di distorsione. Conviene solo con un "
       "grandangolo molto distorcente, fotografato abbastanza da fissarli "
       "tutti."),
    NL("OpenCV met de volledige reeks vertekeningstermen. Alleen de moeite "
       "waard bij een sterk vertekenende groothoek die vaak genoeg is "
       "gefotografeerd om ze alle vast te leggen."),
    RU("OpenCV со всем набором членов дисторсии. Оправдан только для сильно "
       "искажающего широкоугольника, снятого достаточно много раз, чтобы "
       "определить их все."),
    TR("OpenCV'nin bozulma terimlerinin tamamıyla. Yalnızca çok bozan bir "
       "geniş açı objektif, hepsini belirleyecek kadar çok çekildiyse "
       "değer."));

SS_MSG(lens_fisheye_opencv,
    EN("Fisheye (OpenCV)"),
    JA("魚眼（OpenCV）"),
    ZH_HANS("鱼眼（OpenCV）"),
    ZH_HANT("魚眼（OpenCV）"),
    KO("어안(OpenCV)"),
    DE("Fisheye (OpenCV)"),
    FR("Fisheye (OpenCV)"),
    ES("Ojo de pez (OpenCV)"),
    PT("Olho de peixe (OpenCV)"),
    IT("Fisheye (OpenCV)"),
    NL("Fisheye (OpenCV)"),
    RU("Фишай (OpenCV)"),
    TR("Balıkgözü (OpenCV)"));

SS_MSG(lens_fisheye_opencv_help,
    EN("A fisheye lens, up to about 180 degrees across.\n- The standard model for one fisheye, and for the wide modes of most action cameras.\n- It is also known as the Kannala-Brandt model, and COLMAP calls it OPENCV_FISHEYE."),
    JA("およそ180度までの魚眼レンズです。 単一の魚眼、および多くのアクションカメラの広角モードの標準的なモデルです。\nKannala-Brandt モデルとも呼ばれ、COLMAP では OPENCV_FISHEYE という名前です。"),
    ZH_HANS("视场约到 180 度的鱼眼镜头。单个鱼眼的标准模型，多数运动相机的广角模式也是这个。它也叫 Kannala-Brandt 模型，COLMAP 里的名字是 OPENCV_FISHEYE。"),
    ZH_HANT("視場約到 180 度的魚眼鏡頭。單個魚眼的標準模型，多數運動相機的廣角模式也是這個。它也叫 Kannala-Brandt 模型，COLMAP 裡的名字是 OPENCV_FISHEYE。"),
    KO("대략 180도까지의 어안 렌즈입니다. 어안 하나에 대한 표준 모델이며, 대부분 액션캠의 광각 모드도 이것입니다. Kannala-Brandt 모델이라고도 하며, COLMAP에서는 OPENCV_FISHEYE입니다."),
    DE("Ein Fischaugenobjektiv bis etwa 180 Grad.\n- Das Standardmodell für ein einzelnes Fischauge und für die Weitwinkelmodi der meisten Actionkameras.\n- Es heißt auch Kannala-Brandt-Modell, in COLMAP OPENCV_FISHEYE."),
    FR("Un objectif fisheye, jusqu'à environ 180 degrés.\n- Le modèle standard pour un fisheye, et pour les modes grand-angle de la plupart des caméras d'action.\n- On l'appelle aussi modèle de Kannala-Brandt, et COLMAP le nomme OPENCV_FISHEYE."),
    ES("Un objetivo ojo de pez, de hasta unos 180 grados.\n- El modelo estándar para un ojo de pez, y para los modos angulares de casi todas las cámaras de acción.\n- También se conoce como modelo de Kannala-Brandt, y en COLMAP se llama OPENCV_FISHEYE."),
    PT("Uma lente olho de peixe, de até cerca de 180 graus.\n- O modelo padrão para um olho de peixe, e para os modos grande-angulares da maioria das câmeras de ação.\n- Também é conhecido como modelo de Kannala-Brandt, e no COLMAP chama-se OPENCV_FISHEYE."),
    IT("Fisheye fino a circa 180°, anche per modalità ampie delle action cam. Modello Kannala-Brandt, chiamato OPENCV_FISHEYE in COLMAP."),
    NL("Een fisheye-objectief, tot ongeveer 180 graden.\n- Het standaardmodel voor één fisheye, en voor de groothoekstanden van de meeste actiecamera's.\n- Het heet ook het Kannala-Brandt-model, en in COLMAP OPENCV_FISHEYE."),
    RU("Объектив «рыбий глаз» примерно до 180 градусов.\n- Стандартная модель для одного фишая и для широкоугольных режимов большинства экшн-камер.\n- Она же модель Каннала — Брандта, а в COLMAP называется OPENCV_FISHEYE."),
    TR("Yaklaşık 180 dereceye kadar bir balıkgözü objektif.\n- Tek balıkgözü için ve çoğu aksiyon kamerasının geniş açı kipleri için standart model.\n- Kannala-Brandt modeli olarak da bilinir, COLMAP'te adı OPENCV_FISHEYE'dir."));

SS_MSG(lens_same_as_above,
    EN("(same as above)"),
    JA("（上と同じ）"),   ZH_HANS("（同上）"),  ZH_HANT("（同上）"),
    KO("(위와 같음)"),   DE("(wie oben)"),    FR("(comme ci-dessus)"),
    ES("(igual que arriba)"), PT("(igual ao de cima)"), IT("(come sopra)"),
    NL("(zelfde als hierboven)"), RU("(как выше)"),
    TR("(yukarıdakiyle aynı)"));

SS_MSG(lens_same_as_above_help,
    EN("Follows the lens model in the row above. Set a different model to start a new group; following rows inherit that choice."),
    JA("上の行のレンズモデルに従います。別のモデルを選ぶと新しいグループとなり、後の行がその選択を引き継ぎます。"),
    ZH_HANS("沿用上一行的镜头模型。选择其他模型会开始新组，后续行继承该选择。"),
    ZH_HANT("沿用上一列的鏡頭模型。選擇其他模型會開始新組，後續列繼承該選擇。"),
    KO("위 행의 렌즈 모델을 따릅니다. 다른 모델을 고르면 새 그룹이 시작되어 이후 행이 그 선택을 따릅니다."),
    DE("Übernimmt das Objektivmodell der Zeile darüber. Ein anderes Modell beginnt eine neue Gruppe; folgende Zeilen übernehmen diese Wahl."),
    FR("Suit le modèle de la ligne précédente. Un autre modèle ouvre un nouveau groupe ; les lignes suivantes héritent de ce choix."),
    ES("Sigue el modelo de la fila anterior. Elegir otro inicia un grupo nuevo; las filas siguientes heredan esa elección."),
    PT("Segue o modelo da linha acima. Escolher outro inicia um novo grupo; as linhas seguintes herdam essa escolha."),
    IT("Segue il modello della riga sopra. Un altro modello avvia un nuovo gruppo; le righe seguenti ereditano la scelta."),
    NL("Volgt het lensmodel van de rij erboven. Een ander model begint een nieuwe groep; volgende rijen erven die keuze."),
    RU("Наследует модель объектива строки выше. Другая модель начинает новую группу; следующие строки наследуют этот выбор."),
    TR("Üst satırdaki mercek modelini izler. Başka model yeni grup başlatır; sonraki satırlar bu seçimi devralır."));

SS_MSG(lens_fisheye_thin_prism,
    EN("Fisheye (thin prism)"),
    JA("魚眼（薄プリズム）"),
    ZH_HANS("鱼眼（薄棱镜）"),
    ZH_HANT("魚眼（薄稜鏡）"),
    KO("어안(얇은 프리즘)"),
    DE("Fisheye (dünnes Prisma)"),
    FR("Fisheye (prisme mince)"),
    ES("Ojo de pez (prisma delgado)"),
    PT("Olho de peixe (prisma fino)"),
    IT("Fisheye (prisma sottile)"),
    NL("Fisheye (dun prisma)"),
    RU("Фишай (тонкая призма)"),
    TR("Balıkgözü (ince prizma)"));

SS_MSG(lens_fisheye_thin_prism_help,
    EN("Fisheye with extra distortion parameters, for very wide lenses such as unstitched 360° cameras."),
    JA("追加の歪みパラメーターを持つ魚眼モデルです。未合成の360度カメラなど、非常に広いレンズ用です。"),
    ZH_HANS("带额外畸变参数的鱼眼模型，适用于未拼接 360° 相机等超广角镜头。"),
    ZH_HANT("帶額外畸變參數的魚眼模型，適用於未拼接 360° 相機等超廣角鏡頭。"),
    KO("추가 왜곡 매개변수를 갖춘 어안 모델로, 합성 전 360° 카메라 같은 매우 넓은 렌즈용입니다."),
    DE("Fischauge mit zusätzlichen Verzeichnungsparametern für sehr weite Objektive, etwa ungestitchte 360°-Kameras."),
    FR("Fisheye avec paramètres de distorsion supplémentaires, pour objectifs très larges comme les caméras 360° non assemblées."),
    ES("Ojo de pez con parámetros de distorsión extra, para lentes muy amplias como cámaras 360° sin coser."),
    PT("Olho de peixe com parâmetros extra de distorção, para lentes muito amplas como câmaras 360° não unidas."),
    IT("Fisheye con parametri di distorsione aggiuntivi, per lenti molto ampie come le camere 360° non cucite."),
    NL("Fisheye met extra vervormingsparameters, voor zeer brede lenzen zoals niet-samengevoegde 360°-camera’s."),
    RU("Рыбий глаз с дополнительными параметрами дисторсии для сверхшироких объективов, например несшитых камер 360°."),
    TR("Birleştirilmemiş 360° kameralar gibi çok geniş mercekler için ek bozulma parametreli balıkgözü modeli."));

SS_MSG(lens_equirectangular,
    EN("Equirectangular panorama"),
    JA("正距円筒パノラマ"),
    ZH_HANS("等距柱状全景"),
    ZH_HANT("等距柱狀全景"),
    KO("정거원통 파노라마"),
    DE("Äquirektangulares Panorama"),
    FR("Panorama équirectangulaire"),
    ES("Panorama equirrectangular"),
    PT("Panorama equirretangular"),
    IT("Panorama equirettangolare"),
    NL("Equirectangulair panorama"),
    RU("Эквиректангулярная панорама"),
    TR("Eş dikdörtgen panorama"));

SS_MSG(lens_equirectangular_help,
    EN("A stitched 360° panorama covering the whole sphere, with a 2:1 aspect ratio. Not for raw dual-fisheye frames."),
    JA("全天球を覆う2:1の合成済み360度パノラマ用です。未合成のデュアル魚眼フレームには使いません。"),
    ZH_HANS("覆盖整个球面的已拼接 360° 全景，宽高比 2:1。不适用于原始双鱼眼帧。"),
    ZH_HANT("覆蓋整個球面的已拼接 360° 全景，寬高比 2:1。不適用於原始雙魚眼影格。"),
    KO("전체 구를 덮는 2:1 비율의 합성된 360° 파노라마용입니다. 원본 이중 어안 프레임용이 아닙니다."),
    DE("Zusammengefügtes 360°-Panorama der ganzen Kugel im Verhältnis 2:1. Nicht für rohe Doppel-Fischaugenbilder."),
    FR("Panorama 360° assemblé couvrant toute la sphère, au format 2:1. Pas pour les images brutes à deux fisheyes."),
    ES("Panorama 360° cosido de toda la esfera, con proporción 2:1. No es para fotogramas de doble ojo de pez sin coser."),
    PT("Panorama 360° unido de toda a esfera, com proporção 2:1. Não é para fotogramas brutos de olho de peixe duplo."),
    IT("Panorama 360° cucito dell’intera sfera, formato 2:1. Non per fotogrammi dual-fisheye grezzi."),
    NL("Samengevoegd 360°-panorama van de hele bol met verhouding 2:1. Niet voor ruwe dubbele fisheyebeelden."),
    RU("Сшитая панорама 360° всей сферы с пропорциями 2:1. Не для исходных кадров с двумя кругами рыбьего глаза."),
    TR("Tüm küreyi kapsayan, 2:1 oranında birleştirilmiş 360° panorama. Ham çift balıkgözü kareler için değildir."));

SS_MSG(lens_warn_not_2to1,
    EN("A panorama has to be twice as wide as it is tall; this input is "
       "{0} x {1}. Reconstruction will read the wrong angle out of every "
       "pixel."),
    JA("パノラマは横が縦の2倍でなければなりませんが、この入力は {0} x {1} "
       "です。このままでは各画素の角度が誤って読み取られます。"),
    ZH_HANS("全景必须宽是高的两倍，而这个输入是 {0} x {1}。这样重建会把每个"
            "像素的角度算错。"),
    ZH_HANT("全景必須寬是高的兩倍，而這個輸入是 {0} x {1}。這樣重建會把每個"
            "像素的角度算錯。"),
    KO("파노라마는 가로가 세로의 두 배여야 하는데 이 입력은 {0} x {1}입니다. "
       "이대로면 재구성이 픽셀마다 잘못된 각도를 읽습니다."),
    DE("Ein Panorama muss doppelt so breit wie hoch sein; diese Eingabe ist "
       "{0} x {1}. Die Rekonstruktion liest dann aus jedem Pixel den falschen "
       "Winkel."),
    FR("Un panorama doit être deux fois plus large que haut ; cette entrée "
       "fait {0} x {1}. La reconstruction lira alors un angle faux dans "
       "chaque pixel."),
    ES("Un panorama tiene que ser el doble de ancho que de alto; esta entrada "
       "es de {0} x {1}. La reconstrucción leerá un ángulo equivocado en cada "
       "píxel."),
    PT("Um panorama tem de ser duas vezes mais largo que alto; esta entrada é "
       "de {0} x {1}. A reconstrução vai ler um ângulo errado em cada pixel."),
    IT("Un panorama deve essere largo il doppio dell'altezza; questo ingresso "
       "è {0} x {1}. La ricostruzione leggerà un angolo sbagliato da ogni "
       "pixel."),
    NL("Een panorama moet twee keer zo breed als hoog zijn; deze invoer is "
       "{0} x {1}. De reconstructie leest dan uit elke pixel de verkeerde "
       "hoek."),
    RU("Панорама должна быть вдвое шире, чем выше, а этот вход — {0} x {1}. "
       "Восстановление возьмёт из каждого пикселя неверный угол."),
    TR("Bir panoramanın eni boyunun iki katı olmalıdır; bu girdi {0} x {1}. "
       "Yeniden oluşturma her pikselden yanlış açıyı okur."));

SS_MSG(lens_warn_dual_fisheye,
    EN("This capture records two fisheye circles per frame, not a stitched panorama.\nPick a fisheye model -- Fisheye (thin prism) is the one these cameras fit."),
    JA("この素材は1フレームに魚眼の円を2つ記録しており、合成済みのパノラマではありません。\n魚眼のモデルを選んでください。 これらのカメラには「魚眼（薄プリズム）」が合います。"),
    ZH_HANS("这份素材每帧记录两个鱼眼圆，并不是拼接好的全景。\n请选鱼眼模型——这类相机适合“鱼眼（薄棱镜）”。"),
    ZH_HANT("這份素材每格記錄兩個魚眼圓，並不是拼接好的全景。\n請選魚眼模型——這類相機適合「魚眼（薄稜鏡）」。"),
    KO("이 촬영본은 한 프레임에 어안 원을 두 개 기록하며, 이어 붙인 파노라마가 아닙니다. 어안 모델을 고르세요. 이런 카메라에는 '어안(얇은 프리즘)'이 맞습니다."),
    DE("Diese Aufnahme zeichnet zwei Fischaugenkreise je Bild auf, kein zusammengefügtes Panorama.\nWählen Sie ein Fischaugenmodell -- Fisheye (dünnes Prisma) passt zu diesen Kameras."),
    FR("Cette prise enregistre deux cercles fisheye par image, pas un panorama assemblé.\nChoisissez un modèle fisheye : Fisheye (prisme mince) est celui qui convient à ces caméras."),
    ES("Esta toma graba dos círculos de ojo de pez por fotograma, no un panorama unido.\nElija un modelo de ojo de pez: Ojo de pez (prisma delgado) es el que encaja con estas cámaras."),
    PT("Esta captura grava dois círculos olho de peixe por quadro, não um panorama costurado.\nEscolha um modelo olho de peixe: Olho de peixe (prisma fino) é o que serve para essas câmeras."),
    IT("Questa ripresa registra due cerchi fisheye per fotogramma, non un panorama cucito.\nScelga un modello fisheye: Fisheye (prisma sottile) è quello adatto a queste fotocamere."),
    NL("Deze opname legt per beeld twee fisheye-cirkels vast, geen aaneengenaaid panorama.\nKies een fisheye-model -- Fisheye (dun prisma) past bij deze camera's."),
    RU("Эта съёмка записывает по два круга «рыбьего глаза» на кадр, а не сшитую панораму.\nВыберите модель фишая — этим камерам подходит «Фишай (тонкая призма)»."),
    TR("Bu çekim kare başına iki balıkgözü dairesi kaydeder, birleştirilmiş bir panorama değil.\nBir balıkgözü modeli seçin -- bu kameralara Balıkgözü (ince prizma) uyar."));

SS_MSG(lens_warn_needs_fisheye,
    EN("This capture is two fisheye circles per frame. An ordinary lens model "
       "may not fit them -- pick Fisheye (thin prism)."),
    JA("この素材は1フレームに魚眼の円が2つあります。通常のレンズモデルでは"
       "合わないことがあります。「魚眼（薄プリズム）」を選んでください。"),
    ZH_HANS("这份素材每帧是两个鱼眼圆，普通镜头模型可能拟合不了——请选“鱼眼"
            "（薄棱镜）”。"),
    ZH_HANT("這份素材每格是兩個魚眼圓，普通鏡頭模型可能擬合不了——請選「魚眼"
            "（薄稜鏡）」。"),
    KO("이 촬영본은 한 프레임에 어안 원이 두 개입니다. 일반 렌즈 모델로는 맞지 "
       "않을 수 있으니 '어안(얇은 프리즘)'을 고르세요."),
    DE("Diese Aufnahme besteht aus zwei Fischaugenkreisen je Bild. Ein "
       "gewöhnliches Objektivmodell passt darauf womöglich nicht -- wählen "
       "Sie Fisheye (dünnes Prisma)."),
    FR("Cette prise comporte deux cercles fisheye par image. Un modèle "
       "d'objectif ordinaire risque de ne pas s'y ajuster : choisissez "
       "Fisheye (prisme mince)."),
    ES("Esta toma son dos círculos de ojo de pez por fotograma. Un modelo de "
       "objetivo normal puede no ajustarse a ellos: elija Ojo de pez (prisma "
       "delgado)."),
    PT("Esta captura tem dois círculos olho de peixe por quadro. Um modelo de "
       "lente comum pode não se ajustar a eles -- escolha Olho de peixe "
       "(prisma fino)."),
    IT("Questa ripresa è fatta di due cerchi fisheye per fotogramma. Un "
       "modello di obiettivo comune potrebbe non adattarcisi: scelga Fisheye "
       "(prisma sottile)."),
    NL("Deze opname bestaat uit twee fisheye-cirkels per beeld. Een gewoon "
       "objectiefmodel past daar mogelijk niet op -- kies Fisheye (dun "
       "prisma)."),
    RU("В этой съёмке по два круга «рыбьего глаза» на кадр. Обычная модель "
       "объектива может к ним не подойти — выберите «Фишай (тонкая призма)»."),
    TR("Bu çekim kare başına iki balıkgözü dairesidir. Sıradan bir objektif "
       "modeli bunlara oturmayabilir -- Balıkgözü (ince prizma) seçin."));

// ---------------------------------------------------------------------------
// The common-subject palette (src/app/gui/MaskPrompt.h)
//
// The chip LABELS below are translated; the words they put in the box are
// not, and must not be. SAM 3's text encoder is trained on English, and a
// prompt in anything else finds noticeably less of what it names -- so the
// box stays English however the interface is set, and this palette is how a
// user who does not write English still gets a good prompt.
//
// These are label translations, not dictionary entries: each one should be
// what a speaker would call the thing IN A PHOTOGRAPH, not the closest
// dictionary word to the English.
// ---------------------------------------------------------------------------

SS_MSG(mask_english_only,
    EN("The model reads English only, so this box stays English whatever the "
       "interface language is. Pick from the list below, or type English "
       "words."),
    JA("モデルは英語しか読み取れないため、この欄は表示言語に関わらず英語のまま"
       "です。下の一覧から選ぶか、英語で入力してください。"),
    ZH_HANS("模型只能读英文，所以无论界面用哪种语言，这个输入框都保持英文。"
            "可以从下面的列表里选，或者直接输入英文词。"),
    ZH_HANT("模型只能讀英文，所以無論介面用哪種語言，這個輸入框都維持英文。"
            "可以從下面的清單挑選，或直接輸入英文詞。"),
    KO("모델은 영어만 읽으므로 인터페이스 언어와 관계없이 이 칸은 영어로 "
       "유지됩니다. 아래 목록에서 고르거나 영어 낱말을 입력하세요."),
    DE("Das Modell liest nur Englisch, deshalb bleibt dieses Feld englisch, "
       "unabhängig von der Sprache der Oberfläche. Wählen Sie aus der Liste "
       "unten, oder tippen Sie englische Wörter."),
    FR("Le modèle ne lit que l'anglais : ce champ reste donc en anglais quelle "
       "que soit la langue de l'interface. Choisissez dans la liste ci-dessous, "
       "ou tapez des mots anglais."),
    ES("El modelo solo lee inglés, así que este campo sigue en inglés sea cual "
       "sea el idioma de la interfaz. Elija de la lista de abajo, o escriba "
       "palabras en inglés."),
    PT("O modelo só lê inglês, por isso este campo continua em inglês seja qual "
       "for o idioma da interface. Escolha na lista abaixo, ou digite palavras "
       "em inglês."),
    IT("Il modello legge solo l'inglese, quindi questo campo resta in inglese "
       "qualunque sia la lingua dell'interfaccia. Scelga dall'elenco qui sotto, "
       "oppure scriva parole inglesi."),
    NL("Het model leest alleen Engels, dus dit veld blijft Engels ongeacht de "
       "taal van de interface. Kies uit de lijst hieronder, of typ Engelse "
       "woorden."),
    RU("Модель читает только по-английски, поэтому это поле остаётся "
       "английским при любом языке интерфейса. Выберите из списка ниже или "
       "введите английские слова."),
    TR("Model yalnızca İngilizce okur, bu yüzden arayüz dili ne olursa olsun "
       "bu kutu İngilizce kalır. Aşağıdaki listeden seçin veya İngilizce "
       "sözcük yazın."));

SS_MSG(mask_subjects,
    EN("Common subjects"),
    JA("よく使う対象"),
    ZH_HANS("常见对象"),
    ZH_HANT("常見對象"),
    KO("자주 쓰는 대상"),
    DE("Häufige Motive"),
    FR("Sujets courants"),
    ES("Sujetos habituales"),
    PT("Assuntos comuns"),
    IT("Soggetti comuni"),
    NL("Veelgebruikte onderwerpen"),
    RU("Частые объекты"),
    TR("Sık kullanılan konular"));

SS_MSG(mask_subjects_help,
    EN("Click to put the English word in the box above; click again to take it "
       "out. A highlighted chip is already in the box."),
    JA("押すと上の欄に英語の語が入り、もう一度押すと外れます。色が付いている"
       "ものは既に入っています。"),
    ZH_HANS("点一下把这个英文词放进上面的框，再点一下取出。高亮的表示已经在框里。"),
    ZH_HANT("按一下把這個英文詞放進上面的框，再按一下取出。標亮的表示已經在框裡。"),
    KO("누르면 위 칸에 영어 낱말이 들어가고, 다시 누르면 빠집니다. 강조된 것은 "
       "이미 들어 있는 것입니다."),
    DE("Klicken setzt das englische Wort in das Feld oben, nochmals klicken "
       "nimmt es wieder heraus. Hervorgehobene stehen bereits darin."),
    FR("Cliquez pour mettre le mot anglais dans le champ ci-dessus ; cliquez à "
       "nouveau pour l'enlever. Les éléments en surbrillance y sont déjà."),
    ES("Pulse para poner la palabra inglesa en el campo de arriba; púlsela otra "
       "vez para quitarla. Las resaltadas ya están dentro."),
    PT("Clique para pôr a palavra em inglês no campo acima; clique de novo para "
       "tirá-la. As destacadas já estão lá."),
    IT("Clicchi per mettere la parola inglese nel campo qui sopra; clicchi di "
       "nuovo per toglierla. Quelle evidenziate ci sono già."),
    NL("Klik om het Engelse woord in het veld hierboven te zetten; klik "
       "nogmaals om het eruit te halen. Gemarkeerde staan er al in."),
    RU("Нажмите, чтобы поставить английское слово в поле выше; нажмите ещё "
       "раз, чтобы убрать. Выделенные уже стоят в поле."),
    TR("Tıklayınca İngilizce sözcük yukarıdaki kutuya girer, yeniden "
       "tıklayınca çıkar. Vurgulu olanlar zaten kutudadır."));

SS_MSG(subj_person,
    EN("Person"),       JA("人物"),        ZH_HANS("人"),      ZH_HANT("人"),
    KO("사람"),          DE("Person"),     FR("Personne"),    ES("Persona"),
    PT("Pessoa"),       IT("Persona"),    NL("Persoon"),     RU("Человек"),
    TR("İnsan"));

SS_MSG(subj_tourist,
    EN("Tourist"),      JA("観光客"),      ZH_HANS("游客"),    ZH_HANT("遊客"),
    KO("관광객"),        DE("Tourist"),    FR("Touriste"),    ES("Turista"),
    PT("Turista"),      IT("Turista"),    NL("Toerist"),     RU("Турист"),
    TR("Turist"));

SS_MSG(subj_visitor,
    EN("Visitor"),      JA("来場者"),      ZH_HANS("参观者"),  ZH_HANT("參觀者"),
    KO("방문객"),        DE("Besucher"),   FR("Visiteur"),    ES("Visitante"),
    PT("Visitante"),    IT("Visitatore"), NL("Bezoeker"),    RU("Посетитель"),
    TR("Ziyaretçi"));

SS_MSG(subj_photographer,
    EN("Photographer"), JA("撮影者"),      ZH_HANS("摄影师"),  ZH_HANT("攝影師"),
    KO("사진가"),        DE("Fotograf"),   FR("Photographe"), ES("Fotógrafo"),
    PT("Fotógrafo"),    IT("Fotografo"),  NL("Fotograaf"),   RU("Фотограф"),
    TR("Fotoğrafçı"));

SS_MSG(subj_pedestrian,
    EN("Pedestrian"), JA("歩行者"), ZH_HANS("行人"), ZH_HANT("行人"),
    KO("보행자"), DE("Fußgänger"), FR("Piéton"), ES("Peatón"),
    PT("Pedestre"), IT("Pedone"), NL("Voetganger"), RU("Пешеход"),
    TR("Yaya"));

SS_MSG(subj_child,
    EN("Child"), JA("子ども"), ZH_HANS("儿童"), ZH_HANT("兒童"),
    KO("어린이"), DE("Kind"), FR("Enfant"), ES("Niño"),
    PT("Criança"), IT("Bambino"), NL("Kind"), RU("Ребёнок"),
    TR("Çocuk"));

SS_MSG(subj_guard,
    EN("Security guard"), JA("警備員"), ZH_HANS("保安"), ZH_HANT("保全"),
    KO("경비원"), DE("Wachpersonal"), FR("Agent de sécurité"), ES("Guardia de seguridad"),
    PT("Segurança"), IT("Guardia di sicurezza"), NL("Beveiliger"), RU("Охранник"),
    TR("Güvenlik görevlisi"));

SS_MSG(subj_worker,
    EN("Worker / staff"), JA("作業員・スタッフ"), ZH_HANS("工作人员"), ZH_HANT("工作人員"),
    KO("작업자·직원"), DE("Personal"), FR("Personnel"), ES("Personal"),
    PT("Funcionário"), IT("Personale"), NL("Personeel"), RU("Сотрудник"),
    TR("Görevli"));

SS_MSG(subj_cyclist,
    EN("Cyclist"), JA("自転車に乗る人"), ZH_HANS("骑车人"), ZH_HANT("騎車人"),
    KO("자전거 탄 사람"), DE("Radfahrer"), FR("Cycliste"), ES("Ciclista"),
    PT("Ciclista"), IT("Ciclista"), NL("Fietser"), RU("Велосипедист"),
    TR("Bisikletli"));

SS_MSG(subj_handbag,
    EN("Handbag"), JA("ハンドバッグ"), ZH_HANS("手提包"), ZH_HANT("手提包"),
    KO("핸드백"), DE("Handtasche"), FR("Sac à main"), ES("Bolso"),
    PT("Bolsa"), IT("Borsa"), NL("Handtas"), RU("Сумка"),
    TR("El çantası"));

SS_MSG(subj_umbrella,
    EN("Umbrella"), JA("傘"), ZH_HANS("雨伞"), ZH_HANT("雨傘"),
    KO("우산"), DE("Regenschirm"), FR("Parapluie"), ES("Paraguas"),
    PT("Guarda-chuva"), IT("Ombrello"), NL("Paraplu"), RU("Зонт"),
    TR("Şemsiye"));

SS_MSG(subj_stroller,
    EN("Stroller"), JA("ベビーカー"), ZH_HANS("婴儿车"), ZH_HANT("嬰兒車"),
    KO("유모차"), DE("Kinderwagen"), FR("Poussette"), ES("Cochecito"),
    PT("Carrinho de bebê"), IT("Passeggino"), NL("Kinderwagen"), RU("Коляска"),
    TR("Bebek arabası"));

SS_MSG(subj_selfie_stick,
    EN("Selfie stick"), JA("自撮り棒"), ZH_HANS("自拍杆"), ZH_HANT("自拍棒"),
    KO("셀카봉"), DE("Selfie-Stick"), FR("Perche à selfie"), ES("Palo selfie"),
    PT("Pau de selfie"), IT("Bastone per selfie"), NL("Selfiestick"), RU("Селфи-палка"),
    TR("Selfie çubuğu"));

SS_MSG(subj_phone,
    EN("Phone held up"), JA("掲げたスマホ"), ZH_HANS("举起的手机"), ZH_HANT("舉起的手機"),
    KO("들고 있는 휴대폰"), DE("Hochgehaltenes Handy"), FR("Téléphone brandi"), ES("Móvil en alto"),
    PT("Celular erguido"), IT("Telefono alzato"), NL("Opgeheven telefoon"), RU("Поднятый телефон"),
    TR("Kaldırılmış telefon"));

SS_MSG(subj_hand,
    EN("Hand"),         JA("手"),          ZH_HANS("手"),      ZH_HANT("手"),
    KO("손"),            DE("Hand"),       FR("Main"),        ES("Mano"),
    PT("Mão"),          IT("Mano"),       NL("Hand"),        RU("Рука"),
    TR("El"));

SS_MSG(subj_shoe,
    EN("Shoe"),         JA("靴"),         ZH_HANS("鞋"),       ZH_HANT("鞋"),
    KO("신발"),          DE("Schuh"),      FR("Chaussure"),     ES("Zapato"),
    PT("Sapato"),       IT("Scarpa"),     NL("Schoen"),       RU("Обувь"),
    TR("Ayakkabı"));

SS_MSG(subj_dog,
    EN("Dog"),          JA("犬"),          ZH_HANS("狗"),      ZH_HANT("狗"),
    KO("개"),            DE("Hund"),       FR("Chien"),       ES("Perro"),
    PT("Cachorro"),     IT("Cane"),       NL("Hond"),        RU("Собака"),
    TR("Köpek"));

SS_MSG(subj_animal,
    EN("Animal"),       JA("動物"),        ZH_HANS("动物"),     ZH_HANT("動物"),
    KO("동물"),          DE("Tier"),       FR("Animal"),      ES("Animal"),
    PT("Animal"),       IT("Animale"),    NL("Dier"),        RU("Животное"),
    TR("Hayvan"));

SS_MSG(subj_car,
    EN("Car"),          JA("車"),          ZH_HANS("汽车"),     ZH_HANT("汽車"),
    KO("자동차"),        DE("Auto"),       FR("Voiture"),     ES("Coche"),
    PT("Carro"),        IT("Automobile"), NL("Auto"),        RU("Машина"),
    TR("Araba"));

SS_MSG(subj_bicycle,
    EN("Bicycle"),      JA("自転車"),      ZH_HANS("自行车"),   ZH_HANT("腳踏車"),
    KO("자전거"),        DE("Fahrrad"),    FR("Vélo"),        ES("Bicicleta"),
    PT("Bicicleta"),    IT("Bicicletta"), NL("Fiets"),       RU("Велосипед"),
    TR("Bisiklet"));

SS_MSG(subj_vehicle,
    EN("Vehicle"),      JA("乗り物"),      ZH_HANS("车辆"),     ZH_HANT("車輛"),
    KO("차량"),          DE("Fahrzeug"),   FR("Véhicule"),    ES("Vehículo"),
    PT("Veículo"),      IT("Veicolo"),    NL("Voertuig"),    RU("Транспорт"),
    TR("Araç"));

SS_MSG(subj_license_plate,
    EN("License plate"), JA("ナンバープレート"), ZH_HANS("车牌"), ZH_HANT("車牌"),
    KO("번호판"), DE("Nummernschild"), FR("Plaque d'immatriculation"),
    ES("Matrícula"), PT("Placa"), IT("Targa"), NL("Kenteken"),
    RU("Номерной знак"), TR("Plaka"));

SS_MSG(subj_sky,
    EN("Sky"),          JA("空"),          ZH_HANS("天空"),     ZH_HANT("天空"),
    KO("하늘"),          DE("Himmel"),     FR("Ciel"),        ES("Cielo"),
    PT("Céu"),          IT("Cielo"),      NL("Lucht"),       RU("Небо"),
    TR("Gökyüzü"));

SS_MSG(subj_cloud,
    EN("Cloud"), JA("雲"), ZH_HANS("云"), ZH_HANT("雲"),
    KO("구름"), DE("Wolke"), FR("Nuage"), ES("Nube"),
    PT("Nuvem"), IT("Nuvola"), NL("Wolk"), RU("Облако"),
    TR("Bulut"));

SS_MSG(subj_shadow,
    EN("Shadow of a person"),
    JA("人の影"),
    ZH_HANS("人的影子"),
    ZH_HANT("人的影子"),
    KO("사람 그림자"),
    DE("Schatten einer Person"),
    FR("Ombre d'une personne"),
    ES("Sombra de una persona"),
    PT("Sombra de uma pessoa"),
    IT("Ombra di una persona"),
    NL("Schaduw van een persoon"),
    RU("Тень человека"),
    TR("İnsan gölgesi"));

SS_MSG(subj_water,
    EN("Water"),        JA("水面"),        ZH_HANS("水面"),     ZH_HANT("水面"),
    KO("물"),            DE("Wasser"),     FR("Eau"),         ES("Agua"),
    PT("Água"),         IT("Acqua"),      NL("Water"),       RU("Вода"),
    TR("Su"));

SS_MSG(subj_wave,
    EN("Wave"), JA("波"), ZH_HANS("波浪"), ZH_HANT("波浪"),
    KO("파도"), DE("Welle"), FR("Vague"), ES("Ola"),
    PT("Onda"), IT("Onda"), NL("Golf"), RU("Волна"),
    TR("Dalga"));

SS_MSG(subj_reflection,
    EN("Reflection"),   JA("映り込み"),    ZH_HANS("倒影"),     ZH_HANT("倒影"),
    KO("반사"),          DE("Spiegelung"), FR("Reflet"),      ES("Reflejo"),
    PT("Reflexo"),      IT("Riflesso"),   NL("Weerspiegeling"), RU("Отражение"),
    TR("Yansıma"));

SS_MSG(subj_camera,
    EN("Camera"), JA("カメラ"), ZH_HANS("相机"), ZH_HANT("相機"),
    KO("카메라"), DE("Kamera"), FR("Appareil photo"), ES("Cámara"),
    PT("Câmera"), IT("Fotocamera"), NL("Camera"), RU("Камера"),
    TR("Kamera"));

SS_MSG(subj_tripod,
    EN("Tripod"),       JA("三脚"),        ZH_HANS("三脚架"),   ZH_HANT("三腳架"),
    KO("삼각대"),        DE("Stativ"),     FR("Trépied"),     ES("Trípode"),
    PT("Tripé"),        IT("Treppiede"),  NL("Statief"),     RU("Штатив"),
    TR("Tripod"));

SS_MSG(subj_backpack,
    EN("Backpack"),     JA("バックパック"),  ZH_HANS("背包"),     ZH_HANT("背包"),
    KO("배낭"),         DE("Rucksack"),     FR("Sac à dos"),    ES("Mochila"),
    PT("Mochila"),      IT("Zaino"),       NL("Rugzak"),        RU("Рюкзак"),
    TR("Sırt çantası"));

SS_MSG(subj_helmet,
    EN("Helmet"),       JA("ヘルメット"),   ZH_HANS("头盔"),     ZH_HANT("頭盔"),
    KO("헬멧"),          DE("Helm"),       FR("Casque"),        ES("Casco"),
    PT("Capacete"),     IT("Casco"),      NL("Helm"),          RU("Шлем"),
    TR("Kask"));

SS_MSG(subj_watermark,
    EN("Watermark or timestamp"),
    JA("透かし・日時表示"),
    ZH_HANS("水印或时间戳"),
    ZH_HANT("浮水印或時間戳"),
    KO("워터마크나 날짜 표시"),
    DE("Wasserzeichen oder Zeitstempel"),
    FR("Filigrane ou horodatage"),
    ES("Marca de agua o fecha"),
    PT("Marca d'água ou data"),
    IT("Filigrana o data"),
    NL("Watermerk of tijdstempel"),
    RU("Водяной знак или дата"),
    TR("Filigran veya zaman damgası"));

SS_MSG(subj_painting,
    EN("Painting"), JA("絵画"), ZH_HANS("绘画"), ZH_HANT("繪畫"),
    KO("그림"), DE("Gemälde"), FR("Tableau"), ES("Cuadro"),
    PT("Quadro"), IT("Dipinto"), NL("Schilderij"), RU("Картина"),
    TR("Tablo"));

SS_MSG(subj_poster,
    EN("Poster"), JA("ポスター"), ZH_HANS("海报"), ZH_HANT("海報"),
    KO("포스터"), DE("Plakat"), FR("Affiche"), ES("Póster"),
    PT("Cartaz"), IT("Poster"), NL("Poster"), RU("Плакат"),
    TR("Afiş"));

SS_MSG(subj_screen,
    EN("Screen"), JA("画面"), ZH_HANS("屏幕"), ZH_HANT("螢幕"),
    KO("화면"), DE("Bildschirm"), FR("Écran"), ES("Pantalla"),
    PT("Ecrã"), IT("Schermo"), NL("Scherm"), RU("Экран"),
    TR("Ekran"));

SS_MSG(subj_person_painting,
    EN("Person in a painting"),
    JA("絵の中の人物"),
    ZH_HANS("画里的人"),
    ZH_HANT("畫裡的人"),
    KO("그림 속 인물"),
    DE("Person auf einem Gemälde"),
    FR("Personnage dans un tableau"),
    ES("Persona en un cuadro"),
    PT("Pessoa num quadro"),
    IT("Persona in un dipinto"),
    NL("Persoon op een schilderij"),
    RU("Человек на картине"),
    TR("Tablodaki insan"));

SS_MSG(subj_statue,
    EN("Statue of a person"),
    JA("人物の彫像"),
    ZH_HANS("人像雕塑"),
    ZH_HANT("人像雕塑"),
    KO("인물 조각상"),
    DE("Statue eines Menschen"),
    FR("Statue de personne"),
    ES("Estatua de una persona"),
    PT("Estátua de pessoa"),
    IT("Statua di persona"),
    NL("Standbeeld van een persoon"),
    RU("Статуя человека"),
    TR("İnsan heykeli"));

SS_MSG(subj_mannequin,
    EN("Mannequin"),
    JA("マネキン"),
    ZH_HANS("人体模特"),
    ZH_HANT("人體模特"),
    KO("마네킹"),
    DE("Schaufensterpuppe"),
    FR("Mannequin"),
    ES("Maniquí"),
    PT("Manequim"),
    IT("Manichino"),
    NL("Etalagepop"),
    RU("Манекен"),
    TR("Manken"));

// ===========================================================================
// Mask preview window
// ===========================================================================

SS_MSG(preview_title,
    EN("Try the mask"),  JA("マスクを試す"),   ZH_HANS("试一下蒙版"), ZH_HANT("試一下遮罩"),
    KO("마스크 시험해 보기"), DE("Maske ausprobieren"), FR("Essayer le masque"),
    ES("Probar la máscara"), PT("Testar a máscara"), IT("Prova la maschera"),
    NL("Masker uitproberen"), RU("Проверить маску"), TR("Maskeyi dene"));

SS_MSG(preview_legend,
    EN("Red = removed from the reconstruction. Adjust the prompt until only "
       "what you want gone is red."),
    JA("赤は再構成から取り除かれる部分です。消したいものだけが赤くなるまで"
       "プロンプトを調整してください。"),
    ZH_HANS("红色 = 会从重建中剔除。请调整提示词，直到只有你想去掉的东西是红色。"),
    ZH_HANT("紅色 = 會從重建中剔除。請調整提示詞，直到只有你想去掉的東西是紅色。"),
    KO("빨강 = 재구성에서 제외됩니다. 없애고 싶은 것만 빨갛게 될 때까지 프롬프트를 "
       "다듬으세요."),
    DE("Rot = fällt aus der Rekonstruktion heraus. Den Text so lange anpassen, "
       "bis nur noch rot ist, was verschwinden soll."),
    FR("Rouge = retiré de la reconstruction. Ajustez l'invite jusqu'à ce que "
       "seul ce que vous voulez supprimer soit rouge."),
    ES("Rojo = se elimina de la reconstrucción. Ajuste la indicación hasta que "
       "solo esté en rojo lo que quiere quitar."),
    PT("Vermelho = removido da reconstrução. Ajuste o comando até que só o que "
       "você quer tirar fique vermelho."),
    IT("Rosso = tolto dalla ricostruzione. Regoli il testo finché è rosso solo "
       "ciò che vuole eliminare."),
    NL("Rood = valt uit de reconstructie. Pas de prompt aan tot alleen rood is "
       "wat je weg wilt hebben."),
    RU("Красное — то, что уйдёт из реконструкции. Правьте запрос, пока красным "
       "не останется только лишнее."),
    TR("Kırmızı = yeniden oluşturmadan çıkarılır. Yalnızca gitmesini "
       "istedikleriniz kırmızı olana dek istemi ayarlayın."));

SS_MSG(preview_polarity_help,
    EN("\"Remove\" is for distractors -- people, cars, the photographer's shadow. \"Keep only\" is for object captures, where everything but the subject should be ignored."),
    JA("「取り除く」は邪魔物、たとえば人、車、撮影者の影に使います。「だけを残す」は物体の撮影用で、被写体以外をすべて無視したいときに使います。"),
    ZH_HANS("“移除”用于干扰物——行人、汽车、摄影者的影子。“只保留”用于物体拍摄，此时除主体外的一切都该被忽略。"),
    ZH_HANT("「移除」用於干擾物——行人、汽車、攝影者的影子。「只保留」用於物體拍攝，此時除主體外的一切都該被忽略。"),
    KO("'제거'는 지나가는 사람, 자동차, 촬영자의 그림자 같은 방해물용입니다. '만 남기기'는 물체 촬영용으로, 피사체 말고는 전부 무시하고 싶을 때 씁니다."),
    DE("„Entfernen“ ist für Störendes -- Passanten, Autos, der eigene Schatten. „Nur behalten“ ist für Objektaufnahmen, bei denen alles außer dem Motiv ignoriert werden soll."),
    FR("« Retirer » sert pour les gêneurs -- passants, voitures, l'ombre du photographe. « Ne garder que » sert aux prises d'objet, où tout sauf le sujet doit être ignoré."),
    ES("«Quitar» es para elementos molestos: transeúntes, coches, la sombra del fotógrafo. «Conservar solo» es para capturas de objetos, donde todo salvo el sujeto debe ignorarse."),
    PT("“Remover” é para elementos indesejados: pessoas passando, carros, a sombra do fotógrafo. “Manter só” é para capturas de objetos, em que tudo além do sujeito deve ser ignorado."),
    IT("«Rimuovere»: elementi di disturbo, persone, auto, ombre. «Tenere solo»: riprese di oggetti, per ignorare tutto tranne il soggetto."),
    NL("‘Verwijderen’: storende mensen, auto’s of schaduwen. ‘Alleen houden’: objectopnamen waarbij alles behalve het onderwerp wordt genegeerd."),
    RU("«Убрать» — для помех: прохожих, машин, тени фотографа. «Оставить только» — для съёмки предметов, когда всё, кроме объекта, нужно игнорировать."),
    TR("“Kaldır” istenmeyenler içindir -- geçen insanlar, arabalar, fotoğrafçının gölgesi. “Yalnızca tut” nesne çekimleri içindir; orada özne dışındaki her şey yok sayılmalıdır."));

SS_MSG(preview_what_kept,
    EN("What should be kept?"),
    JA("何を残しますか？"),
    ZH_HANS("要保留什么？"),
    ZH_HANT("要保留什麼？"),
    KO("무엇을 남길까요?"),
    DE("Was soll bleiben?"),
    FR("Que faut-il garder ?"),
    ES("¿Qué se debe conservar?"),
    PT("O que deve ser mantido?"),
    IT("Che cosa va tenuto?"),
    NL("Wat moet blijven?"),
    RU("Что оставить?"),
    TR("Ne tutulsun?"));

// Try the mask, for a pick with no text prompt: the clicks are all there is.
SS_MSG(preview_clicks_only,
    EN("This model reads no text. Click the image to pick objects: left click on "
       "the object, right click on what is not it."),
    JA("このモデルはテキストを読みません。画像をクリックして物体を選んでください。対"
       "象は左クリック、対象でないところは右クリックです。"),
    ZH_HANS("这个模型不读文字。请在图像上点击来选物体：左键点目标，右键点不属于它的地方。"),
    ZH_HANT("這個模型不讀文字。請在影像上點擊來選物件：左鍵點目標，右鍵點不屬於它的地方。"),
    KO("이 모델은 텍스트를 읽지 않습니다. 이미지를 클릭해 물체를 고르세요. 물체는 왼쪽 클릭, 그 물체가 아닌 곳은 오른쪽 클릭입니다."),
    DE("Dieses Modell liest keinen Text. Objekte durch Klicken ins Bild wählen: "
       "Linksklick auf das Objekt, Rechtsklick auf das, was nicht dazugehört."),
    FR("Ce modèle ne lit pas de texte. Cliquez sur l'image pour choisir des "
       "objets : clic gauche sur l'objet, clic droit sur ce qui n'en fait pas "
       "partie."),
    ES("Este modelo no lee texto. Haga clic en la imagen para elegir objetos: "
       "clic izquierdo sobre el objeto, clic derecho sobre lo que no lo es."),
    PT("Este modelo não lê texto. Clique na imagem para escolher objetos: clique "
       "esquerdo no objeto, clique direito no que não faz parte dele."),
    IT("Questo modello non legge testo. Clicchi sull'immagine per scegliere gli "
       "oggetti: clic sinistro sull'oggetto, clic destro su ciò che non lo è."),
    NL("Dit model leest geen tekst. Klik in het beeld om objecten te kiezen: "
       "linksklik op het object, rechtsklik op wat er niet bij hoort."),
    RU("Эта модель не читает текст. Выбирайте объекты щелчками по изображению: "
       "левой кнопкой по объекту, правой — по тому, что к нему не относится."),
    TR("Bu model metin okumaz. Nesne seçmek için görüntüye tıklayın: nesneye sol "
       "tık, ona ait olmayana sağ tık."));

SS_MSG(preview_what_removed,
    EN("What should be removed?"),
    JA("何を取り除きますか？"),
    ZH_HANS("要移除什么？"),
    ZH_HANT("要移除什麼？"),
    KO("무엇을 제거할까요?"),
    DE("Was soll weg?"),
    FR("Que faut-il retirer ?"),
    ES("¿Qué se debe quitar?"),
    PT("O que deve ser removido?"),
    IT("Che cosa va rimosso?"),
    NL("Wat moet weg?"),
    RU("Что убрать?"),
    TR("Ne kaldırılsın?"));

SS_MSG(preview_prompt_help_keep,
    EN("Plain words for the subject of the capture, separated by semicolons. "
       "Everything else is cut out of the reconstruction."),
    JA("撮影の被写体をふつうの言葉でセミコロン区切りに書きます。それ以外は"
       "すべて再構成から取り除かれます。"),
    ZH_HANS("用平常的词语写出拍摄的主体，以分号分隔。其余的一切都会从重建中剔除。"),
    ZH_HANT("用平常的詞語寫出拍攝的主體，以分號分隔。其餘的一切都會從重建中剔除。"),
    KO("촬영의 피사체를 평범한 낱말로 세미콜론으로 구분해 적으세요. 그 밖의 "
       "모든 것은 재구성에서 제외됩니다."),
    DE("Einfache Wörter für das Motiv der Aufnahme, durch Semikolon getrennt. "
       "Alles andere fällt aus der Rekonstruktion heraus."),
    FR("Des mots ordinaires pour le sujet de la prise, séparés par des "
       "points-virgules. Tout le reste est retiré de la reconstruction."),
    ES("Palabras corrientes para el sujeto de la captura, separadas por punto "
       "y coma. Todo lo demás se elimina de la reconstrucción."),
    PT("Palavras simples para o sujeito da captura, separadas por ponto e "
       "vírgula. Todo o resto é retirado da reconstrução."),
    IT("Parole comuni per il soggetto della ripresa, separate da punto e "
       "virgola. Tutto il resto viene tolto dalla ricostruzione."),
    NL("Gewone woorden voor het onderwerp van de opname, gescheiden door "
       "puntkomma's. Al het andere valt uit de reconstructie."),
    RU("Обычные слова для объекта съёмки через точку с запятой. Всё остальное "
       "исключается из реконструкции."),
    TR("Çekimin öznesi için noktalı virgülle ayrılmış sıradan sözcükler. Geri "
       "kalan her şey yeniden oluşturmadan çıkarılır."));

SS_MSG(preview_prompt_help_remove,
    EN("Plain words for the things to take out of the reconstruction, separated by semicolons.\nAnything that moved, reflected, or was not part of the scene is a good candidate."),
    JA("再構成から外したいものをふつうの言葉でセミコロン区切りに書きます。\n動いたもの、映り込んだもの、シーンの一部ではなかったものが候補です。"),
    ZH_HANS("用平常的词语写出要从重建中拿掉的东西，以分号分隔。\n凡是移动过的、有反射的、本来就不属于场景的，都是合适的候选。"),
    ZH_HANT("用平常的詞語寫出要從重建中拿掉的東西，以分號分隔。\n凡是移動過的、有反射的、本來就不屬於場景的，都是合適的候選。"),
    KO("재구성에서 빼고 싶은 것들을 평범한 낱말로 세미콜론으로 구분해 적으세요. 움직였던 것, 비쳤던 것, 원래 장면의 일부가 아니었던 것이 좋은 후보입니다."),
    DE("Einfache Wörter für das, was aus der Rekonstruktion heraus soll, durch Semikolon getrennt.\nAlles, was sich bewegte, spiegelte oder nicht zur Szene gehörte, ist ein guter Kandidat."),
    FR("Des mots ordinaires pour ce qu'il faut sortir de la reconstruction, séparés par des points-virgules.\nTout ce qui bougeait, se reflétait ou ne faisait pas partie de la scène est un bon candidat."),
    ES("Palabras corrientes para lo que hay que sacar de la reconstrucción, separadas por punto y coma.\nTodo lo que se movía, se reflejaba o no formaba parte de la escena es buen candidato."),
    PT("Palavras simples para o que deve sair da reconstrução, separadas por ponto e vírgula.\nTudo o que se movia, refletia ou não fazia parte da cena é bom candidato."),
    IT("Parole comuni per ciò che va tolto dalla ricostruzione, separate da punto e virgola.\nTutto ciò che si muoveva, si rifletteva o non faceva parte della scena è un buon candidato."),
    NL("Gewone woorden voor wat uit de reconstructie moet, gescheiden door puntkomma's.\nAlles wat bewoog, weerspiegelde of geen deel van de scène was, is een goede kandidaat."),
    RU("Обычные слова для того, что нужно вынести из реконструкции, через точку с запятой.\nХорошие кандидаты — всё, что двигалось, отражалось или не относилось к сцене."),
    TR("Yeniden oluşturmadan çıkarılacak şeyler için noktalı virgülle ayrılmış sıradan sözcükler.\nHareket eden, yansıyan ya da sahnenin parçası olmayan her şey iyi bir adaydır."));

SS_MSG(preview_but_remove_these,
    EN("...but remove these"),
    JA("…ただしこれらは取り除く"),
    ZH_HANS("…但要移除这些"),
    ZH_HANT("…但要移除這些"),
    KO("…단, 이것들은 제거"),
    DE("… aber diese entfernen"),
    FR("… mais retirer ceci"),
    ES("… pero quitar estos"),
    PT("… mas remover estes"),
    IT("… ma rimuovere questi"),
    NL("… maar deze verwijderen"),
    RU("…но эти убрать"),
    TR("…ama bunları kaldır"));

SS_MSG(preview_but_keep_these,
    EN("...but keep these"),
    JA("…ただしこれらは残す"),
    ZH_HANS("…但要保留这些"),
    ZH_HANT("…但要保留這些"),
    KO("…단, 이것들은 남기기"),
    DE("… aber diese behalten"),
    FR("… mais garder ceci"),
    ES("… pero conservar estos"),
    PT("… mas manter estes"),
    IT("… ma tenere questi"),
    NL("… maar deze houden"),
    RU("…но эти оставить"),
    TR("…ama bunları tut"));

SS_MSG(preview_negative_help_keep,
    EN("Exceptions: things that match the line above but should still go. "
       "Optional."),
    JA("例外です。上の行に当てはまるけれど、それでも取り除きたいものを書きます。"
       "省略できます。"),
    ZH_HANS("例外：符合上一行、但仍然应当剔除的东西。可以留空。"),
    ZH_HANT("例外：符合上一行、但仍然應當剔除的東西。可以留空。"),
    KO("예외입니다. 위 줄에 해당하지만 그래도 없애야 할 것들. 비워 둬도 됩니다."),
    DE("Ausnahmen: Dinge, die auf die Zeile darüber passen, aber trotzdem weg "
       "sollen. Optional."),
    FR("Exceptions : ce qui correspond à la ligne du dessus mais doit quand "
       "même partir. Facultatif."),
    ES("Excepciones: cosas que coinciden con la línea de arriba pero deben "
       "irse igual. Opcional."),
    PT("Exceções: coisas que correspondem à linha acima mas devem sair mesmo "
       "assim. Opcional."),
    IT("Eccezioni: cose che corrispondono alla riga sopra ma devono comunque "
       "sparire. Facoltativo."),
    NL("Uitzonderingen: dingen die bij de regel hierboven passen maar toch weg "
       "moeten. Optioneel."),
    RU("Исключения: то, что подходит под строку выше, но всё же должно уйти. "
       "Необязательно."),
    TR("İstisnalar: yukarıdaki satıra uyan ama yine de gitmesi gerekenler. "
       "İsteğe bağlı."));

SS_MSG(preview_negative_help_remove,
    EN("Exceptions: things that match the line above but should stay. "
       "Optional."),
    JA("例外です。上の行に当てはまるけれど、残したいものを書きます。"
       "省略できます。"),
    ZH_HANS("例外：符合上一行、但应当保留的东西。可以留空。"),
    ZH_HANT("例外：符合上一行、但應當保留的東西。可以留空。"),
    KO("예외입니다. 위 줄에 해당하지만 남겨야 할 것들. 비워 둬도 됩니다."),
    DE("Ausnahmen: Dinge, die auf die Zeile darüber passen, aber bleiben "
       "sollen. Optional."),
    FR("Exceptions : ce qui correspond à la ligne du dessus mais doit rester. "
       "Facultatif."),
    ES("Excepciones: cosas que coinciden con la línea de arriba pero deben "
       "quedarse. Opcional."),
    PT("Exceções: coisas que correspondem à linha acima mas devem ficar. "
       "Opcional."),
    IT("Eccezioni: cose che corrispondono alla riga sopra ma devono restare. "
       "Facoltativo."),
    NL("Uitzonderingen: dingen die bij de regel hierboven passen maar moeten "
       "blijven. Optioneel."),
    RU("Исключения: то, что подходит под строку выше, но должно остаться. "
       "Необязательно."),
    TR("İstisnalar: yukarıdaki satıra uyan ama kalması gerekenler. İsteğe "
       "bağlı."));

SS_MSG(objects_to_click,
    EN("Objects to click on"),
    JA("クリックして指定する物体"),
    ZH_HANS("要点选的物体"),
    ZH_HANT("要點選的物體"),
    KO("클릭할 물체"),
    DE("Objekte zum Anklicken"),
    FR("Objets à cliquer"),
    ES("Objetos que marcar"),
    PT("Objetos para clicar"),
    IT("Oggetti su cui cliccare"),
    NL("Objecten om aan te klikken"),
    RU("Объекты для указания"),
    TR("Tıklanacak nesneler"));

SS_MSG(objects_to_click_help,
    EN("Use a separate object for each person or thing. Clicks apply only to their frame; add new clicks on later frames to correct drift."),
    JA("人物や物ごとに別の対象を使ってください。クリックはそのフレームのみ有効です。追跡がずれたら後のフレームでクリックを追加してください。"),
    ZH_HANS("每个人或物体使用单独的对象。点击仅适用于当前帧；追踪偏移时在后续帧重新点击纠正。"),
    ZH_HANT("每個人或物體使用單獨的對象。點擊僅適用於目前影格；追蹤偏移時在後續影格重新點擊修正。"),
    KO("사람이나 사물마다 별도 객체를 사용하세요. 클릭은 해당 프레임에만 적용됩니다. 추적이 벗어나면 이후 프레임에서 다시 클릭하세요."),
    DE("Für jede Person oder Sache ein eigenes Objekt nutzen. Klicks gelten nur für ihr Bild; Abdriften durch neue Klicks in späteren Bildern korrigieren."),
    FR("Utilisez un objet distinct par personne ou chose. Les clics valent pour leur image ; ajoutez-en sur les images suivantes pour corriger la dérive."),
    ES("Usa un objeto distinto por persona o cosa. Los clics solo valen para su fotograma; añade clics en fotogramas posteriores para corregir desvíos."),
    PT("Use um objeto separado por pessoa ou coisa. Os cliques só valem para o seu fotograma; adicione cliques nos seguintes para corrigir desvios."),
    IT("Usare un oggetto distinto per ogni persona o cosa. I clic valgono per il loro fotogramma; aggiungerne nei successivi per correggere la deriva."),
    NL("Gebruik een apart object per persoon of ding. Klikken gelden alleen voor hun beeld; voeg klikken op latere beelden toe om afwijkingen te corrigeren."),
    RU("Для каждого человека или предмета создайте отдельный объект. Клики относятся к своему кадру; исправляйте дрейф новыми кликами на следующих кадрах."),
    TR("Her kişi veya nesne için ayrı bir nesne kullanın. Tıklamalar yalnızca kendi karelerinde geçerlidir; kaymayı sonraki karelerde yeni tıklamalarla düzeltin."));

// {0} object number, {1} clicks on this frame, {2} clicks on other frames.
SS_MSG(object_with_clicks,
    EN("Object {0} ({1} here, {2} elsewhere)"),
    JA("物体 {0}（このフレーム {1}、他 {2}）"),
    ZH_HANS("物体 {0}（本帧 {1}，其他 {2}）"),
    ZH_HANT("物體 {0}（本影格 {1}，其他 {2}）"),
    KO("물체 {0}(여기 {1}, 다른 곳 {2})"),
    DE("Objekt {0} ({1} hier, {2} anderswo)"),
    FR("Objet {0} ({1} ici, {2} ailleurs)"),
    ES("Objeto {0} ({1} aquí, {2} en otros)"),
    PT("Objeto {0} ({1} aqui, {2} em outros)"),
    IT("Oggetto {0} ({1} qui, {2} altrove)"),
    NL("Object {0} ({1} hier, {2} elders)"),
    RU("Объект {0} ({1} здесь, {2} в других кадрах)"),
    TR("Nesne {0} ({1} burada, {2} başka yerde)"));

SS_MSG(object_no_clicks,
    EN("Object {0} (no clicks yet)"),
    JA("物体 {0}（まだクリックなし）"),
    ZH_HANS("物体 {0}（还没有点击）"),
    ZH_HANT("物體 {0}（還沒有點擊）"),
    KO("물체 {0}(아직 클릭 없음)"),
    DE("Objekt {0} (noch keine Klicks)"),
    FR("Objet {0} (aucun clic pour l'instant)"),
    ES("Objeto {0} (aún sin clics)"),
    PT("Objeto {0} (ainda sem cliques)"),
    IT("Oggetto {0} (ancora nessun clic)"),
    NL("Object {0} (nog geen klikken)"),
    RU("Объект {0} (пока без щелчков)"),
    TR("Nesne {0} (henüz tıklama yok)"));

SS_MSG(object_clear,
    EN("Clear"),         JA("消去"),          ZH_HANS("清除"),     ZH_HANT("清除"),
    KO("지우기"),         DE("Löschen"),      FR("Effacer"),      ES("Borrar"),
    PT("Limpar"),        IT("Cancella"),     NL("Wissen"),       RU("Очистить"),
    TR("Temizle"));

SS_MSG(object_another,
    EN("Another object"), JA("物体を追加"),   ZH_HANS("再加一个物体"), ZH_HANT("再加一個物體"),
    KO("물체 하나 더"),   DE("Weiteres Objekt"), FR("Autre objet"),
    ES("Otro objeto"),   PT("Outro objeto"), IT("Un altro oggetto"),
    NL("Nog een object"), RU("Ещё объект"),  TR("Başka bir nesne"));

SS_MSG(object_another_help,
    EN("Adds an object for the next thing you click on."),
    JA("次にクリックするもののために物体を1つ追加します。"),
    ZH_HANS("为你接下来要点选的东西新增一个物体。"),
    ZH_HANT("為你接下來要點選的東西新增一個物體。"),
    KO("다음에 클릭할 것을 위해 물체를 하나 추가합니다."),
    DE("Legt ein Objekt für das an, was Sie als Nächstes anklicken."),
    FR("Ajoute un objet pour ce que vous cliquerez ensuite."),
    ES("Añade un objeto para lo próximo que marque."),
    PT("Adiciona um objeto para o próximo item em que você clicar."),
    IT("Aggiunge un oggetto per la prossima cosa su cui cliccherà."),
    NL("Voegt een object toe voor wat je hierna aanklikt."),
    RU("Добавляет объект для того, на что вы щёлкнете следующим."),
    TR("Bir sonraki tıklayacağınız şey için bir nesne ekler."));

SS_MSG(object_clear_all,
    EN("Clear all"),     JA("すべて消去"),    ZH_HANS("全部清除"),  ZH_HANT("全部清除"),
    KO("모두 지우기"),    DE("Alle löschen"), FR("Tout effacer"), ES("Borrar todo"),
    PT("Limpar tudo"),   IT("Cancella tutto"), NL("Alles wissen"),
    RU("Очистить всё"),  TR("Hepsini temizle"));

// ===========================================================================
// Fixed areas of the frame (the stencil -- app/FrameMask.h)
// ===========================================================================

SS_MSG(stencil_section,
    EN("Fixed areas of the frame"),
    JA("画面の決まった位置"),
    ZH_HANS("画面中固定的区域"),
    ZH_HANT("畫面中固定的區域"),
    KO("화면에서 늘 같은 자리"),
    DE("Feste Bereiche des Bildes"),
    FR("Zones fixes de l'image"),
    ES("Zonas fijas del fotograma"),
    PT("Áreas fixas do quadro"),
    IT("Zone fisse del fotogramma"),
    NL("Vaste gebieden van het beeld"),
    RU("Постоянные участки кадра"),
    TR("Karenin sabit alanları"));

SS_MSG(stencil_section_help,
    EN("Shapes exclude areas fixed across the capture, such as fisheye edges, watermarks or camera poles. No model required."),
    JA("図形で魚眼の縁、透かし、カメラの棒など撮影中に固定された領域を除外します。モデルは不要です。"),
    ZH_HANS("用形状排除拍摄中固定的区域，如鱼眼边缘、水印、相机杆。无需模型。"),
    ZH_HANT("用形狀排除拍攝中固定的區域，如魚眼邊緣、浮水印、相機桿。無需模型。"),
    KO("도형으로 어안 테두리·워터마크·카메라 봉 같은 촬영 내 고정 영역을 제외합니다. 모델이 필요 없습니다."),
    DE("Formen schließen feste Aufnahmebereiche aus, etwa Fischaugenränder, Wasserzeichen oder Kamerastangen. Kein Modell nötig."),
    FR("Les formes excluent les zones fixes : bords fisheye, filigranes ou perches. Aucun modèle requis."),
    ES("Las formas excluyen zonas fijas: bordes de ojo de pez, marcas de agua o palos. Sin modelo."),
    PT("As formas excluem zonas fixas: bordas de olho de peixe, marcas de água ou varas. Sem modelo."),
    IT("Le forme escludono aree fisse: bordi fisheye, filigrane o aste. Nessun modello richiesto."),
    NL("Vormen sluiten vaste gebieden uit, zoals fisheyeranden, watermerken of camerastokken. Geen model nodig."),
    RU("Фигуры исключают фиксированные области съёмки: края рыбьего глаза, водяные знаки, шесты камеры. Модель не нужна."),
    TR("Şekiller, balıkgözü kenarı, filigran veya kamera çubuğu gibi sabit alanları dışlar. Model gerekmez."));

SS_MSG(stencil_border,
    EN("Cut away the fisheye border"),
    JA("魚眼の黒枠を切り落とす"),
    ZH_HANS("裁掉鱼眼黑边"),
    ZH_HANT("裁掉魚眼黑邊"),
    KO("어안 검은 테두리 잘라내기"),
    DE("Den Fisheye-Rand wegschneiden"),
    FR("Découper le bord du fisheye"),
    ES("Recortar el borde de ojo de pez"),
    PT("Recortar a borda olho-de-peixe"),
    IT("Ritagliare il bordo fisheye"),
    NL("De fisheye-rand wegsnijden"),
    RU("Срезать чёрный край фишая"),
    TR("Balıkgözü kenarını kes"));

SS_MSG(stencil_border_help,
    EN("Detects the lens circle and keeps its interior. Recomputed per camera when building the dataset, including both lenses of 360° cameras."),
    JA("レンズ円を検出して内側を残します。データセット作成時にカメラごとに再計算し、360度カメラの両レンズにも個別に適用します。"),
    ZH_HANS("检测镜头圆形并保留内部。创建数据集时逐相机重新计算，360° 相机的双镜头分别处理。"),
    ZH_HANT("偵測鏡頭圓形並保留內部。建立資料集時逐相機重新計算，360° 相機的雙鏡頭分別處理。"),
    KO("렌즈 원을 감지해 안쪽만 남깁니다. 데이터셋을 만들 때 360° 카메라 양쪽 렌즈를 포함해 카메라별로 다시 계산합니다."),
    DE("Erkennt den Objektivkreis und behält sein Inneres. Beim Datensatzbau je Kamera neu berechnet, auch für beide 360°-Objektive."),
    FR("Détecte le cercle de l’objectif et garde l’intérieur. Recalculé par caméra à la création du jeu, y compris pour les deux objectifs 360°."),
    ES("Detecta el círculo de lente y conserva el interior. Se recalcula por cámara al crear el dataset, incluidas ambas lentes 360°."),
    PT("Deteta o círculo da lente e mantém o interior. Recalculado por câmara ao criar o conjunto, incluindo ambas as lentes 360°."),
    IT("Rileva il cerchio della lente e tiene l’interno. Ricalcolato per camera alla creazione del dataset, anche per entrambe le lenti 360°."),
    NL("Detecteert de lenscirkel en behoudt de binnenkant. Bij datasetbouw per camera herberekend, ook voor beide 360°-lenzen."),
    RU("Находит круг объектива и сохраняет внутренность. При сборке набора вычисляется для каждой камеры, включая обе линзы 360°."),
    TR("Mercek çemberini algılar ve içini tutar. Veri kümesi oluşturulurken iki 360° merceği de dâhil her kamera için yeniden hesaplar."));

SS_MSG(stencil_shrink,
    EN("Shrink"),
    JA("内側に寄せる"),
    ZH_HANS("往里收"),
    ZH_HANT("往裡收"),
    KO("안쪽으로"),
    DE("Enger"),
    FR("Resserrer"),
    ES("Estrechar"),
    PT("Estreitar"),
    IT("Stringi"),
    NL("Krimpen"),
    RU("Сузить"),
    TR("Daralt"));

SS_MSG(stencil_shrink_help,
    EN("Shrinks the detected radius by this percentage. Ctrl+click to enter a value. For an offset or uneven border, edit the border ellipse."),
    JA("検出した半径をこの割合だけ縮めます。Ctrl+クリックで数値を入力できます。境界がずれている場合は楕円を編集してください。"),
    ZH_HANS("按此百分比缩小检测出的半径。Ctrl+单击可输入数值。边界偏心或不均匀时，可编辑边界椭圆。"),
    ZH_HANT("按此百分比縮小偵測出的半徑。Ctrl+點擊可輸入數值。邊界偏心或不均勻時，可編輯邊界橢圓。"),
    KO("감지된 반지름을 이 비율만큼 줄입니다. Ctrl+클릭으로 값을 입력합니다. 경계가 치우치거나 고르지 않으면 경계 타원을 편집하세요."),
    DE("Verkleinert den erkannten Radius um diesen Prozentsatz. Strg+Klick zur Werteingabe. Bei versetztem oder ungleichmäßigem Rand die Randellipse bearbeiten."),
    FR("Réduit le rayon détecté de ce pourcentage. Ctrl+clic pour saisir une valeur. Si le bord est décentré ou irrégulier, modifiez son ellipse."),
    ES("Reduce el radio detectado en este porcentaje. Ctrl+clic para introducir un valor. Si el borde está desplazado o es irregular, edita su elipse."),
    PT("Reduz o raio detectado nesta porcentagem. Ctrl+clique para inserir um valor. Se a borda estiver deslocada ou irregular, edite sua elipse."),
    IT("Riduce il raggio rilevato di questa percentuale. Ctrl+clic per inserire un valore. Se il bordo è decentrato o irregolare, modifica la sua ellisse."),
    NL("Verkleint de gedetecteerde straal met dit percentage. Ctrl+klik om een waarde in te voeren. Bewerk de randellips bij een verschoven of onregelmatige rand."),
    RU("Уменьшает найденный радиус на указанный процент. Ctrl+щелчок для ввода значения. Если граница смещена или неровная, измените её эллипс."),
    TR("Algılanan yarıçapı bu yüzde kadar küçültür. Değer girmek için Ctrl+tıklayın. Sınır kaymış veya düzensizse sınır elipsini düzenleyin."));

SS_MSG(stencil_edit_border,
    EN("Edit border ellipse"), JA("境界の楕円を編集"),
    ZH_HANS("编辑边界椭圆"), ZH_HANT("編輯邊界橢圓"),
    KO("경계 타원 편집"), DE("Randellipse bearbeiten"),
    FR("Modifier l'ellipse du bord"), ES("Editar elipse del borde"),
    PT("Editar elipse da borda"), IT("Modifica ellisse del bordo"),
    NL("Randellips bewerken"), RU("Изменить эллипс границы"),
    TR("Sınır elipsini düzenle"));

SS_MSG(stencil_edit_border_help,
    EN("Turns the detected border into a movable, resizable ellipse and disables auto-detection. Enables per-camera areas; other cameras keep their borders."),
    JA("検出した縁を移動・拡縮可能な楕円にし、自動検出を止めます。カメラ別領域を有効にし、他カメラの縁は保持します。"),
    ZH_HANS("将检测边框转为可移动缩放的椭圆，并停用自动检测。启用逐相机区域；其他相机保留各自边框。"),
    ZH_HANT("將偵測邊框轉為可移動縮放的橢圓，並停用自動偵測。啟用逐相機區域；其他相機保留各自邊框。"),
    KO("검출된 경계를 이동·크기 변경 가능한 타원으로 바꾸고 자동 감지를 끕니다. 카메라별 영역을 켜며 다른 카메라 경계는 유지합니다."),
    DE("Macht den Rand zur beweglichen, skalierbaren Ellipse und deaktiviert die Automatik. Aktiviert Bereiche je Kamera; andere Kameras behalten ihre Ränder."),
    FR("Transforme le bord en ellipse déplaçable et redimensionnable, sans détection auto. Active les zones par caméra ; les autres gardent leurs bords."),
    ES("Convierte el borde en elipse movible y escalable, desactiva detección automática. Activa zonas por cámara; las otras conservan sus bordes."),
    PT("Torna a borda numa elipse móvel e redimensionável, desliga a deteção automática. Ativa zonas por câmara; as outras mantêm as bordas."),
    IT("Trasforma il bordo in ellisse mobile e ridimensionabile, senza rilevamento auto. Attiva aree per camera; le altre mantengono i propri bordi."),
    NL("Maakt de rand een verplaatsbare, schaalbare ellips en zet autodetectie uit. Activeert gebieden per camera; andere camera’s behouden hun rand."),
    RU("Превращает край в перемещаемый и масштабируемый эллипс без автопоиска. Включает области по камерам; остальные сохраняют края."),
    TR("Kenarı taşınabilir, ölçeklenebilir elipse çevirip otomatik algılamayı kapatır. Kamera başına alanları açar; diğer kameralar kenarlarını korur."));

SS_MSG(stencil_looking,
    EN("Looking for the border..."),
    JA("枠を探しています..."),
    ZH_HANS("正在寻找边框……"),
    ZH_HANT("正在尋找邊框……"),
    KO("테두리를 찾는 중..."),
    DE("Der Rand wird gesucht..."),
    FR("Recherche du bord..."),
    ES("Buscando el borde..."),
    PT("Procurando a borda..."),
    IT("Ricerca del bordo..."),
    NL("De rand wordt gezocht..."),
    RU("Идёт поиск края..."),
    TR("Kenar aranıyor..."));

SS_MSG(stencil_look_again,
    EN("Look again"),
    JA("もう一度探す"),
    ZH_HANS("重新找一次"),
    ZH_HANT("重新找一次"),
    KO("다시 찾기"),
    DE("Erneut suchen"),
    FR("Chercher encore"),
    ES("Buscar otra vez"),
    PT("Procurar de novo"),
    IT("Cerca di nuovo"),
    NL("Opnieuw zoeken"),
    RU("Искать снова"),
    TR("Yeniden ara"));

SS_MSG(stencil_border_none,
    EN("No border was found here. Draw a circle instead, or leave this off."),
    JA("ここでは枠が見つかりませんでした。代わりに円を描くか、これを外して"
       "ください。"),
    ZH_HANS("这里没找到边框。可以改为自己画一个圆，或者不勾选这一项。"),
    ZH_HANT("這裡沒找到邊框。可以改為自己畫一個圓，或者不勾選這一項。"),
    KO("여기서는 테두리를 찾지 못했습니다. 대신 원을 그리거나 이 항목을 꺼 두세요."),
    DE("Hier wurde kein Rand gefunden. Zeichnen Sie stattdessen einen Kreis, "
       "oder lassen Sie das aus."),
    FR("Aucun bord trouvé ici. Dessinez plutôt un cercle, ou laissez ceci "
       "décoché."),
    ES("Aquí no se encontró borde. Dibuje un círculo en su lugar, o deje esto "
       "sin marcar."),
    PT("Nenhuma borda foi achada aqui. Desenhe um círculo, ou deixe isto "
       "desmarcado."),
    IT("Qui non è stato trovato alcun bordo. Disegni invece un cerchio, o "
       "lasci questa opzione spenta."),
    NL("Hier is geen rand gevonden. Teken in plaats daarvan een cirkel, of "
       "laat dit uit."),
    RU("Здесь край не найден. Нарисуйте круг сами или снимите этот флажок."),
    TR("Burada kenar bulunamadı. Yerine bir daire çizin ya da bunu kapalı "
       "bırakın."));

SS_MSG(stencil_shapes,
    EN("Block out an area"),
    JA("範囲を塗りつぶす"),
    ZH_HANS("挡掉一块区域"),
    ZH_HANT("擋掉一塊區域"),
    KO("영역 가리기"),
    DE("Einen Bereich abdecken"),
    FR("Masquer une zone"),
    ES("Tapar una zona"),
    PT("Tapar uma área"),
    IT("Coprire una zona"),
    NL("Een gebied afdekken"),
    RU("Закрыть участок"),
    TR("Bir alanı kapat"));

SS_MSG(stencil_shapes_help,
    EN("Draw exclusions with the tools above. Subtract or Ctrl draws areas to keep, such as a lens circle. Ctrl+Z undoes."),
    JA("上のツールで除外領域を描きます。「差し引き」かCtrlでレンズ円など残す領域を描きます。Ctrl+Zで戻します。"),
    ZH_HANS("用上方工具绘制排除区域。减去或 Ctrl 绘制要保留的区域，如镜头圆形。Ctrl+Z 撤销。"),
    ZH_HANT("用上方工具繪製排除區域。減去或 Ctrl 繪製要保留的區域，如鏡頭圓形。Ctrl+Z 復原。"),
    KO("위 도구로 제외 영역을 그립니다. 빼기 또는 Ctrl로 렌즈 원 같은 유지 영역을 그립니다. Ctrl+Z는 실행 취소입니다."),
    DE("Mit den Werkzeugen oben Ausschlüsse zeichnen. Subtrahieren oder Ctrl zeichnet zu behaltende Bereiche, etwa den Objektivkreis. Ctrl+Z macht rückgängig."),
    FR("Dessinez les exclusions avec les outils ci-dessus. Soustraire ou Ctrl dessine ce qu’il faut garder, comme le cercle de l’objectif. Ctrl+Z annule."),
    ES("Dibuja exclusiones con las herramientas de arriba. Restar o Ctrl dibuja zonas a conservar, como el círculo de lente. Ctrl+Z deshace."),
    PT("Desenhe exclusões com as ferramentas acima. Subtrair ou Ctrl desenha áreas a manter, como o círculo da lente. Ctrl+Z anula."),
    IT("Disegnare esclusioni con gli strumenti sopra. Sottrai o Ctrl disegna aree da tenere, come il cerchio della lente. Ctrl+Z annulla."),
    NL("Teken uitsluitingen met de gereedschappen bovenaan. Aftrekken of Ctrl tekent te behouden delen, zoals de lenscirkel. Ctrl+Z maakt ongedaan."),
    RU("Рисуйте исключения инструментами сверху. Вычитание или Ctrl рисует сохраняемые области, например круг объектива. Ctrl+Z отменяет."),
    TR("Üstteki araçlarla dışlanan alanları çizin. Çıkar veya Ctrl, mercek çemberi gibi tutulacak alanları çizer. Ctrl+Z geri alır."));

SS_MSG(stencil_shape_box,
    EN("Box {0}"),       JA("四角 {0}"),      ZH_HANS("方框 {0}"),  ZH_HANT("方框 {0}"),
    KO("상자 {0}"),       DE("Rechteck {0}"), FR("Rectangle {0}"),
    ES("Rectángulo {0}"), PT("Retângulo {0}"), IT("Rettangolo {0}"),
    NL("Rechthoek {0}"), RU("Прямоугольник {0}"), TR("Dikdörtgen {0}"));

SS_MSG(stencil_shape_circle,
    EN("Circle {0}"),    JA("円 {0}"),        ZH_HANS("圆 {0}"),    ZH_HANT("圓 {0}"),
    KO("원 {0}"),         DE("Kreis {0}"),    FR("Cercle {0}"),   ES("Círculo {0}"),
    PT("Círculo {0}"),   IT("Cerchio {0}"),  NL("Cirkel {0}"),   RU("Круг {0}"),
    TR("Daire {0}"));

SS_MSG(stencil_shape_path,
    EN("Path {0}"),       JA("パス {0}"),       ZH_HANS("路径 {0}"),  ZH_HANT("路徑 {0}"),
    KO("패스 {0}"),        DE("Pfad {0}"),       FR("Tracé {0}"),
    ES("Trazado {0}"),    PT("Traçado {0}"),    IT("Tracciato {0}"),
    NL("Pad {0}"),        RU("Контур {0}"),     TR("Yol {0}"));

SS_MSG(stencil_shape_stroke,
    EN("Brush stroke {0}"), JA("ブラシ {0}"),   ZH_HANS("笔刷 {0}"),  ZH_HANT("筆刷 {0}"),
    KO("브러시 {0}"),       DE("Pinselstrich {0}"), FR("Trait de pinceau {0}"),
    ES("Trazo de pincel {0}"), PT("Pincelada {0}"), IT("Pennellata {0}"),
    NL("Penseelstreek {0}"), RU("Мазок кисти {0}"), TR("Fırça darbesi {0}"));

SS_MSG(stencil_tool_select,
    EN("Select"),        JA("選択"),          ZH_HANS("选择"),     ZH_HANT("選取"),
    KO("선택"),           DE("Auswahl"),       FR("Sélection"),
    ES("Seleccionar"),   PT("Selecionar"),    IT("Seleziona"),
    NL("Selecteren"),    RU("Выбор"),         TR("Seç"));

SS_MSG(stencil_tool_select_help,
    EN("Clicks on the picture prompt the model, as without a tool. Pick a shape "
       "in the list to move or resize it."),
    JA("画像のクリックはツールなしのときと同じくモデルへの指示になります。一覧で図形を"
       "選ぶと、移動や大きさの変更ができます。"),
    ZH_HANS("在图片上单击会像不用工具时一样提示模型。在列表中选中一个图形即可移动或"
            "改变大小。"),
    ZH_HANT("在圖片上點一下會像不用工具時一樣提示模型。在清單中選取一個圖形即可移動或"
            "改變大小。"),
    KO("사진을 클릭하면 도구가 없을 때처럼 모델에 지시합니다. 목록에서 도형을 고르면 "
       "옮기거나 크기를 바꿀 수 있습니다."),
    DE("Klicks ins Bild geben dem Modell Hinweise, wie ohne Werkzeug. Eine Form in "
       "der Liste wählen, um sie zu verschieben oder ihre Größe zu ändern."),
    FR("Les clics sur l'image guident le modèle, comme sans outil. Choisissez une "
       "forme dans la liste pour la déplacer ou la redimensionner."),
    ES("Los clics en la imagen guían al modelo, como sin herramienta. Elija una "
       "forma en la lista para moverla o cambiar su tamaño."),
    PT("Os cliques na imagem orientam o modelo, como sem ferramenta. Escolha uma "
       "forma na lista para movê-la ou redimensioná-la."),
    IT("I clic sull'immagine guidano il modello, come senza strumento. Scegli una "
       "forma nell'elenco per spostarla o ridimensionarla."),
    NL("Klikken op de afbeelding sturen het model, zoals zonder gereedschap. Kies "
       "een vorm in de lijst om hem te verplaatsen of te vergroten."),
    RU("Щелчки по картинке подсказывают модели, как и без инструмента. Выберите "
       "фигуру в списке, чтобы сдвинуть её или изменить размер."),
    TR("Resme tıklamak, araç yokken olduğu gibi modele ipucu verir. Taşımak veya "
       "boyutlandırmak için listeden bir biçim seçin."));

SS_MSG(stencil_brush_size,
    EN("Brush: {0}%"),   JA("ブラシ: {0}%"),  ZH_HANS("笔刷：{0}%"), ZH_HANT("筆刷：{0}%"),
    KO("브러시: {0}%"),   DE("Pinsel: {0} %"), FR("Pinceau : {0} %"),
    ES("Pincel: {0} %"), PT("Pincel: {0}%"),  IT("Pennello: {0}%"),
    NL("Penseel: {0}%"), RU("Кисть: {0}%"),   TR("Fırça: %{0}"));

SS_MSG(stencil_brush_size_help,
    EN("The brush and eraser radius, as a share of the picture's shorter side, so "
       "it means the same at any resolution. [ and ] step it."),
    JA("ブラシと消しゴムの半径で、画像の短い辺に対する割合です。解像度が違っても"
       "同じ意味になります。[ と ] で変わります。"),
    ZH_HANS("画笔和橡皮擦的半径，按图片短边的比例计，所以在任何分辨率下含义相同。"
            "[ 和 ] 可调整。"),
    ZH_HANT("筆刷和橡皮擦的半徑，按圖片短邊的比例計，所以在任何解析度下含義相同。"
            "[ 和 ] 可調整。"),
    KO("브러시와 지우개의 반지름으로, 사진 짧은 변에 대한 비율이라 어떤 해상도에서도 "
       "같은 뜻입니다. [ 와 ] 로 바꿉니다."),
    DE("Radius von Pinsel und Radierer als Anteil der kürzeren Bildseite, also bei "
       "jeder Auflösung gleich. [ und ] ändern ihn."),
    FR("Le rayon du pinceau et de la gomme, en part du petit côté de l'image, donc "
       "identique à toute résolution. [ et ] le modifient."),
    ES("El radio del pincel y del borrador, como parte del lado corto de la imagen, "
       "así que significa lo mismo a cualquier resolución. [ y ] lo cambian."),
    PT("O raio do pincel e da borracha, como fração do lado menor da imagem, por "
       "isso vale o mesmo em qualquer resolução. [ e ] o alteram."),
    IT("Il raggio di pennello e gomma, come quota del lato corto dell'immagine, "
       "quindi uguale a ogni risoluzione. [ e ] lo cambiano."),
    NL("De straal van penseel en gum, als deel van de korte zijde van de afbeelding, "
       "dus gelijk bij elke resolutie. [ en ] veranderen hem."),
    RU("Радиус кисти и ластика как доля короткой стороны картинки, поэтому он "
       "одинаков при любом разрешении. [ и ] меняют его."),
    TR("Fırça ve silginin yarıçapı, resmin kısa kenarının bir payı olarak; bu yüzden "
       "her çözünürlükte aynıdır. [ ve ] değiştirir."));

SS_MSG(stencil_saved_areas,
    EN("Saved drawn areas"),
    JA("保存した描画範囲"),
    ZH_HANS("已保存的绘制区域"),
    ZH_HANT("已儲存的繪製區域"),
    KO("저장한 그린 영역"),
    DE("Gespeicherte Zeichnungen"),
    FR("Zones dessinées enregistrées"),
    ES("Áreas dibujadas guardadas"),
    PT("Áreas desenhadas salvas"),
    IT("Aree disegnate salvate"),
    NL("Opgeslagen getekende gebieden"),
    RU("Сохранённые области"),
    TR("Kayıtlı çizili alanlar"));

SS_MSG(stencil_load,
    EN("Load..."),       JA("読み込む..."),   ZH_HANS("载入..."),   ZH_HANT("載入..."),
    KO("불러오기..."),    DE("Laden..."),      FR("Charger..."),
    ES("Cargar..."),     PT("Carregar..."),   IT("Carica..."),
    NL("Laden..."),      RU("Загрузить..."),  TR("Yükle..."));

SS_MSG(stencil_save,
    EN("Save..."),       JA("保存..."),       ZH_HANS("保存..."),   ZH_HANT("儲存..."),
    KO("저장..."),        DE("Speichern..."),  FR("Enregistrer..."),
    ES("Guardar..."),    PT("Salvar..."),     IT("Salva..."),
    NL("Opslaan..."),    RU("Сохранить..."),  TR("Kaydet..."));

SS_MSG(stencil_save_help,
    EN("Save the drawn shapes as SVG in normalized coordinates. The fitted lens circle is excluded.\n- Cameras with separate, different shapes get separate files. Loading the set restores each to its camera.\n- Reuse saved sets on other inputs, the dataset screen, or in dataset presets."),
    JA("ここで描いたもの（検出したレンズの円は含みません）を、正規化座標の SVG ファイルとして保存します。カメラごとに別の範囲を使い、カメラで内容が違うときはカメラごとにファイルを分け、読み込むとそれぞれのカメラに戻ります。\n保存したものは別の入力に読み込めるほか、データセット画面で全入力とデータセットのプリセットに使えます。"),
    ZH_HANS("把这里画的内容（不含检测到的镜头圆）保存为归一化坐标的 SVG 文件。每台相机单独设置区域且各相机不同时，每台相机各存一个文件，载入时各自回到对应的相机。\n保存后可以载入到其他输入，也可以在数据集界面中用于所有输入和数据集预设。"),
    ZH_HANT("把這裡畫的內容（不含偵測到的鏡頭圓）儲存為正規化座標的 SVG 檔案。每台相機分別設定區域且各相機不同時，每台相機各存一個檔案，載入時各自回到對應的相機。\n儲存後可以載入到其他輸入，也可以在資料集畫面中用於所有輸入和資料集預設。"),
    KO("여기서 그린 것을(찾아낸 렌즈 원은 빼고) 정규화 좌표의 SVG 파일로 저장합니다.\n- 카메라마다 따로 영역을 지정했고 카메라끼리 다르면 카메라마다 파일을 따로 두고, 불러오면 각자의 카메라로 돌아갑니다.\n- 저장한 것은 다른 입력에 불러올 수 있고, 데이터셋 화면에서 모든 입력과 데이터셋 프리셋에 쓸 수 있습니다."),
    DE("Das hier Gezeichnete ohne den erkannten Objektivkreis als SVG-Datei in normierten Koordinaten speichern.\n- Mit eigenen Bereichen je Kamera und unterschiedlichen Kameras bekommt jede Kamera eine eigene Datei, und beim Laden kommt jede wieder auf ihre Kamera.\n- Gespeichertes lässt sich auf eine andere Eingabe laden und im Datensatz-Bildschirm für alle Eingaben und für Datensatz-Voreinstellungen wählen."),
    FR("Garder ce qui est dessiné ici, sans le cercle d'objectif détecté, dans un fichier SVG en coordonnées normalisées.\n- Avec des zones distinctes par caméra et des caméras qui diffèrent, chaque caméra a son propre fichier, et le chargement remet chacun sur sa caméra.\n- Un ensemble enregistré se charge sur une autre entrée et se choisit dans l'écran du jeu de données pour toutes les entrées et pour les préréglages."),
    ES("Guardar lo dibujado aquí, sin el círculo de objetivo detectado, como archivo SVG en coordenadas normalizadas.\n- Con zonas distintas para cada cámara y cámaras que difieren, cada cámara tiene su propio archivo, y al cargar el juego cada uno vuelve a su cámara.\n- Lo guardado se puede cargar en otra entrada y elegir en la pantalla del conjunto de datos para todas las entradas y los ajustes predefinidos."),
    PT("Guardar o que foi desenhado aqui, sem o círculo de lente detectado, como arquivo SVG em coordenadas normalizadas.\n- Com áreas separadas para cada câmera e câmeras diferentes, cada câmera tem seu próprio arquivo, e ao carregar o conjunto cada um volta à sua câmera.\n- O que foi salvo pode ser carregado em outra entrada e escolhido na tela do conjunto de dados para todas as entradas e para as predefinições."),
    IT("Salva ciò che è disegnato qui, senza il cerchio dell'obiettivo rilevato, come file SVG in coordinate normalizzate.\nCon aree separate per ogni fotocamera e fotocamere diverse, ogni fotocamera ha il suo file, e caricando l'insieme ognuno torna sulla sua fotocamera. Un insieme salvato si carica su un altro input e si sceglie nella schermata del dataset per tutti gli input e per i preset."),
    NL("Bewaar wat hier getekend is, zonder de gevonden lenscirkel, als SVG-bestand in genormaliseerde coördinaten.\n- Met aparte gebieden per camera en camera's die verschillen krijgt elke camera een eigen bestand, en bij het laden gaat elk terug naar zijn camera.\n- Een opgeslagen set kan op een andere invoer worden geladen en op het datasetscherm worden gekozen voor alle invoer en voor datasetvoorinstellingen."),
    RU("Сохранить нарисованное здесь, без найденного круга объектива, в файл SVG в нормированных координатах.\n- При отдельных областях для каждой камеры и разных камерах у каждой камеры свой файл, и при загрузке набора каждый возвращается на свою камеру.\n- Сохранённое можно загрузить на другой вход и выбрать на экране набора данных для всех входов и для пресетов."),
    TR("Burada çizileni, bulunan mercek dairesi olmadan, normalize koordinatlarda bir SVG dosyası olarak sakla.\n- Her kamera için ayrı alanlar açıkken kameralar farklıysa her kameranın kendi dosyası olur ve takım yüklenince her biri kendi kamerasına döner.\n- Kaydedilen, başka bir girdiye yüklenebilir ve veri kümesi ekranında tüm girdiler ve ön ayarlar için seçilebilir."));

SS_MSG(stencil_set_no_camera,
    EN("These areas are for the cameras {0}, and this input has none of them."),
    JA("この範囲はカメラ {0} 用ですが、この入力にはそのどれもありません。"),
    ZH_HANS("这些区域是为相机 {0} 设置的，而此输入没有其中任何一台。"),
    ZH_HANT("這些區域是為相機 {0} 設定的，而此輸入沒有其中任何一台。"),
    KO("이 영역은 카메라 {0} 용인데, 이 입력에는 그중 어느 것도 없습니다."),
    DE("Diese Bereiche gehören zu den Kameras {0}, und diese Eingabe hat keine davon."),
    FR("Ces zones sont pour les caméras {0}, et cette entrée n'en a aucune."),
    ES("Estas zonas son para las cámaras {0}, y esta entrada no tiene ninguna."),
    PT("Estas áreas são para as câmeras {0}, e esta entrada não tem nenhuma delas."),
    IT("Queste aree sono per le fotocamere {0}, e questo input non ne ha nessuna."),
    NL("Deze gebieden zijn voor de camera's {0}, en deze invoer heeft er geen van."),
    RU("Эти области — для камер {0}, а у этого входа нет ни одной из них."),
    TR("Bu alanlar {0} kameraları için ve bu girdide bunların hiçbiri yok."));

SS_MSG(stencil_saved_as,
    EN("Saved: {0}"),    JA("保存しました: {0}"), ZH_HANS("已保存：{0}"), ZH_HANT("已儲存：{0}"),
    KO("저장함: {0}"),    DE("Gespeichert: {0}"), FR("Enregistré : {0}"),
    ES("Guardado: {0}"), PT("Salvo: {0}"),     IT("Salvato: {0}"),
    NL("Opgeslagen: {0}"), RU("Сохранено: {0}"), TR("Kaydedildi: {0}"));

SS_MSG(stencil_save_failed,
    EN("Could not save: {0}"),     JA("保存できませんでした: {0}"),
    ZH_HANS("无法保存：{0}"),       ZH_HANT("無法儲存：{0}"),
    KO("저장할 수 없습니다: {0}"), DE("Speichern fehlgeschlagen: {0}"),
    FR("Enregistrement impossible : {0}"), ES("No se pudo guardar: {0}"),
    PT("Não foi possível salvar: {0}"), IT("Impossibile salvare: {0}"),
    NL("Opslaan mislukt: {0}"),    RU("Не удалось сохранить: {0}"),
    TR("Kaydedilemedi: {0}"));

SS_MSG(stencil_load_failed,
    EN("Could not read drawn areas: {0}"),   JA("描画範囲を読み込めません: {0}"),
    ZH_HANS("无法读取绘制区域：{0}"),         ZH_HANT("無法讀取繪製區域：{0}"),
    KO("그린 영역을 읽을 수 없습니다: {0}"),  DE("Zeichnung nicht lesbar: {0}"),
    FR("Zones dessinées illisibles : {0}"),  ES("No se pudieron leer las áreas: {0}"),
    PT("Não foi possível ler as áreas: {0}"), IT("Impossibile leggere le aree: {0}"),
    NL("Getekende gebieden onleesbaar: {0}"), RU("Не удалось прочитать области: {0}"),
    TR("Çizili alanlar okunamadı: {0}"));

SS_MSG(stencil_in_dataset,
    EN("In this dataset"),     JA("このデータセット内"),   ZH_HANS("此数据集中"),
    ZH_HANT("此資料集中"),      KO("이 데이터셋 안"),       DE("In diesem Datensatz"),
    FR("Dans ce jeu de données"), ES("En este conjunto de datos"),
    PT("Neste conjunto de dados"), IT("In questo dataset"), NL("In deze dataset"),
    RU("В этом наборе данных"), TR("Bu veri kümesinde"));

SS_MSG(stencil_other_file,
    EN("Other file..."),       JA("ほかのファイル..."),    ZH_HANS("其他文件..."),
    ZH_HANT("其他檔案..."),     KO("다른 파일..."),         DE("Andere Datei..."),
    FR("Autre fichier..."),    ES("Otro archivo..."),      PT("Outro arquivo..."),
    IT("Altro file..."),       NL("Ander bestand..."),     RU("Другой файл..."),
    TR("Başka dosya..."));

SS_MSG(stencil_edit,
    EN("Edit"),          JA("編集"),          ZH_HANS("编辑"),     ZH_HANT("編輯"),
    KO("편집"),           DE("Bearbeiten"),    FR("Modifier"),      ES("Editar"),
    PT("Editar"),        IT("Modifica"),      NL("Bewerken"),      RU("Изменить"),
    TR("Düzenle"));

SS_MSG(stencil_editing,
    EN("Editing: {0}"),        JA("編集中: {0}"),          ZH_HANS("正在编辑：{0}"),
    ZH_HANT("正在編輯：{0}"),    KO("편집 중: {0}"),         DE("In Bearbeitung: {0}"),
    FR("En cours de modification : {0}"), ES("Editando: {0}"),  PT("Editando: {0}"),
    IT("In modifica: {0}"),    NL("Wordt bewerkt: {0}"),   RU("Редактируется: {0}"),
    TR("Düzenleniyor: {0}"));

SS_MSG(stencil_save_changes,
    EN("Save changes"),        JA("変更を保存"),           ZH_HANS("保存更改"),
    ZH_HANT("儲存變更"),        KO("변경 내용 저장"),        DE("Änderungen speichern"),
    FR("Enregistrer les modifications"), ES("Guardar cambios"), PT("Salvar alterações"),
    IT("Salva modifiche"),     NL("Wijzigingen opslaan"),  RU("Сохранить изменения"),
    TR("Değişiklikleri kaydet"));

SS_MSG(stencil_delete_title,
    EN("Delete Drawn Areas"),  JA("描画範囲を削除"),       ZH_HANS("删除绘制区域"),
    ZH_HANT("刪除繪製區域"),    KO("그린 영역 삭제"),       DE("Zeichnung löschen"),
    FR("Supprimer les zones dessinées"), ES("Eliminar áreas dibujadas"),
    PT("Excluir áreas desenhadas"), IT("Elimina aree disegnate"),
    NL("Getekende gebieden verwijderen"), RU("Удаление областей"), TR("Çizili alanları sil"));

// IRREVERSIBLE -- human review in every language.
SS_MSG(stencil_delete_confirm,
    EN("Delete the saved drawn areas \"{0}\"?"),
    JA("保存した描画範囲「{0}」を削除しますか？"),
    ZH_HANS("要删除已保存的绘制区域「{0}」吗？"),
    ZH_HANT("要刪除已儲存的繪製區域「{0}」嗎？"),
    KO("저장한 그린 영역 ‘{0}’을(를) 삭제할까요?"),
    DE("Die gespeicherte Zeichnung „{0}“ löschen?"),
    FR("Supprimer les zones dessinées enregistrées « {0} » ?"),
    ES("¿Eliminar las áreas dibujadas guardadas «{0}»?"),
    PT("Excluir as áreas desenhadas salvas “{0}”?"),
    IT("Eliminare le aree disegnate salvate «{0}»?"),
    NL("De opgeslagen getekende gebieden „{0}” verwijderen?"),
    RU("Удалить сохранённые области «{0}»?"),
    TR("«{0}» kayıtlı çizili alanları silinsin mi?"));

SS_MSG(stencil_deleted,
    EN("Deleted: {0}"),        JA("削除しました: {0}"),    ZH_HANS("已删除：{0}"),
    ZH_HANT("已刪除：{0}"),     KO("삭제했습니다: {0}"),    DE("Gelöscht: {0}"),
    FR("Supprimé : {0}"),      ES("Eliminado: {0}"),       PT("Excluído: {0}"),
    IT("Eliminato: {0}"),      NL("Verwijderd: {0}"),      RU("Удалено: {0}"),
    TR("Silindi: {0}"));

SS_MSG(stencil_delete_failed,
    EN("Could not delete: {0}"), JA("削除できませんでした: {0}"), ZH_HANS("无法删除：{0}"),
    ZH_HANT("無法刪除：{0}"),   KO("삭제하지 못했습니다: {0}"), DE("Löschen fehlgeschlagen: {0}"),
    FR("Suppression impossible : {0}"), ES("No se pudo eliminar: {0}"),
    PT("Não foi possível excluir: {0}"), IT("Impossibile eliminare: {0}"),
    NL("Verwijderen mislukt: {0}"), RU("Не удалось удалить: {0}"), TR("Silinemedi: {0}"));

SS_MSG(stencil_rename_title,
    EN("Rename Drawn Areas"),  JA("描画範囲の名前を変更"), ZH_HANS("重命名绘制区域"),
    ZH_HANT("重新命名繪製區域"), KO("그린 영역 이름 바꾸기"), DE("Zeichnung umbenennen"),
    FR("Renommer les zones dessinées"), ES("Renombrar áreas dibujadas"),
    PT("Renomear áreas desenhadas"), IT("Rinomina aree disegnate"),
    NL("Getekende gebieden hernoemen"), RU("Переименование областей"),
    TR("Çizili alanları yeniden adlandır"));

SS_MSG(stencil_renamed,
    EN("Renamed to: {0}"),     JA("名前を変更しました: {0}"), ZH_HANS("已重命名为：{0}"),
    ZH_HANT("已重新命名為：{0}"), KO("이름을 바꿨습니다: {0}"), DE("Umbenannt in: {0}"),
    FR("Renommé en : {0}"),    ES("Renombrado a: {0}"),    PT("Renomeado para: {0}"),
    IT("Rinominato in: {0}"),  NL("Hernoemd naar: {0}"),   RU("Новое имя: {0}"),
    TR("Yeni ad: {0}"));

SS_MSG(stencil_rename_failed,
    EN("Could not rename: {0}"), JA("名前を変更できませんでした: {0}"), ZH_HANS("无法重命名：{0}"),
    ZH_HANT("無法重新命名：{0}"), KO("이름을 바꾸지 못했습니다: {0}"),
    DE("Umbenennen fehlgeschlagen: {0}"), FR("Renommage impossible : {0}"),
    ES("No se pudo renombrar: {0}"), PT("Não foi possível renomear: {0}"),
    IT("Impossibile rinominare: {0}"), NL("Hernoemen mislukt: {0}"),
    RU("Не удалось переименовать: {0}"), TR("Yeniden adlandırılamadı: {0}"));

SS_MSG(stencil_name_taken,
    EN("Saved drawn areas named \"{0}\" already exist."),
    JA("「{0}」という名前の描画範囲はすでにあります。"),
    ZH_HANS("名为「{0}」的绘制区域已存在。"),
    ZH_HANT("名為「{0}」的繪製區域已存在。"),
    KO("‘{0}’(이)라는 이름의 그린 영역이 이미 있습니다."),
    DE("Eine Zeichnung namens „{0}“ gibt es bereits."),
    FR("Des zones dessinées nommées « {0} » existent déjà."),
    ES("Ya existen áreas dibujadas llamadas «{0}»."),
    PT("Já existem áreas desenhadas chamadas “{0}”."),
    IT("Esistono già aree disegnate chiamate «{0}»."),
    NL("Er bestaan al getekende gebieden met de naam „{0}”."),
    RU("Области с именем «{0}» уже есть."),
    TR("«{0}» adlı çizili alanlar zaten var."));

SS_MSG(stencil_pick_file,
    EN("Load drawn areas"),  JA("描画範囲を読み込む"),   ZH_HANS("载入绘制区域"),
    ZH_HANT("載入繪製區域"),    KO("그린 영역 불러오기"),   DE("Zeichnung laden"),
    FR("Charger des zones dessinées"), ES("Cargar áreas dibujadas"),
    PT("Carregar áreas desenhadas"), IT("Carica aree disegnate"),
    NL("Getekende gebieden laden"), RU("Загрузить области"), TR("Çizili alanları yükle"));

SS_MSG(stencil_autosaved,
    EN("Drawn areas kept with the dataset: {0}"),
    JA("描画範囲をデータセットと一緒に保存しました: {0}"),
    ZH_HANS("绘制区域已随数据集保存：{0}"),
    ZH_HANT("繪製區域已隨資料集儲存：{0}"),
    KO("그린 영역을 데이터셋과 함께 저장했습니다: {0}"),
    DE("Zeichnung beim Datensatz gespeichert: {0}"),
    FR("Zones dessinées conservées avec le jeu de données : {0}"),
    ES("Áreas dibujadas guardadas con el conjunto de datos: {0}"),
    PT("Áreas desenhadas guardadas com o conjunto de dados: {0}"),
    IT("Aree disegnate salvate con il dataset: {0}"),
    NL("Getekende gebieden bij de dataset bewaard: {0}"),
    RU("Области сохранены вместе с набором данных: {0}"),
    TR("Çizili alanlar veri kümesiyle birlikte saklandı: {0}"));

SS_MSG(stencil_areas_preset,
    EN("Drawn areas"),   JA("描画範囲"),      ZH_HANS("绘制区域"),  ZH_HANT("繪製區域"),
    KO("그린 영역"),      DE("Zeichnung"),     FR("Zones dessinées"),
    ES("Áreas dibujadas"), PT("Áreas desenhadas"), IT("Aree disegnate"),
    NL("Getekende gebieden"), RU("Области"),   TR("Çizili alanlar"));

SS_MSG(stencil_areas_help,
    EN("Shapes over what is never scene: a watermark, the operator, the pole.\n- Pick a set saved from Try the mask..., or Draw... to draw on this input there.\n- A lens that follows gets the set's list for that lens, or the first lens's where it has none.\n- When every row draws one set, a dataset preset keeps its name for a batch run."),
    JA("透かし、撮影者、棒など、シーンではないものに重ねる図形です。\n- 「マスクを試す…」で保存したセットを選ぶか、「描く...」でこの入力にそこで描きます。\n- 追従するレンズには、セットのそのレンズ用の図形が、なければ最初のレンズのものが入ります。\n- すべての行が同じセットを描いていれば、その名前がデータセットのプリセットに残り、バッチ処理でも使われます。"),
    ZH_HANS("盖在不属于场景的东西上的图形：水印、拍摄者、杆子。\n- 选一组在“试一下蒙版…”里保存的区域，或用“绘制...”到那里给这个输入画。\n- 跟随的镜头会得到这组中属于它的区域，没有的话就用第一个镜头的。\n- 所有行都画同一组时，数据集预设会记下它的名字，批处理也会用到。"),
    ZH_HANT("蓋在不屬於場景的東西上的圖形：浮水印、拍攝者、桿子。\n- 選一組在「試一下遮罩…」裡儲存的區域，或用「繪製...」到那裡給這個輸入畫。\n- 跟隨的鏡頭會得到這組中屬於它的區域，沒有的話就用第一個鏡頭的。\n- 所有行都畫同一組時，資料集預設會記下它的名字，批次處理也會用到。"),
    KO("워터마크, 촬영자, 장대처럼 장면이 아닌 것 위에 얹는 도형입니다.\n- '마스크 시험해 보기…'에서 저장한 세트를 고르거나, '그리기...'로 거기서 이 입력에 그립니다.\n- 따르는 렌즈에는 세트에서 그 렌즈 몫이, 없으면 첫 렌즈의 것이 들어갑니다.\n- 모든 행이 한 세트를 그리면 데이터셋 프리셋이 그 이름을 기억해 일괄 처리에도 쓰입니다."),
    DE("Formen für Wasserzeichen, Filmende oder Stangen.\n- Gespeicherte Bereiche unter „Maske ausprobieren“ wählen oder mit „Zeichnen“ erstellen.\n- Folgende Objektive nutzen ihre eigenen Bereiche, sonst die des ersten.\n- Nutzen alle Zeilen dasselbe Set, speichert das Datensatz-Preset dessen Namen für Stapelläufe."),
    FR("Des formes sur ce qui n'est jamais la scène : un filigrane, l'opérateur, la perche. Choisissez un ensemble enregistré depuis « Essayer le masque… », ou « Dessiner...\n- » pour dessiner là-bas sur cette entrée.\n- Les objectifs qui suivent reçoivent les zones de leur objectif, ou à défaut celles du premier.\n- Quand chaque ligne dessine le même, un préréglage du jeu de données garde son nom pour un traitement par lots."),
    ES("Formas sobre lo que nunca es escena: una marca de agua, el operador, el palo.\n- Elija un conjunto guardado desde «Probar la máscara…», o «Dibujar...» para dibujar allí sobre esta entrada.\n- Los objetivos que siguen reciben las áreas de su objetivo, o las del primero si no las hay.\n- Si todas las filas dibujan el mismo, el ajuste predefinido guarda su nombre para un lote."),
    PT("Formas sobre o que nunca é cena: uma marca d'água, o operador, o bastão.\n- Escolha um conjunto salvo em «Testar a máscara…», ou «Desenhar...» para desenhar lá nesta entrada.\n- As lentes que seguem recebem as áreas da sua lente, ou as da primeira quando não houver.\n- Quando todas as linhas desenham o mesmo, a predefinição do conjunto de dados guarda o nome dele para um lote."),
    IT("Forme su ciò che non è mai scena: una filigrana, l'operatore, l'asta.\n- Scegli un insieme salvato da «Prova la maschera…», o «Disegna...» per disegnare lì su questo input.\n- Gli obiettivi che seguono ricevono le aree del proprio obiettivo, o quelle del primo se mancano.\n- Se ogni riga disegna lo stesso, il preset del dataset ne conserva il nome per un'elaborazione in batch."),
    NL("Vormen over wat nooit scène is: een watermerk, de cameraman, de stok.\n- Kies een set opgeslagen via ‘Masker uitproberen…’, of ‘Tekenen...’ om daar op deze invoer te tekenen.\n- Lenzen die volgen krijgen de gebieden van hun lens, of anders die van de eerste.\n- Tekent elke rij dezelfde, dan bewaart een datasetvoorinstelling de naam voor een batch."),
    RU("Фигуры поверх того, что не относится к сцене: водяного знака, оператора, палки.\n- Выберите набор, сохранённый в «Проверить маску…», или «Нарисовать...», чтобы нарисовать там на этом входе.\n- Следующие объективы получают области своего объектива, а если их нет -- области первого.\n- Если все строки рисуют один набор, пресет набора данных хранит его имя для пакетного запуска."),
    TR("Hiç sahne olmayanın üstüne çizilen biçimler: filigran, çeken kişi, çubuk.\n- \"Maskeyi dene…\" içinden kaydedilmiş bir set seçin ya da orada bu girdiye çizmek için \"Çiz...\".\n- İzleyen mercekler setten kendi merceğinin alanlarını, yoksa ilk merceğinkini alır.\n- Her satır aynı seti çiziyorsa, veri kümesi ön ayarı adını toplu işlem için saklar."));

SS_MSG(stencil_rows_help,
    EN("There is one row per input lens.\n- A matching row follows the one above until you choose different settings. It shows “same as above.”\n- An input’s first lens follows the previous input’s first lens. For one camera, set up the first row."),
    JA("入力ごと、複数レンズのカメラならレンズごとに 1 行です。追従先の行（すぐ上の行、入力の最初のレンズならすぐ上の入力の最初のレンズ）と最初から同じ行は「（上と同じ）」と表示され、別の選択肢を選ぶまでその行と一緒に変わります。\n1台のカメラで撮った素材なら最初の行だけで設定できます。"),
    ZH_HANS("每个输入一行，有多个镜头的相机则每个镜头一行。一开始就与它跟随的那一行（上一行，输入的第一个镜头则是正上方那个输入的第一个镜头）相同的行显示“（同上）”，在给它选别的选项之前，那一行变了它也跟着变。\n所以用一台相机拍的素材在第一行设好即可。"),
    ZH_HANT("每個輸入一行，有多個鏡頭的相機則每個鏡頭一行。一開始就與它跟隨的那一行（上一行，輸入的第一個鏡頭則是正上方那個輸入的第一個鏡頭）相同的行顯示「（同上）」，在給它選別的選項之前，那一行變了它也跟著變。\n所以用一台相機拍的素材在第一行設好即可。"),
    KO("입력마다 한 행, 렌즈가 여럿인 카메라는 렌즈마다 한 행입니다. 따르는 행(바로 위 행, 입력의 첫 렌즈라면 바로 위 입력의 첫 렌즈)과 처음부터 같은 행은 '(위와 같음)'으로 보이고, 다른 것을 고르기 전까지 그 행이 바뀌면 함께 바뀝니다.\n한 카메라로 찍은 촬영은 첫 행에서 설정하면 됩니다."),
    DE("Eine Zeile je Eingabe, bei einer Kamera mit mehreren Objektiven je Objektiv. Eine Zeile, die anfangs der Zeile gleicht, der sie folgt -- der darüber, beim ersten Objektiv einer Eingabe dem ersten Objektiv der Eingabe direkt darüber --, zeigt (wie oben) und ändert sich mit ihr, bis für sie etwas anderes gewählt wird.\nEine Aufnahme mit einer Kamera wird also in ihrer ersten Zeile eingerichtet."),
    FR("Une ligne par entrée, et par objectif pour une caméra qui en a plusieurs. Une ligne qui égale au départ la ligne qu'elle suit -- celle du dessus, ou pour le premier objectif d'une entrée le premier objectif de l'entrée juste au-dessus -- affiche (comme ci-dessus) et change avec elle, jusqu'à ce qu'on lui choisisse autre chose.\nUne prise faite avec une seule caméra se règle donc sur sa première ligne."),
    ES("Una fila por entrada, y por objetivo en una cámara con varios. Una fila que al principio coincide con la fila a la que sigue -- la de arriba, o para el primer objetivo de una entrada el primer objetivo de la entrada justo encima -- muestra (igual que arriba) y cambia con ella hasta que se le elija otra cosa.\nAsí, una captura hecha con una cámara se ajusta en su primera fila."),
    PT("Uma linha por entrada, e por lente numa câmera com várias. Uma linha que de início coincide com a linha que segue -- a de cima, ou, para a primeira lente de uma entrada, a primeira lente da entrada logo acima -- mostra (igual ao de cima) e muda com ela até que se escolha outra coisa para ela.\nAssim, uma captura feita com uma câmera se ajusta na primeira linha."),
    IT("Una riga per input, e per obiettivo in una fotocamera che ne ha più d'uno.\n- Una riga che all'inizio coincide con la riga che segue -- quella sopra, o per il primo obiettivo di un input il primo obiettivo dell'input subito sopra -- mostra (come sopra) e cambia con quella finché non le si sceglie altro.\n- Una ripresa fatta con una sola fotocamera si imposta quindi sulla prima riga."),
    NL("Eén rij per invoer, en per lens bij een camera met meerdere. Een rij die aanvankelijk gelijk is aan de rij die hij volgt -- de rij erboven, of voor de eerste lens van een invoer de eerste lens van de invoer direct erboven -- toont (zelfde als hierboven) en verandert mee tot je er iets anders voor kiest.\nEen opname met één camera stel je dus in op de eerste rij."),
    RU("Строка на каждый вход, а у камеры с несколькими объективами -- на каждый объектив. Строка, которая сначала совпадает со строкой, за которой следует, -- верхней, а для первого объектива входа первым объективом входа прямо над ней, -- показывает «(как выше)» и меняется вместе с ней, пока для неё не выбрано что-то другое.\nСъёмку одной камерой достаточно настроить в первой строке."),
    TR("Her girdi için bir satır, birden çok merceği olan kamerada her mercek için bir satır.\n- İzlediği satırla -- üstteki satır ya da bir girdinin ilk merceği için hemen üstteki girdinin ilk merceği -- başta aynı olan bir satır (yukarıdakiyle aynı) gösterir ve onun için başka bir şey seçilene kadar onunla birlikte değişir.\n- Tek kamerayla çekilmiş bir çekim bu yüzden ilk satırında ayarlanır."));

SS_MSG(stencil_rows_same_help,
    EN("The same as {0}, and changes when it does. Pick a choice of this row's own "
       "to stop following it."),
    JA("{0} と同じで、それが変わると一緒に変わります。追従をやめるには、この行"
       "独自の選択肢を選んでください。"),
    ZH_HANS("与 {0} 相同，它变了这一行也跟着变。选一个这一行自己的选项即可不再跟随。"),
    ZH_HANT("與 {0} 相同，它變了這一行也跟著變。選一個這一行自己的選項即可不再跟隨。"),
    KO("{0}과(와) 같으며, 그것이 바뀌면 함께 바뀝니다. 따르지 않으려면 이 행만의 "
       "선택지를 고르세요."),
    DE("Wie {0}, und ändert sich mit ihr. Eine eigene Auswahl für diese Zeile beendet "
       "das Folgen."),
    FR("Comme {0}, et change avec elle. Choisissez une option propre à cette ligne "
       "pour cesser de la suivre."),
    ES("Igual que {0}, y cambia cuando ella cambia. Elija una opción propia de esta "
       "fila para dejar de seguirla."),
    PT("Igual a {0}, e muda quando ela muda. Escolha uma opção própria desta linha "
       "para deixar de segui-la."),
    IT("Come {0}, e cambia quando cambia quella. Scegli un'opzione propria di questa "
       "riga per smettere di seguirla."),
    NL("Zelfde als {0}, en verandert mee. Kies een eigen keuze voor deze rij om niet "
       "meer te volgen."),
    RU("Как {0}, и меняется вместе с ней. Выберите для этой строки свой вариант, "
       "чтобы перестать за ней следовать."),
    TR("{0} ile aynı ve o değişince değişir. İzlemeyi bırakmak için bu satıra özgü "
       "bir seçenek seçin."));

SS_MSG(stencil_rows_input,
    EN("Input"), JA("入力"), ZH_HANS("输入"), ZH_HANT("輸入"), KO("입력"),
    DE("Eingabe"), FR("Entrée"), ES("Entrada"), PT("Entrada"), IT("Input"),
    NL("Invoer"), RU("Вход"), TR("Girdi"));

SS_MSG(stencil_rows_border,
    EN("Fisheye border"), JA("魚眼の黒枠"), ZH_HANS("鱼眼黑边"), ZH_HANT("魚眼黑邊"),
    KO("어안 테두리"), DE("Fisheye-Rand"), FR("Bord du fisheye"),
    ES("Borde de ojo de pez"), PT("Borda olho-de-peixe"), IT("Bordo fisheye"),
    NL("Fisheye-rand"), RU("Край фишая"), TR("Balıkgözü kenarı"));

SS_MSG(stencil_rows_cut,
    EN("Cut away"), JA("切り落とす"), ZH_HANS("裁掉"), ZH_HANT("裁掉"),
    KO("잘라내기"), DE("Wegschneiden"), FR("Découper"), ES("Recortar"),
    PT("Recortar"), IT("Ritagliare"), NL("Wegsnijden"), RU("Срезать"), TR("Kes"));

SS_MSG(stencil_rows_off,
    EN("Off"), JA("しない"), ZH_HANS("不裁"), ZH_HANT("不裁"),
    KO("안 함"), DE("Aus"), FR("Non"), ES("No"), PT("Não"), IT("No"),
    NL("Uit"), RU("Нет"), TR("Kapalı"));

SS_MSG(stencil_rows_off_help,
    EN("Keep the whole frame up to its edge. Drawn areas still apply."),
    JA("画面を端まで残します。描画範囲はそのまま適用されます。"),
    ZH_HANS("保留整个画面直到边缘。绘制区域照常生效。"),
    ZH_HANT("保留整個畫面直到邊緣。繪製區域照常生效。"),
    KO("화면을 가장자리까지 그대로 둡니다. 그린 영역은 그대로 적용됩니다."),
    DE("Das ganze Bild bis zum Rand behalten. Gezeichnete Bereiche gelten weiter."),
    FR("Garder toute l'image jusqu'au bord. Les zones dessinées s'appliquent "
       "toujours."),
    ES("Conservar el fotograma entero hasta el borde. Las áreas dibujadas siguen "
       "aplicándose."),
    PT("Manter o quadro inteiro até a borda. As áreas desenhadas continuam valendo."),
    IT("Tenere l'intero fotogramma fino al bordo. Le aree disegnate valgono comunque."),
    NL("Het hele beeld tot de rand houden. Getekende gebieden gelden nog steeds."),
    RU("Оставить кадр целиком до края. Нарисованные области по-прежнему действуют."),
    TR("Kareyi kenarına kadar tut. Çizili alanlar yine uygulanır."));

SS_MSG(stencil_rows_none,
    EN("None"), JA("なし"), ZH_HANS("无"), ZH_HANT("無"), KO("없음"),
    DE("Keine"), FR("Aucune"), ES("Ninguna"), PT("Nenhuma"), IT("Nessuna"),
    NL("Geen"), RU("Нет"), TR("Yok"));

SS_MSG(stencil_rows_drawn,
    EN("Drawn by hand"), JA("手描き"), ZH_HANS("手绘"), ZH_HANT("手繪"),
    KO("직접 그림"), DE("Von Hand gezeichnet"), FR("Dessiné à la main"),
    ES("Dibujado a mano"), PT("Desenhado à mão"), IT("Disegnato a mano"),
    NL("Met de hand getekend"), RU("Нарисовано вручную"), TR("Elle çizildi"));

SS_MSG(stencil_rows_draw,
    EN("Draw..."), JA("描く..."), ZH_HANS("绘制..."), ZH_HANT("繪製..."),
    KO("그리기..."), DE("Zeichnen..."), FR("Dessiner..."), ES("Dibujar..."),
    PT("Desenhar..."), IT("Disegna..."), NL("Tekenen..."), RU("Нарисовать..."),
    TR("Çiz..."));

SS_MSG(stencil_rows_draw_help,
    EN("Open Try the mask on this input to draw its areas. Rows that are the same "
       "as this one follow what is drawn."),
    JA("この入力で「マスクを試す」を開き、範囲を描きます。この行と同じ行は、描いた"
       "内容に従います。"),
    ZH_HANS("在这个输入上打开“试一下蒙版”来画它的区域。与这一行相同的行会跟着所画的变。"),
    ZH_HANT("在這個輸入上開啟「試一下遮罩」來畫它的區域。與這一行相同的行會跟著所畫的變。"),
    KO("이 입력에서 '마스크 시험해 보기'를 열어 영역을 그립니다. 이 행과 같은 행은 "
       "그린 대로 따라갑니다."),
    DE("„Maske ausprobieren“ für diese Eingabe öffnen, um ihre Bereiche zu zeichnen. "
       "Zeilen, die dieser gleichen, folgen dem Gezeichneten."),
    FR("Ouvrir « Essayer le masque » sur cette entrée pour dessiner ses zones. Les "
       "lignes identiques à celle-ci suivent ce qui est dessiné."),
    ES("Abrir «Probar la máscara» en esta entrada para dibujar sus áreas. Las filas "
       "iguales a esta siguen lo que se dibuje."),
    PT("Abrir «Testar a máscara» nesta entrada para desenhar suas áreas. As linhas "
       "iguais a esta seguem o que for desenhado."),
    IT("Apri «Prova la maschera» su questo input per disegnarne le aree. Le righe "
       "uguali a questa seguono ciò che viene disegnato."),
    NL("‘Masker uitproberen’ op deze invoer openen om de gebieden te tekenen. Rijen "
       "die gelijk zijn aan deze volgen wat er getekend wordt."),
    RU("Открыть «Проверить маску» на этом входе, чтобы нарисовать его области. "
       "Строки, совпадающие с этой, последуют за нарисованным."),
    TR("Alanlarını çizmek için bu girdide \"Maskeyi dene\"yi açın. Bu satırla aynı "
       "olan satırlar çizileni izler."));

SS_MSG(stencil_add_path_help,
    EN("Click along an edge on the picture to drop anchors; the path snaps to the edge between them.\nClick the first anchor, press Enter or right-click to close it. Ctrl+Z takes the last anchor back; Esc cancels."),
    JA("画像の輪郭に沿ってクリックすると点が置かれ、点と点の間はその輪郭に沿って結ばれます。\n最初の点をクリックするか、Enter または右クリックで閉じます。 Ctrl+Z で最後の点を取り消し、Esc で中止します。"),
    ZH_HANS("沿着图上的边缘点击放下锚点，锚点之间的路径会贴合边缘。点击第一个锚点、按 Enter 或右键即可闭合。Ctrl+Z 撤回最后一个锚点，Esc 取消。"),
    ZH_HANT("沿著圖上的邊緣點擊放下錨點，錨點之間的路徑會貼合邊緣。點擊第一個錨點、按 Enter 或右鍵即可閉合。Ctrl+Z 收回最後一個錨點，Esc 取消。"),
    KO("가장자리를 클릭하면 앵커 사이 경로가 가장자리에 붙습니다. 첫 앵커 클릭, Enter 또는 오른쪽 클릭으로 닫습니다. Ctrl+Z는 앵커 취소, Esc는 전체 취소입니다."),
    DE("Entlang einer Kante im Bild klicken, um Ankerpunkte zu setzen; der Pfad legt sich dazwischen an die Kante.\n- Den ersten Anker anklicken, Eingabe drücken oder rechts klicken schließt ihn.\n- Strg+Z nimmt den letzten Anker zurück, Esc bricht ab."),
    FR("Cliquez le long d'un contour de l'image pour poser des ancres ; le chemin épouse le contour entre elles.\n- Cliquez la première ancre, appuyez sur Entrée ou faites un clic droit pour le fermer.\n- Ctrl+Z retire la dernière ancre, Échap annule."),
    ES("Haga clic a lo largo de un borde de la imagen para poner anclas; el trazado se ajusta al borde entre ellas.\n- Haga clic en la primera ancla, pulse Intro o haga clic derecho para cerrarlo.\n- Ctrl+Z quita la última ancla; Esc cancela."),
    PT("Clique ao longo de um contorno da imagem para pôr âncoras; o traçado cola-se ao contorno entre elas.\n- Clique na primeira âncora, prima Enter ou clique com o botão direito para fechar.\n- Ctrl+Z retira a última âncora; Esc cancela."),
    IT("Clic lungo un bordo: il tracciato segue il bordo tra gli ancoraggi. Primo ancoraggio, Invio o clic destro: chiudi. Ctrl+Z: togli ancoraggio; Esc: annulla."),
    NL("Klik langs een rand in het beeld om ankers te zetten; het pad volgt de rand ertussen.\n- Klik op het eerste anker, druk op Enter of klik rechts om het te sluiten.\n- Ctrl+Z neemt het laatste anker terug; Esc breekt af."),
    RU("Щёлкайте вдоль края на снимке, чтобы ставить опорные точки; контур между ними прилипает к краю.\n- Щёлкните первую точку, нажмите Enter или правую кнопку, чтобы замкнуть.\n- Ctrl+Z убирает последнюю точку, Esc отменяет."),
    TR("Kenar boyunca tıklayın; yol çapalar arasında kenara oturur. İlk çapa, Enter veya sağ tık: kapat. Ctrl+Z: son çapayı geri al. Esc: iptal."));

SS_MSG(stencil_path_anchors,
    EN("Path anchors: {0}"),
    JA("パスの点: {0}"),
    ZH_HANS("路径锚点：{0}"),
    ZH_HANT("路徑錨點：{0}"),
    KO("패스 앵커: {0}"),
    DE("Pfadanker: {0}"),
    FR("Ancres du chemin : {0}"),
    ES("Anclas del trazado: {0}"),
    PT("Âncoras do traçado: {0}"),
    IT("Ancoraggi del tracciato: {0}"),
    NL("Padankers: {0}"),
    RU("Точек контура: {0}"),
    TR("Yol çapaları: {0}"));

SS_MSG(stencil_removes_inside,
    EN("removes the inside"),
    JA("内側を消す"),
    ZH_HANS("去掉内部"),
    ZH_HANT("去掉內部"),
    KO("안쪽을 없앰"),
    DE("nimmt das Innere weg"),
    FR("retire l'intérieur"),
    ES("quita el interior"),
    PT("tira o interior"),
    IT("toglie l'interno"),
    NL("haalt de binnenkant weg"),
    RU("убирает нутро"),
    TR("içini atar"));

SS_MSG(stencil_keeps_inside,
    EN("keeps the inside"),
    JA("内側を残す"),
    ZH_HANS("保留内部"),
    ZH_HANT("保留內部"),
    KO("안쪽을 남김"),
    DE("behält das Innere"),
    FR("garde l'intérieur"),
    ES("conserva el interior"),
    PT("mantém o interior"),
    IT("tiene l'interno"),
    NL("houdt de binnenkant"),
    RU("оставляет нутро"),
    TR("içini tutar"));

SS_MSG(stencil_flip_help,
    EN("Click to swap keep/remove. Shapes apply from top to bottom, each adding or removing its interior."),
    JA("クリックで残す・除外を切り替えます。図形は上から順に適用し、内側を追加または除外します。"),
    ZH_HANS("点击切换保留/移除。形状自上而下依次应用，分别添加或移除其内部。"),
    ZH_HANT("點擊切換保留/移除。形狀自上而下依序套用，分別新增或移除其內部。"),
    KO("클릭해 유지/제거를 바꿉니다. 도형은 위에서 아래로 적용하며 각각 안쪽을 추가하거나 제거합니다."),
    DE("Klicken wechselt Behalten/Entfernen. Formen werden von oben nach unten angewandt und fügen ihr Inneres hinzu oder entfernen es."),
    FR("Cliquer inverse garder/retirer. Les formes s’appliquent de haut en bas, chacune ajoutant ou retirant son intérieur."),
    ES("Haz clic para cambiar conservar/quitar. Las formas se aplican de arriba abajo, añadiendo o quitando su interior."),
    PT("Clique para trocar manter/remover. As formas aplicam-se de cima para baixo, adicionando ou removendo o interior."),
    IT("Clic per scambiare mantieni/rimuovi. Le forme si applicano dall’alto in basso, aggiungendo o togliendo il loro interno."),
    NL("Klik om behouden/verwijderen om te wisselen. Vormen werken van boven naar beneden en voegen hun binnenkant toe of verwijderen die."),
    RU("Клик меняет сохранить/удалить. Фигуры применяются сверху вниз, добавляя или убирая свою внутренность."),
    TR("Tut/kaldır değiştirmek için tıklayın. Şekiller yukarıdan aşağı uygulanır; her biri içini ekler veya kaldırır."));

SS_MSG(stencil_drag_hint,
    EN("Drag it on the picture to move it, or its white dots to resize it."),
    JA("画像の上でドラッグすると動き、白い点をドラッグすると大きさが変わります。"),
    ZH_HANS("在图上拖动它可以移动，拖动白点可以改变大小。"),
    ZH_HANT("在圖上拖曳它可以移動，拖曳白點可以改變大小。"),
    KO("사진 위에서 끌면 옮겨지고, 흰 점을 끌면 크기가 바뀝니다."),
    DE("Im Bild ziehen verschiebt sie, an den weißen Punkten ziehen ändert die "
       "Größe."),
    FR("Faites-la glisser sur l'image pour la déplacer, ou ses points blancs "
       "pour la redimensionner."),
    ES("Arrástrela sobre la imagen para moverla, o sus puntos blancos para "
       "cambiar su tamaño."),
    PT("Arraste-a na imagem para movê-la, ou os pontos brancos para "
       "redimensioná-la."),
    IT("La trascini sull'immagine per spostarla, o i suoi punti bianchi per "
       "ridimensionarla."),
    NL("Sleep hem op het beeld om te verplaatsen, of zijn witte stippen om te "
       "schalen."),
    RU("Тяните её по снимку, чтобы двигать, а белые точки -- чтобы менять "
       "размер."),
    TR("Taşımak için resimde sürükleyin, boyutlandırmak için beyaz noktalarını "
       "sürükleyin."));

SS_MSG(stencil_shape_curve,
    EN("Pen shape {0}"),  JA("ペン図形 {0}"),   ZH_HANS("钢笔形状 {0}"), ZH_HANT("鋼筆形狀 {0}"),
    KO("펜 도형 {0}"),     DE("Zeichenstift-Form {0}"), FR("Forme à la plume {0}"),
    ES("Forma de pluma {0}"), PT("Forma da caneta {0}"), IT("Forma a penna {0}"),
    NL("Penvorm {0}"),    RU("Фигура пером {0}"), TR("Kalem şekli {0}"));

SS_MSG(stencil_tool_points,
    EN("Points"),        JA("ポイント"),      ZH_HANS("锚点"),     ZH_HANT("錨點"),
    KO("포인트"),         DE("Punkte"),        FR("Points"),
    ES("Puntos"),        PT("Pontos"),        IT("Punti"),
    NL("Punten"),        RU("Точки"),         TR("Noktalar"));

SS_MSG(stencil_tool_points_help,
    EN("Direct selection: drag a pen shape's anchors (squares) and handles (circles).\n- A handle of a smooth anchor turns the other with it;\n- Alt+drag moves it alone, Shift keeps 45-degree steps, Delete removes the picked anchor.\n- Click inside a shape to pick it; other shapes move and resize as under Select."),
    JA("ダイレクト選択: ペン図形の点（四角）とハンドル（丸）をドラッグします。\n- なめらかな点のハンドルは反対側も一緒に回り、Alt+ドラッグで片方だけ、Shift で 45 度刻みになります。\n- Delete で選んだ点を削除します。 図形の内側をクリックすると選べます。\n- ほかの図形は「選択」と同じく移動と大きさの変更ができます。"),
    ZH_HANS("直接选择：拖动钢笔形状的锚点（方块）和手柄（圆点）。\n- 平滑锚点的一侧手柄会带着另一侧转动； Alt+拖动只移动一侧，Shift 以 45 度为步长，Delete 删除选中的锚点。\n- 在形状内部单击即可选中； 其他形状与“选择”一样可移动和改变大小。"),
    ZH_HANT("直接選取：拖曳鋼筆形狀的錨點（方塊）和控制把手（圓點）。\n- 平滑錨點的一側把手會帶著另一側轉動； Alt+拖曳只移動一側，Shift 以 45 度為步長，Delete 刪除選取的錨點。\n- 在形狀內部點一下即可選取； 其他形狀與「選取」一樣可移動和改變大小。"),
    KO("직접 선택: 펜 도형의 앵커(네모)와 핸들(동그라미)을 드래그합니다.\n- 부드러운 앵커의 핸들은 반대쪽도 함께 돌고, Alt+드래그는 한쪽만 옮기며, Shift는 45도 단위로 맞춥니다.\n- Delete는 고른 앵커를 지웁니다.\n- 도형 안쪽을 클릭하면 고를 수 있고, 다른 도형은 선택 도구처럼 옮기거나 크기를 바꿉니다."),
    DE("Direktauswahl: die Anker (Quadrate) und Griffe (Kreise) einer Zeichenstift-Form ziehen.\n- Der Griff eines glatten Ankers dreht den anderen mit;\n- Alt+Ziehen bewegt ihn allein, Umschalt rastet in 45-Grad-Schritten ein, Entf löscht den gewählten Anker.\n- Ein Klick in eine Form wählt sie; andere Formen werden wie unter Auswählen verschoben und skaliert."),
    FR("Sélection directe : faites glisser les ancres (carrés) et poignées (cercles) d'une forme à la plume.\n- La poignée d'une ancre lisse fait tourner l'autre ;\n- Alt+glisser la déplace seule, Maj force des pas de 45 degrés, Suppr retire l'ancre choisie.\n- Un clic dans une forme la choisit ; les autres formes se déplacent et se redimensionnent comme avec Sélectionner."),
    ES("Selección directa: arrastre las anclas (cuadrados) y tiradores (círculos) de una forma de pluma.\n- El tirador de un ancla suave gira el otro con él;\n- Alt+arrastrar lo mueve solo, Mayús fija pasos de 45 grados y Supr quita el ancla elegida.\n- Un clic dentro de una forma la elige; las demás se mueven y redimensionan como con Seleccionar."),
    PT("Seleção direta: arraste as âncoras (quadrados) e alças (círculos) de uma forma da caneta.\n- A alça de uma âncora suave gira a outra junto;\n- Alt+arrastar a move sozinha, Shift fixa passos de 45 graus e Delete remove a âncora escolhida.\n- Um clique dentro de uma forma a escolhe; as outras se movem e redimensionam como em Selecionar."),
    IT("Selezione diretta: trascini gli ancoraggi (quadrati) e le maniglie (cerchi) di una forma a penna.\n- La maniglia di un ancoraggio morbido fa ruotare l'altra;\n- Alt+trascina la sposta da sola, Maiusc blocca a passi di 45 gradi, Canc toglie l'ancoraggio scelto.\n- Un clic dentro una forma la sceglie; le altre si spostano e ridimensionano come con Seleziona."),
    NL("Directe selectie: sleep de ankers (vierkantjes) en grepen (rondjes) van een penvorm.\n- De greep van een glad anker draait de andere mee;\n- Alt+slepen beweegt hem alleen, Shift houdt stappen van 45 graden aan, Delete verwijdert het gekozen anker.\n- Klik in een vorm om hem te kiezen; andere vormen verschuiven en schalen zoals bij Selecteren."),
    RU("Прямое выделение: перетаскивайте опорные точки (квадраты) и ручки (кружки) фигуры, нарисованной пером.\n- Ручка гладкой точки поворачивает и вторую;\n- Alt+перетаскивание двигает её одну, Shift держит шаг 45 градусов, Delete удаляет выбранную точку.\n- Щелчок внутри фигуры выбирает её; остальные фигуры двигаются и меняют размер, как в «Выделении»."),
    TR("Doğrudan seçim: bir kalem şeklinin çapalarını (kareler) ve tutamaçlarını (daireler) sürükleyin.\n- Düzgün bir çapanın tutamacı diğerini de döndürür;\n- Alt+sürükleme onu tek başına taşır, Shift 45 derecelik adımlar tutar, Delete seçilen çapayı siler.\n- Bir şeklin içine tıklamak onu seçer; diğer şekiller Seç aracındaki gibi taşınır ve boyutlanır."));

SS_MSG(stencil_tool_pen_help,
    EN("Click for a corner or drag for a curve.\n- Shift snaps to 45°. Alt moves one handle. Space moves the anchor. Ctrl+drag moves an existing point.\n- Close with the first anchor, Enter or right-click. Backspace or Ctrl+Z undoes an anchor. Esc cancels.\n- On a selected shape, click the outline to add an anchor or click an anchor to delete it.\n- Alt+click makes an anchor a corner. Alt+drag pulls out new handles."),
    JA("ベクター編集ソフトと同じく、クリックで角、ドラッグで曲線の点を置きます。 Shift で 45 度刻み、ドラッグ中の Alt で片方のハンドルだけ、ドラッグ中の Space で点そのものを動かし、Ctrl+ドラッグで置いた点を動かせます。\n- 最初の点をクリックするか Enter か右クリックで閉じます。 Backspace か Ctrl+Z で点を戻し、Esc で中止します。\n- ペン図形を選んでいるときは、輪郭のクリックで点を追加、点のクリックで削除、点の Alt+クリックで角に、Alt+ドラッグで新しいハンドルを引き出します。"),
    ZH_HANS("与矢量绘图软件一样：点击放下角点，拖动放下曲线点。 Shift 以 45 度为步长；拖动时按 Alt 只移动一侧手柄；拖动时按 Space 移动锚点本身； Ctrl+拖动移动已放下的点。\n- 点击第一个锚点、按 Enter 或右键闭合； Backspace 或 Ctrl+Z 撤回一个锚点，Esc 取消。\n- 选中钢笔形状时，点击轮廓添加锚点，点击锚点将其删除，Alt+点击锚点将其变为角点，Alt+拖动则拉出新手柄。"),
    ZH_HANT("與向量繪圖軟體一樣：點擊放下角點，拖曳放下曲線點。 Shift 以 45 度為步長；拖曳時按 Alt 只移動一側控制把手；拖曳時按 Space 移動錨點本身； Ctrl+拖曳移動已放下的點。\n- 點擊第一個錨點、按 Enter 或右鍵閉合； Backspace 或 Ctrl+Z 收回一個錨點，Esc 取消。\n- 選取鋼筆形狀時，點擊輪廓新增錨點，點擊錨點將其刪除，Alt+點擊錨點將其變為角點，Alt+拖曳則拉出新的控制把手。"),
    KO("벡터 편집기처럼 클릭하면 모서리, 드래그하면 곡선 앵커를 놓습니다. Shift는 45도 단위로 맞추고, 드래그 중 Alt는 핸들 한쪽만, 드래그 중 Space는 앵커 자체를 옮기며, Ctrl+드래그는 이미 놓은 점을 옮깁니다. 첫 앵커 클릭, Enter 또는 오른쪽 클릭으로 닫고, Backspace나 Ctrl+Z는 앵커를 되돌리며 Esc는 취소합니다.\n펜 도형을 고른 상태에서는 윤곽을 클릭해 앵커를 더하고, 앵커를 클릭해 지우고, 앵커를 Alt+클릭해 모서리로 만들거나 Alt+드래그해 새 핸들을 뽑습니다."),
    DE("Klick setzt eine Ecke, Ziehen eine Kurve, wie in einem Vektorprogramm. Umschalt rastet in 45-Grad-Schritten ein;\n- Alt beim Ziehen bewegt nur einen Griff, Leertaste beim Ziehen den Anker selbst;\n- Strg+Ziehen verschiebt einen schon gesetzten Punkt. Erster Anker, Eingabe oder Rechtsklick schließt den Pfad;\n- Rücktaste oder Strg+Z nimmt einen Anker zurück, Esc bricht ab. Ist eine Zeichenstift-Form gewählt, fügt ein Klick auf ihre Kontur einen Anker hinzu, ein Klick auf einen Anker löscht ihn, Alt+Klick macht ihn zur Ecke und Alt+Ziehen zieht neue Griffe heraus."),
    FR("Un clic pose un angle, un glissé une courbe, comme dans un logiciel vectoriel. Maj force des pas de 45 degrés ; Alt pendant le glissé déplace une seule poignée, Espace l'ancre elle-même ; Ctrl+glisser déplace un point déjà posé.\n- La première ancre, Entrée ou un clic droit ferme le tracé ; Retour arrière ou Ctrl+Z retire une ancre, Échap annule.\n- Une forme à la plume choisie, un clic sur son contour ajoute une ancre, un clic sur une ancre la supprime, Alt+clic en fait un angle et Alt+glisser en tire de nouvelles poignées."),
    ES("Un clic pone una esquina y un arrastre una curva, como en un editor vectorial. Mayús fija pasos de 45 grados;\n- Alt al arrastrar mueve un solo tirador y Espacio el ancla; Ctrl+arrastrar mueve un punto ya puesto.\n- La primera ancla, Intro o clic derecho cierra el trazado; Retroceso o Ctrl+Z quita un ancla, Esc cancela.\n- Con una forma de pluma elegida, un clic en su contorno añade un ancla, un clic en un ancla la borra, Alt+clic la vuelve esquina y Alt+arrastrar saca tiradores nuevos."),
    PT("Um clique põe um canto e um arraste uma curva, como num editor vetorial. Shift fixa passos de 45 graus;\n- Alt ao arrastar move uma só alça e Espaço a própria âncora; Ctrl+arrastar move um ponto já posto.\n- A primeira âncora, Enter ou clique direito fecha o traçado; Backspace ou Ctrl+Z tira uma âncora, Esc cancela.\n- Com uma forma da caneta escolhida, um clique no contorno acrescenta uma âncora, um clique numa âncora a apaga, Alt+clique a torna canto e Alt+arrastar puxa alças novas."),
    IT("Un clic mette un angolo, un trascinamento una curva, come in un editor vettoriale. Maiusc blocca a passi di 45 gradi;\n- Alt durante il trascinamento muove una sola maniglia, Spazio l'ancoraggio stesso;\n- Ctrl+trascina sposta un punto già messo. Il primo ancoraggio, Invio o il clic destro chiude il tracciato;\n- Backspace o Ctrl+Z toglie un ancoraggio, Esc annulla. Con una forma a penna scelta, un clic sul contorno aggiunge un ancoraggio, un clic su un ancoraggio lo elimina, Alt+clic lo rende un angolo e Alt+trascina estrae nuove maniglie."),
    NL("Klik zet een hoek, slepen een kromme, zoals in een vectorprogramma. Shift houdt stappen van 45 graden aan;\n- Alt tijdens het slepen beweegt één greep, Spatie het anker zelf; Ctrl+slepen verplaatst een al gezet punt.\n- Het eerste anker, Enter of rechtsklik sluit het pad; Backspace of Ctrl+Z neemt een anker terug, Esc breekt af.\n- Met een penvorm gekozen voegt een klik op de omtrek een anker toe, verwijdert een klik op een anker het, maakt Alt+klik er een hoek van en trekt Alt+slepen nieuwe grepen uit."),
    RU("Щелчок ставит угол, перетаскивание — кривую, как в векторном редакторе. Shift держит шаг 45 градусов;\n- Alt при перетаскивании двигает одну ручку, пробел — саму точку; Ctrl+перетаскивание двигает уже поставленную точку.\n- Первая точка, Enter или правая кнопка замыкают контур; Backspace или Ctrl+Z убирает точку, Esc отменяет.\n- Когда выбрана фигура пером, щелчок по контуру добавляет точку, щелчок по точке удаляет её, Alt+щелчок делает её угловой, а Alt+перетаскивание вытягивает новые ручки."),
    TR("Bir vektör düzenleyicideki gibi tıklama bir köşe, sürükleme bir eğri bırakır. Shift 45 derecelik adımlar tutar;\n- sürüklerken Alt tek bir tutamacı, Boşluk çapanın kendisini taşır; Ctrl+sürükleme önceden konmuş bir noktayı taşır.\n- İlk çapa, Enter veya sağ tık yolu kapatır; Backspace veya Ctrl+Z bir çapayı geri alır, Esc iptal eder.\n- Bir kalem şekli seçiliyken dış hattına tıklamak çapa ekler, bir çapaya tıklamak onu siler, Alt+tıklama onu köşe yapar, Alt+sürükleme yeni tutamaçlar çeker."));

SS_MSG(stencil_per_camera,
    EN("Separate areas for each camera"),
    JA("カメラごとに別の範囲"),
    ZH_HANS("每台相机单独设置区域"),
    ZH_HANT("每台相機分別設定區域"),
    KO("카메라마다 따로 영역 지정"),
    DE("Eigene Bereiche je Kamera"),
    FR("Zones distinctes par caméra"),
    ES("Zonas distintas para cada cámara"),
    PT("Áreas separadas para cada câmera"),
    IT("Aree separate per ogni fotocamera"),
    NL("Aparte gebieden per camera"),
    RU("Отдельные области для каждой камеры"),
    TR("Her kamera için ayrı alanlar"));

SS_MSG(stencil_per_camera_help,
    EN("Give each output camera its own border and shapes. Cameras start with the shared settings.\n- Use the camera picker or frame slider to choose which camera to edit.\n- Turn off to share the displayed camera’s settings across all cameras."),
    JA("この入力は複数のカメラを書き出します（デュアル魚眼ファイルの 2 つのレンズ、360 度動画の各ビュー、写真のサブフォルダーなど）。\n- チェックすると、カメラごとに境界の設定と図形を持てます。\n- どのカメラも共通の設定から始まり、カメラの選択（またはフレームのスライダー）でツールが編集するカメラを選びます。\n- 外すと、表示中のカメラの設定を残して 1 組に戻ります。"),
    ZH_HANS("这个输入会写出多台相机：双鱼眼文件的两个镜头、360 视频的各个视图，或照片的子文件夹。勾选后每台相机都有自己的边界设置和形状。\n每台相机都从共用的设置开始，相机选择（或帧滑块）决定工具编辑哪一台。取消勾选则回到一组设置，保留当前显示的相机的。"),
    ZH_HANT("這個輸入會寫出多台相機：雙魚眼檔案的兩個鏡頭、360 影片的各個視角，或照片的子資料夾。\n- 勾選後每台相機都有自己的邊界設定和形狀。每台相機都從共用的設定開始，相機選擇（或影格滑桿）決定工具編輯哪一台。\n- 取消勾選則回到一組設定，保留目前顯示的相機的。"),
    KO("이 입력은 여러 카메라를 씁니다. 듀얼 어안 파일의 두 렌즈, 360 영상의 각 뷰, 사진의 하위 폴더 같은 경우입니다. 체크하면 카메라마다 경계 설정과 도형을 따로 둡니다.\n- 모든 카메라는 공통 설정에서 시작하고, 카메라 선택(또는 프레임 슬라이더)으로 도구가 편집할 카메라를 고릅니다.\n- 체크를 풀면 보이는 카메라의 설정을 남기고 한 벌로 돌아갑니다."),
    DE("Diese Eingabe schreibt mehrere Kameras: die zwei Objektive einer Dual-Fisheye-Datei, die Ansichten eines 360-Videos oder Unterordner mit Fotos.\n- Angehakt bekommt jede ihre eigene Randeinstellung und eigene Formen. Jede Kamera beginnt mit den gemeinsamen, und die Kameraauswahl (oder der Bildregler) bestimmt, welche die Werkzeuge bearbeiten.\n- Abgewählt gilt wieder ein Satz, der der gezeigten Kamera."),
    FR("Cette entrée écrit plusieurs caméras : les deux objectifs d'un fichier double fisheye, les vues d'une vidéo 360 ou des sous-dossiers de photos.\nCochez pour donner à chacune son propre réglage de bord et ses formes. Chaque caméra part des réglages communs, et le choix de caméra (ou le curseur d'image) désigne celle que les outils modifient. Décochez pour revenir à un seul jeu, celui de la caméra affichée."),
    ES("Esta entrada escribe varias cámaras: los dos objetivos de un archivo de doble ojo de pez, las vistas de un video 360 o subcarpetas de fotos.\nMárquelo para dar a cada una su propio ajuste de borde y sus formas. Cada cámara parte de las comunes, y el selector de cámara (o el deslizador de fotogramas) elige cuál editan las herramientas. Desmárquelo para volver a un solo juego, el de la cámara mostrada."),
    PT("Esta entrada grava várias câmeras: as duas lentes de um arquivo olho de peixe duplo, as vistas de um vídeo 360 ou subpastas de fotos. Marque para dar a cada uma sua própria configuração de borda e suas formas.\n- Cada câmera parte das comuns, e o seletor de câmera (ou o controle de quadros) escolhe qual as ferramentas editam.\n- Desmarque para voltar a um só conjunto, o da câmera mostrada."),
    IT("Questo ingresso scrive più fotocamere: i due obiettivi di un file doppio fisheye, le viste di un video 360 o sottocartelle di foto. Selezionandolo, ognuna ha la propria impostazione del bordo e le proprie forme.\nOgni fotocamera parte da quelle comuni, e la scelta della fotocamera (o il cursore dei fotogrammi) indica quale modificano gli strumenti. Deselezionandolo si torna a un solo insieme, quello della fotocamera mostrata."),
    NL("Deze invoer schrijft meerdere camera's: de twee lenzen van een dubbel-fisheyebestand, de aanzichten van een 360-video of submappen met foto's.\n- Aangevinkt krijgt elke camera een eigen randinstelling en eigen vormen.\n- Elke camera begint met de gedeelde, en de camerakeuze (of de beeldschuif) bepaalt welke de gereedschappen bewerken.\n- Uitgevinkt geldt weer één set: die van de getoonde camera."),
    RU("Этот вход даёт несколько камер: два объектива файла с двойным «рыбьим глазом», виды 360-градусного видео или подпапки с фотографиями. Отметьте, чтобы у каждой были свои настройка границы и фигуры.\n- Каждая камера начинает с общих, а выбор камеры (или ползунок кадров) определяет, какую правят инструменты.\n- Снимите отметку, чтобы вернуться к одному набору — набору показанной камеры."),
    TR("Bu girdi birden çok kamera yazar: çift balıkgözü bir dosyanın iki merceği, 360 videonun görünümleri ya da fotoğraf alt klasörleri.\n- İşaretlerseniz her birinin kendi sınır ayarı ve şekilleri olur. Her kamera ortak olanlardan başlar;\n- kamera seçimi (ya da kare kaydırıcısı) araçların hangisini düzenleyeceğini seçer. İşareti kaldırınca, gösterilen kameranınki kalarak tek bir takıma dönülür."));

SS_MSG(stencil_per_camera_editing,
    EN("Editing camera: {0}"),
    JA("編集中のカメラ: {0}"),
    ZH_HANS("正在编辑的相机：{0}"),
    ZH_HANT("正在編輯的相機：{0}"),
    KO("편집 중인 카메라: {0}"),
    DE("Bearbeitete Kamera: {0}"),
    FR("Caméra modifiée : {0}"),
    ES("Cámara en edición: {0}"),
    PT("Câmera em edição: {0}"),
    IT("Fotocamera in modifica: {0}"),
    NL("Bewerkte camera: {0}"),
    RU("Редактируемая камера: {0}"),
    TR("Düzenlenen kamera: {0}"));

SS_MSG(stencil_drag_hint_pen,
    EN("Drag it on the picture to move it; Points (A) edits its anchors and handles."),
    JA("画像上でドラッグすると移動します。点とハンドルは「ポイント」（A）で編集します。"),
    ZH_HANS("在图片上拖动即可移动；锚点和手柄用“锚点”（A）编辑。"),
    ZH_HANT("在圖片上拖曳即可移動；錨點和控制把手用「錨點」（A）編輯。"),
    KO("사진 위에서 드래그하면 옮겨집니다. 앵커와 핸들은 포인트(A)로 편집합니다."),
    DE("Auf dem Bild ziehen, um sie zu verschieben; Punkte (A) bearbeitet Anker und Griffe."),
    FR("Faites-la glisser sur l'image pour la déplacer ; Points (A) modifie ses ancres et "
       "poignées."),
    ES("Arrástrela sobre la imagen para moverla; Puntos (A) edita sus anclas y tiradores."),
    PT("Arraste-a sobre a imagem para movê-la; Pontos (A) edita suas âncoras e alças."),
    IT("La trascini sull'immagine per spostarla; Punti (A) ne modifica ancoraggi e maniglie."),
    NL("Sleep hem op de afbeelding om hem te verplaatsen; Punten (A) bewerkt ankers en grepen."),
    RU("Перетащите её на изображении, чтобы сдвинуть; «Точки» (A) правят опорные точки и "
       "ручки."),
    TR("Taşımak için resim üzerinde sürükleyin; çapaları ve tutamaçları Noktalar (A) "
       "düzenler."));

SS_MSG(stencil_points_hint,
    EN("Drag its squares (anchors) and circles (handles); Alt+drag a handle to move it "
       "alone. Delete removes the picked anchor."),
    JA("四角（点）と丸（ハンドル）をドラッグします。Alt+ドラッグでハンドルを片方だけ動かし、"
       "Delete で選んだ点を削除します。"),
    ZH_HANS("拖动方块（锚点）和圆点（手柄）；Alt+拖动手柄只移动这一侧。Delete 删除选中的"
            "锚点。"),
    ZH_HANT("拖曳方塊（錨點）和圓點（控制把手）；Alt+拖曳把手只移動這一側。Delete 刪除選取"
            "的錨點。"),
    KO("네모(앵커)와 동그라미(핸들)를 드래그하세요. 핸들을 Alt+드래그하면 그쪽만 옮겨지고, "
       "Delete는 고른 앵커를 지웁니다."),
    DE("Quadrate (Anker) und Kreise (Griffe) ziehen; Alt+Ziehen bewegt einen Griff allein. "
       "Entf löscht den gewählten Anker."),
    FR("Faites glisser ses carrés (ancres) et cercles (poignées) ; Alt+glisser déplace une "
       "poignée seule. Suppr retire l'ancre choisie."),
    ES("Arrastre sus cuadrados (anclas) y círculos (tiradores); Alt+arrastrar mueve un "
       "tirador solo. Supr quita el ancla elegida."),
    PT("Arraste seus quadrados (âncoras) e círculos (alças); Alt+arrastar move uma alça "
       "sozinha. Delete remove a âncora escolhida."),
    IT("Trascini i quadrati (ancoraggi) e i cerchi (maniglie); Alt+trascina sposta una "
       "maniglia da sola. Canc toglie l'ancoraggio scelto."),
    NL("Sleep de vierkantjes (ankers) en rondjes (grepen); Alt+slepen beweegt één greep "
       "alleen. Delete verwijdert het gekozen anker."),
    RU("Перетаскивайте квадраты (опорные точки) и кружки (ручки); Alt+перетаскивание "
       "двигает одну ручку. Delete удаляет выбранную точку."),
    TR("Karelerini (çapalar) ve dairelerini (tutamaçlar) sürükleyin; Alt+sürükleme bir "
       "tutamacı tek başına taşır. Delete seçilen çapayı siler."));

SS_MSG(mask_border_enable,
    EN("Remove fixed areas of the frame"),
    JA("画面の決まった位置を取り除く"),
    ZH_HANS("去掉画面中固定的区域"),
    ZH_HANT("去掉畫面中固定的區域"),
    KO("화면에서 늘 같은 자리를 없애기"),
    DE("Feste Bereiche des Bildes entfernen"),
    FR("Retirer les zones fixes de l'image"),
    ES("Quitar las zonas fijas del fotograma"),
    PT("Tirar as áreas fixas do quadro"),
    IT("Togliere le zone fisse del fotogramma"),
    NL("Vaste gebieden van het beeld weghalen"),
    RU("Убрать постоянные участки кадра"),
    TR("Karenin sabit alanlarını çıkar"));

SS_MSG(mask_border_enable_help,
    EN("Excludes fixed areas such as fisheye borders, watermarks or the camera pole. Set up in “Try the mask”; no model download needed."),
    JA("魚眼の縁、透かし、カメラの棒など固定領域を除外します。「マスクを試す」で設定します。モデル取得は不要です。"),
    ZH_HANS("排除鱼眼边框、水印或相机杆等固定区域。在“试用蒙版”中设置，无需下载模型。"),
    ZH_HANT("排除魚眼邊框、浮水印或相機桿等固定區域。在「試用遮罩」中設定，無需下載模型。"),
    KO("어안 테두리·워터마크·카메라 봉 같은 고정 영역을 제외합니다. “마스크 시험”에서 설정하며 모델 다운로드는 필요 없습니다."),
    DE("Schließt feste Bereiche wie Fischaugenränder, Wasserzeichen oder Kamerastangen aus. In „Maske testen“ einrichten; kein Modelldownload nötig."),
    FR("Exclut les zones fixes : bord fisheye, filigrane, perche. Régler dans « Essayer le masque » ; aucun modèle à télécharger."),
    ES("Excluye zonas fijas: bordes de ojo de pez, marcas de agua o palo. Configura en «Probar máscara»; sin descarga de modelo."),
    PT("Exclui zonas fixas: bordas de olho de peixe, marcas de água ou vara. Configure em «Experimentar máscara»; sem descarregar modelo."),
    IT("Esclude aree fisse: bordi fisheye, filigrane o asta. Configurare in «Prova maschera»; nessun modello da scaricare."),
    NL("Sluit vaste delen uit, zoals fisheyeranden, watermerken of de camerastok. Instellen in ‘Masker proberen’; geen modeldownload nodig."),
    RU("Исключает фиксированные области: край рыбьего глаза, водяной знак, шест камеры. Настройка в «Проба маски»; модель не нужна."),
    TR("Balıkgözü kenarı, filigran veya kamera çubuğu gibi sabit alanları dışlar. “Maskeyi dene” içinde ayarlayın; model indirmek gerekmez."));

SS_MSG(mask_for_features,
    EN("Hide masked areas from the reconstruction too"),
    JA("マスクした部分を再構成からも隠す"),
    ZH_HANS("重建时也避开被蒙住的区域"),
    ZH_HANT("重建時也避開被遮住的區域"),
    KO("가린 부분을 재구성에서도 빼기"),
    DE("Maskierte Bereiche auch vor der Rekonstruktion verbergen"),
    FR("Cacher aussi les zones masquées à la reconstruction"),
    ES("Ocultar también a la reconstrucción las zonas enmascaradas"),
    PT("Esconder as áreas mascaradas também da reconstrução"),
    IT("Nascondere le zone mascherate anche alla ricostruzione"),
    NL("Gemaskeerde gebieden ook voor de reconstructie verbergen"),
    RU("Скрывать закрытые маской участки и от реконструкции"),
    TR("Maskelenen alanları yeniden kurmadan da gizle"));

SS_MSG(mask_for_features_help,
    EN("Excludes masked areas from camera reconstruction. Off lets reconstruction use the whole frame; training still receives the masks."),
    JA("マスク領域をカメラの再構成から除外します。オフでは画面全体を再構成に使いますが、学習にはマスクを渡します。"),
    ZH_HANS("相机重建时排除蒙版区域。关闭后重建使用整幅图像；训练仍会使用蒙版。"),
    ZH_HANT("相機重建時排除遮罩區域。關閉後重建使用整幅影像；訓練仍會使用遮罩。"),
    KO("카메라 재구성에서 마스크 영역을 제외합니다. 끄면 전체 프레임으로 재구성하지만 학습에는 마스크를 계속 전달합니다."),
    DE("Schließt maskierte Bereiche von der Kamerarekonstruktion aus. Aus nutzt das ganze Bild; das Training erhält weiterhin die Masken."),
    FR("Exclut les zones masquées de la reconstruction des caméras. Désactivé, utilise toute l’image ; l’entraînement reçoit toujours les masques."),
    ES("Excluye las zonas enmascaradas de la reconstrucción de cámaras. Desactivado usa toda la imagen; el entrenamiento sigue recibiendo las máscaras."),
    PT("Exclui as áreas mascaradas da reconstrução das câmaras. Desligado usa a imagem inteira; o treino continua a receber as máscaras."),
    IT("Esclude le aree mascherate dalla ricostruzione delle camere. Disattivato usa tutto il fotogramma; il training riceve comunque le maschere."),
    NL("Sluit gemaskeerde delen uit van camerareconstructie. Uit gebruikt het hele beeld; de training krijgt nog steeds de maskers."),
    RU("Исключает маскированные области из реконструкции камер. Выкл. использует весь кадр; обучение по-прежнему получает маски."),
    TR("Maskeli alanları kamera yeniden oluşturmasından çıkarır. Kapalıyken tüm kare kullanılır; eğitim yine maskeleri alır."));

// {0} is the object number under the cursor.
SS_MSG(click_tooltip,
    EN("Left-click: this is object {0}.  Right-click: not this."),
    JA("左クリック: これが物体 {0} です。  右クリック: これは違います。"),
    ZH_HANS("左键：这是物体 {0}。  右键：不是这个。"),
    ZH_HANT("左鍵：這是物體 {0}。  右鍵：不是這個。"),
    KO("왼쪽 클릭: 이것이 물체 {0}입니다.  오른쪽 클릭: 이건 아닙니다."),
    DE("Linksklick: das ist Objekt {0}.  Rechtsklick: das nicht."),
    FR("Clic gauche : c'est l'objet {0}.  Clic droit : pas ça."),
    ES("Clic izquierdo: esto es el objeto {0}.  Clic derecho: esto no."),
    PT("Clique esquerdo: isto é o objeto {0}.  Clique direito: isto não."),
    IT("Clic sinistro: questo è l'oggetto {0}.  Clic destro: questo no."),
    NL("Linkerklik: dit is object {0}.  Rechterklik: dit niet."),
    RU("Левый щелчок: это объект {0}.  Правый щелчок: это не он."),
    TR("Sol tık: bu, nesne {0}.  Sağ tık: bu değil."));

// Progress inside the preview panel, in the order they appear.

SS_MSG(preview_working,
    EN("working..."),        JA("処理中..."),      ZH_HANS("正在处理……"),
    ZH_HANT("正在處理……"),    KO("작업 중..."),     DE("wird bearbeitet..."),
    FR("en cours..."),       ES("trabajando..."), PT("trabalhando..."),
    IT("in corso..."),       NL("bezig..."),      RU("идёт работа..."),
    TR("çalışıyor..."));

SS_MSG(preview_loading_frame,
    EN("loading the frame..."),
    JA("フレームを読み込んでいます..."),
    ZH_HANS("正在读取这一帧……"),
    ZH_HANT("正在讀取這一格……"),
    KO("프레임을 읽는 중..."),
    DE("Einzelbild wird geladen..."),
    FR("chargement de l'image..."),
    ES("cargando el fotograma..."),
    PT("carregando o quadro..."),
    IT("caricamento del fotogramma..."),
    NL("beeld wordt geladen..."),
    RU("загрузка кадра..."),
    TR("kare yükleniyor..."));

SS_MSG(preview_loading_model,
    EN("loading the model (the first run is slow)..."),
    JA("モデルを読み込んでいます（初回は時間がかかります）..."),
    ZH_HANS("正在载入模型（第一次比较慢）……"),
    ZH_HANT("正在載入模型（第一次比較慢）……"),
    KO("모델을 읽는 중(처음은 느립니다)..."),
    DE("Modell wird geladen (der erste Lauf dauert)..."),
    FR("chargement du modèle (le premier passage est lent)..."),
    ES("cargando el modelo (la primera vez es lenta)..."),
    PT("carregando o modelo (a primeira vez é lenta)..."),
    IT("caricamento del modello (la prima volta è lenta)..."),
    NL("model wordt geladen (de eerste keer duurt het)..."),
    RU("загрузка модели (первый раз медленно)..."),
    TR("model yükleniyor (ilk çalıştırma yavaştır)..."));

SS_MSG(preview_preparing,
    EN("preparing..."),      JA("準備しています..."), ZH_HANS("正在准备……"),
    ZH_HANT("正在準備……"),    KO("준비 중..."),       DE("wird vorbereitet..."),
    FR("préparation..."),    ES("preparando..."),   PT("preparando..."),
    IT("preparazione..."),   NL("voorbereiden..."), RU("подготовка..."),
    TR("hazırlanıyor..."));

SS_MSG(preview_segmenting,
    EN("segmenting..."),
    JA("領域を切り分けています..."),
    ZH_HANS("正在分割……"),
    ZH_HANT("正在分割……"),
    KO("나누는 중..."),
    DE("wird segmentiert..."),
    FR("segmentation..."),
    ES("segmentando..."),
    PT("segmentando..."),
    IT("segmentazione..."),
    NL("segmenteren..."),
    RU("сегментация..."),
    TR("bölütleniyor..."));

SS_MSG(preview_say_or_click,
    EN("Say what to look for above, or click the object in the picture."),
    JA("上に探すものを書くか、写真の中の物体をクリックしてください。"),
    ZH_HANS("在上面写出要找的东西，或者在图上点一下那个物体。"),
    ZH_HANT("在上面寫出要找的東西，或者在圖上點一下那個物體。"),
    KO("위에 찾을 것을 적거나, 사진 속 물체를 누르세요."),
    DE("Oben eintragen, wonach gesucht werden soll, oder das Objekt im Bild "
       "anklicken."),
    FR("Indiquez ci-dessus ce qu'il faut chercher, ou cliquez sur l'objet dans "
       "l'image."),
    ES("Escriba arriba qué buscar, o pulse el objeto en la imagen."),
    PT("Escreva acima o que procurar, ou clique no objeto na imagem."),
    IT("Scriva sopra che cosa cercare, oppure clicchi l'oggetto nell'immagine."),
    NL("Zet hierboven wat er gezocht moet worden, of klik het object in het "
       "beeld aan."),
    RU("Впишите выше, что искать, или щёлкните объект на снимке."),
    TR("Yukarıya ne aranacağını yazın ya da resimdeki nesneye tıklayın."));

SS_MSG(preview_frame,
    EN("frame {0}"),     JA("フレーム {0}"),  ZH_HANS("第 {0} 帧"), ZH_HANT("第 {0} 影格"),
    KO("{0}번 프레임"),   DE("Bild {0}"),     FR("image {0}"),    ES("fotograma {0}"),
    PT("quadro {0}"),    IT("fotogramma {0}"), NL("beeld {0}"),   RU("кадр {0}"),
    TR("kare {0}"));

SS_MSG(preview_frame_help,
    EN("Check prompts on several sample frames. In video, click to correct an object: the correction applies from that frame onward."),
    JA("複数のサンプルフレームで指示を確認してください。動画ではクリックで対象を修正でき、そのフレーム以降に適用されます。"),
    ZH_HANS("在多个示例帧上检查提示。视频中可点击纠正对象，修正从该帧起生效。"),
    ZH_HANT("在多個範例影格上檢查提示。影片中可點擊修正對象，修正從該影格起生效。"),
    KO("여러 샘플 프레임에서 프롬프트를 확인하세요. 영상에서 클릭으로 객체를 수정하면 해당 프레임부터 적용됩니다."),
    DE("Prompts an mehreren Beispielbildern prüfen. Im Video ein Objekt per Klick korrigieren; die Korrektur gilt ab diesem Bild."),
    FR("Vérifiez les consignes sur plusieurs images. En vidéo, cliquez pour corriger un objet ; la correction s’applique dès cette image."),
    ES("Comprueba los prompts en varios fotogramas. En vídeo, haz clic para corregir un objeto: se aplica desde ese fotograma."),
    PT("Verifique os pedidos em vários fotogramas. Em vídeo, clique para corrigir um objeto: aplica-se desse fotograma em diante."),
    IT("Verificare i prompt su più fotogrammi. Nei video, cliccare per correggere un oggetto: vale da quel fotogramma in poi."),
    NL("Controleer prompts op meerdere voorbeeldbeelden. Klik in video om een object te corrigeren; dit geldt vanaf dat beeld."),
    RU("Проверьте подсказки на нескольких кадрах. В видео кликните для исправления объекта: оно действует с этого кадра."),
    TR("İstemleri birkaç örnek karede denetleyin. Videoda nesneyi düzeltmek için tıklayın; düzeltme o kareden itibaren uygulanır."));

SS_MSG(preview_camera,
    EN("Which camera"),  JA("どのカメラ"),    ZH_HANS("哪台相机"),  ZH_HANT("哪台相機"),
    KO("어느 카메라"),     DE("Welche Kamera"), FR("Quelle caméra"), ES("Qué cámara"),
    PT("Qual câmera"),   IT("Quale camera"),  NL("Welke camera"),  RU("Какая камера"),
    TR("Hangi kamera"));

SS_MSG(preview_camera_help,
    EN("Each lens or unwrapped 360° view has separate images. A prompt drawn on one does not apply to the others."),
    JA("各レンズや360度から展開した視点は別々の画像です。1つに描いた指示は他には適用されません。"),
    ZH_HANS("每个镜头或展开的 360° 视角有独立图像。某个视角上绘制的提示不适用于其他视角。"),
    ZH_HANT("每個鏡頭或展開的 360° 視角有獨立影像。某個視角上繪製的提示不適用於其他視角。"),
    KO("각 렌즈나 펼친 360° 뷰는 별도 이미지입니다. 한 뷰에 그린 프롬프트는 다른 뷰에 적용되지 않습니다."),
    DE("Jedes Objektiv oder entpackte 360°-Sichtfeld hat eigene Bilder. Ein dort gezeichneter Prompt gilt nicht für die anderen."),
    FR("Chaque objectif ou vue 360° dépliée a ses propres images. Une consigne dessinée sur l’une ne vaut pas pour les autres."),
    ES("Cada lente o vista 360° desplegada tiene imágenes propias. Un prompt dibujado en una no se aplica a las demás."),
    PT("Cada lente ou vista 360° desdobrada tem imagens próprias. Um pedido desenhado numa não se aplica às outras."),
    IT("Ogni lente o vista 360° sviluppata ha immagini proprie. Un prompt disegnato su una non si applica alle altre."),
    NL("Elke lens of uitgepakte 360°-weergave heeft eigen beelden. Een getekende prompt op één geldt niet voor de andere."),
    RU("У каждого объектива или развёрнутого вида 360° свои снимки. Нарисованная на одном подсказка не применяется к остальным."),
    TR("Her mercek veya açılmış 360° görünümün resimleri ayrıdır. Birine çizilen istem diğerlerine uygulanmaz."));

SS_MSG(preview_try_it,
    EN("Try it"),        JA("試す"),          ZH_HANS("试一下"),   ZH_HANT("試一下"),
    KO("해 보기"),        DE("Ausprobieren"), FR("Essayer"),      ES("Probar"),
    PT("Testar"),        IT("Prova"),        NL("Uitproberen"),  RU("Проверить"),
    TR("Dene"));

// Shown in the preview when this machine has no in-process video decoding, so
// the frame has to come from ffmpeg, and ffmpeg is not there either.
// {0} the command that was looked for.
SS_MSG(preview_needs_ffmpeg,
    EN("This video can only be read here with ffmpeg, which was not found ('{0}').\nInstall it, or set its path under Tool locations."),
    JA("この動画をここで読むには ffmpeg が必要ですが、見つかりませんでした（{0}）。\nffmpeg を入れるか、「外部ツールの場所」でパスを指定してください。"),
    ZH_HANS("这里只能用 ffmpeg 读取该视频，但没有找到它（{0}）。\n请安装 ffmpeg，或在“工具位置”中指定它的路径。"),
    ZH_HANT("這裡只能用 ffmpeg 讀取該影片，但沒有找到它（{0}）。\n請安裝 ffmpeg，或在「工具位置」中指定它的路徑。"),
    KO("이 동영상은 여기서 ffmpeg으로만 읽을 수 있는데 ffmpeg을 찾지 못했습니다({0}). ffmpeg을 설치하거나 '도구 위치'에서 경로를 지정하세요."),
    DE("Dieses Video lässt sich hier nur mit ffmpeg lesen, und ffmpeg wurde nicht gefunden ('{0}').\nInstallieren Sie es, oder tragen Sie seinen Pfad unter \"Speicherorte der Werkzeuge\" ein."),
    FR("Cette vidéo ne peut être lue ici qu'avec ffmpeg, qui est introuvable (« {0} »).\nInstallez-le, ou indiquez son chemin sous « Emplacement des outils »."),
    ES("Aquí este vídeo solo se puede leer con ffmpeg, que no se ha encontrado («{0}»).\nInstálelo, o indique su ruta en «Ubicación de las herramientas»."),
    PT("Aqui este vídeo só pode ser lido com o ffmpeg, que não foi encontrado (\"{0}\").\nInstale-o, ou informe o caminho dele em \"Local das ferramentas\"."),
    IT("Qui questo video si può leggere solo con ffmpeg, che non è stato trovato (\"{0}\").\nLo installi, oppure indichi il suo percorso in \"Percorsi degli strumenti\"."),
    NL("Deze video kan hier alleen met ffmpeg worden gelezen, en ffmpeg is niet gevonden ('{0}').\nInstalleer het, of geef het pad op onder \"Locatie van hulpprogramma's\"."),
    RU("Здесь это видео читается только через ffmpeg, а он не найден («{0}»).\nУстановите его или укажите путь в разделе «Расположение инструментов»."),
    TR("Bu video burada yalnızca ffmpeg ile okunabilir, ffmpeg ise bulunamadı ('{0}').\nKurun ya da yolunu \"Araç konumları\" altında belirtin."));

// Neither the built-in decoder nor ffmpeg produced a picture.
SS_MSG(preview_frame_unreadable,
    EN("No frame could be read from this video."),
    JA("この動画からフレームを読み取れませんでした。"),
    ZH_HANS("无法从这个视频中读取任何一帧。"),
    ZH_HANT("無法從這個影片中讀取任何一格。"),
    KO("이 동영상에서 프레임을 읽지 못했습니다."),
    DE("Aus diesem Video ließ sich kein Bild lesen."),
    FR("Aucune image n'a pu être lue dans cette vidéo."),
    ES("No se ha podido leer ningún fotograma de este vídeo."),
    PT("Não foi possível ler nenhum quadro deste vídeo."),
    IT("Non è stato possibile leggere alcun fotogramma da questo video."),
    NL("Er kon geen beeld uit deze video worden gelezen."),
    RU("Из этого видео не удалось прочитать ни одного кадра."),
    TR("Bu videodan hiçbir kare okunamadı."));

// A click or a prompt in "Try the mask" with no model to find it; {0} is the
// mask_enable checkbox, which taking the model also ticks.
SS_MSG(preview_needs_model,
    EN("Clicks and prompts need a segmentation model. Pick one below; using it "
       "also ticks \"{0}\"."),
    JA("クリックやプロンプトにはセグメンテーションモデルが必要です。下で選んでください。"
       "使うと「{0}」にもチェックが入ります。"),
    ZH_HANS("点击和提示需要一个分割模型。请在下面选一个；使用它也会勾选“{0}”。"),
    ZH_HANT("點擊和提示需要一個分割模型。請在下面選一個；使用它也會勾選「{0}」。"),
    KO("클릭과 프롬프트에는 분할 모델이 필요합니다. 아래에서 고르세요. 사용하면 "
       "\"{0}\"도 함께 켜집니다."),
    DE("Klicks und Beschreibungen brauchen ein Segmentierungsmodell. Wählen Sie "
       "unten eines; wird es verwendet, ist auch „{0}“ angehakt."),
    FR("Les clics et les descriptions demandent un modèle de segmentation. "
       "Choisissez-en un ci-dessous ; l'utiliser coche aussi « {0} »."),
    ES("Los clics y las descripciones necesitan un modelo de segmentación. Elija "
       "uno abajo; al usarlo también se marca «{0}»."),
    PT("Cliques e descrições precisam de um modelo de segmentação. Escolha um "
       "abaixo; usá-lo também marca \"{0}\"."),
    IT("Clic e descrizioni richiedono un modello di segmentazione. Sceglietene "
       "uno qui sotto; usarlo spunta anche «{0}»."),
    NL("Klikken en beschrijvingen hebben een segmentatiemodel nodig. Kies er "
       "hieronder een; als u het gebruikt, wordt ook \"{0}\" aangevinkt."),
    RU("Для щелчков и подсказок нужна модель сегментации. Выберите её ниже; "
       "при этом также включится «{0}»."),
    TR("Tıklamalar ve istemler bir bölütleme modeli gerektirir. Aşağıdan birini "
       "seçin; kullanmak \"{0}\" seçeneğini de işaretler."));

SS_MSG(preview_use_model,
    EN("Use this model"),
    JA("このモデルを使う"),
    ZH_HANS("使用这个模型"),
    ZH_HANT("使用這個模型"),
    KO("이 모델 사용"),
    DE("Dieses Modell verwenden"),
    FR("Utiliser ce modèle"),
    ES("Usar este modelo"),
    PT("Usar este modelo"),
    IT("Usa questo modello"),
    NL("Dit model gebruiken"),
    RU("Использовать эту модель"),
    TR("Bu modeli kullan"));

SS_MSG(preview_kept_fraction,
    EN("{0}% of the frame is kept"),
    JA("フレームの {0}% が残ります"),
    ZH_HANS("这一帧保留了 {0}%"),
    ZH_HANT("這一影格保留了 {0}%"),
    KO("프레임의 {0}%가 남습니다"),
    DE("{0} % des Bildes bleiben erhalten"),
    FR("{0} % de l'image est conservé"),
    ES("se conserva el {0} % del fotograma"),
    PT("{0}% do quadro é mantido"),
    IT("resta il {0}% del fotogramma"),
    NL("{0}% van het beeld blijft over"),
    RU("остаётся {0}% кадра"),
    TR("karenin %{0}'i tutuluyor"));

SS_MSG(preview_features_only_help,
    EN("What the reconstruction takes no feature point from, though training "
       "still uses it -- the sky, say. Hatched in amber on the picture."),
    JA("再構成は特徴点を取らず、学習はそのまま使うもの（空など）です。画像上では"
       "琥珀色の斜線で表示されます。"),
    ZH_HANS("重建时不从中取特征点、训练却照样使用的东西，比如天空。画面上以琥珀色"
            "斜线显示。"),
    ZH_HANT("重建時不從中取特徵點、訓練卻照樣使用的東西，比如天空。畫面上以琥珀色"
            "斜線顯示。"),
    KO("재구성은 특징점을 뽑지 않지만 학습은 그대로 쓰는 것(하늘 등)입니다. "
       "그림에서는 호박색 빗금으로 보입니다."),
    DE("Woraus die Rekonstruktion keinen Merkmalspunkt nimmt, was das Training "
       "aber weiter nutzt, etwa der Himmel. Im Bild bernsteinfarben schraffiert."),
    FR("Ce dont la reconstruction ne tire aucun point d'intérêt mais que "
       "l'entraînement utilise quand même, comme le ciel. Hachuré d'ambre sur "
       "l'image."),
    ES("Lo que la reconstrucción no usa para ningún punto característico pero el "
       "entrenamiento sí, como el cielo. Rayado en ámbar sobre la imagen."),
    PT("O que a reconstrução não usa para nenhum ponto de característica, mas o "
       "treino usa, como o céu. Hachurado em âmbar na imagem."),
    IT("Ciò da cui la ricostruzione non prende alcun punto caratteristico ma che "
       "l'addestramento usa comunque, come il cielo. Tratteggiato in ambra "
       "sull'immagine."),
    NL("Waar de reconstructie geen kenmerkpunt uit haalt, maar wat de training "
       "wel gebruikt, zoals de lucht. Amberkleurig gearceerd op het beeld."),
    RU("То, из чего реконструкция не берёт ни одной особой точки, а обучение всё "
       "равно использует, например небо. На снимке — янтарная штриховка."),
    TR("Yeniden kurmanın hiçbir öznitelik noktası almadığı ama eğitimin yine de "
       "kullandığı şeyler, örneğin gökyüzü. Görüntüde kehribar renkli taramayla "
       "gösterilir."));

SS_MSG(preview_legend_features,
    EN("Hatched amber = trained on, but hidden from the reconstruction."),
    JA("琥珀色の斜線 = 学習には使うが、再構成からは隠す部分です。"),
    ZH_HANS("琥珀色斜线 = 训练照用，但重建时避开。"),
    ZH_HANT("琥珀色斜線 = 訓練照用，但重建時避開。"),
    KO("호박색 빗금 = 학습에는 쓰지만 재구성에서는 뺍니다."),
    DE("Bernsteinfarben schraffiert = wird trainiert, aber vor der "
       "Rekonstruktion verborgen."),
    FR("Hachuré d'ambre = entraîné, mais caché à la reconstruction."),
    ES("Rayado en ámbar = se entrena, pero se oculta a la reconstrucción."),
    PT("Hachurado em âmbar = treinado, mas escondido da reconstrução."),
    IT("Tratteggio ambra = addestrato, ma nascosto alla ricostruzione."),
    NL("Amberkleurig gearceerd = wordt getraind, maar voor de reconstructie "
       "verborgen."),
    RU("Янтарная штриховка — используется в обучении, но скрыто от "
       "реконструкции."),
    TR("Kehribar tarama = eğitimde kullanılır ama yeniden kurmadan gizlenir."));

SS_MSG(preview_features_kept_fraction,
    EN("{0}% of the frame is left for feature points"),
    JA("特徴点に使えるのはフレームの {0}% です"),
    ZH_HANS("这一帧有 {0}% 可以取特征点"),
    ZH_HANT("這一影格有 {0}% 可以取特徵點"),
    KO("특징점을 뽑을 수 있는 부분: 프레임의 {0}%"),
    DE("{0} % des Bildes bleiben für Merkmalspunkte"),
    FR("{0} % de l'image reste pour les points d'intérêt"),
    ES("queda el {0} % del fotograma para puntos característicos"),
    PT("{0}% do quadro fica para pontos de característica"),
    IT("resta il {0}% del fotogramma per i punti caratteristici"),
    NL("{0}% van het beeld blijft over voor kenmerkpunten"),
    RU("для особых точек остаётся {0}% кадра"),
    TR("karenin %{0}'i öznitelik noktalarına kalıyor"));

SS_MSG(preview_almost_nothing_kept,
    EN("Almost nothing is left -- the prompt matched very little of the "
       "frame."),
    JA("ほとんど何も残っていません。プロンプトがフレームのごく一部にしか"
       "当てはまりませんでした。"),
    ZH_HANS("几乎什么都没剩下——提示词只匹配到画面里很小的一部分。"),
    ZH_HANT("幾乎什麼都沒剩下——提示詞只匹配到畫面裡很小的一部分。"),
    KO("거의 아무것도 남지 않았습니다. 프롬프트가 프레임의 아주 일부에만 "
       "맞았습니다."),
    DE("Es bleibt fast nichts übrig -- die Eingabe traf nur sehr wenig des "
       "Bildes."),
    FR("Il ne reste presque rien : l'invite n'a couvert qu'une toute petite "
       "partie de l'image."),
    ES("Casi no queda nada: la indicación coincidió con muy poco del "
       "fotograma."),
    PT("Quase nada sobrou -- o comando correspondeu a muito pouco do quadro."),
    IT("Non resta quasi nulla: il testo ha corrisposto a pochissimo del "
       "fotogramma."),
    NL("Er blijft bijna niets over -- de prompt paste bij maar heel weinig van "
       "het beeld."),
    RU("Почти ничего не осталось — запрос совпал лишь с малой частью кадра."),
    TR("Neredeyse hiçbir şey kalmadı -- istem karenin çok azıyla eşleşti."));

SS_MSG(preview_almost_all_masked,
    EN("Almost everything is masked out -- did you mean \"Keep only what I "
       "name\"?"),
    JA("ほとんどすべてがマスクされています。「指定したものだけを残す」の"
       "つもりではありませんか？"),
    ZH_HANS("几乎所有内容都被蒙掉了——你是不是想选“只保留我指名的东西”？"),
    ZH_HANT("幾乎所有內容都被遮掉了——你是不是想選「只保留我指名的東西」？"),
    KO("거의 모든 것이 마스킹되었습니다. '내가 말한 것만 남기기'를 뜻하신 "
       "건가요?"),
    DE("Fast alles ist maskiert -- war „Nur behalten, was ich nenne“ gemeint?"),
    FR("Presque tout est masqué -- vouliez-vous dire « Ne garder que ce que je "
       "nomme » ?"),
    ES("Casi todo está enmascarado: ¿quería decir «Conservar solo lo que yo "
       "nombre»?"),
    PT("Quase tudo está mascarado -- você queria dizer “Manter só o que eu "
       "nomear”?"),
    IT("È mascherato quasi tutto: intendeva «Tenere solo ciò che indico»?"),
    NL("Bijna alles is gemaskeerd -- bedoelde je ‘Alleen houden wat ik noem’?"),
    RU("Замаскировано почти всё — вы имели в виду «Оставить только то, что я "
       "назову»?"),
    TR("Neredeyse her şey maskelendi -- “Yalnızca adını verdiğimi tut”u mu "
       "demek istediniz?"));

// ===========================================================================
// Advanced: built-in reconstruction
// ===========================================================================

SS_MSG(capture_type,
    EN("Capture type"),  JA("撮影の種類"),    ZH_HANS("拍摄类型"),  ZH_HANT("拍攝類型"),
    KO("촬영 종류"),      DE("Aufnahmeart"),  FR("Type de prise"), ES("Tipo de captura"),
    PT("Tipo de captura"), IT("Tipo di ripresa"), NL("Soort opname"),
    RU("Тип съёмки"),    TR("Çekim türü"));

SS_MSG(capture_photos,
    EN("Individual photos"),
    JA("個別の写真"),    ZH_HANS("单张照片"),  ZH_HANT("單張相片"),
    KO("낱장 사진"),     DE("Einzelfotos"),   FR("Photos individuelles"),
    ES("Fotos sueltas"), PT("Fotos avulsas"), IT("Fotografie singole"),
    NL("Losse foto's"),  RU("Отдельные фотографии"), TR("Tek tek fotoğraflar"));

SS_MSG(capture_video,
    EN("Video frames"),  JA("動画のフレーム"), ZH_HANS("视频帧"),   ZH_HANT("影片影格"),
    KO("동영상 프레임"), DE("Videobilder"),   FR("Images de vidéo"),
    ES("Fotogramas de vídeo"), PT("Quadros de vídeo"), IT("Fotogrammi video"),
    NL("Videobeelden"),  RU("Кадры видео"),  TR("Video kareleri"));

SS_MSG(capture_internet,
    EN("Unordered internet collection"),
    JA("順不同のインターネット写真集"),
    ZH_HANS("无序的网络图片集"),
    ZH_HANT("無序的網路圖片集"),
    KO("순서 없는 인터넷 사진 모음"),
    DE("Ungeordnete Internetsammlung"),
    FR("Collection internet sans ordre"),
    ES("Colección de internet sin orden"),
    PT("Coleção da internet sem ordem"),
    IT("Raccolta internet senza ordine"),
    NL("Ongeordende internetverzameling"),
    RU("Неупорядоченная подборка из интернета"),
    TR("Sırasız internet derlemesi"));

SS_MSG(capture_type_help,
    EN("What the input is, which sets the pairing strategy and how forgiving the mapper is. Video frames follows the first input's Sequential switch."),
    JA("入力が何であるかです。ペアの作り方と、マッパーがどれだけ寛容かがこれで決まります。「動画のフレーム」は先頭の入力の「撮影順」に従います。"),
    ZH_HANS("输入是什么，它决定了配对策略以及建图器有多宽容。“视频帧”跟随第一个输入的“按顺序”开关。"),
    ZH_HANT("輸入是什麼，它決定了配對策略以及建圖器有多寬容。「影片影格」跟隨第一個輸入的「按順序」開關。"),
    KO("입력이 무엇인지입니다. 짝짓기 전략과 매퍼가 얼마나 관대한지가 여기서 정해집니다. '동영상 프레임'은 첫 입력의 '순서대로' 스위치를 따릅니다."),
    DE("Was die Eingabe ist; davon hängen die Paarungsstrategie und die Nachsicht des Mappers ab. „Videobilder“ folgt dem Schalter „Sequenziell“ der ersten Eingabe."),
    FR("Ce qu'est l'entrée, ce qui fixe la stratégie d'appariement et la tolérance du mapper. « Images de vidéo » suit l'interrupteur « Séquentiel » de la première entrée."),
    ES("El tipo de entrada fija la estrategia de emparejamiento y la tolerancia del mapeador. «Fotogramas de vídeo» sigue «Secuencial» de la primera entrada."),
    PT("O que é a entrada, o que define a estratégia de pareamento e a tolerância do mapeador. “Quadros de vídeo” segue o interruptor “Sequencial” da primeira entrada."),
    IT("Il tipo di ingresso determina abbinamento e tolleranza del mapper. «Fotogrammi video» segue «Sequenziale» del primo ingresso."),
    NL("Wat de invoer is; dat bepaalt de koppelstrategie en hoe mild de mapper is. ‘Videobeelden’ volgt de schakelaar ‘Sequentieel’ van de eerste invoer."),
    RU("Чем является вход; от этого зависят стратегия составления пар и снисходительность маппера. «Кадры видео» следует переключателю «По порядку» первого входа."),
    TR("Girdinin ne olduğu; eşleştirme stratejisini ve haritalayıcının ne kadar hoşgörülü olduğunu bu belirler. “Video kareleri”, ilk girdinin “Sıralı” anahtarını izler."));

SS_MSG(features,
    EN("Features"),      JA("特徴"),          ZH_HANS("特征"),     ZH_HANT("特徵"),
    KO("특징점"),         DE("Merkmale"),     FR("Points caractéristiques"),
    ES("Características"), PT("Características"), IT("Caratteristiche"),
    NL("Kenmerken"),     RU("Особые точки"), TR("Öznitelikler"));

SS_MSG(features_sift,
    EN("SIFT (classic)"), JA("SIFT（古典的）"), ZH_HANS("SIFT（经典）"),
    ZH_HANT("SIFT（經典）"), KO("SIFT(고전적)"), DE("SIFT (klassisch)"),
    FR("SIFT (classique)"), ES("SIFT (clásico)"), PT("SIFT (clássico)"),
    IT("SIFT (classico)"), NL("SIFT (klassiek)"), RU("SIFT (классический)"),
    TR("SIFT (klasik)"));

SS_MSG(features_aliked_n16,
    EN("ALIKED N16-rot (learned)"),
    JA("ALIKED N16-rot（学習型）"),
    ZH_HANS("ALIKED N16-rot（学习型）"),
    ZH_HANT("ALIKED N16-rot（學習型）"),
    KO("ALIKED N16-rot(학습형)"),
    DE("ALIKED N16-rot (gelernt)"),
    FR("ALIKED N16-rot (appris)"),
    ES("ALIKED N16-rot (aprendido)"),
    PT("ALIKED N16-rot (aprendido)"),
    IT("ALIKED N16-rot (appreso)"),
    NL("ALIKED N16-rot (geleerd)"),
    RU("ALIKED N16-rot (обученный)"),
    TR("ALIKED N16-rot (öğrenilmiş)"));

SS_MSG(features_aliked_n32,
    EN("ALIKED N32 (learned, wider)"),
    JA("ALIKED N32（学習型・広め）"),
    ZH_HANS("ALIKED N32（学习型，更宽）"),
    ZH_HANT("ALIKED N32（學習型，更寬）"),
    KO("ALIKED N32(학습형, 더 넓음)"),
    DE("ALIKED N32 (gelernt, breiter)"),
    FR("ALIKED N32 (appris, plus large)"),
    ES("ALIKED N32 (aprendido, más amplio)"),
    PT("ALIKED N32 (aprendido, mais amplo)"),
    IT("ALIKED N32 (appreso, più ampio)"),
    NL("ALIKED N32 (geleerd, breder)"),
    RU("ALIKED N32 (обученный, шире)"),
    TR("ALIKED N32 (öğrenilmiş, daha geniş)"));

SS_MSG(features_loma_b128,
    EN("LoMa-B128 (learned, compact)"),
    JA("LoMa-B128（学習型・小さめ）"),
    ZH_HANS("LoMa-B128（学习型，紧凑）"),
    ZH_HANT("LoMa-B128（學習型，精簡）"),
    KO("LoMa-B128(학습형, 소형)"),
    DE("LoMa-B128 (gelernt, kompakt)"),
    FR("LoMa-B128 (appris, compact)"),
    ES("LoMa-B128 (aprendido, compacto)"),
    PT("LoMa-B128 (aprendido, compacto)"),
    IT("LoMa-B128 (appreso, compatto)"),
    NL("LoMa-B128 (geleerd, compact)"),
    RU("LoMa-B128 (обученный, компактный)"),
    TR("LoMa-B128 (öğrenilmiş, küçük)"));

SS_MSG(features_loma_b,
    EN("LoMa-B (learned, most accurate)"),
    JA("LoMa-B（学習型・最も高精度）"),
    ZH_HANS("LoMa-B（学习型，最准确）"),
    ZH_HANT("LoMa-B（學習型，最準確）"),
    KO("LoMa-B(학습형, 가장 정확)"),
    DE("LoMa-B (gelernt, am genauesten)"),
    FR("LoMa-B (appris, le plus précis)"),
    ES("LoMa-B (aprendido, el más preciso)"),
    PT("LoMa-B (aprendido, o mais preciso)"),
    IT("LoMa-B (appreso, il più preciso)"),
    NL("LoMa-B (geleerd, nauwkeurigst)"),
    RU("LoMa-B (обученный, самый точный)"),
    TR("LoMa-B (öğrenilmiş, en doğru)"));

SS_MSG(features_help,
    EN("SIFT needs no download. ALIKED downloads 3–4 MB and matches more pairs on difficult captures. N32 is slower and slightly stronger without large rotations."),
    JA("SIFTは取得不要です。ALIKEDは3–4 MBを取得し、難しい撮影でより多くのペアを照合します。N32は低速ですが、大きな回転がない場面でやや高精度です。"),
    ZH_HANS("SIFT 无需下载。ALIKED 下载 3–4 MB，在困难场景匹配更多图像对。N32 较慢，在无大幅旋转时稍强。"),
    ZH_HANT("SIFT 無需下載。ALIKED 下載 3–4 MB，在困難場景匹配更多影像對。N32 較慢，在無大幅旋轉時稍強。"),
    KO("SIFT는 다운로드가 없습니다. ALIKED는 3–4 MB를 받아 어려운 촬영에서 더 많은 쌍을 매칭합니다. N32는 느리지만 큰 회전이 없을 때 조금 더 좋습니다."),
    DE("SIFT braucht keinen Download. ALIKED lädt 3–4 MB und findet bei schwierigen Aufnahmen mehr Paare. N32 ist langsamer, ohne starke Drehung etwas besser."),
    FR("SIFT ne télécharge rien. ALIKED télécharge 3–4 MB et apparie plus de paires difficiles. N32 est plus lent, un peu meilleur sans fortes rotations."),
    ES("SIFT no descarga nada. ALIKED descarga 3–4 MB y empareja más vistas difíciles. N32 es más lento y algo mejor sin giros grandes."),
    PT("SIFT dispensa transferências. ALIKED descarrega 3–4 MB e combina mais pares difíceis. N32 é mais lento e um pouco melhor sem grandes rotações."),
    IT("SIFT non richiede download. ALIKED scarica 3–4 MB e abbina più coppie difficili. N32 è più lento, leggermente migliore senza grandi rotazioni."),
    NL("SIFT vereist geen download. ALIKED downloadt 3–4 MB en koppelt meer paren bij lastige opnamen. N32 is trager, iets beter zonder grote rotaties."),
    RU("SIFT не требует загрузки. ALIKED загружает 3–4 MB и сопоставляет больше сложных пар. N32 медленнее, немного лучше без сильных поворотов."),
    TR("SIFT indirme gerektirmez. ALIKED, 3–4 MB indirip zor çekimlerde daha çok çifti eşleştirir. N32 daha yavaştır; büyük dönüşler yoksa biraz daha iyidir."));

SS_MSG(matcher,
    EN("Matcher"),       JA("マッチャー"),    ZH_HANS("匹配器"),   ZH_HANT("配對器"),
    KO("매처"),           DE("Zuordner"),     FR("Appariement"),  ES("Emparejador"),
    PT("Correspondedor"), IT("Abbinatore"),  NL("Matcher"),      RU("Сопоставитель"),
    TR("Eşleştirici"));

SS_MSG(matcher_brute_force,
    EN("Brute force"),   JA("総当たり"),      ZH_HANS("暴力匹配"),  ZH_HANT("暴力比對"),
    KO("완전 탐색"),      DE("Brute Force"),  FR("Force brute"),  ES("Fuerza bruta"),
    PT("Força bruta"),   IT("Forza bruta"),  NL("Brute kracht"), RU("Полный перебор"),
    TR("Kaba kuvvet"));

SS_MSG(matcher_lightglue,
    EN("LightGlue (learned)"),
    JA("LightGlue（学習型）"), ZH_HANS("LightGlue（学习型）"),
    ZH_HANT("LightGlue（學習型）"), KO("LightGlue(학습형)"),
    DE("LightGlue (gelernt)"), FR("LightGlue (appris)"),
    ES("LightGlue (aprendido)"), PT("LightGlue (aprendido)"),
    IT("LightGlue (appreso)"), NL("LightGlue (geleerd)"),
    RU("LightGlue (обученный)"), TR("LightGlue (öğrenilmiş)"));

SS_MSG(matcher_loma,
    EN("LoMa (learned)"),
    JA("LoMa（学習型）"), ZH_HANS("LoMa（学习型）"),
    ZH_HANT("LoMa（學習型）"), KO("LoMa(학습형)"),
    DE("LoMa (gelernt)"), FR("LoMa (appris)"),
    ES("LoMa (aprendido)"), PT("LoMa (aprendido)"),
    IT("LoMa (appreso)"), NL("LoMa (geleerd)"),
    RU("LoMa (обученный)"), TR("LoMa (öğrenilmiş)"));

SS_MSG(matcher_flexglue,
    EN("FlexGlue (learned, any features)"),
    JA("FlexGlue（学習型、任意の特徴）"), ZH_HANS("FlexGlue（学习型，任意特征）"),
    ZH_HANT("FlexGlue（學習型，任意特徵）"), KO("FlexGlue(학습형, 모든 특징)"),
    DE("FlexGlue (gelernt, alle Merkmale)"), FR("FlexGlue (appris, toutes caractéristiques)"),
    ES("FlexGlue (aprendido, cualquier característica)"),
    PT("FlexGlue (aprendido, quaisquer características)"),
    IT("FlexGlue (appreso, qualsiasi caratteristica)"), NL("FlexGlue (geleerd, alle kenmerken)"),
    RU("FlexGlue (обученный, любые признаки)"), TR("FlexGlue (öğrenilmiş, her öznitelik)"));

SS_MSG(matcher_flexglue_help,
    EN("Matches any detector using image features and rejects non-overlapping pairs. Slow: use a shortlist of pairs. Downloads 570 MB on first use."),
    JA("画像特徴を使って任意の検出器の点を照合し、重なりのないペアを除外します。低速なので候補ペアを絞ってください。初回は570 MBを取得します。"),
    ZH_HANS("利用图像特征匹配任意检测器的关键点，排除无重叠图像对。速度较慢，适合少量候选对。首次使用下载 570 MB。"),
    ZH_HANT("利用影像特徵匹配任意偵測器的關鍵點，排除無重疊影像對。速度較慢，適合少量候選對。首次使用下載 570 MB。"),
    KO("이미지 특징으로 모든 검출기의 키포인트를 매칭하며 겹치지 않는 쌍을 제외합니다. 느리므로 후보 쌍을 줄이세요. 첫 사용 시 570 MB를 받습니다."),
    DE("Gleicht jeden Detektor über Bildmerkmale ab und verwirft Paare ohne Überlappung. Langsam: nur wenige Kandidaten nutzen. Erstmaliger Download: 570 MB."),
    FR("Apparie tout détecteur via les caractéristiques des images et rejette les paires sans recouvrement. Lent : limiter les paires. Premier téléchargement : 570 MB."),
    ES("Empareja cualquier detector por rasgos de imagen y descarta pares sin solape. Lento: limita los pares candidatos. Primera descarga: 570 MB."),
    PT("Combina qualquer detetor por características da imagem e rejeita pares sem sobreposição. Lento: limite os pares candidatos. Primeira transferência: 570 MB."),
    IT("Abbina qualsiasi rilevatore tramite le immagini e scarta coppie senza sovrapposizione. Lento: limitare le coppie candidate. Primo download: 570 MB."),
    NL("Koppelt elke detector via beeldkenmerken en verwerpt paren zonder overlap. Traag: beperk de kandidaatparen. Eerste download: 570 MB."),
    RU("Сопоставляет точки любого детектора по признакам изображений, отклоняя пары без перекрытия. Медленно: сократите список пар. Первая загрузка: 570 MB."),
    TR("Her dedektörü görüntü özellikleriyle eşleştirir, örtüşmeyen çiftleri eler. Yavaştır: aday çiftleri azaltın. İlk kullanımda 570 MB indirir."));

SS_MSG(matcher_flexglue_wide_lens,
    EN("FlexGlue is meant for ordinary lenses. This capture has fisheye or panorama "
       "images, which it may match poorly."),
    JA("FlexGlue は通常のレンズ向けです。この素材には魚眼またはパノラマの画像があり、"
       "うまく照合できないことがあります。"),
    ZH_HANS("FlexGlue 面向普通镜头。这份素材含有鱼眼或全景图像，它可能匹配得不好。"),
    ZH_HANT("FlexGlue 針對一般鏡頭。這份素材含有魚眼或全景影像，它可能比對得不好。"),
    KO("FlexGlue 는 일반 렌즈용입니다. 이 촬영본에는 어안 또는 파노라마 이미지가 있어 "
       "잘 맞추지 못할 수 있습니다."),
    DE("FlexGlue ist für gewöhnliche Objektive gedacht. Diese Aufnahme enthält "
       "Fischaugen- oder Panoramabilder, die es womöglich schlecht zuordnet."),
    FR("FlexGlue est conçu pour des objectifs ordinaires. Cette prise contient des "
       "images fisheye ou panoramiques, qu'il risque de mal apparier."),
    ES("FlexGlue está pensado para objetivos normales. Esta toma tiene imágenes de ojo "
       "de pez o panorámicas, que puede emparejar mal."),
    PT("O FlexGlue foi feito para lentes comuns. Esta captura tem imagens olho de peixe "
       "ou panorâmicas, que ele pode emparelhar mal."),
    IT("FlexGlue è pensato per obiettivi comuni. Questa ripresa contiene immagini "
       "fisheye o panoramiche, che potrebbe abbinare male."),
    NL("FlexGlue is bedoeld voor gewone objectieven. Deze opname bevat fisheye- of "
       "panoramabeelden, die het mogelijk slecht matcht."),
    RU("FlexGlue рассчитан на обычные объективы. В этой съёмке есть изображения "
       "«рыбьего глаза» или панорамы — их он может сопоставить плохо."),
    TR("FlexGlue sıradan objektifler için tasarlanmıştır. Bu çekimde balıkgözü ya da "
       "panorama görüntüleri var; bunları kötü eşleştirebilir."));

SS_MSG(license_flexglue_title,
    EN("DINOv3 License (Meta) and FlexGlue License (MIT)"),
    JA("DINOv3 ライセンス（Meta）と FlexGlue ライセンス（MIT）"),
    ZH_HANS("DINOv3 许可协议（Meta）与 FlexGlue 许可协议（MIT）"),
    ZH_HANT("DINOv3 授權條款（Meta）與 FlexGlue 授權條款（MIT）"),
    KO("DINOv3 라이선스(Meta) 및 FlexGlue 라이선스(MIT)"),
    DE("DINOv3-Lizenz (Meta) und FlexGlue-Lizenz (MIT)"),
    FR("Licence DINOv3 (Meta) et licence FlexGlue (MIT)"),
    ES("Licencia de DINOv3 (Meta) y licencia de FlexGlue (MIT)"),
    PT("Licença do DINOv3 (Meta) e licença do FlexGlue (MIT)"),
    IT("Licenza DINOv3 (Meta) e licenza FlexGlue (MIT)"),
    NL("DINOv3-licentie (Meta) en FlexGlue-licentie (MIT)"),
    RU("Лицензия DINOv3 (Meta) и лицензия FlexGlue (MIT)"),
    TR("DINOv3 Lisansı (Meta) ve FlexGlue Lisansı (MIT)"));

// LEGAL -- human review in every language. Mirrors dense::terms (RoMa v2), which
// carries the same DINOv3 weights.
SS_MSG(license_flexglue_terms,
    EN("FlexGlue uses DINOv3 weights under Meta's custom terms. Review those terms and FlexGlue's MIT license before downloading. Weights are downloaded at runtime and are not bundled. A local checkpoint path is also supported."),
    JA("FlexGlue は Meta の独自条件に従う DINOv3 重みを使用します。ダウンロード前にその条件と FlexGlue の MIT ライセンスをご確認ください。重みは実行時に取得し、同梱しません。ローカルのチェックポイントも使用できます。"),
    ZH_HANS("FlexGlue 使用遵循 Meta 自定义条款的 DINOv3 权重。下载前请阅读这些条款及 FlexGlue 的 MIT 许可。权重在运行时下载，不随程序打包。也支持本地检查点路径。"),
    ZH_HANT("FlexGlue 使用遵循 Meta 自訂條款的 DINOv3 權重。下載前請閱讀這些條款及 FlexGlue 的 MIT 授權。權重在執行時下載，不隨程式打包。也支援本機檢查點路徑。"),
    KO("FlexGlue는 Meta의 사용자 지정 약관에 따른 DINOv3 가중치를 사용합니다. 다운로드 전에 해당 약관과 FlexGlue MIT 라이선스를 확인하세요. 가중치는 실행 시 다운로드되며 포함되지 않습니다. 로컬 체크포인트 경로도 지원합니다."),
    DE("FlexGlue verwendet DINOv3-Gewichte unter Metas eigenen Bedingungen. Lesen Sie diese und FlexGlues MIT-Lizenz vor dem Download. Gewichte werden zur Laufzeit geladen und nicht mitgeliefert. Ein lokaler Checkpoint ist ebenfalls möglich."),
    FR("FlexGlue utilise les poids DINOv3 soumis aux conditions de Meta. Consultez ces conditions et la licence MIT de FlexGlue avant le téléchargement. Les poids sont téléchargés à l'exécution et ne sont pas inclus. Un checkpoint local est aussi accepté."),
    ES("FlexGlue usa pesos DINOv3 bajo las condiciones propias de Meta. Revise esas condiciones y la licencia MIT de FlexGlue antes de descargar. Los pesos se descargan al ejecutar y no se incluyen. También se acepta un checkpoint local."),
    PT("FlexGlue usa pesos DINOv3 sob os termos próprios da Meta. Leia esses termos e a licença MIT do FlexGlue antes de baixar. Os pesos são baixados em execução e não são incluídos. Um checkpoint local também é aceito."),
    IT("FlexGlue usa pesi DINOv3 soggetti alle condizioni di Meta. Leggere queste condizioni e la licenza MIT di FlexGlue prima del download. I pesi si scaricano durante l'esecuzione e non sono inclusi. È supportato anche un checkpoint locale."),
    NL("FlexGlue gebruikt DINOv3-gewichten onder Meta's eigen voorwaarden. Lees die voorwaarden en FlexGlues MIT-licentie voor het downloaden. Gewichten worden tijdens gebruik gedownload en niet meegeleverd. Een lokaal checkpoint wordt ook ondersteund."),
    RU("FlexGlue использует веса DINOv3 по особым условиям Meta. Изучите эти условия и лицензию MIT FlexGlue до загрузки. Веса загружаются при работе и не входят в программу. Также поддерживается локальный файл весов."),
    TR("FlexGlue, Meta'nın özel koşulları altındaki DINOv3 ağırlıklarını kullanır. İndirmeden önce bu koşulları ve FlexGlue MIT lisansını inceleyin. Ağırlıklar çalışma sırasında indirilir ve paketlenmez. Yerel kontrol noktası yolu da desteklenir."));

SS_MSG(matcher_help,
    EN("How descriptors are matched. LightGlue is a learned matcher: it "
       "generally gives a better success rate on hard pairs, at the cost of "
       "much slower matching."),
    JA("記述子をどう照合するかです。LightGlue は学習型のマッチャーで、難しい"
       "ペアでも成功率がおおむね高くなりますが、照合はずっと遅くなります。"),
    ZH_HANS("描述子如何匹配。LightGlue 是学习型匹配器：在困难配对上成功率通常"
            "更高，代价是匹配慢得多。"),
    ZH_HANT("描述子如何比對。LightGlue 是學習型配對器：在困難配對上成功率通常"
            "更高，代價是比對慢得多。"),
    KO("기술자를 어떻게 맞출지입니다. LightGlue는 학습형 매처로, 어려운 쌍에서 "
       "대체로 성공률이 더 높지만 매칭이 훨씬 느립니다."),
    DE("Wie Deskriptoren zugeordnet werden. LightGlue ist ein gelernter "
       "Zuordner: er hat bei schwierigen Paaren meist eine bessere "
       "Trefferquote, ordnet dafür aber viel langsamer zu."),
    FR("Comment les descripteurs sont appariés. LightGlue est un apparieur "
       "appris : il réussit généralement mieux sur les paires difficiles, au "
       "prix d'un appariement bien plus lent."),
    ES("Cómo se emparejan los descriptores. LightGlue es un emparejador "
       "aprendido: suele acertar más en pares difíciles, a costa de un "
       "emparejamiento mucho más lento."),
    PT("Como os descritores são casados. O LightGlue é um correspondedor "
       "aprendido: costuma ter mais sucesso em pares difíceis, ao custo de um "
       "casamento bem mais lento."),
    IT("Come vengono abbinati i descrittori. LightGlue è un abbinatore "
       "appreso: in genere riesce meglio sulle coppie difficili, al prezzo di "
       "un abbinamento molto più lento."),
    NL("Hoe descriptors worden gematcht. LightGlue is een geleerde matcher: "
       "die slaagt bij lastige paren doorgaans beter, maar matcht wel veel "
       "trager."),
    RU("Как сопоставляются дескрипторы. LightGlue — обученный сопоставитель: "
       "на трудных парах он обычно успешнее, но сопоставляет намного "
       "медленнее."),
    TR("Betimleyicilerin nasıl eşleştirileceği. LightGlue öğrenilmiş bir "
       "eşleştiricidir: zor çiftlerde genellikle daha başarılıdır, "
       "karşılığında eşleştirme çok daha yavaştır."));

SS_MSG(matcher_needs_learned,
    EN("LightGlue needs the learned descriptors -- pick an ALIKED frontend "
       "above to enable it."),
    JA("LightGlue には学習型の記述子が必要です。上で ALIKED のフロントエンドを"
       "選ぶと使えるようになります。"),
    ZH_HANS("LightGlue 需要学习型描述子——请在上面选一个 ALIKED 前端来启用它。"),
    ZH_HANT("LightGlue 需要學習型描述子——請在上面選一個 ALIKED 前端來啟用它。"),
    KO("LightGlue에는 학습형 기술자가 필요합니다. 위에서 ALIKED 프런트엔드를 "
       "고르면 켜집니다."),
    DE("LightGlue braucht die gelernten Deskriptoren -- oben ein "
       "ALIKED-Frontend wählen, um es freizuschalten."),
    FR("LightGlue exige les descripteurs appris -- choisissez un frontal "
       "ALIKED ci-dessus pour l'activer."),
    ES("LightGlue necesita los descriptores aprendidos: elija arriba un "
       "frontal ALIKED para activarlo."),
    PT("O LightGlue precisa dos descritores aprendidos -- escolha acima um "
       "front-end ALIKED para habilitá-lo."),
    IT("LightGlue richiede i descrittori appresi: scelga sopra un frontend "
       "ALIKED per abilitarlo."),
    NL("LightGlue heeft de geleerde descriptors nodig -- kies hierboven een "
       "ALIKED-frontend om het aan te zetten."),
    RU("LightGlue нужны обученные дескрипторы — выберите выше фронтенд ALIKED, "
       "чтобы он стал доступен."),
    TR("LightGlue öğrenilmiş betimleyicilere ihtiyaç duyar -- etkinleştirmek "
       "için yukarıdan bir ALIKED ön ucu seçin."));

SS_MSG(feat_get_model,
    EN("Get the feature model"),
    JA("特徴のモデルを取得"), ZH_HANS("获取特征模型"), ZH_HANT("取得特徵模型"),
    KO("특징 모델 받기"), DE("Merkmalsmodell holen"),
    FR("Obtenir le modèle de points caractéristiques"),
    ES("Obtener el modelo de características"),
    PT("Obter o modelo de características"),
    IT("Ottieni il modello delle caratteristiche"),
    NL("Kenmerkmodel ophalen"), RU("Получить модель особых точек"),
    TR("Öznitelik modelini getir"));

SS_MSG(feat_model_ready,
    EN("Feature model ready."),
    JA("特徴のモデルの準備ができました。"),
    ZH_HANS("特征模型已就绪。"), ZH_HANT("特徵模型已就緒。"),
    KO("특징 모델이 준비되었습니다."), DE("Merkmalsmodell bereit."),
    FR("Modèle de points caractéristiques prêt."),
    ES("Modelo de características listo."),
    PT("Modelo de características pronto."),
    IT("Modello delle caratteristiche pronto."),
    NL("Kenmerkmodel gereed."), RU("Модель особых точек готова."),
    TR("Öznitelik modeli hazır."));

SS_MSG(feat_model_first,
    EN("the feature model has not been downloaded yet"),
    JA("特徴のモデルがまだダウンロードされていません"),
    ZH_HANS("还没有下载特征模型"), ZH_HANT("還沒有下載特徵模型"),
    KO("특징 모델을 아직 내려받지 않았습니다"),
    DE("das Merkmalsmodell ist noch nicht heruntergeladen"),
    FR("le modèle de points caractéristiques n'est pas encore téléchargé"),
    ES("el modelo de características todavía no está descargado"),
    PT("o modelo de características ainda não foi baixado"),
    IT("il modello delle caratteristiche non è ancora stato scaricato"),
    NL("het kenmerkmodel is nog niet gedownload"),
    RU("модель особых точек ещё не загружена"),
    TR("öznitelik modeli henüz indirilmedi"));

SS_MSG(mapper_schedule,
    EN("Mapper schedule"), JA("マッパーの進め方"), ZH_HANS("建图策略"), ZH_HANT("建圖策略"),
    KO("매퍼 진행 방식"), DE("Mapper-Ablauf"), FR("Ordonnancement du mapper"),
    ES("Estrategia del mapeador"), PT("Estratégia do mapeador"),
    IT("Strategia del mapper"), NL("Mapper-schema"),
    RU("Схема работы маппера"), TR("Haritalayıcı planı"));

SS_MSG(mapper_flat,
    EN("Flat (one reconstruction)"),
    JA("フラット（1つの再構成）"),
    ZH_HANS("扁平（单个重建）"),
    ZH_HANT("扁平（單個重建）"),
    KO("평면(재구성 하나)"),
    DE("Flach (eine Rekonstruktion)"),
    FR("Plat (une seule reconstruction)"),
    ES("Plano (una sola reconstrucción)"),
    PT("Plano (uma única reconstrução)"),
    IT("Piatto (una sola ricostruzione)"),
    NL("Vlak (één reconstructie)"),
    RU("Плоская (одна реконструкция)"),
    TR("Düz (tek yeniden oluşturma)"));

SS_MSG(mapper_bottom_up,
    EN("Bottom-up (atoms, merged upwards)"),
    JA("ボトムアップ（小さな塊を作って統合）"),
    ZH_HANS("自下而上（先建小块再向上合并）"),
    ZH_HANT("由下而上（先建小塊再向上合併）"),
    KO("상향식(작은 덩어리를 만들어 위로 병합)"),
    DE("Von unten (Atome, nach oben zusammengeführt)"),
    FR("Ascendant (atomes, fusionnés vers le haut)"),
    ES("Ascendente (átomos, fusionados hacia arriba)"),
    PT("Ascendente (átomos, mesclados para cima)"),
    IT("Dal basso (atomi, uniti verso l'alto)"),
    NL("Bottom-up (atomen, naar boven samengevoegd)"),
    RU("Снизу вверх (атомы, объединяемые вверх)"),
    TR("Aşağıdan yukarı (atomlar, yukarı doğru birleştirilir)"));

SS_MSG(mapper_global,
    EN("Global (experimental)"), JA("グローバル（実験的）"),
    ZH_HANS("全局（实验性）"), ZH_HANT("全域（實驗性）"),
    KO("전역(실험적)"), DE("Global (experimentell)"),
    FR("Global (expérimental)"), ES("Global (experimental)"),
    PT("Global (experimental)"), IT("Globale (sperimentale)"),
    NL("Globaal (experimenteel)"), RU("Глобальная (экспериментально)"),
    TR("Küresel (deneysel)"));

SS_MSG(mapper_schedule_help,
    EN("- Flat: default, adds cameras one by one.\n- Bottom-up: reconstructs small groups and merges them.\n- Global: solves camera rotations and positions together; experimental."),
    JA("- 平面：既定。カメラを1台ずつ追加します。\n- ボトムアップ：小グループを再構成して結合します。\n- グローバル：カメラの回転と位置をまとめて解きます。実験的です。"),
    ZH_HANS("- 平面：默认，逐个添加相机。\n- 自底向上：重建小组并合并。\n- 全局：联合求解相机旋转和位置，实验性。"),
    ZH_HANT("- 平面：預設，逐個新增相機。\n- 自底向上：重建小組並合併。\n- 全域：聯合求解相機旋轉和位置，實驗性。"),
    KO("- 평면: 기본값, 카메라를 하나씩 추가합니다.\n- 상향식: 작은 그룹을 재구성해 병합합니다.\n- 전역: 회전·위치를 함께 풉니다. 실험적입니다."),
    DE("- Flach: Standard, fügt Kameras einzeln hinzu.\n- Bottom-up: rekonstruiert kleine Gruppen und vereint sie.\n- Global: löst Kameradrehungen und Positionen gemeinsam; experimentell."),
    FR("- Plat : défaut, ajoute les caméras une à une.\n- Ascendant : reconstruit de petits groupes et les fusionne.\n- Global : résout rotations et positions ensemble ; expérimental."),
    ES("- Plano: por defecto, añade cámaras una a una.\n- Ascendente: reconstruye grupos pequeños y los fusiona.\n- Global: resuelve giros y posiciones juntos; experimental."),
    PT("- Plano: padrão, adiciona câmaras uma a uma.\n- Ascendente: reconstrói pequenos grupos e funde-os.\n- Global: resolve rotações e posições em conjunto; experimental."),
    IT("- Piatto: predefinito, aggiunge camere una per volta.\n- Dal basso: ricostruisce piccoli gruppi e li unisce.\n- Globale: risolve rotazioni e posizioni insieme; sperimentale."),
    NL("- Vlak: standaard, voegt camera’s één voor één toe.\n- Bottom-up: reconstrueert kleine groepen en voegt ze samen.\n- Globaal: lost camerarotaties en posities samen op; experimenteel."),
    RU("- Плоский: по умолчанию, добавляет камеры по одной.\n- Снизу вверх: реконструирует малые группы и объединяет их.\n- Глобальный: решает повороты и положения вместе; экспериментальный."),
    TR("- Düz: varsayılan, kameraları tek tek ekler.\n- Aşağıdan yukarı: küçük grupları oluşturup birleştirir.\n- Küresel: kamera dönüş ve konumlarını birlikte çözer; deneysel."));

SS_MSG(prefilter_sequential,
    EN("Also match neighbouring frames"),
    JA("隣り合うフレームもマッチングする"),
    ZH_HANS("同时匹配相邻帧"),
    ZH_HANT("同時比對相鄰影格"),
    KO("이웃 프레임도 매칭"),
    DE("Auch benachbarte Bilder zuordnen"),
    FR("Apparier aussi les images voisines"),
    ES("Emparejar también los fotogramas vecinos"),
    PT("Comparar também os quadros vizinhos"),
    IT("Abbinare anche i fotogrammi vicini"),
    NL("Ook naburige beelden matchen"),
    RU("Сопоставлять и соседние кадры"),
    TR("Komşu kareleri de eşleştir"));

SS_MSG(prefilter_sequential_help,
    EN("Adds file-order neighbor matches to content pre-selection, preventing weak gaps in walks at extra matching cost. Automatic enables it from 100 images."),
    JA("内容による事前選択にファイル順の隣接照合を加え、歩行撮影の弱い隙間を防ぎます。追加の照合が必要です。自動では100枚以上で有効です。"),
    ZH_HANS("在内容预选外增加文件顺序邻近匹配，防止行走拍摄断连，但增加匹配开销。自动模式从 100 张起启用。"),
    ZH_HANT("在內容預選外增加檔案順序鄰近匹配，防止行走拍攝斷連，但增加匹配開銷。自動模式從 100 張起啟用。"),
    KO("내용 사전 선택에 파일 순서 이웃 매칭을 더해 보행 촬영의 끊김을 막습니다. 매칭 비용이 추가됩니다. 자동은 100장부터 켭니다."),
    DE("Ergänzt Inhaltsvorauswahl um Dateinachbarn gegen Lücken in Rundgängen; mehr Zuordnungsarbeit. Automatisch ab 100 Bildern."),
    FR("Ajoute les voisins par ordre de fichiers à la présélection pour éviter les ruptures d’un parcours ; coût accru. Automatique dès 100 images."),
    ES("Añade vecinos en orden de archivo a la preselección para evitar cortes en recorridos; más coste de emparejamiento. Automático desde 100 imágenes."),
    PT("Adiciona vizinhos por ordem de ficheiro à pré-seleção para evitar falhas num percurso; mais custo. Automático a partir de 100 imagens."),
    IT("Aggiunge vicini in ordine di file alla preselezione per evitare interruzioni nei percorsi; costo extra. Automatico da 100 immagini."),
    NL("Voegt bestandsburen toe aan inhoudsvoorselectie tegen gaten in wandelingen; extra koppelwerk. Automatisch vanaf 100 beelden."),
    RU("Добавляет соседей по порядку файлов к отбору по содержимому, закрывая слабые связи прохода ценой лишних сравнений. Авто от 100 снимков."),
    TR("İçerik ön seçimine dosya sırası komşularını ekleyerek yürüyüşte kopmayı önler; ek eşleştirme maliyeti vardır. Otomatik 100 resimden itibaren açar."));

SS_MSG(sequential_overlap,
    EN("Sequential overlap"),
    JA("逐次マッチングの重なり"),
    ZH_HANS("顺序匹配的重叠数"),
    ZH_HANT("循序比對的重疊數"),
    KO("순차 겹침"),
    DE("Sequenzielle Überlappung"),
    FR("Recouvrement séquentiel"),
    ES("Solapamiento secuencial"),
    PT("Sobreposição sequencial"),
    IT("Sovrapposizione sequenziale"),
    NL("Sequentiële overlap"),
    RU("Перекрытие при последовательном сопоставлении"),
    TR("Sıralı örtüşme"));

SS_MSG(sequential_overlap_help,
    EN("How many neighbouring frames each frame is matched against."),
    JA("各フレームを、隣り合う何フレームと照合するかです。"),
    ZH_HANS("每一帧要和多少相邻帧做匹配。"),
    ZH_HANT("每一影格要和多少相鄰影格做比對。"),
    KO("각 프레임을 이웃한 몇 개의 프레임과 매칭할지입니다."),
    DE("Mit wie vielen benachbarten Bildern jedes Bild verglichen wird."),
    FR("Nombre d'images voisines auxquelles chaque image est appariée."),
    ES("Con cuántos fotogramas vecinos se empareja cada fotograma."),
    PT("Com quantos quadros vizinhos cada quadro é comparado."),
    IT("Con quanti fotogrammi vicini viene confrontato ogni fotogramma."),
    NL("Met hoeveel naburige beelden elk beeld wordt gematcht."),
    RU("Со сколькими соседними кадрами сопоставляется каждый кадр."),
    TR("Her karenin kaç komşu kareyle eşleştirileceği."));

SS_MSG(initial_focal_px,
    EN("Initial focal length (px, 0 = auto)"),
    JA("初期焦点距離（px、0 で自動）"),
    ZH_HANS("初始焦距（像素，0 = 自动）"),
    ZH_HANT("初始焦距（像素，0 = 自動）"),
    KO("초기 초점거리(px, 0 = 자동)"),
    DE("Anfangsbrennweite (px, 0 = automatisch)"),
    FR("Focale initiale (px, 0 = auto)"),
    ES("Focal inicial (px, 0 = automático)"),
    PT("Distância focal inicial (px, 0 = automático)"),
    IT("Focale iniziale (px, 0 = automatico)"),
    NL("Beginbrandpuntsafstand (px, 0 = automatisch)"),
    RU("Начальное фокусное расстояние (пикс., 0 — авто)"),
    TR("Başlangıç odak uzaklığı (px, 0 = otomatik)"));

SS_MSG(initial_focal_px_help,
    EN("Starting guess for the focal length, in pixels of the source image.\n- 0 reads EXIF and falls back to a guess from the image size.\n- Worth setting for a fisheye, where a bad initial guess can stop the reconstruction from starting at all."),
    JA("焦点距離の初期推定値を、元画像のピクセル単位で指定します。 0 なら EXIF を読み、なければ画像サイズから推定します。\n魚眼では初期値が悪いと再構成がそもそも始まらないことがあるので、指定する価値があります。"),
    ZH_HANS("焦距的初始猜测值，以源图像的像素为单位。填 0 会读 EXIF，读不到就按图像尺寸估计。鱼眼值得设一下：初值不好可能让重建根本起不来。"),
    ZH_HANT("焦距的初始猜測值，以來源影像的像素為單位。填 0 會讀 EXIF，讀不到就按影像尺寸估計。魚眼值得設一下：初值不好可能讓重建根本起不來。"),
    KO("원본 이미지 픽셀 단위의 초기 초점 거리입니다. 0은 EXIF, 없으면 이미지 크기로 추정합니다. 어안의 잘못된 초기값은 재구성 시작을 막을 수 있습니다."),
    DE("Anfangsschätzung der Brennweite, in Pixeln des Ausgangsbildes.\n- 0 liest EXIF und fällt auf eine Schätzung aus der Bildgröße zurück.\n- Bei einem Fischauge lohnt es sich, denn eine schlechte Anfangsschätzung kann die Rekonstruktion ganz verhindern."),
    FR("Estimation de départ de la focale, en pixels de l'image source.\n- 0 lit l'EXIF et retombe sur une estimation d'après la taille d'image.\n- Utile pour un fisheye, où une mauvaise valeur de départ peut empêcher la reconstruction de démarrer."),
    ES("Estimación inicial de la focal, en píxeles de la imagen de origen.\n- 0 lee el EXIF y recurre a una estimación por el tamaño de imagen.\n- Vale la pena fijarla en un ojo de pez, donde un mal valor inicial puede impedir que la reconstrucción arranque."),
    PT("Palpite inicial da distância focal, em pixels da imagem de origem.\n- 0 lê o EXIF e recorre a uma estimativa pelo tamanho da imagem.\n- Vale definir num olho de peixe, onde um palpite inicial ruim pode impedir a reconstrução de começar."),
    IT("Focale iniziale in pixel sorgente. 0 usa EXIF o una stima dalla dimensione. Per il fisheye conviene impostarla: una stima errata può impedire la ricostruzione."),
    NL("Beginschatting van de brandpuntsafstand, in pixels van het bronbeeld.\n- 0 leest EXIF en valt terug op een schatting uit de beeldgrootte.\n- De moeite waard bij een fisheye, waar een slechte beginschatting de reconstructie helemaal kan blokkeren."),
    RU("Начальная оценка фокусного расстояния в пикселях исходного изображения.\n- 0 читает EXIF, а при его отсутствии оценивает по размеру изображения.\n- Для фишая задать стоит: плохая начальная оценка может вовсе не дать реконструкции начаться."),
    TR("Odak uzaklığı için başlangıç tahmini, kaynak görüntünün pikselleri cinsinden.\n- 0, EXIF'i okur ve bulamazsa görüntü boyutundan tahmin eder.\n- Balıkgözünde ayarlamaya değer: kötü bir başlangıç tahmini yeniden oluşturmanın hiç başlamamasına yol açabilir."));

SS_MSG(initial_distortion,
    EN("Initial distortion (k1,k2,...)"),
    JA("歪みの初期値（k1,k2,...）"),
    ZH_HANS("畸变初值（k1,k2,...）"),
    ZH_HANT("畸變初值（k1,k2,...）"),
    KO("왜곡 초기값(k1,k2,...)"),
    DE("Anfangsverzeichnung (k1,k2,...)"),
    FR("Distorsion initiale (k1,k2,...)"),
    ES("Distorsión inicial (k1,k2,...)"),
    PT("Distorção inicial (k1,k2,...)"),
    IT("Distorsione iniziale (k1,k2,...)"),
    NL("Beginvertekening (k1,k2,...)"),
    RU("Начальная дисторсия (k1,k2,...)"),
    TR("Başlangıç bozulması (k1,k2,...)"));

SS_MSG(initial_distortion_hint,
    EN("empty = start at zero"),
    JA("空欄なら 0 から"),
    ZH_HANS("留空则从 0 开始"),
    ZH_HANT("留空則從 0 開始"),
    KO("비우면 0 에서 시작"),
    DE("leer = bei null beginnen"),
    FR("vide = partir de zéro"),
    ES("vacío = empezar en cero"),
    PT("vazio = começar do zero"),
    IT("vuoto = partire da zero"),
    NL("leeg = bij nul beginnen"),
    RU("пусто — начать с нуля"),
    TR("boş = sıfırdan başla"));

SS_MSG(initial_distortion_help,
    EN("Where the distortion coefficients start, in the order the chosen lens model lists them (OpenCV: k1,k2,p1,p2).\n- Empty starts at zero, which is right unless the lens is already calibrated.\n- Combine with \"Distortion refinement: never\" to hold a known calibration exactly."),
    JA("歪み係数の開始値を、選んだレンズモデルの並び順で指定します（OpenCV なら k1,k2,p1,p2）。\n- 空欄なら 0 から始まり、レンズが校正済みでない限りそれが適切です。\n- 「歪みの最適化: しない」と組み合わせると、既知の校正値をそのまま固定できます。"),
    ZH_HANS("畸变系数的起始值，按所选镜头模型列出的顺序填写（OpenCV 为 k1,k2,p1,p2）。\n留空则从 0 开始； 除非镜头已经标定，否则留空就是对的。 与「畸变优化: 不优化」搭配，可原样固定一组已知标定值。"),
    ZH_HANT("畸變係數的起始值，按所選鏡頭模型列出的順序填寫（OpenCV 為 k1,k2,p1,p2）。\n留空則從 0 開始； 除非鏡頭已經標定，否則留空就是對的。 與「畸變最佳化: 不最佳化」搭配，可原樣固定一組已知標定值。"),
    KO("왜곡 계수의 시작값을 선택한 렌즈 모델이 나열하는 순서로 적습니다(OpenCV 는 k1,k2,p1,p2).\n- 비우면 0 에서 시작하며, 렌즈가 이미 보정되어 있지 않다면 그것이 맞습니다.\n- ‘왜곡 보정 최적화: 하지 않음’과 함께 쓰면 알려진 보정값을 그대로 고정할 수 있습니다."),
    DE("Wo die Verzeichnungskoeffizienten beginnen, in der Reihenfolge des gewählten Objektivmodells (OpenCV: k1,k2,p1,p2).\n- Leer beginnt bei null, was richtig ist, solange das Objektiv nicht bereits kalibriert ist.\n- Zusammen mit „Verzeichnung mitoptimieren: nie“ hält es eine bekannte Kalibrierung exakt fest."),
    FR("Où démarrent les coefficients de distorsion, dans l'ordre où le modèle d'objectif choisi les énumère (OpenCV : k1,k2,p1,p2).\n- Vide, ils partent de zéro, ce qui convient sauf si l'objectif est déjà calibré.\n- À combiner avec « Affinage de la distorsion : jamais » pour figer exactement un étalonnage connu."),
    ES("Dónde empiezan los coeficientes de distorsión, en el orden en que los enumera el modelo de óptica elegido (OpenCV: k1,k2,p1,p2).\n- Vacío empieza en cero, que es lo correcto salvo que la óptica ya esté calibrada.\n- Combínalo con «Refinado de la distorsión: nunca» para fijar exactamente una calibración conocida."),
    PT("Onde os coeficientes de distorção começam, na ordem em que o modelo de lente escolhido os lista (OpenCV: k1,k2,p1,p2).\n- Vazio começa do zero, o que é o certo a menos que a lente já esteja calibrada.\n- Combine com «Refino da distorção: nunca» para fixar exatamente uma calibração conhecida."),
    IT("Da dove partono i coefficienti di distorsione, nell'ordine in cui il modello di obiettivo scelto li elenca (OpenCV: k1,k2,p1,p2).\n- Vuoto parte da zero, che è giusto a meno che l'obiettivo non sia già calibrato.\n- Insieme a «Affinamento della distorsione: mai» blocca esattamente una calibrazione nota."),
    NL("Waar de vertekeningscoëfficiënten beginnen, in de volgorde die het gekozen lensmodel aanhoudt (OpenCV: k1,k2,p1,p2).\n- Leeg begint bij nul, wat klopt tenzij de lens al gekalibreerd is.\n- Samen met \"Vertekening bijstellen: nooit\" houdt het een bekende kalibratie precies vast."),
    RU("С чего начинаются коэффициенты дисторсии, в том порядке, в каком их перечисляет выбранная модель объектива (OpenCV: k1,k2,p1,p2).\n- Пустое поле начинает с нуля, и это верно, если объектив ещё не откалиброван.\n- Вместе с «Уточнение дисторсии: никогда» это точно закрепляет известную калибровку."),
    TR("Bozulma katsayılarının nereden başlayacağı, seçilen objektif modelinin sıraladığı düzende (OpenCV: k1,k2,p1,p2).\n- Boş bırakılırsa sıfırdan başlar; objektif zaten kalibre edilmiş değilse doğrusu budur.\n- \"Bozulmanın iyileştirilmesi: asla\" ile birlikte bilinen bir kalibrasyonu tam olarak sabitler."));

SS_MSG(sfm_distortion_refinement,
    EN("Distortion refinement"),
    JA("歪みの最適化"),
    ZH_HANS("畸变优化"),
    ZH_HANT("畸變最佳化"),
    KO("왜곡 보정 최적화"),
    DE("Verzeichnung mitoptimieren"),
    FR("Affinage de la distorsion"),
    ES("Refinado de la distorsión"),
    PT("Refino da distorção"),
    IT("Affinamento della distorsione"),
    NL("Vertekening bijstellen"),
    RU("Уточнение дисторсии"),
    TR("Bozulmanın iyileştirilmesi"));

SS_MSG(sfm_distortion_during,
    EN("During mapping"),
    JA("マッピング中"),
    ZH_HANS("建图过程中"),
    ZH_HANT("建圖過程中"),
    KO("매핑 중"),
    DE("Während des Mappings"),
    FR("Pendant le mapping"),
    ES("Durante el mapeo"),
    PT("Durante o mapeamento"),
    IT("Durante il mapping"),
    NL("Tijdens het mappen"),
    RU("Во время реконструкции"),
    TR("Haritalama sırasında"));

SS_MSG(sfm_distortion_final,
    EN("Final pass only"),
    JA("最後の仕上げだけ"),
    ZH_HANS("只在最后一遍"),
    ZH_HANT("只在最後一遍"),
    KO("마지막 단계에서만"),
    DE("Nur im letzten Durchgang"),
    FR("Seulement à la passe finale"),
    ES("Solo en la pasada final"),
    PT("Só na passagem final"),
    IT("Solo nella passata finale"),
    NL("Alleen in de laatste ronde"),
    RU("Только на финальном проходе"),
    TR("Yalnızca son geçişte"));

SS_MSG(sfm_distortion_never,
    EN("Never"),
    JA("しない"),
    ZH_HANS("不优化"),
    ZH_HANT("不最佳化"),
    KO("하지 않음"),
    DE("Nie"),
    FR("Jamais"),
    ES("Nunca"),
    PT("Nunca"),
    IT("Mai"),
    NL("Nooit"),
    RU("Никогда"),
    TR("Asla"));

SS_MSG(sfm_distortion_refinement_help,
    EN("Final pass only: holds initial distortion during mapping, then fits it once; steadier for low distortion. The “Never” option locks a trusted calibration."),
    JA("最終段階のみ：構築中は初期歪みを保ち、最後に一度調整します。歪みの小さいレンズで安定します。「しない」は既知の補正を固定します。"),
    ZH_HANS("仅最终阶段：建图时固定初始畸变，最后优化一次，对低畸变更稳定。“从不”锁定可信的标定。"),
    ZH_HANT("僅最終階段：建圖時固定初始畸變，最後最佳化一次，對低畸變更穩定。「永不」鎖定可信的校正。"),
    KO("최종 단계만: 매핑 중 초기 왜곡을 유지하고 마지막에 한 번 풉니다. 작은 왜곡에서 안정적입니다. “안 함”은 알려진 보정을 고정합니다."),
    DE("Nur letzter Durchlauf: hält die Anfangsverzeichnung beim Aufbau, passt sie am Ende an; stabiler bei wenig Verzeichnung. „Nie“ fixiert bekannte Kalibrierung."),
    FR("Passe finale seule : garde la distorsion initiale puis l’ajuste une fois ; plus stable si faible. « Jamais » verrouille une calibration fiable."),
    ES("Solo pase final: mantiene la distorsión inicial y la ajusta al final; más estable con poca distorsión. «Nunca» fija una calibración fiable."),
    PT("Só passagem final: mantém a distorção inicial e ajusta-a no fim; mais estável com pouca distorção. «Nunca» fixa uma calibração fiável."),
    IT("Solo passo finale: mantiene la distorsione iniziale e la adatta alla fine; più stabile se bassa. «Mai» blocca una calibrazione nota."),
    NL("Alleen eindronde: houdt beginvervorming vast en past die achteraf aan; stabieler bij weinig vervorming. ‘Nooit’ zet bekende kalibratie vast."),
    RU("Только финал: держит исходную дисторсию при построении, уточняя в конце; устойчивее при малой дисторсии. «Никогда» фиксирует известную калибровку."),
    TR("Yalnız son geçiş: başlangıç bozulmasını tutar, sonunda ayarlar; az bozulmada daha kararlı. “Asla” güvenilen kalibrasyonu sabitler."));

SS_MSG(sfm_metric_gps,
    EN("GPS"),
    JA("GPS"),
    ZH_HANS("GPS"),
    ZH_HANT("GPS"),
    KO("GPS"),
    DE("GPS"),
    FR("GPS"),
    ES("GPS"),
    PT("GPS"),
    IT("GPS"),
    NL("Gps"),
    RU("GPS"),
    TR("GPS"));

SS_MSG(sfm_metric_gps_off,
    EN("Off"),
    JA("使わない"),
    ZH_HANS("不使用"),
    ZH_HANT("不使用"),
    KO("사용 안 함"),
    DE("Aus"),
    FR("Désactivé"),
    ES("Desactivado"),
    PT("Desligado"),
    IT("Disattivato"),
    NL("Uit"),
    RU("Выключено"),
    TR("Kapalı"));

SS_MSG(sfm_metric_gps_horizontal,
    EN("Latitude and longitude"),
    JA("緯度と経度"),
    ZH_HANS("经纬度"),
    ZH_HANT("經緯度"),
    KO("위도와 경도"),
    DE("Breite und Länge"),
    FR("Latitude et longitude"),
    ES("Latitud y longitud"),
    PT("Latitude e longitude"),
    IT("Latitudine e longitudine"),
    NL("Breedte en lengte"),
    RU("Широта и долгота"),
    TR("Enlem ve boylam"));

SS_MSG(sfm_metric_gps_full,
    EN("With altitude"),
    JA("高度も使う"),
    ZH_HANS("连高度一起用"),
    ZH_HANT("連高度一起用"),
    KO("고도까지 사용"),
    DE("Mit Höhe"),
    FR("Avec l'altitude"),
    ES("Con la altitud"),
    PT("Com a altitude"),
    IT("Con la quota"),
    NL("Met hoogte"),
    RU("С высотой"),
    TR("Yükseklikle"));

SS_MSG(sfm_metric_gps_auto,
    EN("Automatic"),
    JA("自動"),
    ZH_HANS("自动"),
    ZH_HANT("自動"),
    KO("자동"),
    DE("Automatisch"),
    FR("Automatique"),
    ES("Automático"),
    PT("Automático"),
    IT("Automatico"),
    NL("Automatisch"),
    RU("Автоматически"),
    TR("Otomatik"));

SS_MSG(section_sensors,
    EN("Sensors"),       JA("センサー"),      ZH_HANS("传感器"),    ZH_HANT("感測器"),
    KO("센서"),           DE("Sensoren"),     FR("Capteurs"),      ES("Sensores"),
    PT("Sensores"),      IT("Sensori"),      NL("Sensoren"),      RU("Датчики"),
    TR("Sensörler"));

SS_MSG(sfm_sensor_gauge,
    EN("Video IMU"),
    JA("動画のIMU"),
    ZH_HANS("视频 IMU"),
    ZH_HANT("影片 IMU"),
    KO("동영상 IMU"),
    DE("IMU des Videos"),
    FR("IMU de la vidéo"),
    ES("IMU del vídeo"),
    PT("IMU do vídeo"),
    IT("IMU del video"),
    NL("IMU van de video"),
    RU("IMU видео"),
    TR("Videonun IMU'su"));

SS_MSG(sfm_sensor_gauge_off,
    EN("Ignore"),        JA("使わない"),      ZH_HANS("不使用"),    ZH_HANT("不使用"),
    KO("사용 안 함"),     DE("Ignorieren"),   FR("Ignorer"),       ES("Ignorar"),
    PT("Ignorar"),       IT("Ignora"),       NL("Negeren"),       RU("Не использовать"),
    TR("Yok say"));

SS_MSG(sfm_sensor_gauge_up,
    EN("Which way is up"),
    JA("上方向のみ"),
    ZH_HANS("仅确定上方向"),
    ZH_HANT("僅確定上方向"),
    KO("위쪽 방향만"),
    DE("Nur die Richtung nach oben"),
    FR("Seulement le haut"),
    ES("Solo la vertical"),
    PT("Apenas a vertical"),
    IT("Solo la verticale"),
    NL("Alleen welke kant boven is"),
    RU("Только направление вверх"),
    TR("Yalnızca yukarı yönü"));

SS_MSG(sfm_sensor_gauge_auto,
    EN("Up and metric scale"),
    JA("上方向と実寸"),
    ZH_HANS("上方向与实际尺度"),
    ZH_HANT("上方向與實際尺度"),
    KO("위쪽 방향과 실척"),
    DE("Oben und metrischer Maßstab"),
    FR("Haut et échelle métrique"),
    ES("Vertical y escala métrica"),
    PT("Vertical e escala métrica"),
    IT("Verticale e scala metrica"),
    NL("Boven en metrische schaal"),
    RU("Верх и масштаб в метрах"),
    TR("Yukarı yönü ve metrik ölçek"));

SS_MSG(sfm_sensor_gauge_help,
    EN("Use video IMU data to estimate up and accelerometer scale. GPS has its own setting.\n- Each estimate is checked separately. Panning or missing IMU can leave fewer usable estimates.\n- Choose Ignore if the video’s IMU data is wrong."),
    JA("動画が映像と一緒に記録しているIMU (ジャイロ、加速度計、記録された姿勢) を使い、上がどちらかと、加速度計からシーンが何メートルかを求めます。\n- それぞれ不確かさ付きで推定し、成り立たないものは捨てるので、向きを変えるだけの撮影やIMUのないファイルでは使われる情報が減るだけです。\n- 動画のGPSは上の設定です。 「使わない」は、IMUの記録が明らかに誤っているファイル向けです。"),
    ZH_HANS("使用视频随画面一同记录的 IMU (陀螺仪、加速度计和记录的姿态)：判断上方向，并由加速度计得出场景实际有多少米。\n- 每一项都带不确定度拟合，不成立时会被舍弃，所以只转动不移动的拍摄或没有 IMU 的文件只是可用信息更少。\n- 视频的 GPS 由上面的选项管。若已知某文件的 IMU 数据有误，可以选“不使用”。"),
    ZH_HANT("使用影片隨畫面一同記錄的 IMU (陀螺儀、加速度計和記錄的姿態)：判斷上方向，並由加速度計得出場景實際有多少公尺。\n- 每一項都帶不確定度擬合，不成立時會被捨棄，所以只轉動不移動的拍攝或沒有 IMU 的檔案只是可用資訊更少。\n- 影片的 GPS 由上面的選項管。若已知某檔案的 IMU 資料有誤，可以選「不使用」。"),
    KO("동영상이 영상과 함께 기록한 IMU (자이로, 가속도계, 기록된 자세) 를 씁니다. 어느 쪽이 위인지, 그리고 가속도계로 장면이 몇 미터인지를 구합니다.\n- 각각 불확실도와 함께 맞추고 맞지 않으면 버리므로, 방향만 돌리는 촬영이나 IMU 가 없는 파일은 쓰이는 정보가 줄어들 뿐입니다.\n- 동영상의 GPS 는 위의 설정입니다. '사용 안 함'은 IMU 기록이 틀린 것이 확실한 파일을 위한 것입니다."),
    DE("Nutzt die IMU, die ein Video neben den Bildern aufzeichnet -- Gyroskop, Beschleunigungsmesser und aufgezeichnete Lage: wo oben ist und, aus dem Beschleunigungsmesser, wie viele Meter die Szene misst. Jedes wird mit eigener Unsicherheit geschätzt und verworfen, wenn es nicht trägt;\n- eine Aufnahme, die nur schwenkt, oder eine Datei ohne IMU bekommt einfach weniger davon.\n- Das GPS des Videos ist die Einstellung darüber. Ignorieren ist für eine Datei, deren IMU nachweislich falsch ist."),
    FR("Utilise l'IMU qu'une vidéo enregistre avec ses images -- gyroscope, accéléromètre et attitude enregistrée : où est le haut et, par l'accéléromètre, combien de mètres fait la scène. Chacun est estimé avec son incertitude et abandonné s'il ne tient pas ; une prise qui ne fait que pivoter ou un fichier sans IMU en reçoit simplement moins.\nLe GPS de la vidéo est le réglage au-dessus. Ignorer convient à un fichier dont l'IMU est connue comme fausse."),
    ES("Usa la IMU que un vídeo graba junto a sus imágenes -- giroscopio, acelerómetro y actitud registrada: hacia dónde está arriba y, por el acelerómetro, cuántos metros mide la escena. Cada uno se ajusta con su incertidumbre y se descarta si no se sostiene;\n- una toma que solo gira o un archivo sin IMU simplemente recibe menos. El GPS del vídeo es el ajuste de arriba.\n- Ignorar es para un archivo cuya IMU se sabe errónea."),
    PT("Usa a IMU que um vídeo grava junto das imagens -- giroscópio, acelerómetro e atitude registada: para onde é cima e, pelo acelerómetro, quantos metros mede a cena.\n- Cada um é ajustado com a sua incerteza e descartado se não se sustentar;\n- uma captura que só roda ou um ficheiro sem IMU recebe apenas menos. O GPS do vídeo é a opção acima.\n- Ignorar serve para um ficheiro cuja IMU se sabe errada."),
    IT("Usa l'IMU che un video registra accanto alle immagini -- giroscopio, accelerometro e assetto registrato: dove è l'alto e, dall'accelerometro, quanti metri misura la scena.\n- Ciascuno è stimato con la propria incertezza e scartato se non regge; una ripresa che ruota soltanto o un file senza IMU ne riceve semplicemente meno.\n- Il GPS del video è l'impostazione qui sopra. Ignora serve per un file la cui IMU è notoriamente sbagliata."),
    NL("Gebruikt de IMU die een video naast de beelden opneemt -- gyroscoop, versnellingsmeter en opgenomen stand: welke kant boven is en, uit de versnellingsmeter, hoeveel meter de scène meet.\n- Elk wordt met eigen onzekerheid geschat en losgelaten als het niet standhoudt; een opname die alleen draait of een bestand zonder IMU krijgt er eenvoudig minder van.\n- De gps van de video is de instelling hierboven. Negeren is voor een bestand waarvan de IMU aantoonbaar fout is."),
    RU("Использует IMU, которую видео пишет рядом с кадрами, — гироскоп, акселерометр и записанную ориентацию: где верх и, по акселерометру, сколько метров занимает сцена. Каждое оценивается со своей неопределённостью и отбрасывается, если не подтверждается, поэтому съёмка, где камера только поворачивается, или файл без IMU просто получат меньше.\nGPS видео — настройка выше. «Не использовать» — для файла с заведомо неверными данными IMU."),
    TR("Bir videonun görüntülerin yanında kaydettiği IMU'yu -- jiroskop, ivmeölçer ve kaydedilen duruş -- kullanır: yukarının nerede olduğunu ve ivmeölçerden sahnenin kaç metre olduğunu.\n- Her biri kendi belirsizliğiyle kestirilir ve tutmazsa bırakılır; yalnızca dönen bir çekim ya da IMU'suz bir dosya daha azını alır. Videonun GPS'i yukarıdaki ayardır.\n- Yok say, IMU'su yanlış olduğu bilinen bir dosya içindir."));

SS_MSG(sfm_exif_attitude,
    EN("Photo attitude"),
    JA("写真の姿勢"),
    ZH_HANS("照片姿态"),
    ZH_HANT("相片姿態"),
    KO("사진의 자세"),
    DE("Lage der Fotos"),
    FR("Attitude des photos"),
    ES("Actitud de las fotos"),
    PT("Atitude das fotos"),
    IT("Assetto delle foto"),
    NL("Stand van de foto's"),
    RU("Ориентация фотографий"),
    TR("Fotoğrafların duruşu"));

SS_MSG(sfm_exif_attitude_auto,
    EN("Up and north"),
    JA("上方向と北"),
    ZH_HANS("上方向与北向"),
    ZH_HANT("上方向與北向"),
    KO("위쪽 방향과 북쪽"),
    DE("Oben und Norden"),
    FR("Haut et nord"),
    ES("Vertical y norte"),
    PT("Vertical e norte"),
    IT("Verticale e nord"),
    NL("Boven en noord"),
    RU("Верх и север"),
    TR("Yukarı yönü ve kuzey"));

SS_MSG(sfm_exif_attitude_help,
    EN("Use the drone’s recorded gimbal angles for up and north.\n- Without them, up comes from camera orientation. Downward views and inverted gimbals can mislead it.\n- Reconstruction rejects disagreeing photos or the full angle set when it conflicts.\n- Choose Ignore if the camera writes incorrect angles."),
    JA("ドローンが写真ごとに書き込むカメラ姿勢 (ジンバルのヨー・ピッチ・ロール) を使い、上方向と北を決めます。\n- これがないと上方向はカメラの構え方から推測しますが、真下を向いたカメラやジンバルで上下逆になったカメラでは外れます。\n- 再構成と食い違う写真は多数決で除かれ、全体が矛盾する場合は使いません。\n- 「使わない」は角度を誤って書くカメラ向けです。"),
    ZH_HANS("使用无人机写入每张照片的相机姿态 (云台的偏航、俯仰、横滚) 来确定上方向和北向。没有它时，上方向要根据相机的持握方式推测，而相机朝正下方、或被云台翻转过来时这种推测会失效。\n与重建不符的照片会在投票中被排除，整体矛盾时则不使用。 “不使用”适用于已知会写错角度的相机。"),
    ZH_HANT("使用無人機寫入每張相片的相機姿態 (雲台的偏航、俯仰、橫滾) 來確定上方向和北向。沒有它時，上方向要依相機的持握方式推測，而相機朝正下方、或被雲台翻轉過來時這種推測會失效。\n與重建不符的相片會在投票中被排除，整體矛盾時則不使用。 「不使用」適用於已知會寫錯角度的相機。"),
    KO("드론이 사진마다 기록하는 카메라 자세 (짐벌의 요·피치·롤) 로 위 방향과 북쪽을 정합니다. 이것이 없으면 위 방향은 카메라를 든 방식으로 추측하는데, 바로 아래를 보는 카메라나 짐벌이 위아래로 뒤집은 카메라에서는 틀립니다.\n- 재구성과 맞지 않는 사진은 다수결로 빠지고, 전체가 모순되면 쓰지 않습니다.\n- 사용 안 함은 각도를 잘못 기록하는 카메라를 위한 것입니다."),
    DE("Nutzt die Kameralage, die eine Drohne in jedes Foto schreibt -- Gier-, Nick- und Rollwinkel des Gimbals --, um oben und Norden zu bestimmen.\n- Ohne sie wird oben aus der Haltung der Kameras geschätzt, was bei einer senkrecht nach unten blickenden Kamera oder einer vom Gimbal auf den Kopf gedrehten versagt.\n- Fotos, die der Rekonstruktion widersprechen, werden überstimmt, und ein Satz, dem sie widerspricht, wird nicht verwendet. Ignorieren ist für eine Kamera, die bekanntermaßen falsche Winkel schreibt."),
    FR("Utilise l'attitude de caméra qu'un drone écrit dans chaque photo -- lacet, tangage et roulis de la nacelle -- pour fixer le haut et le nord.\nSans elle, le haut est deviné d'après la tenue des caméras, ce qui échoue pour une caméra tournée vers le sol ou retournée par sa nacelle. Les photos en désaccord avec la reconstruction sont mises en minorité, et un ensemble qu'elle contredit n'est pas utilisé. Ignorer convient à une caméra connue pour écrire de faux angles."),
    ES("Usa la actitud de cámara que un dron escribe en cada foto -- guiñada, cabeceo y alabeo del gimbal -- para fijar la vertical y el norte. Sin ella, la vertical se adivina por cómo se sostuvieron las cámaras, lo que falla con una cámara que mira hacia abajo o que su gimbal puso boca abajo.\n- Las fotos que no concuerdan con la reconstrucción quedan en minoría, y un conjunto que ella contradice no se usa.\n- Ignorar es para una cámara que se sabe que escribe ángulos erróneos."),
    PT("Usa a atitude de câmera que um drone escreve em cada foto -- guinada, arfagem e rolamento do gimbal -- para fixar a vertical e o norte.\n- Sem ela, a vertical é adivinhada pela forma como as câmeras foram seguras, o que falha com uma câmera virada para baixo ou que o gimbal pôs de cabeça para baixo.\n- As fotos que não concordam com a reconstrução ficam em minoria, e um conjunto que ela contradiz não é usado.\n- Ignorar é para uma câmera que se sabe escrever ângulos errados."),
    IT("Usa l'assetto della fotocamera che un drone scrive in ogni foto -- imbardata, beccheggio e rollio del gimbal -- per stabilire la verticale e il nord.\n- Senza, la verticale si indovina da come erano tenute le fotocamere, il che fallisce con una fotocamera rivolta verso il basso o capovolta dal gimbal.\n- Le foto in disaccordo con la ricostruzione vengono messe in minoranza, e un insieme che essa contraddice non viene usato. Ignora serve per una fotocamera nota per scrivere angoli sbagliati."),
    NL("Gebruikt de camerastand die een drone in elke foto schrijft -- gier-, stamp- en rolhoek van de gimbal -- om boven en noord te bepalen.\n- Zonder die wordt boven geraden uit hoe de camera's werden vastgehouden, wat mislukt bij een camera die recht naar beneden kijkt of die de gimbal op z'n kop heeft gedraaid.\n- Foto's die de reconstructie tegenspreken worden weggestemd, en een set die zij tegenspreekt wordt niet gebruikt. Negeren is voor een camera waarvan bekend is dat hij foute hoeken schrijft."),
    RU("Использует ориентацию камеры, которую дрон записывает в каждый снимок, — рыскание, тангаж и крен подвеса, — чтобы задать верх и север.\nБез неё верх угадывается по тому, как держали камеры, а это не срабатывает для камеры, смотрящей вниз, или перевёрнутой подвесом. Снимки, расходящиеся с реконструкцией, остаются в меньшинстве, а набор, которому она противоречит, не используется. «Не использовать» — для камеры, которая заведомо пишет неверные углы."),
    TR("Bir dronun her fotoğrafa yazdığı kamera duruşunu -- gimbalın sapma, yunuslama ve yatış açılarını -- kullanarak yukarıyı ve kuzeyi belirler. Bu olmadan yukarı, kameraların nasıl tutulduğundan tahmin edilir; bu da aşağı bakan ya da gimbalın ters çevirdiği bir kamerada başarısız olur.\n- Yeniden yapılandırmayla çelişen fotoğraflar oylamada azınlıkta kalır ve onun çeliştiği bir küme kullanılmaz.\n- Yok say, açıları yanlış yazdığı bilinen bir kamera içindir."));

SS_MSG(sensors_reading,
    EN("reading sensors..."),
    JA("センサーを読み取り中..."),
    ZH_HANS("正在读取传感器..."),
    ZH_HANT("正在讀取感測器..."),
    KO("센서 읽는 중..."),
    DE("Sensoren werden gelesen..."),
    FR("lecture des capteurs..."),
    ES("leyendo sensores..."),
    PT("a ler sensores..."),
    IT("lettura sensori..."),
    NL("sensoren lezen..."),
    RU("чтение датчиков..."),
    TR("sensörler okunuyor..."));

SS_MSG(sensors_none,
    EN("no sensor data"),
    JA("センサー記録なし"),
    ZH_HANS("无传感器数据"),
    ZH_HANT("無感測器資料"),
    KO("센서 기록 없음"),
    DE("keine Sensordaten"),
    FR("aucune donnée de capteur"),
    ES("sin datos de sensores"),
    PT("sem dados de sensores"),
    IT("nessun dato dei sensori"),
    NL("geen sensordata"),
    RU("нет данных датчиков"),
    TR("sensör verisi yok"));

SS_MSG(sensors_imu,
    EN("IMU"),           JA("IMU"),          ZH_HANS("IMU"),      ZH_HANT("IMU"),
    KO("IMU"),           DE("IMU"),          FR("IMU"),           ES("IMU"),
    PT("IMU"),           IT("IMU"),          NL("IMU"),           RU("IMU"),
    TR("IMU"));

SS_MSG(sensors_gps,
    EN("GPS"),           JA("GPS"),          ZH_HANS("GPS"),      ZH_HANT("GPS"),
    KO("GPS"),           DE("GPS"),          FR("GPS"),           ES("GPS"),
    PT("GPS"),           IT("GPS"),          NL("gps"),           RU("GPS"),
    TR("GPS"));

SS_MSG(sensors_photo_gps,
    EN("GPS in {0}/{1}"),
    JA("GPSあり {0}/{1}"),
    ZH_HANS("{0}/{1} 张有 GPS"),
    ZH_HANT("{0}/{1} 張有 GPS"),
    KO("GPS {0}/{1}장"),
    DE("GPS in {0}/{1}"),
    FR("GPS dans {0}/{1}"),
    ES("GPS en {0}/{1}"),
    PT("GPS em {0}/{1}"),
    IT("GPS in {0}/{1}"),
    NL("Gps in {0}/{1}"),
    RU("GPS в {0}/{1}"),
    TR("{0}/{1} dosyada GPS"));

SS_MSG(sensors_photo_attitude,
    EN("Attitude in {0}/{1}"),
    JA("姿勢あり {0}/{1}"),
    ZH_HANS("{0}/{1} 张有姿态"),
    ZH_HANT("{0}/{1} 張有姿態"),
    KO("자세 {0}/{1}장"),
    DE("Lage in {0}/{1}"),
    FR("Attitude dans {0}/{1}"),
    ES("Actitud en {0}/{1}"),
    PT("Atitude em {0}/{1}"),
    IT("Assetto in {0}/{1}"),
    NL("Stand in {0}/{1}"),
    RU("Ориентация в {0}/{1}"),
    TR("{0}/{1} dosyada duruş"));

SS_MSG(sensors_photo_attitude_help,
    EN("Attitude is which way the camera pointed -- the yaw, pitch and roll a drone records with each photo.\n- The reconstruction takes which way is up, and north, from it.\n- Off ignores it entirely; Advanced > Sensors sets how it is used when on."),
    JA("姿勢とは、ドローンが写真ごとに記録するカメラの向き (ヨー・ピッチ・ロール) です。\n- 再構成はここから上方向と北を決めます。\n- オフにすると完全に無視します。 オンのときの使い方は 詳細設定 > センサー で決めます。"),
    ZH_HANS("姿态是无人机随每张照片记录的相机朝向 (偏航、俯仰、横滚)。重建由它确定上方向和北向。关闭则完全忽略；开启时如何使用由 高级 > 传感器 决定。"),
    ZH_HANT("姿態是無人機隨每張相片記錄的相機朝向 (偏航、俯仰、橫滾)。重建由它確定上方向和北向。關閉則完全忽略；開啟時如何使用由 進階 > 感測器 決定。"),
    KO("드론 사진의 자세(요·피치·롤)로 위쪽과 북쪽을 정합니다. 끄면 무시합니다. 사용 방식은 고급 > 센서에서 정합니다."),
    DE("Die Lage ist, wohin die Kamera zeigte -- Gier-, Nick- und Rollwinkel, wie eine Drohne sie zu jedem Foto aufzeichnet.\n- Die Rekonstruktion nimmt daraus, wo oben und wo Norden ist.\n- Aus ignoriert sie ganz; wie sie eingeschaltet genutzt wird, legt Erweitert > Sensoren fest."),
    FR("Lacet, tangage et roulis des photos du drone donnent le haut et le nord. Désactivé : ignorés. Régler leur usage dans Avancé > Capteurs."),
    ES("La orientación de las fotos del dron (guiñada, cabeceo, alabeo) fija vertical y norte. Desactivada: se ignora. Uso en Avanzado > Sensores."),
    PT("A atitude das fotos do drone (guinada, arfagem, rolamento) dá vertical e norte. Desligada: ignorada. Uso em Avançado > Sensores."),
    IT("L’assetto delle foto del drone (imbardata, beccheggio, rollio) dà verticale e nord. Disattivato: ignorato. Uso in Avanzate > Sensori."),
    NL("De stand is waar de camera heen wees -- gier-, stamp- en rolhoek, zoals een drone die bij elke foto vastlegt.\n- De reconstructie haalt er boven en noord uit.\n- Uit negeert hem volledig; Geavanceerd > Sensoren bepaalt hoe hij aan wordt gebruikt."),
    RU("Ориентация — куда смотрела камера: рыскание, тангаж и крен, как дрон записывает их с каждым снимком.\n- Реконструкция берёт из неё верх и север.\n- Выключено — полностью игнорируется; как она используется во включённом виде, задаёт Дополнительно > Датчики."),
    TR("Duruş, kameranın nereye baktığıdır -- bir dronun her fotoğrafla kaydettiği sapma, yunuslama ve yatış açıları.\n- Yeniden yapılandırma yukarıyı ve kuzeyi buradan alır.\n- Kapalıyken tamamen yok sayılır; açıkken nasıl kullanılacağını Gelişmiş > Sensörler belirler."));

SS_MSG(sensors_carrier_tooltip,
    EN("Telemetry format: {0}"),
    JA("テレメトリ形式: {0}"),
    ZH_HANS("遥测格式：{0}"),
    ZH_HANT("遙測格式：{0}"),
    KO("텔레메트리 형식: {0}"),
    DE("Telemetrieformat: {0}"),
    FR("Format de télémétrie : {0}"),
    ES("Formato de telemetría: {0}"),
    PT("Formato de telemetria: {0}"),
    IT("Formato di telemetria: {0}"),
    NL("Telemetrieformaat: {0}"),
    RU("Формат телеметрии: {0}"),
    TR("Telemetri biçimi: {0}"));

SS_MSG(sensors_video_imu_help,
    EN("This video's IMU -- gyro, accelerometer and recorded orientation.\n- The reconstruction takes which way is up and the metric scale from it, and checks its matches against it.\n- Off ignores it entirely; Advanced > Sensors sets how it is used when on."),
    JA("この動画のIMU (ジャイロ、加速度計、記録された姿勢) です。\n- 再構成はここから上方向と実寸を求め、マッチの検証にも使います。\n- オフにすると完全に無視します。 オンのときの使い方は 詳細設定 > センサー で決めます。"),
    ZH_HANS("这个视频的 IMU (陀螺仪、加速度计和记录的姿态)。重建由它确定上方向和实际尺度，并用它检验匹配。关闭则完全忽略；开启时如何使用由 高级 > 传感器 决定。"),
    ZH_HANT("這個影片的 IMU (陀螺儀、加速度計和記錄的姿態)。重建由它確定上方向和實際尺度，並用它檢驗匹配。關閉則完全忽略；開啟時如何使用由 進階 > 感測器 決定。"),
    KO("동영상 IMU(자이로, 가속도계, 자세)로 위쪽·실척을 정하고 매칭을 검증합니다. 끄면 무시합니다. 사용 방식은 고급 > 센서에서 정합니다."),
    DE("Die IMU dieses Videos -- Gyroskop, Beschleunigungsmesser und aufgezeichnete Lage.\n- Die Rekonstruktion nimmt daraus, wo oben ist, und den metrischen Maßstab, und prüft ihre Zuordnungen daran.\n- Aus ignoriert sie ganz; wie sie eingeschaltet genutzt wird, legt Erweitert > Sensoren fest."),
    FR("L'IMU de cette vidéo -- gyroscope, accéléromètre et orientation enregistrée.\n- La reconstruction en tire le haut et l'échelle métrique, et y vérifie ses appariements.\n- Désactivée, elle est entièrement ignorée ; Avancé > Capteurs règle son usage quand elle est activée."),
    ES("La IMU de este vídeo: giroscopio, acelerómetro y orientación registrada.\n- La reconstrucción toma de ella la vertical y la escala métrica, y comprueba con ella sus correspondencias.\n- Desactivada, se ignora por completo; Avanzado > Sensores decide cómo se usa cuando está activada."),
    PT("A IMU deste vídeo: giroscópio, acelerómetro e orientação registada.\n- A reconstrução tira dela a vertical e a escala métrica, e verifica com ela as correspondências.\n- Desligada, é totalmente ignorada; Avançado > Sensores define como é usada quando ligada."),
    IT("L'IMU di questo video: giroscopio, accelerometro e orientamento registrato.\n- La ricostruzione ne ricava la verticale e la scala metrica, e vi verifica le corrispondenze.\n- Disattivata, viene ignorata del tutto; Avanzate > Sensori stabilisce come usarla quando è attiva."),
    NL("De IMU van deze video: gyroscoop, versnellingsmeter en vastgelegde stand.\n- De reconstructie haalt er boven en de metrische schaal uit, en toetst er haar overeenkomsten aan.\n- Uit negeert hem volledig; Geavanceerd > Sensoren bepaalt hoe hij aan wordt gebruikt."),
    RU("IMU этого видео — гироскоп, акселерометр и записанная ориентация.\n- Реконструкция берёт из неё направление вверх и масштаб в метрах и проверяет по ней сопоставления.\n- Выключено — полностью игнорируется; как она используется во включённом виде, задаёт Дополнительно > Датчики."),
    TR("Bu videonun IMU'su: jiroskop, ivmeölçer ve kaydedilen duruş.\n- Yeniden oluşturma yukarı yönünü ve metrik ölçeği bundan alır ve eşleşmelerini buna göre denetler.\n- Kapalıyken tamamen yok sayılır; açıkken nasıl kullanılacağını Gelişmiş > Sensörler belirler."));

SS_MSG(sensors_video_gps_help,
    EN("Uses GPS for scale, compass heading and map position. Keep on for outdoor captures tens of metres across; disable for small or indoor scenes."),
    JA("GPSでスケール、方位、地図位置を求めます。数十メートル以上の屋外撮影で使い、小規模・屋内のシーンでは無効にしてください。"),
    ZH_HANS("用 GPS 确定尺度、朝向和地图位置。适用于跨度数十米的室外拍摄；小场景或室内应关闭。"),
    ZH_HANT("用 GPS 確定尺度、朝向和地圖位置。適用於跨度數十公尺的室外拍攝；小場景或室內應關閉。"),
    KO("GPS로 크기·방위·지도 위치를 정합니다. 수십 미터 규모의 야외 촬영에서 켜고, 작은 장면이나 실내에서는 끄세요."),
    DE("Nutzt GPS für Maßstab, Himmelsrichtung und Kartenposition. Für Außenaufnahmen über mehrere zehn Meter; bei kleinen Szenen oder innen deaktivieren."),
    FR("Utilise le GPS pour échelle, cap et position. À garder pour les captures extérieures de dizaines de mètres ; désactiver en intérieur ou petites scènes."),
    ES("Usa GPS para escala, rumbo y posición. Déjalo en exteriores de decenas de metros; desactívalo en escenas pequeñas o interiores."),
    PT("Usa GPS para escala, rumo e posição. Mantenha em exteriores de dezenas de metros; desligue em cenas pequenas ou interiores."),
    IT("Usa GPS per scala, orientamento e posizione. Tenerlo per esterni di decine di metri; disattivarlo per scene piccole o interne."),
    NL("Gebruikt GPS voor schaal, richting en kaartpositie. Aan bij buitenopnamen van tientallen meters; uit bij kleine scènes of binnen."),
    RU("GPS задаёт масштаб, азимут и положение на карте. Для съёмки на улице размером в десятки метров; отключите для малых сцен и помещений."),
    TR("GPS ile ölçek, yön ve harita konumu belirler. Onlarca metrelik dış çekimlerde açık, küçük veya iç mekân sahnelerde kapalı tutun."));

SS_MSG(sensors_photo_gps_help,
    EN("Uses GPS for scale, compass heading and map position. Keep on for outdoor captures tens of metres across; disable for small or indoor scenes."),
    JA("GPSでスケール、方位、地図位置を求めます。数十メートル以上の屋外撮影で使い、小規模・屋内のシーンでは無効にしてください。"),
    ZH_HANS("用 GPS 确定尺度、朝向和地图位置。适用于跨度数十米的室外拍摄；小场景或室内应关闭。"),
    ZH_HANT("用 GPS 確定尺度、朝向和地圖位置。適用於跨度數十公尺的室外拍攝；小場景或室內應關閉。"),
    KO("GPS로 크기·방위·지도 위치를 정합니다. 수십 미터 규모의 야외 촬영에서 켜고, 작은 장면이나 실내에서는 끄세요."),
    DE("Nutzt GPS für Maßstab, Himmelsrichtung und Kartenposition. Für Außenaufnahmen über mehrere zehn Meter; bei kleinen Szenen oder innen deaktivieren."),
    FR("Utilise le GPS pour échelle, cap et position. À garder pour les captures extérieures de dizaines de mètres ; désactiver en intérieur ou petites scènes."),
    ES("Usa GPS para escala, rumbo y posición. Déjalo en exteriores de decenas de metros; desactívalo en escenas pequeñas o interiores."),
    PT("Usa GPS para escala, rumo e posição. Mantenha em exteriores de dezenas de metros; desligue em cenas pequenas ou interiores."),
    IT("Usa GPS per scala, orientamento e posizione. Tenerlo per esterni di decine di metri; disattivarlo per scene piccole o interne."),
    NL("Gebruikt GPS voor schaal, richting en kaartpositie. Aan bij buitenopnamen van tientallen meters; uit bij kleine scènes of binnen."),
    RU("GPS задаёт масштаб, азимут и положение на карте. Для съёмки на улице размером в десятки метров; отключите для малых сцен и помещений."),
    TR("GPS ile ölçek, yön ve harita konumu belirler. Onlarca metrelik dış çekimlerde açık, küçük veya iç mekân sahnelerde kapalı tutun."));

SS_MSG(sensors_off_in_advanced,
    EN("Turned off for every input under Advanced > Sensors: {0}."),
    JA("詳細設定 > センサー の「{0}」で、すべての入力に対してオフになっています。"),
    ZH_HANS("已在 高级 > 传感器 的“{0}”中对所有输入关闭。"),
    ZH_HANT("已在 進階 > 感測器 的「{0}」中對所有輸入關閉。"),
    KO("고급 > 센서의 '{0}'에서 모든 입력에 대해 꺼져 있습니다."),
    DE("Unter Erweitert > Sensoren für alle Eingaben ausgeschaltet: {0}."),
    FR("Désactivé pour toutes les entrées dans Avancé > Capteurs : {0}."),
    ES("Desactivado para todas las entradas en Avanzado > Sensores: {0}."),
    PT("Desligado para todas as entradas em Avançado > Sensores: {0}."),
    IT("Disattivato per tutti gli ingressi in Avanzate > Sensori: {0}."),
    NL("Voor alle invoer uitgeschakeld onder Geavanceerd > Sensoren: {0}."),
    RU("Выключено для всех входов в разделе Дополнительно > Датчики: {0}."),
    TR("Gelişmiş > Sensörler altında tüm girdiler için kapalı: {0}."));

SS_MSG(sensors_unused_colmap,
    EN("COLMAP reads no sensor data; only the built-in reconstruction uses it."),
    JA("COLMAP はセンサーデータを読みません。使うのは内蔵の再構成だけです。"),
    ZH_HANS("COLMAP 不读取传感器数据，只有内置重建会使用。"),
    ZH_HANT("COLMAP 不讀取感測器資料，只有內建重建會使用。"),
    KO("COLMAP은 센서 데이터를 읽지 않습니다. 내장 재구성만 사용합니다."),
    DE("COLMAP liest keine Sensordaten; nur die eingebaute Rekonstruktion nutzt "
       "sie."),
    FR("COLMAP ne lit aucune donnée de capteur ; seule la reconstruction "
       "intégrée les utilise."),
    ES("COLMAP no lee datos de sensores; solo la reconstrucción integrada los "
       "usa."),
    PT("O COLMAP não lê dados de sensores; só a reconstrução integrada os usa."),
    IT("COLMAP non legge i dati dei sensori; li usa solo la ricostruzione "
       "integrata."),
    NL("COLMAP leest geen sensordata; alleen de ingebouwde reconstructie "
       "gebruikt ze."),
    RU("COLMAP не читает данные датчиков; их использует только встроенная "
       "реконструкция."),
    TR("COLMAP sensör verisi okumaz; onu yalnızca yerleşik yeniden oluşturma "
       "kullanır."));

SS_MSG(sensors_section_caption,
    EN("How the sensors left on in the input rows are used. Off here turns that "
       "sensor off for every input."),
    JA("入力の各行でオンのままのセンサーをどう使うかです。ここでオフにすると、"
       "そのセンサーはすべての入力でオフになります。"),
    ZH_HANS("在各输入行中保持开启的传感器如何使用。在这里关闭，则该传感器对所有"
            "输入都关闭。"),
    ZH_HANT("在各輸入列中保持開啟的感測器如何使用。在這裡關閉，則該感測器對所有"
            "輸入都關閉。"),
    KO("입력 행에서 켜 둔 센서를 어떻게 쓸지입니다. 여기서 끄면 그 센서는 모든 "
       "입력에서 꺼집니다."),
    DE("Wie die in den Eingabezeilen eingeschalteten Sensoren genutzt werden. "
       "Aus schaltet den Sensor hier für alle Eingaben ab."),
    FR("Comment sont utilisés les capteurs laissés actifs dans les lignes "
       "d'entrée. Désactiver ici coupe ce capteur pour toutes les entrées."),
    ES("Cómo se usan los sensores que quedan activados en las filas de entrada. "
       "Desactivarlo aquí lo apaga para todas las entradas."),
    PT("Como são usados os sensores deixados ligados nas linhas de entrada. "
       "Desligar aqui desliga esse sensor para todas as entradas."),
    IT("Come si usano i sensori lasciati attivi nelle righe degli ingressi. "
       "Disattivarlo qui lo spegne per tutti gli ingressi."),
    NL("Hoe de sensoren die in de invoerrijen aan staan worden gebruikt. Hier "
       "uitzetten schakelt die sensor voor alle invoer uit."),
    RU("Как используются датчики, оставленные включёнными в строках входов. "
       "Выключение здесь отключает датчик для всех входов."),
    TR("Girdi satırlarında açık bırakılan sensörlerin nasıl kullanılacağı. "
       "Burada kapatmak o sensörü tüm girdiler için kapatır."));

SS_MSG(sfm_metric_gps_help,
    EN("Fit a metric model to video GPS or photo EXIF. The capture should span tens of metres.\n- Latitude/longitude sets scale and heading. IMU or cameras still determine up.\n- Altitude also levels the model. Phone altitude errors can tilt it.\n- Auto uses altitude for DJI drone GPS or EXIF with altitude, horizontal for other fixes, and off without GPS.\n- Skips images without fixes. Failed alignment is reported and saves an unscaled model."),
    JA("GPS (動画が記録する軌跡、または写真の EXIF にある位置) に合わせて、寸法と向きを決めたメートル単位のモデルを書き出します。緯度と経度だけを使うのが安全です。位置から寸法と方位だけを取り、どちらが上かは動画のIMUかカメラ自身に任せます。\n- 高度も使うと傾きまでGPS で決めますが、スマートフォンの高度は誤差が大きく、市街地ではモデル全体が数度傾くことがあります。いずれの場合も撮影範囲は数十メートル必要です。\n- 位置を持たない画像は対象外となり、当てはめられないモデルは寸法なしで書き出してその旨を伝えます。\n- 自動 (既定) は撮影ごとに決めます。\n- DJI の機体自身の GPS や高度付きの写真なら高度も使い、スマートフォン、アクションカメラ、高度のない位置なら緯度と経度だけ、GPS がなければ使いません。"),
    ZH_HANS("按 GPS (视频记录的轨迹，或照片 EXIF 中的位置) 确定尺度和朝向，以米为单位写出模型。只用经纬度更稳妥：只从定位取尺度和方位角，哪边朝上仍交给视频的 IMU 或相机自身判断。\n- 连高度一起用则连倾斜也由 GPS 决定，而手机测得的高度误差很大——在城市里可能让整个模型倾斜几度。\n- 两种方式都要求采集范围有几十米。\n- 不带定位的图像会被略过；拟合不成功时按未定尺度写出并给出说明。自动 (默认) 按每次采集决定：\n- DJI 设备自身的 GPS 或带高度的照片连高度一起用，手机、运动相机或没有高度的定位只用经纬度，没有 GPS 则不使用。"),
    ZH_HANT("按 GPS (影片記錄的軌跡，或相片 EXIF 中的位置) 確定尺度和朝向，以公尺為單位寫出模型。只用經緯度更穩妥：只從定位取尺度和方位角，哪邊朝上仍交給影片的 IMU 或相機自身判斷。\n- 連高度一起用則連傾斜也由 GPS 決定，而手機測得的高度誤差很大——在城市裡可能讓整個模型傾斜幾度。\n- 兩種方式都要求拍攝範圍有數十公尺。\n- 不帶定位的影像會被略過；擬合不成功時按未定尺度寫出並給出說明。自動 (預設) 按每次拍攝決定：\n- DJI 裝置自身的 GPS 或帶高度的照片連高度一起用，手機、運動相機或沒有高度的定位只用經緯度，沒有 GPS 則不使用。"),
    KO("GPS (동영상이 기록한 궤적, 또는 사진 EXIF 의 위치) 에 맞춰 크기와 방향을 정한 미터 단위 모델을 씁니다. 위도와 경도만 쓰는 쪽이 안전합니다. 위치에서 크기와 방위만 가져오고, 어느 쪽이 위인지는 동영상의 IMU 나 카메라 자신에게 맡깁니다.\n- 고도까지 쓰면 기울기도 GPS 로 정하는데, 휴대전화의 고도는 오차가 커서 도심에서는 모델 전체가 몇 도 기울 수 있습니다.\n- 어느 쪽이든 촬영 범위가 수십 미터는 되어야 합니다.\n- 위치가 없는 이미지는 건너뛰고, 맞추지 못한 모델은 크기 없이 쓰며 그 사실을 알립니다.\n- 자동 (기본값) 은 촬영마다 정합니다. DJI 기체 자체의 GPS 나 고도가 있는 사진은 고도까지, 휴대전화나 액션캠, 고도 없는 위치는 위도와 경도만 쓰고, GPS 가 없으면 쓰지 않습니다."),
    DE("Das Modell in Metern schreiben, in Größe und Richtung an das GPS angepasst: die Spur, die ein Video aufzeichnet, oder die Position in den EXIF-Daten eines Fotos.\n- Breite und Länge allein ist die sichere Wahl: sie nehmen Maßstab und Himmelsrichtung aus den Positionen und überlassen das Oben der IMU des Videos oder den Kameras selbst. Mit der Höhe richtet auch das GPS die Szene aus, das ein Telefon schlecht misst -- in der Stadt kann das das ganze Modell um Grade kippen.\n- In beiden Fällen muss die Aufnahme zehner Meter groß sein.\n- Bilder ohne Position werden übergangen; ein Modell, das nicht passt, wird unskaliert geschrieben und sagt das.\n- Automatisch, die Vorgabe, entscheidet je Aufnahme: mit Höhe für das eigene GPS einer DJI-Drohne oder Fotos, deren GPS eine Höhe trägt, Breite und Länge für ein Telefon, eine Actionkamera oder Positionen ohne Höhe, und aus, wo nichts GPS trägt."),
    FR("Écrire le modèle en mètres, dimensionné et orienté d'après le GPS : la trace qu'enregistre une vidéo, ou la position dans les EXIF d'une photo.\n- La latitude et la longitude seules sont le choix sûr :\n- elles prennent l'échelle et le cap dans les positions et laissent le haut à l'IMU de la vidéo ou aux caméras elles-mêmes. Avec l'altitude, le GPS redresse aussi la scène, or un téléphone la mesure mal -- en ville cela peut incliner tout le modèle de plusieurs degrés.\n- Dans les deux cas la prise doit faire des dizaines de mètres. Les images sans position sont ignorées ; un modèle qui ne s'ajuste pas est écrit sans échelle et le signale.\n- Automatique, le défaut, décide par prise : avec l'altitude pour le GPS propre d'un drone DJI ou des photos dont le GPS porte une altitude, latitude et longitude pour un téléphone, une caméra d'action ou des points sans altitude, et désactivé quand rien ne porte de GPS."),
    ES("Escribir el modelo en metros, con el tamaño y el giro que da el GPS: la traza que graba un vídeo o la posición en los EXIF de una foto.\n- Solo latitud y longitud es la opción segura: toma la escala y el rumbo de las posiciones y deja el arriba a la IMU del vídeo o a las propias cámaras. Con la altitud el GPS también nivela la escena, y un teléfono la mide mal:\n- en ciudad puede inclinar el modelo entero varios grados. En ambos casos la toma debe medir decenas de metros.\n- Las imágenes sin posición se pasan por alto; un modelo que no se puede ajustar se escribe sin escalar y lo dice.\n- Automático, el valor por defecto, decide en cada toma: con la altitud para el GPS propio de un dron DJI o fotos cuyo GPS trae altitud, latitud y longitud para un teléfono, una cámara de acción o posiciones sin altitud, y desactivado cuando nada trae GPS."),
    PT("Escrever o modelo em metros, dimensionado e virado conforme o GPS: o trajeto que um vídeo grava ou a posição nos EXIF de uma foto. Só latitude e longitude é a escolha segura:\n- tira a escala e o rumo das posições e deixa o para cima à IMU do vídeo ou às próprias câmeras.\n- Com a altitude o GPS também nivela a cena, e um telemóvel mede-a mal -- na cidade pode inclinar o modelo inteiro em graus. Em qualquer dos casos a captura tem de ter dezenas de metros. As imagens sem posição são ignoradas;\n- um modelo que não se ajusta é escrito sem escala e avisa disso. Automático, o padrão, decide em cada captura:\n- com a altitude para o GPS próprio de um drone DJI ou fotos cujo GPS traz altitude, latitude e longitude para um telemóvel, uma câmara de ação ou posições sem altitude, e desligado quando nada traz GPS."),
    IT("Scrivere il modello in metri, dimensionato e ruotato in base al GPS: la traccia che un video registra o la posizione negli EXIF di una foto.\n- Solo latitudine e longitudine è la scelta sicura: prende scala e direzione dalle posizioni e lascia l'alto all'IMU del video o alle camere stesse. Con la quota anche l'inclinazione viene dal GPS, che un telefono misura male:\n- in città può inclinare l'intero modello di gradi. In entrambi i casi la ripresa deve misurare decine di metri.\n- Le immagini senza posizione vengono ignorate; un modello che non si stima viene scritto senza scala e lo segnala.\n- Automatico, il predefinito, decide per ogni ripresa: con la quota per il GPS di un drone DJI o per foto il cui GPS porta la quota, latitudine e longitudine per un telefono, una action cam o punti senza quota, e disattivato quando nulla porta GPS."),
    NL("Het model in meters schrijven, op maat en gedraaid volgens de gps: het spoor dat een video opneemt, of de positie in de EXIF van een foto.\n- Alleen breedte en lengte is de veilige keuze: die halen de schaal en de kompasrichting uit de posities en laten het boven aan de IMU van de video of aan de camera's zelf.\n- Met de hoogte zet de gps de scène ook waterpas, en die meet een telefoon slecht -- in een stad kan dat het hele model graden doen kantelen. In beide gevallen moet de opname tientallen meters groot zijn.\n- Beelden zonder positie worden overgeslagen; een model dat niet past wordt ongeschaald geschreven en meldt dat.\n- Automatisch, de standaard, beslist per opname: met hoogte voor de eigen gps van een DJI-drone of foto's waarvan de gps een hoogte draagt, breedte en lengte voor een telefoon, een actiecamera of posities zonder hoogte, en uit waar niets gps draagt."),
    RU("Записать модель в метрах, с размером и поворотом по GPS: по треку, который пишет видео, или по координатам в EXIF снимка. Только широта и долгота — безопасный выбор:\n- масштаб и направление берутся из координат, а где верх, решают IMU видео или сами камеры.\n- С высотой по GPS задаётся и наклон, а телефон измеряет её плохо — в городе это может наклонить всю модель на градусы. В обоих случаях съёмка должна быть десятки метров. Изображения без координат пропускаются;\n- модель, которую подобрать не удалось, пишется без масштаба и сообщает об этом.\n- Автоматически (по умолчанию) — решение для каждой съёмки: с высотой для собственного GPS дрона DJI или снимков, чей GPS несёт высоту, широта и долгота для телефона, экшн-камеры или отсчётов без высоты, и выключено, если GPS нигде нет."),
    TR("Modeli GPS'e göre ölçeklenmiş ve döndürülmüş olarak metre biriminde yaz: bir videonun kaydettiği iz ya da bir fotoğrafın EXIF'indeki konum.\n- Yalnızca enlem ve boylam güvenli seçimdir:\n- ölçeği ve pusula yönünü konumlardan alır, yukarının neresi olduğunu videonun IMU'suna ya da kameralara bırakır.\n- Yükseklik de eklenirse sahneyi GPS düzler; telefonun yükseklik ölçümü kötüdür ve şehirde tüm modeli derecelerce yatırabilir. Her iki durumda da çekim onlarca metre olmalı.\n- Konumu olmayan görüntüler atlanır; oturtulamayan model ölçeksiz yazılır ve bunu bildirir. Varsayılan Otomatik, her çekim için karar verir: bir DJI dronun kendi GPS'i ya da GPS'i yükseklik taşıyan fotoğraflar için yükseklikle, telefon, aksiyon kamerası ya da yüksekliksiz konumlar için enlem ve boylamla, hiçbir şey GPS taşımıyorsa kapalı."));

SS_MSG(rig_none,
    EN("No rig"), JA("リグなし"), ZH_HANS("无装置"), ZH_HANT("無裝置"), KO("리그 없음"),
    DE("Kein Rig"), FR("Pas de rig"), ES("Sin rig"), PT("Sem rig"), IT("Nessun rig"),
    NL("Geen rig"), RU("Без рига"), TR("Rig yok"));

SS_MSG(rig_own,
    EN("This input's lenses"), JA("この入力のレンズ"), ZH_HANS("此输入的镜头"),
    ZH_HANT("此輸入的鏡頭"), KO("이 입력의 렌즈"), DE("Die Objektive dieser Eingabe"),
    FR("Les objectifs de cette entrée"), ES("Las lentes de esta entrada"),
    PT("As lentes desta entrada"), IT("Gli obiettivi di questo ingresso"),
    NL("De lenzen van deze invoer"), RU("Объективы этого входа"), TR("Bu girdinin lensleri"));

SS_MSG(rig_dual_fisheye,
    EN("Rig {0}: {1} and {2} are the two lenses of one dual-fisheye camera"),
    JA("リグ {0}: {1} と {2} は 1 台のデュアル魚眼カメラの 2 つのレンズ"),
    ZH_HANS("装置 {0}：{1} 和 {2} 是同一台双鱼眼相机的两个镜头"),
    ZH_HANT("裝置 {0}：{1} 和 {2} 是同一台雙魚眼相機的兩個鏡頭"),
    KO("리그 {0}: {1} 와 {2} 는 한 듀얼 어안 카메라의 두 렌즈"),
    DE("Rig {0}: {1} und {2} sind die zwei Objektive einer Dual-Fisheye-Kamera"),
    FR("Rig {0} : {1} et {2} sont les deux objectifs d'une caméra double fisheye"),
    ES("Rig {0}: {1} y {2} son las dos lentes de una cámara de doble ojo de pez"),
    PT("Rig {0}: {1} e {2} são as duas lentes de uma câmera olho de peixe dupla"),
    IT("Rig {0}: {1} e {2} sono i due obiettivi di una fotocamera doppio fisheye"),
    NL("Rig {0}: {1} en {2} zijn de twee lenzen van één dual-fisheyecamera"),
    RU("Риг {0}: {1} и {2} -- два объектива одной камеры с двумя «рыбьими глазами»"),
    TR("Düzenek {0}: {1} ve {2}, tek bir çift balıkgözü kameranın iki merceği"));

SS_MSG(rig_dual_fisheye_help,
    EN("Use for dual-fisheye frames in two folders with matching names and capture times.\n- Starts with opposing lenses. Refines a small rotation and separation along their common axis.\n- Dual-fisheye video is detected automatically."),
    JA("デュアル魚眼の 360 カメラ（Insta360、DJI Osmo 360 など）のフレームで、2 つのレンズが 2 つのフォルダーに、レンズごとに同じファイル名で書き出されている場合に使います。\n- 再構成は、測定を待たずにレンズが正反対を向いている状態から始め、両者の違いを小さな回転と共通の軸に沿った距離だけに制限します。 2 つのフォルダーのフレームが同じ瞬間に撮られていない場合はオフのままにしてください。\n- こうしたカメラの動画はこれがなくても認識されます。"),
    ZH_HANS("用于双鱼眼 360 相机（Insta360、大疆 Osmo 360 等）的帧：两个镜头分别写入两个文件夹，每个镜头一张图、文件名相同。\n- 重建会直接从两个镜头朝向相反开始，而不是等着测量，并且只允许两者相差一个小旋转和沿公共轴的距离。\n- 如果两个文件夹的帧不是同一时刻拍的，请不要勾选。这类相机的视频无需勾选也会被识别。"),
    ZH_HANT("用於雙魚眼 360 相機（Insta360、DJI Osmo 360 等）的影格：兩個鏡頭分別寫入兩個資料夾，每個鏡頭一張圖、檔名相同。\n- 重建會直接從兩個鏡頭朝向相反開始，而不是等著測量，並且只允許兩者相差一個小旋轉和沿共同軸的距離。\n- 如果兩個資料夾的影格不是同一時刻拍的，請不要勾選。這類相機的影片無需勾選也會被辨識。"),
    KO("듀얼 어안 360 카메라(Insta360, DJI Osmo 360 등)의 프레임으로, 두 렌즈가 두 폴더에 렌즈마다 같은 파일 이름으로 저장된 경우에 씁니다.\n- 재구성은 측정을 기다리지 않고 렌즈가 서로 반대를 향한 상태에서 시작하며, 둘의 차이를 작은 회전과 공통 축을 따른 거리로만 제한합니다.\n- 두 폴더의 프레임이 같은 순간에 찍히지 않았다면 끄세요. 이런 카메라의 동영상은 이것 없이도 인식됩니다."),
    DE("Für Bilder einer Dual-Fisheye-360-Kamera (Insta360, DJI Osmo 360 und ähnliche), deren zwei Objektive in zwei Ordner geschrieben wurden, ein Bild pro Objektiv mit gleichen Dateinamen. Die Rekonstruktion beginnt dann mit entgegengesetzt blickenden Objektiven, statt das erst zu messen, und erlaubt nur eine kleine Drehung und den Abstand entlang der gemeinsamen Achse zwischen ihnen.\nAus lassen, wenn die Bilder der beiden Ordner nicht zu denselben Zeitpunkten entstanden. Ein Video einer solchen Kamera wird auch ohne dies erkannt."),
    FR("Pour des images d'une caméra 360 double fisheye (Insta360, DJI Osmo 360 et semblables) dont les deux objectifs ont été écrits dans deux dossiers, une image par objectif avec les mêmes noms de fichiers.\n- La reconstruction part alors d'objectifs regardant en sens opposé au lieu d'attendre de le mesurer, et ne leur laisse qu'une petite rotation et la distance le long de leur axe commun. Laisser décoché si les images des deux dossiers n'ont pas été prises aux mêmes instants.\n- Une vidéo d'une telle caméra est reconnue sans cela."),
    ES("Para fotogramas de una cámara 360 de doble ojo de pez (Insta360, DJI Osmo 360 y similares) cuyas dos lentes se escribieron en dos carpetas, una imagen por lente con los mismos nombres de archivo.\n- La reconstrucción parte entonces de lentes que miran en sentidos opuestos en lugar de esperar a medirlo, y solo les deja una pequeña rotación y la distancia a lo largo de su eje común. Déjelo desactivado si los fotogramas de las dos carpetas no se tomaron en los mismos instantes.\n- Un vídeo de esa cámara se reconoce sin esto."),
    PT("Para quadros de uma câmera 360 olho de peixe dupla (Insta360, DJI Osmo 360 e semelhantes) cujas duas lentes foram gravadas em duas pastas, uma imagem por lente com os mesmos nomes de arquivo.\n- A reconstrução parte então de lentes olhando em sentidos opostos em vez de esperar para medir isso, e só lhes deixa uma pequena rotação e a distância ao longo do eixo comum.\n- Deixe desligado se os quadros das duas pastas não foram tirados nos mesmos instantes.\n- Um vídeo de uma câmera assim é reconhecido sem isto."),
    IT("Per fotogrammi di una fotocamera 360 doppio fisheye (Insta360, DJI Osmo 360 e simili) i cui due obiettivi sono stati scritti in due cartelle, un'immagine per obiettivo con gli stessi nomi di file.\nLa ricostruzione parte allora da obiettivi rivolti in versi opposti invece di aspettare di misurarlo, e lascia loro solo una piccola rotazione e la distanza lungo l'asse comune. Lasciare spento se i fotogrammi delle due cartelle non sono stati presi negli stessi istanti. Un video di una tale fotocamera è riconosciuto anche senza."),
    NL("Voor beelden van een dual-fisheye-360-camera (Insta360, DJI Osmo 360 en dergelijke) waarvan de twee lenzen naar twee mappen zijn geschreven, één beeld per lens met dezelfde bestandsnamen. De reconstructie begint dan met lenzen die tegengesteld kijken in plaats van dat eerst te meten, en staat alleen een kleine draaiing en de afstand langs hun gemeenschappelijke as toe. Uit laten als de beelden van de twee mappen niet op dezelfde momenten zijn gemaakt.\nEen video van zo'n camera wordt ook zonder dit herkend."),
    RU("Для кадров двухобъективной 360-камеры с «рыбьими глазами» (Insta360, DJI Osmo 360 и подобные), у которой два объектива записаны в две папки, по снимку на объектив с одинаковыми именами файлов. Реконструкция тогда сразу исходит из того, что объективы смотрят в противоположные стороны, а не ждёт, пока это будет измерено, и допускает между ними лишь небольшой поворот и расстояние вдоль общей оси.\nНе включайте, если кадры двух папок сняты не в одни и те же моменты. Видео с такой камеры распознаётся и без этого."),
    TR("Çift balıkgözü bir 360 kameranın (Insta360, DJI Osmo 360 ve benzerleri) iki merceği iki klasöre, mercek başına aynı dosya adlarıyla bir görüntü olarak yazılmış kareleri için.\n- Yeniden oluşturma bunu ölçmeyi beklemek yerine merceklerin zıt yönlere baktığı durumdan başlar ve aralarında yalnızca küçük bir dönmeye ve ortak eksen boyunca mesafeye izin verir.\n- İki klasörün kareleri aynı anlarda çekilmediyse kapalı bırakın. Böyle bir kameranın videosu bu olmadan da tanınır."));

SS_MSG(rig_help,
    EN("A rig fixes the lenses’ relative poses and shares one pose per frame.\n- A multi-lens file is its own rig.\n- Rows with the same letter pair frames by filename. Multiple inputs can share a rig when they supply the same lenses."),
    JA("同じリグ上のレンズは相対姿勢が固定で、再構成はそれを利用します。フレームごとに1 姿勢、空を向いたレンズも隣のレンズから配置されます。複数レンズのファイルはそれ自体がリグです。\n- 同じ文字を選んだ行はファイル名で 1 つのリグになります。\n- 各入力が同じレンズを持つなら、複数の動画の背後にある 1 つのリグとして扱います。"),
    ZH_HANS("同一装置上的镜头保持固定的相对位姿，重建会利用这一点：每帧一个位姿，朝天的镜头由相邻镜头定位。\n多镜头文件自成一个装置；选同一字母的行按文件名组成一个装置——各输入镜头相同时，视作多段视频背后的同一装置。"),
    ZH_HANT("同一裝置上的鏡頭保持固定的相對姿態，重建會利用這一點：每幀一個姿態，朝天的鏡頭由相鄰鏡頭定位。\n多鏡頭檔案自成一個裝置；選同一字母的列按檔名組成一個裝置——各輸入鏡頭相同時，視作多段影片背後的同一裝置。"),
    KO("한 리그의 렌즈들은 상대 자세가 고정되어 있고 재구성은 그것을 이용합니다. 프레임마다 자세 하나, 하늘을 향한 렌즈도 이웃이 배치합니다.\n- 다중 렌즈 파일은 그 자체가 리그이고, 같은 글자를 고른 행은 파일 이름으로 하나의 리그가 됩니다.\n- 각 입력이 같은 렌즈를 내면 여러 영상 뒤의 한 리그로 봅니다."),
    DE("Objektive eines Rigs behalten eine feste relative Pose, und die Rekonstruktion nutzt das: eine Pose je Frame, ein Objektiv zum Himmel von seinem Nachbarn platziert.\n- Eine Mehrlinsendatei ist ihr eigenes Rig;\n- Zeilen mit demselben Buchstaben bilden nach Dateiname ein Rig -- über Eingaben hinweg als ein Rig hinter mehreren Videos, wenn jede dieselben Objektive beisteuert."),
    FR("Les objectifs d'un même rig gardent une pose relative fixe, et la reconstruction s'en sert : une pose par image, un objectif vers le ciel placé par son voisin.\nUn fichier multi-objectifs est son propre rig ; les lignes partageant une lettre forment un rig par nom de fichier -- entre entrées comme un seul rig derrière plusieurs vidéos, quand chacune apporte les mêmes objectifs."),
    ES("Las lentes de un mismo rig mantienen una pose relativa fija y la reconstrucción lo aprovecha: una pose por cuadro, una lente hacia el cielo colocada por su vecina.\n- Un archivo multilente es su propio rig;\n- las filas que comparten una letra forman un rig por nombre de archivo -- entre entradas como un solo rig tras varios vídeos, cuando cada una aporta las mismas lentes."),
    PT("As lentes de um mesmo rig mantêm uma pose relativa fixa e a reconstrução usa isso: uma pose por quadro, uma lente virada ao céu posicionada pela vizinha.\n- Um ficheiro multilente é o seu próprio rig;\n- linhas que partilham uma letra formam um rig por nome de ficheiro -- entre entradas como um só rig por trás de vários vídeos, quando cada uma traz as mesmas lentes."),
    IT("Gli obiettivi di uno stesso rig mantengono una posa relativa fissa e la ricostruzione lo sfrutta: una posa per fotogramma, un obiettivo verso il cielo posizionato dal vicino. Un file multi-obiettivo è un rig a sé;\nle righe che condividono una lettera formano un rig per nome di file -- tra ingressi come un solo rig dietro più video, quando ciascuno porta gli stessi obiettivi."),
    NL("Lenzen op één rig houden een vaste relatieve pose en de reconstructie gebruikt dat: één pose per frame, een lens naar de lucht geplaatst door zijn buur.\n- Een bestand met meerdere lenzen is zijn eigen rig;\n- rijen met dezelfde letter vormen één rig op bestandsnaam -- over invoeren heen als één rig achter meerdere video's, wanneer elke dezelfde lenzen levert."),
    RU("Объективы одного рига сохраняют фиксированную относительную позу, и реконструкция этим пользуется: одна поза на кадр, объектив в небо размещается по соседу.\n- Файл с несколькими объективами -- сам себе риг;\n- строки с одной буквой образуют риг по имени файла -- между входами как один риг за несколькими видео, когда каждый даёт те же объективы."),
    TR("Bir rigdeki lensler sabit göreli duruşu korur ve yeniden kurulum bunu kullanır: kare başına bir duruş, gökyüzüne bakan lens komşusunca yerleştirilir.\n- Çok lensli bir dosya kendi rigidir;\n- aynı harfi paylaşan satırlar dosya adına göre tek rig olur -- her girdi aynı lensleri veriyorsa girdiler arasında birkaç videonun ardındaki tek rig olarak."));

SS_MSG(rig_guess,
    EN("Guess rigs from folder names"), JA("フォルダ名からリグを推定"),
    ZH_HANS("按文件夹名推断装置"), ZH_HANT("按資料夾名稱推斷裝置"),
    KO("폴더 이름으로 리그 추정"), DE("Rigs aus Ordnernamen erraten"),
    FR("Deviner les rigs d'après les noms de dossier"),
    ES("Deducir los rigs por el nombre de las carpetas"),
    PT("Deduzir os rigs pelos nomes das pastas"),
    IT("Dedurre i rig dai nomi delle cartelle"),
    NL("Rigs afleiden uit mapnamen"), RU("Определить риги по именам папок"),
    TR("Düzenekleri klasör adlarından tahmin et"));

SS_MSG(rig_guess_help,
    EN("Group folders with matching filenames and a single name difference, such as left/right or cam0/cam1.\n- Assigns one letter per rig and clears other rig assignments.\n- Runs automatically when adding folders if none is assigned."),
    JA("名前が 1 か所だけ異なり（left と right、cam0 と cam1）、画像のファイル名が共通する写真フォルダを 1 つのリグにまとめ、リグごとに文字を割り当てます。\n- それ以外のフォルダはリグから外します。\n- フォルダを追加したとき、まだどれもリグに入っていなければ自動で 1 回行われます。"),
    ZH_HANS("把名称只有一处不同（left 与 right、cam0 与 cam1）且图像文件名相同的照片文件夹归入同一装置，每个装置一个字母，其余文件夹移出装置。\n添加文件夹时若还没有任何文件夹属于装置，会自动执行一次。"),
    ZH_HANT("把名稱只有一處不同（left 與 right、cam0 與 cam1）且影像檔名相同的照片資料夾歸入同一裝置，每個裝置一個字母，其餘資料夾移出裝置。\n新增資料夾時若還沒有任何資料夾屬於裝置，會自動執行一次。"),
    KO("이름이 한 부분만 다르고(left 와 right, cam0 와 cam1) 이미지 파일 이름이 같은 사진 폴더들을 한 리그로 묶고 리그마다 글자를 붙이며, 나머지 폴더는 리그에서 뺍니다.\n폴더를 추가했을 때 아직 리그에 든 폴더가 없으면 한 번 자동으로 실행됩니다."),
    DE("Legt Fotoordner, deren Namen sich nur in einem Teil unterscheiden (left und right, cam0 und cam1) und deren Bilder dieselben Dateinamen tragen, auf ein Rig, einen Buchstaben je Rig, und nimmt alle anderen Ordner von ihrem Rig.\nGeschieht einmal von selbst, wenn Ordner hinzukommen und noch keiner auf einem Rig liegt."),
    FR("Place sur un même rig les dossiers de photos dont les noms ne diffèrent que d'une partie (left et right, cam0 et cam1) et dont les images portent les mêmes noms de fichier, une lettre par rig, et retire les autres dossiers de leur rig.\nFait automatiquement une fois quand des dossiers sont ajoutés et qu'aucun n'est encore sur un rig."),
    ES("Pone en un mismo rig las carpetas de fotos cuyos nombres difieren en una sola parte (left y right, cam0 y cam1) y cuyas imágenes comparten nombre de archivo, una letra por rig, y saca del rig a las demás carpetas.\nSe hace solo una vez al añadir carpetas si ninguna está aún en un rig."),
    PT("Coloca num mesmo rig as pastas de fotos cujos nomes diferem numa só parte (left e right, cam0 e cam1) e cujas imagens partilham nomes de ficheiro, uma letra por rig, e tira as restantes pastas do seu rig.\nÉ feito sozinho uma vez ao adicionar pastas, se nenhuma estiver ainda num rig."),
    IT("Mette su uno stesso rig le cartelle di foto i cui nomi differiscono in una sola parte (left e right, cam0 e cam1) e le cui immagini hanno gli stessi nomi di file, una lettera per rig, e toglie le altre cartelle dal loro rig.\nAvviene da solo una volta quando si aggiungono cartelle e nessuna è ancora su un rig."),
    NL("Zet fotomappen waarvan de namen in één deel verschillen (left en right, cam0 en cam1) en waarvan de beelden dezelfde bestandsnamen hebben op één rig, een letter per rig, en haalt alle andere mappen van hun rig.\nGebeurt één keer vanzelf wanneer mappen worden toegevoegd en er nog geen op een rig staat."),
    RU("Объединяет в один риг папки с фото, имена которых отличаются только одной частью (left и right, cam0 и cam1), а снимки имеют одинаковые имена файлов, по букве на риг, и снимает остальные папки с их ригов.\nВыполняется само один раз при добавлении папок, если ни одна ещё не на риге."),
    TR("Adları yalnızca bir kısımda farklı olan (left ve right, cam0 ve cam1) ve görüntüleri aynı dosya adlarını taşıyan fotoğraf klasörlerini tek bir düzeneğe koyar, her düzeneğe bir harf verir ve diğer klasörleri düzeneklerinden çıkarır.\nKlasörler eklendiğinde henüz hiçbiri bir düzenekte değilse bir kez kendiliğinden yapılır."));

SS_MSG(sync_lenses,
    EN("Synchronize lenses"), JA("レンズを同期"), ZH_HANS("同步镜头"), ZH_HANT("同步鏡頭"),
    KO("렌즈 동기화"), DE("Objektive synchronisieren"), FR("Synchroniser les objectifs"),
    ES("Sincronizar lentes"), PT("Sincronizar lentes"), IT("Sincronizza gli obiettivi"),
    NL("Lenzen synchroniseren"), RU("Синхронизировать объективы"), TR("Lensleri eşzamanla"));

SS_MSG(sync_lenses_help,
    EN("Keep the same instants from every lens of a dual-fisheye file (one sharpness window over both), so every frame is a rig frame.\nOff, each lens keeps its own sharpest frame and only the coincidences form rig frames. Built-in decoder only."),
    JA("デュアル魚眼ファイルの全レンズで同じ瞬間を残します（シャープさの判定窓は両方共通）。\n- すべてのフレームがリグフレームになります。\n- オフなら各レンズが自分のいちばん鮮明なフレームを残し、偶然一致したものだけがリグフレームになります。\n- 内蔵デコーダのみ。"),
    ZH_HANS("双鱼眼文件的每个镜头保留相同时刻（清晰度窗口对两者共用），这样每一帧都是装置帧。关闭时各镜头保留各自最清晰的帧，只有恰好重合的才构成装置帧。仅内置解码器。"),
    ZH_HANT("雙魚眼檔案的每個鏡頭保留相同時刻（清晰度視窗對兩者共用），這樣每一幀都是裝置幀。關閉時各鏡頭保留各自最清晰的幀，只有恰好重合的才構成裝置幀。僅內建解碼器。"),
    KO("이중 어안 파일의 모든 렌즈에서 같은 순간을 남깁니다(선명도 창은 둘에 하나).\n- 그러면 모든 프레임이 리그 프레임이 됩니다.\n- 끄면 렌즈마다 제일 선명한 프레임을 따로 남기고 우연히 겹친 것만 리그 프레임이 됩니다. 내장 디코더에서만."),
    DE("Aus jedem Objektiv einer Dual-Fisheye-Datei dieselben Augenblicke behalten (ein Schärfefenster über beide), damit jedes Bild ein Rig-Frame ist.\n- Aus: jedes Objektiv behält sein schärfstes Bild, und nur die Zufallstreffer bilden Rig-Frames.\n- Nur mit dem eingebauten Decoder."),
    FR("Synchronise les deux objectifs d’un fichier double fisheye. Décodeur intégré uniquement.\n- Activé : une fenêtre de netteté commune garde les mêmes instants pour le rig.\n- Désactivé : chaque objectif choisit son image nette. Seules les coïncidences forment le rig."),
    ES("Conservar los mismos instantes de cada lente de un archivo doble ojo de pez (una ventana de nitidez sobre ambas), para que cada cuadro sea un cuadro de rig.\n- Apagado, cada lente conserva su propio cuadro más nítido y solo las coincidencias forman cuadros de rig.\n- Solo con el decodificador integrado."),
    PT("Guardar os mesmos instantes de cada lente de um ficheiro de duplo olho de peixe (uma janela de nitidez sobre ambas), para que cada quadro seja um quadro de rig.\n- Desligado, cada lente guarda o seu quadro mais nítido e só as coincidências formam quadros de rig.\n- Apenas com o descodificador integrado."),
    IT("Tenere gli stessi istanti da ogni obiettivo di un file dual fisheye (una finestra di nitidezza su entrambi), così ogni fotogramma è un fotogramma di rig.\n- Spento, ogni obiettivo tiene il proprio fotogramma più nitido e solo le coincidenze formano fotogrammi di rig.\n- Solo con il decoder integrato."),
    NL("Dezelfde momenten van elke lens van een dual-fisheye-bestand houden (één scherptevenster over beide), zodat elk frame een rigframe is.\n- Uit houdt elke lens zijn eigen scherpste frame en alleen de toevalstreffers vormen rigframes.\n- Alleen met de ingebouwde decoder."),
    RU("Сохранять одни и те же мгновения с каждого объектива двойного фишая (одно окно резкости на оба), чтобы каждый кадр был кадром рига.\n- Выкл.: каждый объектив оставляет свой самый резкий кадр, и лишь совпадения образуют кадры рига.\n- Только со встроенным декодером."),
    TR("Çift balıkgözü dosyasının her lensinden aynı anları tut (iki lens için tek keskinlik penceresi);\n- böylece her kare bir rig karesi olur.\n- Kapalıyken her lens kendi en keskin karesini tutar ve yalnızca çakışanlar rig karesi olur.\n- Yalnızca yerleşik çözücüyle."));

SS_MSG(sfm_final_free_rig,
    EN("Release the rig at the end"), JA("最後にリグを解放"), ZH_HANS("最后解除装置约束"),
    ZH_HANT("最後解除裝置約束"), KO("마지막에 리그 해제"), DE("Rig am Ende freigeben"),
    FR("Libérer le rig à la fin"), ES("Liberar el rig al final"), PT("Liberar o rig no fim"),
    IT("Rilascia il rig alla fine"), NL("Rig aan het eind loslaten"),
    RU("Освободить риг в конце"), TR("Sonunda rigi serbest bırak"));

SS_MSG(sfm_final_free_rig_help,
    EN("Final bundle adjustment gives each image its own pose, ignoring the rig. For flexible mounts or unsynchronized lenses. Off keeps rig constraints."),
    JA("最後のバンドル調整でリグを無視し、各画像の姿勢を個別に調整します。たわむマウントや非同期レンズ用です。オフはリグ制約を保ちます。"),
    ZH_HANS("最终束调整忽略相机组约束，独立优化每图位姿。适用于支架变形或镜头不同步。关闭则保留相机组约束。"),
    ZH_HANT("最終束調整忽略相機組約束，獨立最佳化每圖位姿。適用於支架變形或鏡頭不同步。關閉則保留相機組約束。"),
    KO("최종 번들 조정에서 리그를 무시하고 이미지별 포즈를 풉니다. 휘는 마운트나 비동기 렌즈용입니다. 끄면 리그 제약을 유지합니다."),
    DE("Letzte Bündelung gibt jedem Bild eine eigene Pose ohne Rig. Für flexible Halter oder asynchrone Objektive. Aus behält Rig-Bedingungen."),
    FR("L’ajustement final donne une pose par image sans le rig. Pour supports flexibles ou objectifs désynchronisés. Désactivé conserve le rig."),
    ES("El ajuste final da pose propia a cada imagen, sin rig. Para monturas flexibles o lentes desincronizadas. Desactivado conserva el rig."),
    PT("O ajuste final dá pose própria a cada imagem, sem rig. Para suportes flexíveis ou lentes dessincronizadas. Desligado mantém o rig."),
    IT("L’aggiustamento finale dà una posa a ogni immagine senza rig. Per supporti flessibili o lenti asincrone. Spento mantiene i vincoli del rig."),
    NL("Eindbundeling geeft elk beeld een eigen pose zonder rig. Voor buigende houders of niet-synchrone lenzen. Uit behoudt rigbeperkingen."),
    RU("Финальное уравнивание даёт каждому снимку свою позу без рига. Для гибких креплений или несинхронных линз. Выкл. сохраняет ограничения рига."),
    TR("Son demet ayarı, düzeneği yok sayıp her resme ayrı poz verir. Esnek yuva veya eşzamanlı olmayan mercekler için. Kapalıyken düzenek kısıtları kalır."));

SS_MSG(sfm_per_image_intrinsics,
    EN("Per-image intrinsics at the end"),
    JA("最後に画像ごとの内部パラメータ"),
    ZH_HANS("最后改为逐图像内参"),
    ZH_HANT("最後改為逐影像內參"),
    KO("마지막에 이미지별 내부 파라미터"),
    DE("Am Ende innere Orientierung je Bild"),
    FR("Paramètres internes par image à la fin"),
    ES("Parámetros internos por imagen al final"),
    PT("Parâmetros internos por imagem no fim"),
    IT("Parametri interni per immagine alla fine"),
    NL("Interne parameters per beeld aan het eind"),
    RU("В конце — внутренние параметры по кадрам"),
    TR("Sonda görüntü başına iç parametreler"));

SS_MSG(sfm_per_image_intrinsics_help,
    EN("After shared-camera reconstruction, refines focal length and distortion per image. Follows zoom/focus drift but makes the final solve much larger."),
    JA("カメラ共有での再構成後、画像ごとに焦点距離と歪みを調整します。ズームやフォーカスのずれを補いますが、最終計算が大きくなります。"),
    ZH_HANS("共享相机重建后，逐图优化焦距和畸变。可跟随变焦或对焦漂移，但最终求解规模大得多。"),
    ZH_HANT("共用相機重建後，逐圖最佳化焦距和畸變。可跟隨變焦或對焦漂移，但最終求解規模大得多。"),
    KO("공유 카메라 재구성 후 이미지별 초점 거리와 왜곡을 조정합니다. 줌·초점 변화를 따르지만 최종 풀이 규모가 커집니다."),
    DE("Verfeinert nach gemeinsamer Kamera-Rekonstruktion Brennweite und Verzeichnung je Bild. Folgt Zoom-/Fokusdrift, vergrößert aber die letzte Lösung stark."),
    FR("Après reconstruction à caméras partagées, affine focale et distorsion par image. Suit les dérives de zoom ou mise au point ; calcul final bien plus gros."),
    ES("Tras reconstruir con cámaras compartidas, afina focal y distorsión por imagen. Sigue cambios de zoom/enfoque, pero agranda mucho el cálculo final."),
    PT("Após reconstruir com câmaras partilhadas, refina focal e distorção por imagem. Segue mudanças de zoom/foco, mas aumenta muito o cálculo final."),
    IT("Dopo la ricostruzione con camere condivise, affina focale e distorsione per immagine. Segue zoom/fuoco, ma ingrandisce molto il calcolo finale."),
    NL("Verfijnt na reconstructie met gedeelde camera’s brandpuntsafstand en vervorming per beeld. Volgt zoom/focusdrift, maar maakt de eindberekening veel groter."),
    RU("После реконструкции с общими камерами уточняет фокус и дисторсию по снимкам. Следует дрейфу зума/фокуса, но сильно увеличивает финальную задачу."),
    TR("Ortak kameralı modelden sonra resim başına odak ve bozulma ayarlar. Yakınlaştırma/odak kaymasını izler, ancak son çözümü çok büyütür."));

SS_MSG(max_features_auto,
    EN("Max features per image (0 = auto)"),
    JA("画像あたりの特徴数の上限（0 で自動）"),
    ZH_HANS("每张图像的最大特征数（0 = 自动）"),
    ZH_HANT("每張影像的最大特徵數（0 = 自動）"),
    KO("이미지당 최대 특징점 수(0 = 자동)"),
    DE("Höchstzahl Merkmale je Bild (0 = automatisch)"),
    FR("Points max par image (0 = auto)"),
    ES("Características máximas por imagen (0 = automático)"),
    PT("Máximo de características por imagem (0 = automático)"),
    IT("Caratteristiche massime per immagine (0 = automatico)"),
    NL("Max. kenmerken per beeld (0 = automatisch)"),
    RU("Предел особых точек на снимок (0 — авто)"),
    TR("Görüntü başına en çok öznitelik (0 = otomatik)"));

SS_MSG(max_features_auto_help,
    EN("Keypoints per image; nonzero overrides quality. SIFT keeps largest scales, learned detectors highest scores. SIFT needs more points than ALIKED."),
    JA("画像ごとの特徴点数です。0以外は品質設定を上書きします。SIFTは大きいスケール、学習検出器は高スコアを優先し、SIFTはALIKEDより多く必要です。"),
    ZH_HANS("每图关键点数，非零时覆盖质量预设。SIFT 优先大尺度，学习检测器优先高分；SIFT 比 ALIKED 需要更多点。"),
    ZH_HANT("每圖關鍵點數，非零時覆蓋品質預設。SIFT 優先大尺度，學習偵測器優先高分；SIFT 比 ALIKED 需要更多點。"),
    KO("이미지당 키포인트 수입니다. 0 외 값은 품질을 덮어씁니다. SIFT는 큰 스케일, 학습 검출기는 높은 점수를 우선하며 SIFT가 ALIKED보다 많이 필요합니다."),
    DE("Punkte je Bild; ungleich 0 ersetzt die Qualität. SIFT priorisiert große Skalen, gelernte Detektoren hohe Scores. SIFT braucht mehr Punkte als ALIKED."),
    FR("Points par image ; non nul remplace la qualité. SIFT privilégie les grandes échelles, les détecteurs appris les scores. SIFT exige plus de points qu’ALIKED."),
    ES("Puntos por imagen; distinto de 0 anula la calidad. SIFT prioriza escalas grandes, detectores aprendidos puntuación alta. SIFT requiere más puntos que ALIKED."),
    PT("Pontos por imagem; não zero substitui a qualidade. SIFT prioriza escalas grandes, detetores aprendidos pontuações altas. SIFT exige mais pontos que ALIKED."),
    IT("Punti per immagine; diverso da 0 sostituisce la qualità. SIFT privilegia scale grandi, i rilevatori appresi punteggi alti. SIFT richiede più punti di ALIKED."),
    NL("Punten per beeld; niet nul vervangt kwaliteit. SIFT kiest grote schalen, geleerde detectoren hoge scores. SIFT vereist meer punten dan ALIKED."),
    RU("Точки на снимок; не 0 заменяет качество. SIFT берёт крупные масштабы, обученные детекторы — высокие оценки. SIFT нужно больше точек, чем ALIKED."),
    TR("Resim başına nokta; 0 dışı kaliteyi geçersiz kılar. SIFT büyük ölçekleri, öğrenilmiş dedektörler yüksek puanları tutar. SIFT, ALIKED’den çok nokta ister."));

SS_MSG(max_image_size_auto,
    EN("Max image size (0 = auto)"),
    JA("画像サイズの上限（0 で自動）"),
    ZH_HANS("最大图像尺寸（0 = 自动）"),
    ZH_HANT("最大影像尺寸（0 = 自動）"),
    KO("최대 이미지 크기(0 = 자동)"),
    DE("Maximale Bildgröße (0 = automatisch)"),
    FR("Taille d'image max (0 = auto)"),
    ES("Tamaño máximo de imagen (0 = automático)"),
    PT("Tamanho máximo da imagem (0 = automático)"),
    IT("Dimensione massima immagine (0 = automatico)"),
    NL("Max. beeldgrootte (0 = automatisch)"),
    RU("Предел размера изображения (0 — авто)"),
    TR("En büyük görüntü boyutu (0 = otomatik)"));

SS_MSG(max_image_size_auto_help,
    EN("Longest edge the feature extractor runs on; bigger images are downscaled first. Keypoints are still reported in the source image's pixels."),
    JA("特徴抽出を行う長辺の長さです。これより大きい画像は先に縮小されます。キーポイントの座標は元画像のピクセルで報告されます。"),
    ZH_HANS("特征提取所用的最长边长度；更大的图像会先缩小。关键点坐标仍以源图像的像素给出。"),
    ZH_HANT("特徵擷取所用的最長邊長度；更大的影像會先縮小。關鍵點座標仍以來源影像的像素給出。"),
    KO("특징 추출을 수행하는 긴 변의 길이입니다. 그보다 큰 이미지는 먼저 축소됩니다. 키포인트 좌표는 여전히 원본 이미지의 픽셀로 보고됩니다."),
    DE("Längste Kante, auf der die Merkmalsextraktion läuft; größere Bilder werden zuvor verkleinert. Schlüsselpunkte werden weiterhin in Pixeln des Ausgangsbildes angegeben."),
    FR("Plus grand côté sur lequel l'extraction de points s'exécute ; les images plus grandes sont d'abord réduites. Les points clés restent exprimés en pixels de l'image source."),
    ES("Lado más largo sobre el que se ejecuta la extracción de características; las imágenes mayores se reducen antes.\nLos puntos clave se siguen dando en píxeles de la imagen de origen."),
    PT("Maior lado sobre o qual a extração de características roda; imagens maiores são reduzidas antes. Os pontos-chave continuam em pixels da imagem de origem."),
    IT("Lato massimo per estrarre caratteristiche; riduce prima le immagini più grandi. Le coordinate dei punti restano in pixel dell’immagine sorgente."),
    NL("Langste zijde waarop de kenmerkextractie draait; grotere beelden worden eerst verkleind. Sleutelpunten worden nog steeds in pixels van het bronbeeld gegeven."),
    RU("Наибольшая сторона, на которой работает выделение особых точек; изображения крупнее сначала уменьшаются. Координаты точек всё равно даются в пикселях исходного изображения."),
    TR("Öznitelik çıkarımının çalıştığı en uzun kenar; daha büyük görüntüler önce küçültülür. Anahtar noktalar yine kaynak görüntünün pikselleri cinsinden bildirilir."));

SS_MSG(flip_found_masks,
    EN("They mark what to remove"),
    JA("取り除く側が塗られている"),
    ZH_HANS("蒙版画的是要去掉的部分"),
    ZH_HANT("遮罩畫的是要去掉的部分"),
    KO("지울 부분이 칠해져 있음"),
    DE("Sie zeichnen das zu Entfernende"),
    FR("Ils peignent ce qui est à retirer"),
    ES("Marcan lo que se quita"),
    PT("Marcam o que se remove"),
    IT("Segnano ciò che va tolto"),
    NL("Ze markeren wat weg moet"),
    RU("Они отмечают удаляемое"),
    TR("Kaldırılacak yeri işaretliyorlar"));

SS_MSG(flip_found_masks_help,
    EN("Enable if existing masks are white over areas to exclude. Normally white means used pixels. Generated masks always use normal polarity."),
    JA("既存マスクが除外領域を白で示す場合に有効にします。通常は白が使用領域です。生成するマスクは常に通常の極性です。"),
    ZH_HANS("已有蒙版用白色表示排除区域时启用。通常白色表示使用的像素。新生成的蒙版始终采用正常极性。"),
    ZH_HANT("已有遮罩用白色表示排除區域時啟用。通常白色表示使用的像素。新產生的遮罩始終採用正常極性。"),
    KO("기존 마스크가 제외 영역을 흰색으로 나타내면 켜세요. 보통 흰색은 사용 픽셀입니다. 생성된 마스크는 항상 정상 극성을 씁니다."),
    DE("Aktivieren, wenn vorhandene Masken Ausschlüsse weiß zeigen. Normalerweise heißt weiß: genutzt. Erzeugte Masken haben immer normale Polarität."),
    FR("Activer si les masques existants sont blancs sur les exclusions. Normalement blanc signifie utilisé. Les masques générés gardent la polarité normale."),
    ES("Activa si las máscaras existentes usan blanco para excluir. Normalmente blanco significa usar. Las máscaras generadas mantienen la polaridad normal."),
    PT("Ative se as máscaras existentes usam branco para excluir. Normalmente branco significa usar. As máscaras geradas mantêm a polaridade normal."),
    IT("Attivare se le maschere esistenti indicano in bianco le esclusioni. Di solito bianco significa usato. Le maschere generate hanno polarità normale."),
    NL("Schakel in als bestaande maskers uitsluitingen wit tonen. Normaal betekent wit: gebruikt. Gemaakte maskers hebben altijd normale polariteit."),
    RU("Включите, если белое в существующих масках означает исключение. Обычно белое означает используемые пиксели. Новые маски имеют обычную полярность."),
    TR("Mevcut maskelerde beyaz alanlar dışlanacaksa açın. Normalde beyaz, kullanılan piksellerdir. Üretilen maskeler daima normal kutupludur."));

SS_MSG(photo_import,
    EN("Photos into the dataset"),
    JA("写真をデータセットへ"),
    ZH_HANS("照片进入数据集的方式"),
    ZH_HANT("照片進入資料集的方式"),
    KO("사진을 데이터셋으로"),
    DE("Fotos in den Datensatz"),
    FR("Photos vers le jeu de données"),
    ES("Fotos hacia el conjunto de datos"),
    PT("Fotos para o conjunto de dados"),
    IT("Foto verso il set di dati"),
    NL("Foto's naar de dataset"),
    RU("Фотографии в набор данных"),
    TR("Fotoğraflar veri kümesine"));

SS_MSG(photo_import_help,
    EN("Import mode for photos. The first three keep an images/ folder for reopening without the source path. Videos always write frames into the dataset."),
    JA("写真の取り込み方式です。最初の3方式はimages/を保持し、元パスなしで再度開けます。動画のフレームは常にデータセット内に書きます。"),
    ZH_HANS("照片导入方式。前三种保留 images/ 文件夹，重新打开无需源路径。视频帧始终写入数据集。"),
    ZH_HANT("照片匯入方式。前三種保留 images/ 資料夾，重新開啟無需來源路徑。影片影格始終寫入資料集。"),
    KO("사진 가져오기 방식입니다. 처음 세 방식은 images/를 유지해 원본 경로 없이 다시 열 수 있습니다. 영상 프레임은 항상 데이터셋에 기록합니다."),
    DE("Fotoimport. Die ersten drei Modi behalten images/ zum Öffnen ohne Quellpfad. Videobilder werden immer in den Datensatz geschrieben."),
    FR("Import des photos. Les trois premiers modes gardent images/ pour rouvrir sans chemin source. Les vidéos écrivent toujours dans le jeu de données."),
    ES("Importación de fotos. Los tres primeros modos guardan images/ para reabrir sin ruta fuente. Los vídeos siempre escriben fotogramas en el dataset."),
    PT("Importação de fotos. Os três primeiros modos guardam images/ para reabrir sem a origem. Os vídeos escrevem sempre fotogramas no conjunto de dados."),
    IT("Importazione foto. I primi tre modi mantengono images/ per riaprire senza percorso sorgente. I video scrivono sempre fotogrammi nel dataset."),
    NL("Fotoimport. De eerste drie modi bewaren images/ om zonder bronpad te heropenen. Videobeelden worden altijd in de dataset geschreven."),
    RU("Импорт фото. Первые три режима хранят images/ для открытия без исходного пути. Кадры видео всегда пишутся в набор данных."),
    TR("Fotoğraf aktarımı. İlk üç mod, kaynak yol olmadan açmak için images/ tutar. Videolar kareleri daima veri kümesine yazar."));

SS_MSG(photo_import_convert,
    EN("Copy, re-encoded as JPEG"),
    JA("コピーして JPEG に再エンコード"),
    ZH_HANS("复制并重新编码为 JPEG"),
    ZH_HANT("複製並重新編碼為 JPEG"),
    KO("복사하고 JPEG로 다시 인코딩"),
    DE("Kopieren, als JPEG neu kodiert"),
    FR("Copier, réencodées en JPEG"),
    ES("Copiar, recodificadas como JPEG"),
    PT("Copiar, recodificadas como JPEG"),
    IT("Copiare, ricodificate in JPEG"),
    NL("Kopiëren, opnieuw gecodeerd als JPEG"),
    RU("Копировать, перекодировав в JPEG"),
    TR("Kopyala, JPEG olarak yeniden kodla"));

SS_MSG(photo_import_convert_help,
    EN("Convert PNG and BMP to JPEG quality 95 for smaller files and faster training reads.\n- Saves alpha as a separate mask. Transparent areas are black in that mask.\n- Copies 16-bit, EXR and existing JPEG files unchanged."),
    JA("PNG と BMP を品質 95 で再エンコードします。ディスク上で数分の一になり、毎エポックの読み込みも速くなります。\n- アルファチャンネルは JPEG に入れられず、飾りではなく切り抜きなので、写真のとなりにマスクとして書き出します — 透明だったところが黒です。\n- 16 ビットと EXR はそのままコピーし、もともと JPEG の写真は再エンコードしません。"),
    ZH_HANS("把 PNG 和 BMP 以质量 95 重新编码，占用的磁盘小上几倍，每一轮读取也更快。 alpha 通道装不进 JPEG，而且它是抠像而非装饰，因此改写成照片旁边的掩码，原先透明的地方为黑。\n16 位和 EXR 原样复制，本来就是 JPEG 的照片不会再编码一次。"),
    ZH_HANT("把 PNG 和 BMP 以品質 95 重新編碼，佔用的磁碟小上幾倍，每一輪讀取也更快。 alpha 通道裝不進 JPEG，而且它是去背而非裝飾，因此改寫成照片旁邊的遮罩，原先透明的地方為黑。\n16 位元和 EXR 原樣複製，本來就是 JPEG 的照片不會再編碼一次。"),
    KO("PNG와 BMP를 품질 95로 다시 인코딩합니다. 디스크에서 몇 배 작아지고 매 에포크의 읽기도 빨라집니다.\n- 알파 채널은 JPEG에 담을 수 없고 장식이 아니라 오려낸 자리이므로, 사진 옆에 마스크로 씁니다.\n- 투명했던 곳이 검정입니다. 16비트와 EXR은 그대로 복사하며, 이미 JPEG인 사진은 다시 인코딩하지 않습니다."),
    DE("PNG und BMP werden mit Qualität 95 neu kodiert, was auf der Platte um ein Mehrfaches kleiner ist und sich in jeder Epoche schneller liest.\n- Ein Alphakanal passt in kein JPEG und ist ein Freisteller, keine Zierde, also wird er stattdessen als Maske neben das Foto geschrieben, schwarz, wo er durchsichtig war.\n- 16 Bit und EXR werden unverändert kopiert, und ein Foto, das schon JPEG ist, wird nie neu kodiert."),
    FR("Les PNG et les BMP sont réencodés en qualité 95, ce qui occupe plusieurs fois moins de disque et se lit plus vite à chaque époque.\n- Un canal alpha ne tient pas dans un JPEG et c'est un détourage, pas une décoration : il est donc écrit à côté de la photo comme masque, noir là où il était transparent.\n- Le 16 bits et l'EXR sont copiés tels quels, et une photo déjà en JPEG n'est jamais réencodée."),
    ES("Los PNG y los BMP se recodifican con calidad 95, lo que ocupa varias veces menos disco y se lee más rápido en cada época.\n- Un canal alfa no cabe en un JPEG y es un recorte, no un adorno, así que se escribe junto a la foto como máscara, negro donde era transparente.\n- El 16 bits y el EXR se copian sin cambios, y una foto que ya es JPEG no se recodifica nunca."),
    PT("Os PNG e os BMP são recodificados com qualidade 95, o que ocupa várias vezes menos disco e se lê mais depressa a cada época.\n- Um canal alfa não cabe num JPEG e é um recorte, não um enfeite, por isso é escrito ao lado da foto como máscara, preto onde era transparente.\n- O 16 bits e o EXR são copiados sem mudanças, e uma foto que já é JPEG nunca é recodificada."),
    IT("I PNG e i BMP vengono ricodificati a qualità 95: occupano parecchie volte meno disco e si leggono più in fretta a ogni epoca.\n- Un canale alfa non entra in un JPEG ed è un ritaglio, non un ornamento, quindi viene scritto accanto alla foto come maschera, nero dov'era trasparente.\n- Il 16 bit e l'EXR si copiano invariati, e una foto già JPEG non si ricodifica mai."),
    NL("PNG en BMP worden opnieuw gecodeerd op kwaliteit 95, wat meerdere malen minder schijf kost en elke epoch sneller leest.\n- Een alfakanaal past niet in een JPEG en is een uitsnede, geen versiering, dus het wordt naast de foto als masker weggeschreven, zwart waar het doorzichtig was.\n- 16 bits en EXR worden onveranderd gekopieerd, en een foto die al JPEG is wordt nooit opnieuw gecodeerd."),
    RU("PNG и BMP перекодируются с качеством 95: на диске в несколько раз меньше, и каждая эпоха читает их быстрее.\n- Альфа-канал в JPEG не помещается, и это вырезка, а не украшение, поэтому он пишется рядом с фотографией маской — чёрной там, где было прозрачно.\n- 16 бит и EXR копируются без изменений, а фото, уже бывшее JPEG, не перекодируется никогда."),
    TR("PNG ve BMP dosyaları 95 kalitesinde yeniden kodlanır: diskte birkaç kat küçük olur ve her turda daha hızlı okunur.\n- Alfa kanalı bir JPEG'e sığmaz ve süs değil bir kesimdir, bu yüzden fotoğrafın yanına maske olarak yazılır;\n- saydam olduğu yerde siyahtır.\n- 16 bit ve EXR olduğu gibi kopyalanır, zaten JPEG olan bir fotoğraf ise hiç yeniden kodlanmaz."));

SS_MSG(photo_import_copy,
    EN("Copy them"),
    JA("コピーする"),
    ZH_HANS("复制"),
    ZH_HANT("複製"),
    KO("복사"),
    DE("Kopieren"),
    FR("Les copier"),
    ES("Copiarlas"),
    PT("Copiá-las"),
    IT("Copiarle"),
    NL("Kopiëren"),
    RU("Копировать"),
    TR("Kopyala"));

SS_MSG(photo_import_copy_help,
    EN("Preserves file contents. Uses hard links where possible; otherwise copies, including across devices."),
    JA("ファイル内容を保持します。可能ならハードリンクを使い、別デバイスなど対応しない場合はコピーします。"),
    ZH_HANS("保持文件内容不变。优先使用硬链接；跨设备等不支持时复制。"),
    ZH_HANT("保持檔案內容不變。優先使用硬連結；跨裝置等不支援時複製。"),
    KO("파일 내용을 유지합니다. 가능하면 하드 링크를 쓰고, 장치 간 이동 등 불가능한 경우 복사합니다."),
    DE("Erhält den Dateiinhalt. Nutzt wenn möglich Hardlinks; sonst Kopien, auch über Gerätegrenzen hinweg."),
    FR("Préserve les fichiers. Utilise des liens physiques si possible ; sinon copie, notamment entre périphériques."),
    ES("Conserva el contenido. Usa enlaces duros si es posible; si no, copia, también entre dispositivos."),
    PT("Preserva o conteúdo. Usa ligações físicas se possível; caso contrário copia, incluindo entre dispositivos."),
    IT("Preserva i contenuti. Usa collegamenti fisici se possibile; altrimenti copia, anche tra dispositivi diversi."),
    NL("Behoudt bestandsinhoud. Gebruikt waar mogelijk harde links; anders kopieën, ook tussen apparaten."),
    RU("Сохраняет содержимое файлов. По возможности — жёсткие ссылки, иначе копии, в том числе между устройствами."),
    TR("Dosya içeriğini korur. Mümkünse sabit bağlantı kullanır; değilse, aygıtlar arasında da kopyalar."));

SS_MSG(photo_import_move,
    EN("Move them"),
    JA("移動する"),
    ZH_HANS("移动"),
    ZH_HANT("移動"),
    KO("옮기기"),
    DE("Verschieben"),
    FR("Les déplacer"),
    ES("Moverlas"),
    PT("Movê-las"),
    IT("Spostarle"),
    NL("Verplaatsen"),
    RU("Переместить"),
    TR("Taşı"));

SS_MSG(photo_import_move_help,
    EN("Moves photos into the dataset and removes them from the source folder. No undo; another run on that folder will find no photos."),
    JA("写真をデータセットへ移動し、元のフォルダから削除します。取り消せません。同じフォルダで再実行しても写真はありません。"),
    ZH_HANS("将照片移入数据集并从源文件夹删除。无法撤销；再次运行该文件夹时将找不到照片。"),
    ZH_HANT("將照片移入資料集並從來源資料夾刪除。無法復原；再次執行該資料夾時將找不到照片。"),
    KO("사진을 데이터셋으로 옮기고 원래 폴더에서 지웁니다. 되돌릴 수 없으며, 그 폴더로 다시 실행하면 사진이 없습니다."),
    DE("Verschiebt Fotos in den Datensatz und entfernt sie aus dem Quellordner. Kein Rückgängig; ein weiterer Lauf findet dort keine Fotos."),
    FR("Déplace les photos dans le jeu de données et les retire du dossier source. Sans annulation ; un autre traitement n’y trouvera plus de photos."),
    ES("Mueve las fotos al dataset y las elimina de la carpeta fuente. Sin deshacer; otra ejecución no encontrará fotos allí."),
    PT("Move as fotos para o conjunto de dados e remove-as da pasta de origem. Sem anular; outra execução não encontrará fotos lá."),
    IT("Sposta le foto nel dataset e le rimuove dalla cartella sorgente. Non annullabile; un altro avvio non troverà foto lì."),
    NL("Verplaatst foto’s naar de dataset en verwijdert ze uit de bronmap. Niet ongedaan te maken; een volgende run vindt daar geen foto’s."),
    RU("Перемещает фото в набор и удаляет из исходной папки. Отмены нет; следующий запуск по этой папке не найдёт фото."),
    TR("Fotoğrafları veri kümesine taşır, kaynak klasörden kaldırır. Geri alınamaz; aynı klasörde sonraki çalıştırma fotoğraf bulamaz."));

SS_MSG(photo_import_inplace,
    EN("Leave them where they are"),
    JA("元の場所に置いたままにする"),
    ZH_HANS("留在原处"),
    ZH_HANT("留在原處"),
    KO("있던 자리에 그대로 두기"),
    DE("Dort lassen, wo sie sind"),
    FR("Les laisser où elles sont"),
    ES("Dejarlas donde están"),
    PT("Deixá-las onde estão"),
    IT("Lasciarle dove sono"),
    NL("Laten staan waar ze staan"),
    RU("Оставить там, где лежат"),
    TR("Oldukları yerde bırak"));

SS_MSG(photo_import_inplace_help,
    EN("Uses the photo folder without copying. Reopening requires setting image_dir manually. Only works with one photo folder; multiple inputs are copied."),
    JA("写真フォルダをコピーせず使います。再度開くにはimage_dirを手動設定してください。写真フォルダ1つのみ対応し、複数の入力はコピーされます。"),
    ZH_HANS("直接使用照片文件夹，不复制。重新打开时需手动设置 image_dir。仅支持单个照片文件夹；多个输入仍会复制。"),
    ZH_HANT("直接使用照片資料夾，不複製。重新開啟時需手動設定 image_dir。僅支援單個照片資料夾；多個輸入仍會複製。"),
    KO("사진 폴더를 복사 없이 사용합니다. 다시 열 때 image_dir를 수동 설정해야 합니다. 사진 폴더 하나만 지원하며 여러 입력은 복사됩니다."),
    DE("Nutzt den Fotoordner ohne Kopie. Zum Wiederöffnen image_dir manuell setzen. Nur für einen Fotoordner; mehrere Eingaben werden kopiert."),
    FR("Utilise le dossier photo sans copie. Pour rouvrir, définir image_dir manuellement. Un seul dossier photo ; plusieurs entrées sont copiées."),
    ES("Usa la carpeta de fotos sin copiar. Para reabrir, configura image_dir a mano. Solo admite una carpeta; con varias entradas se copian los archivos."),
    PT("Usa a pasta de fotos sem copiar. Para reabrir, defina image_dir manualmente. Só admite uma pasta; várias entradas são copiadas."),
    IT("Usa la cartella foto senza copiarla. Per riaprire, impostare image_dir a mano. Solo una cartella foto; più input vengono copiati."),
    NL("Gebruikt de fotomap zonder kopie. Stel image_dir handmatig in om te heropenen. Alleen voor één fotomap; meerdere invoeren worden gekopieerd."),
    RU("Использует папку фото без копирования. Для открытия снова задайте image_dir вручную. Только одна папка; несколько входов копируются."),
    TR("Fotoğraf klasörünü kopyalamadan kullanır. Yeniden açmak için image_dir elle ayarlanır. Tek klasör desteklenir; birden fazla girdi kopyalanır."));

SS_MSG(drop_intermediate_title,
    EN("Delete the intermediate files after each run?"),
    JA("実行のたびに中間ファイルを削除しますか？"),
    ZH_HANS("每次运行后都删除中间文件吗？"),
    ZH_HANT("每次執行後都刪除中間檔案嗎？"),
    KO("실행이 끝날 때마다 중간 파일을 지울까요?"),
    DE("Die Zwischendateien nach jedem Lauf löschen?"),
    FR("Supprimer les fichiers intermédiaires après chaque exécution ?"),
    ES("¿Borrar los archivos intermedios después de cada ejecución?"),
    PT("Apagar os arquivos intermediários depois de cada execução?"),
    IT("Cancellare i file intermedi dopo ogni esecuzione?"),
    NL("De tussenbestanden na elke run verwijderen?"),
    RU("Удалять промежуточные файлы после каждого запуска?"),
    TR("Her çalıştırmadan sonra ara dosyalar silinsin mi?"));

SS_MSG(drop_intermediate_confirm,
    EN("Delete features and verified pairs? Reconstruction can no longer resume from them; the next run starts over and may take hours."),
    JA("特徴点と検証済みペアを削除しますか？再構成をそこから再開できなくなり、次回は最初からで数時間かかる場合があります。"),
    ZH_HANS("删除特征和已验证图像对？重建将无法从这些结果续跑，下次从头开始，可能耗时数小时。"),
    ZH_HANT("刪除特徵和已驗證影像對？重建將無法從這些結果續跑，下次從頭開始，可能耗時數小時。"),
    KO("특징과 검증된 쌍을 삭제할까요? 이 결과로 재구성을 이어갈 수 없으며, 다음 실행은 처음부터 시작해 몇 시간 걸릴 수 있습니다."),
    DE("Merkmale und geprüfte Paare löschen? Die Rekonstruktion kann daraus nicht fortgesetzt werden; der nächste Lauf beginnt neu und kann Stunden dauern."),
    FR("Supprimer caractéristiques et paires vérifiées ? La reconstruction ne pourra plus les reprendre ; le prochain traitement repart de zéro, parfois pour des heures."),
    ES("¿Borrar características y pares verificados? La reconstrucción ya no podrá reanudarlos; la próxima ejecución empieza de cero y puede tardar horas."),
    PT("Apagar características e pares verificados? A reconstrução já não poderá retomá-los; a próxima execução começa do zero e pode demorar horas."),
    IT("Eliminare caratteristiche e coppie verificate? La ricostruzione non potrà riprenderle; il prossimo avvio riparte da zero e può richiedere ore."),
    NL("Kenmerken en geverifieerde paren verwijderen? Reconstructie kan hiermee niet meer hervatten; de volgende run begint opnieuw en kan uren duren."),
    RU("Удалить признаки и проверенные пары? Продолжить реконструкцию по ним больше нельзя; следующий запуск начнётся заново и может занять часы."),
    TR("Özellikler ve doğrulanmış çiftler silinsin mi? Model bunlardan sürdürülemez; sonraki çalıştırma baştan başlar ve saatler sürebilir."));

SS_MSG(drop_intermediate_button,
    EN("Delete them"),
    JA("削除する"),        ZH_HANS("删除"),      ZH_HANT("刪除"),
    KO("지우기"),          DE("Sie löschen"),
    FR("Les supprimer"),   ES("Borrarlos"),
    PT("Apagá-los"),       IT("Cancellarli"),
    NL("Verwijderen"),     RU("Удалять"),
    TR("Sil"));

SS_MSG(keep_intermediate,
    EN("Keep intermediate files"),
    JA("中間ファイルを残す"),
    ZH_HANS("保留中间文件"),
    ZH_HANT("保留中間檔案"),
    KO("중간 파일 남기기"),
    DE("Zwischendateien behalten"),
    FR("Conserver les fichiers intermédiaires"),
    ES("Conservar los archivos intermedios"),
    PT("Manter os arquivos intermediários"),
    IT("Conservare i file intermedi"),
    NL("Tussenbestanden bewaren"),
    RU("Сохранять промежуточные файлы"),
    TR("Ara dosyaları sakla"));

SS_MSG(keep_intermediate_help,
    EN("Keeps features/, matches.bin and .resume/ after success. These large files allow interrupted or failed reconstruction to resume."),
    JA("成功後もfeatures/、matches.bin、.resume/を残します。大きなファイルですが、中断や失敗した再構成を再開できます。"),
    ZH_HANS("成功后保留 features/、matches.bin 和 .resume/。这些大文件支持中断或失败后的重建续跑。"),
    ZH_HANT("成功後保留 features/、matches.bin 和 .resume/。這些大檔案支援中斷或失敗後的重建續跑。"),
    KO("성공 후 features/, matches.bin, .resume/를 보관합니다. 용량은 크지만 중단되거나 실패한 재구성을 이어갈 수 있습니다."),
    DE("Behält features/, matches.bin und .resume/ nach Erfolg. Diese großen Dateien ermöglichen das Fortsetzen unterbrochener oder fehlgeschlagener Rekonstruktion."),
    FR("Garde features/, matches.bin et .resume/ après succès. Ces gros fichiers permettent de reprendre une reconstruction interrompue ou échouée."),
    ES("Conserva features/, matches.bin y .resume/ tras el éxito. Estos archivos grandes permiten reanudar reconstrucciones interrumpidas o fallidas."),
    PT("Guarda features/, matches.bin e .resume/ após sucesso. Estes ficheiros grandes permitem retomar reconstruções interrompidas ou falhadas."),
    IT("Conserva features/, matches.bin e .resume/ dopo il successo. Questi file grandi permettono di riprendere ricostruzioni interrotte o fallite."),
    NL("Bewaart features/, matches.bin en .resume/ na succes. Deze grote bestanden maken hervatten van onderbroken of mislukte reconstructies mogelijk."),
    RU("Хранит features/, matches.bin и .resume/ после успеха. Эти большие файлы позволяют продолжить прерванную или неудачную реконструкцию."),
    TR("Başarı sonrası features/, matches.bin ve .resume/ tutar. Bu büyük dosyalar kesilen veya başarısız yeniden oluşturmaya devam etmeyi sağlar."));

SS_MSG(extra_sfm_flags_hint,
    EN("extra `spirula sfm` flags, e.g. --max-error 2"),
    JA("`spirula sfm` への追加オプション（例: --max-error 2）"),
    ZH_HANS("额外的 `spirula sfm` 参数，例如 --max-error 2"),
    ZH_HANT("額外的 `spirula sfm` 參數，例如 --max-error 2"),
    KO("추가 `spirula sfm` 옵션, 예: --max-error 2"),
    DE("zusätzliche `spirula sfm`-Optionen, z. B. --max-error 2"),
    FR("options `spirula sfm` supplémentaires, p. ex. --max-error 2"),
    ES("opciones adicionales de `spirula sfm`, p. ej. --max-error 2"),
    PT("opções adicionais do `spirula sfm`, por exemplo --max-error 2"),
    IT("opzioni aggiuntive per `spirula sfm`, ad es. --max-error 2"),
    NL("extra `spirula sfm`-opties, bijv. --max-error 2"),
    RU("дополнительные ключи `spirula sfm`, например --max-error 2"),
    TR("ek `spirula sfm` seçenekleri, örn. --max-error 2"));

SS_MSG(extra_sfm_flags_help,
    EN("Passed to `spirula sfm auto` verbatim. Everything this panel does not "
       "show is reachable here; run `spirula sfm auto --help` for the list."),
    JA("`spirula sfm auto` にそのまま渡されます。このパネルに出ていない設定は"
       "すべてここから指定できます。一覧は `spirula sfm auto --help` で"
       "確認できます。"),
    ZH_HANS("原样传给 `spirula sfm auto`。这个面板没有列出的一切都可以在这里指定；"
            "运行 `spirula sfm auto --help` 查看完整列表。"),
    ZH_HANT("原樣傳給 `spirula sfm auto`。這個面板沒有列出的一切都可以在這裡指定；"
            "執行 `spirula sfm auto --help` 查看完整清單。"),
    KO("`spirula sfm auto`에 그대로 전달됩니다. 이 패널에 없는 것은 모두 여기서 "
       "지정할 수 있습니다. 목록은 `spirula sfm auto --help`로 확인하세요."),
    DE("Wird unverändert an `spirula sfm auto` weitergereicht. Alles, was "
       "dieses Fenster nicht zeigt, ist hier erreichbar; die Liste liefert "
       "`spirula sfm auto --help`."),
    FR("Transmis tel quel à `spirula sfm auto`. Tout ce que ce panneau "
       "n'affiche pas est accessible ici ; lancez `spirula sfm auto --help` "
       "pour la liste."),
    ES("Se pasa tal cual a `spirula sfm auto`. Todo lo que este panel no "
       "muestra se alcanza desde aquí; ejecute `spirula sfm auto --help` para "
       "ver la lista."),
    PT("Repassado tal e qual para `spirula sfm auto`. Tudo o que este painel "
       "não mostra é alcançável aqui; rode `spirula sfm auto --help` para ver "
       "a lista."),
    IT("Passato così com'è a `spirula sfm auto`. Tutto ciò che questo pannello "
       "non mostra è raggiungibile qui; per l'elenco esegua `spirula sfm auto "
       "--help`."),
    NL("Wordt letterlijk doorgegeven aan `spirula sfm auto`. Alles wat dit "
       "paneel niet toont, is hier bereikbaar; draai `spirula sfm auto --help` "
       "voor de lijst."),
    RU("Передаётся в `spirula sfm auto` как есть. Всё, чего нет на этой "
       "панели, доступно отсюда; список выдаёт `spirula sfm auto --help`."),
    TR("`spirula sfm auto` komutuna olduğu gibi aktarılır. Bu panelin "
       "göstermediği her şeye buradan ulaşılır; liste için `spirula sfm auto "
       "--help` çalıştırın."));

SS_MSG(section_fallbacks,
    EN("Fallbacks"),     JA("フォールバック"), ZH_HANS("回退方式"),  ZH_HANT("回退方式"),
    KO("대체 수단"),      DE("Ausweichwege"), FR("Solutions de repli"),
    ES("Alternativas"),  PT("Alternativas"), IT("Ripieghi"),
    NL("Terugvalopties"), RU("Запасные пути"), TR("Yedek yollar"));

SS_MSG(sfm_subprocess,
    EN("Reconstruct in a separate process"),
    JA("再構成を別プロセスで行う"),
    ZH_HANS("在单独的进程中重建"),
    ZH_HANT("在另一個行程中重建"),
    KO("재구성을 별도 프로세스에서"),
    DE("Rekonstruktion in einem eigenen Prozess"),
    FR("Reconstruire dans un processus séparé"),
    ES("Reconstruir en un proceso aparte"),
    PT("Reconstruir num processo separado"),
    IT("Ricostruire in un processo separato"),
    NL("Reconstructie in een apart proces"),
    RU("Реконструкция в отдельном процессе"),
    TR("Yeniden yapılandırmayı ayrı bir süreçte yap"));

SS_MSG(sfm_subprocess_help,
    EN("Runs reconstruction in a child process to contain driver crashes and release its memory on exit. Progress updates are slower."),
    JA("再構成を子プロセスで実行し、ドライバーの異常終了の影響を抑え、終了時にメモリを解放します。進捗更新は遅くなります。"),
    ZH_HANS("在子进程中运行重建，隔离驱动崩溃并在结束时释放内存。进度更新较慢。"),
    ZH_HANT("在子程序中執行重建，隔離驅動程式崩潰並在結束時釋放記憶體。進度更新較慢。"),
    KO("재구성을 자식 프로세스로 실행해 드라이버 충돌의 영향을 줄이고 종료 시 메모리를 반환합니다. 진행 상황 갱신은 느려집니다."),
    DE("Führt die Rekonstruktion als Kindprozess aus: begrenzt Treiberabstürze und gibt danach den Speicher frei. Fortschrittsmeldungen sind langsamer."),
    FR("Exécute la reconstruction dans un processus enfant pour isoler les plantages du pilote et libérer sa mémoire à la fin. Le suivi est plus lent."),
    ES("Ejecuta la reconstrucción en un proceso hijo para aislar fallos del controlador y liberar su memoria al terminar. El progreso se actualiza más lento."),
    PT("Executa a reconstrução num processo filho para isolar falhas do driver e libertar a memória ao terminar. O progresso atualiza mais devagar."),
    IT("Esegue la ricostruzione in un processo figlio per isolare i crash del driver e liberare la memoria al termine. Aggiornamenti meno rapidi."),
    NL("Voert reconstructie in een kindproces uit om drivercrashes te isoleren en daarna geheugen vrij te geven. Voortgang wordt trager bijgewerkt."),
    RU("Запускает реконструкцию в дочернем процессе: изолирует сбои драйвера и освобождает память при выходе. Прогресс обновляется реже."),
    TR("Yeniden oluşturmayı alt süreçte çalıştırarak sürücü çökmelerini yalıtır ve bitince belleği bırakır. İlerleme güncellemeleri daha yavaştır."));

SS_MSG(sfm_ba_cpu,
    EN("Bundle adjustment on the CPU"),
    JA("バンドル調整を CPU で行う"),
    ZH_HANS("在 CPU 上做光束法平差"),
    ZH_HANT("在 CPU 上做光束法平差"),
    KO("번들 조정을 CPU에서"),
    DE("Bündelausgleichung auf der CPU"),
    FR("Ajustement de faisceaux sur le CPU"),
    ES("Ajuste de haces en la CPU"),
    PT("Ajustamento de feixes na CPU"),
    IT("Bundle adjustment sulla CPU"),
    NL("Bundelaanpassing op de CPU"),
    RU("Уравнивание связок на CPU"),
    TR("Demet düzeltmesi CPU'da"));

SS_MSG(sfm_ba_cpu_help,
    EN("Solves the largest reconstruction step on the CPU. Slower, but avoids GPU driver resets or contention. GPU failures also fall back automatically."),
    JA("再構成で最大の工程をCPUで解きます。低速ですが、GPUドライバーのリセットや競合を避けられます。GPUの失敗時も自動で切り替わります。"),
    ZH_HANS("在 CPU 上求解重建中最大的步骤。较慢，但可避免 GPU 驱动重置或争用。GPU 失败时也会自动回退。"),
    ZH_HANT("在 CPU 上求解重建中最大的步驟。較慢，但可避免 GPU 驅動程式重設或爭用。GPU 失敗時也會自動回退。"),
    KO("재구성의 가장 큰 단계를 CPU에서 풉니다. 느리지만 GPU 드라이버 재설정이나 자원 경합을 피합니다. GPU 실패 시에도 자동 전환됩니다."),
    DE("Löst den größten Rekonstruktionsschritt auf der CPU. Langsamer, vermeidet aber GPU-Treiberresets oder Konkurrenz. GPU-Fehler wechseln auch automatisch."),
    FR("Résout la plus grosse étape sur CPU. Plus lent, mais évite les resets du pilote GPU ou la concurrence. Un échec GPU déclenche aussi le repli automatique."),
    ES("Resuelve el mayor paso en CPU. Más lento, pero evita reinicios del controlador GPU o competencia por recursos. Los fallos GPU también activan el cambio."),
    PT("Resolve o maior passo no CPU. Mais lento, mas evita reinícios do driver GPU ou disputa por recursos. Falhas na GPU também ativam a mudança automática."),
    IT("Risolve il passo più grande sulla CPU. Più lento, ma evita reset del driver GPU o contese. Anche gli errori GPU attivano il ripiego automatico."),
    NL("Lost de grootste reconstructiestap op de CPU op. Trager, maar vermijdt GPU-driverresets of concurrentie. Bij GPU-fouten wordt ook automatisch omgeschakeld."),
    RU("Решает крупнейший этап на CPU. Медленнее, но избегает сбросов драйвера GPU и конкуренции за ресурсы. При сбое GPU переключение тоже автоматическое."),
    TR("En büyük adımı CPU üzerinde çözer. Daha yavaştır, ancak GPU sürücü sıfırlaması ve kaynak çekişmesini önler. GPU hataları da otomatik geçiş başlatır."));

SS_MSG(use_ffmpeg,
    EN("Extract frames with ffmpeg"),
    JA("フレームの切り出しに ffmpeg を使う"),
    ZH_HANS("用 ffmpeg 抽取帧"),
    ZH_HANT("用 ffmpeg 抽取影格"),
    KO("ffmpeg으로 프레임 추출"),
    DE("Bilder mit ffmpeg extrahieren"),
    FR("Extraire les images avec ffmpeg"),
    ES("Extraer los fotogramas con ffmpeg"),
    PT("Extrair os quadros com ffmpeg"),
    IT("Estrarre i fotogrammi con ffmpeg"),
    NL("Beelden met ffmpeg uithalen"),
    RU("Извлекать кадры через ffmpeg"),
    TR("Kareleri ffmpeg ile çıkar"));

SS_MSG(use_ffmpeg_help,
    EN("Use an external ffmpeg instead of decoding on the GPU. Worth trying "
       "for a codec or colour transfer the driver mishandles."),
    JA("GPU でデコードする代わりに外部の ffmpeg を使います。ドライバの扱いが"
       "おかしいコーデックや色変換特性のときに試す価値があります。"),
    ZH_HANS("改用外部 ffmpeg，而不在 GPU 上解码。遇到驱动处理不当的编解码器或"
            "色彩传递特性时值得一试。"),
    ZH_HANT("改用外部 ffmpeg，而不在 GPU 上解碼。遇到驅動處理不當的編解碼器或"
            "色彩傳遞特性時值得一試。"),
    KO("GPU에서 디코딩하는 대신 외부 ffmpeg을 씁니다. 드라이버가 잘못 다루는 "
       "코덱이나 색 전달 특성일 때 시도해 볼 만합니다."),
    DE("Ein externes ffmpeg statt der GPU-Dekodierung benutzen. Einen Versuch "
       "wert bei einem Codec oder einer Farbübertragung, mit denen der Treiber "
       "nicht zurechtkommt."),
    FR("Utiliser un ffmpeg externe plutôt que le décodage sur le GPU. À "
       "essayer pour un codec ou une fonction de transfert que le pilote gère "
       "mal."),
    ES("Usar un ffmpeg externo en lugar de decodificar en la GPU. Vale la pena "
       "probarlo con un códec o una transferencia de color que el controlador "
       "maneje mal."),
    PT("Usar um ffmpeg externo em vez de decodificar na GPU. Vale tentar com "
       "um codec ou uma transferência de cor que o driver trate mal."),
    IT("Usare un ffmpeg esterno invece della decodifica su GPU. Vale la pena "
       "provarlo per un codec o una funzione di trasferimento che il driver "
       "gestisce male."),
    NL("Een externe ffmpeg gebruiken in plaats van decoderen op de GPU. Het "
       "proberen waard bij een codec of kleuroverdracht die het "
       "stuurprogramma verkeerd aanpakt."),
    RU("Использовать внешний ffmpeg вместо декодирования на GPU. Стоит "
       "попробовать при кодеке или передаточной функции цвета, с которыми "
       "драйвер справляется плохо."),
    TR("GPU'da çözmek yerine harici bir ffmpeg kullanın. Sürücünün yanlış "
       "işlediği bir kodek veya renk aktarımı için denemeye değer."));

SS_MSG(use_ffmpeg_always,
    EN("This build always uses ffmpeg for video."),
    JA("このビルドは動画に常に ffmpeg を使います。"),
    ZH_HANS("这个版本处理视频时始终使用 ffmpeg。"),
    ZH_HANT("這個版本處理影片時始終使用 ffmpeg。"),
    KO("이 빌드는 동영상에 항상 ffmpeg을 씁니다."),
    DE("Dieser Build benutzt für Video immer ffmpeg."),
    FR("Cette version utilise toujours ffmpeg pour la vidéo."),
    ES("Esta compilación siempre usa ffmpeg para el vídeo."),
    PT("Esta compilação sempre usa ffmpeg para vídeo."),
    IT("Questa build usa sempre ffmpeg per il video."),
    NL("Deze build gebruikt voor video altijd ffmpeg."),
    RU("Эта сборка всегда использует ffmpeg для видео."),
    TR("Bu sürüm video için her zaman ffmpeg kullanır."));

// ===========================================================================
// Advanced: external COLMAP
//
// Every entry here names a COLMAP parameter (Mapper.abs_pose_min_num_inliers,
// SiftMatching.max_ratio) and is read alongside COLMAP's own documentation,
// which exists in English only. So the identifiers stay VERBATIM in every
// language -- they are what the reader types into COLMAP and searches its
// docs for -- and only the prose around them is translated. Someone driving
// an external COLMAP by hand still deserves to read why a knob exists in
// their own language.
// ===========================================================================

SS_MSG(colmap_initial_focal,
    EN("Initial focal length (x width, 0 = unknown)"),
    JA("初期焦点距離（×幅、0 = 不明）"),
    ZH_HANS("初始焦距（× 宽度，0 = 未知）"),
    ZH_HANT("初始焦距（× 寬度，0 = 未知）"),
    KO("초기 초점 거리(× 너비, 0 = 모름)"),
    DE("Anfangsbrennweite (× Breite, 0 = unbekannt)"),
    FR("Distance focale initiale (× largeur, 0 = inconnue)"),
    ES("Distancia focal inicial (× ancho, 0 = desconocida)"),
    PT("Distância focal inicial (× largura, 0 = desconhecida)"),
    IT("Focale iniziale (× larghezza, 0 = sconosciuta)"),
    NL("Beginbrandpuntsafstand (× breedte, 0 = onbekend)"),
    RU("Начальное фокусное расстояние (× ширина, 0 = неизвестно)"),
    TR("Başlangıç odak uzaklığı (× genişlik, 0 = bilinmiyor)"));
SS_MSG(colmap_initial_focal_help,
    EN("fx = fy = factor × width; centered principal point, zero distortion.\nStabilizes initialization, especially fisheye. X5: ~0.269, automatic for .insv."),
    JA("fx = fy = 係数 × 幅。主点は中央、歪みはゼロ。\n特に魚眼の初期化を安定させます。X5は約0.269、.insvでは自動設定。"),
    ZH_HANS("fx = fy = 系数 × 宽；主点居中，畸变为零。\n稳定初始化，尤其是鱼眼。X5 约 0.269，.insv 自动设置。"),
    ZH_HANT("fx = fy = 係數 × 寬；主點居中，畸變為零。\n穩定初始化，尤其是魚眼。X5 約 0.269，.insv 自動設定。"),
    KO("fx = fy = 계수 × 너비. 주점은 중앙, 왜곡은 0.\n특히 어안 초기화를 안정화합니다. X5는 약 0.269, .insv는 자동 설정."),
    DE("fx = fy = Faktor × Breite; Hauptpunkt zentriert, Verzeichnung 0.\nStabilisiert den Start, besonders Fischauge. X5: ~0.269, automatisch für .insv."),
    FR("fx = fy = facteur × largeur ; point principal centré, distorsion nulle.\nStabilise le départ, surtout fisheye. X5 : ~0.269, automatique pour .insv."),
    ES("fx = fy = factor × ancho; punto principal centrado, distorsión cero.\nEstabiliza el inicio, sobre todo ojo de pez. X5: ~0.269, automático en .insv."),
    PT("fx = fy = fator × largura; ponto principal centrado, distorção zero.\nEstabiliza o início, sobretudo olho de peixe. X5: ~0.269, automático em .insv."),
    IT("fx = fy = fattore × larghezza; punto principale centrato, distorsione zero.\nStabilizza l’avvio, specie fisheye. X5: ~0.269, automatico per .insv."),
    NL("fx = fy = factor × breedte; hoofdpunt in het midden, vervorming 0.\nStabiliseert de start, vooral fisheye. X5: ~0.269, automatisch voor .insv."),
    RU("fx = fy = коэффициент × ширина; главная точка в центре, дисторсия 0.\nСтабилизирует запуск, особенно fisheye. X5: ~0.269, для .insv автоматически."),
    TR("fx = fy = katsayı × genişlik; asal nokta merkezde, bozulma 0.\nÖzellikle balıkgözünde başlangıcı dengeler. X5: ~0.269, .insv için otomatik."));

SS_MSG(colmap_camera_params,
    EN("Initial camera params"),
    JA("カメラパラメータの初期値"),
    ZH_HANS("相机参数初值"),
    ZH_HANT("相機參數初值"),
    KO("카메라 파라미터 초기값"),
    DE("Anfangs-Kameraparameter"),
    FR("Paramètres de caméra initiaux"),
    ES("Parámetros de cámara iniciales"),
    PT("Parâmetros de câmera iniciais"),
    IT("Parametri di camera iniziali"),
    NL("Begincameraparameters"),
    RU("Начальные параметры камеры"),
    TR("Başlangıç kamera parametreleri"));
SS_MSG(colmap_camera_params_hint,
    EN("fx,fy,cx,cy,... (overrides focal length)"),
    JA("fx,fy,cx,cy,…（焦点距離より優先）"),
    ZH_HANS("fx,fy,cx,cy,…（优先于焦距）"),
    ZH_HANT("fx,fy,cx,cy,…（優先於焦距）"),
    KO("fx,fy,cx,cy,…(초점 거리보다 우선)"),
    DE("fx,fy,cx,cy,… (hat Vorrang vor der Brennweite)"),
    FR("fx,fy,cx,cy,… (prioritaire sur la distance focale)"),
    ES("fx,fy,cx,cy,… (tiene prioridad sobre la distancia focal)"),
    PT("fx,fy,cx,cy,… (tem prioridade sobre a distância focal)"),
    IT("fx,fy,cx,cy,… (ha la precedenza sulla focale)"),
    NL("fx,fy,cx,cy,… (gaat voor op de brandpuntsafstand)"),
    RU("fx,fy,cx,cy,… (важнее фокусного расстояния)"),
    TR("fx,fy,cx,cy,… (odak uzaklığından önce gelir)"));
SS_MSG(colmap_camera_params_help,
    EN("Raw ImageReader.camera_params for the selected camera model (full calibration prior).\nLeave empty to use the focal-length factor above, or both empty for COLMAP's default initialization."),
    JA("選んだカメラモデルに対する ImageReader.camera_params をそのまま渡します（較正値をすべて与える指定）。\n空にすると上の焦点距離の係数を使い、両方とも空なら COLMAP の既定の初期化になります。"),
    ZH_HANS("按所选相机模型直接给出 ImageReader.camera_params（完整的标定先验）。留空则使用上面的焦距系数；两者都留空则用 COLMAP 的默认初始化。"),
    ZH_HANT("依所選相機模型直接給出 ImageReader.camera_params（完整的校正先驗）。留空則使用上面的焦距係數；兩者都留空則用 COLMAP 的預設初始化。"),
    KO("선택한 카메라 모델에 대한 ImageReader.camera_params를 그대로 넘깁니다(교정값 전체를 지정). 비워 두면 위의 초점 거리 계수를 쓰고, 둘 다 비우면 COLMAP의 기본 초기화를 씁니다."),
    DE("ImageReader.camera_params für das gewählte Kameramodell, unverändert (vollständige Kalibrierung als Vorgabe).\nLeer lassen, um den Brennweitenfaktor oben zu benutzen; sind beide leer, initialisiert COLMAP wie üblich."),
    FR("ImageReader.camera_params : calibration complète du modèle choisi. Vide : facteur focal ci-dessus. Les deux vides : initialisation COLMAP par défaut."),
    ES("ImageReader.camera_params: calibración completa del modelo elegido. Vacío: usa el factor focal de arriba. Ambos vacíos: inicio por defecto de COLMAP."),
    PT("ImageReader.camera_params tal como está, para o modelo de câmera escolhido (calibração completa como valor prévio).\nDeixe vazio para usar o fator de distância focal acima; com os dois vazios, o COLMAP inicializa como de costume."),
    IT("ImageReader.camera_params: calibrazione completa del modello scelto. Vuoto: usa il fattore focale sopra. Entrambi vuoti: inizializzazione COLMAP standard."),
    NL("ImageReader.camera_params: volledige kalibratie voor het cameramodel. Leeg: gebruik de brandpuntsfactor hierboven. Beide leeg: COLMAP-standaard."),
    RU("ImageReader.camera_params: полная калибровка выбранной камеры. Пусто: коэффициент фокуса выше. Оба пусты: начальные значения COLMAP."),
    TR("ImageReader.camera_params: seçilen modelin tam kalibrasyonu. Boşsa yukarıdaki odak katsayısı; ikisi de boşsa COLMAP varsayılanı kullanılır."));

SS_MSG(colmap_max_features,
    EN("Max features (0 = auto)"),
    JA("特徴点の上限（0 = 自動）"),
    ZH_HANS("特征点上限（0 = 自动）"),
    ZH_HANT("特徵點上限（0 = 自動）"),
    KO("특징점 최대 개수(0 = 자동)"),
    DE("Höchstzahl Merkmale (0 = automatisch)"),
    FR("Nombre maximal de points (0 = automatique)"),
    ES("Máximo de puntos característicos (0 = automático)"),
    PT("Máximo de pontos característicos (0 = automático)"),
    IT("Numero massimo di punti (0 = automatico)"),
    NL("Maximum aantal kenmerken (0 = automatisch)"),
    RU("Предел числа точек (0 = автоматически)"),
    TR("En çok öznitelik (0 = otomatik)"));
SS_MSG(colmap_max_features_help,
    EN("SiftExtraction / AlikedExtraction .max_num_features; overrides the "
       "Quality preset when non-zero."),
    JA("SiftExtraction / AlikedExtraction の .max_num_features です。0 以外に"
       "すると品質プリセットより優先されます。"),
    ZH_HANS("即 SiftExtraction / AlikedExtraction 的 .max_num_features；非 0 时"
            "优先于「质量」预设。"),
    ZH_HANT("即 SiftExtraction / AlikedExtraction 的 .max_num_features；非 0 時"
            "優先於「品質」預設。"),
    KO("SiftExtraction / AlikedExtraction의 .max_num_features입니다. 0이 아니면 "
       "품질 프리셋보다 우선합니다."),
    DE("SiftExtraction / AlikedExtraction .max_num_features; ungleich null hat "
       "es Vorrang vor der Qualitätsvoreinstellung."),
    FR("SiftExtraction / AlikedExtraction .max_num_features ; non nul, il "
       "prime sur le préréglage de qualité."),
    ES("SiftExtraction / AlikedExtraction .max_num_features; si no es cero, "
       "tiene prioridad sobre el ajuste de calidad."),
    PT("SiftExtraction / AlikedExtraction .max_num_features; se não for zero, "
       "tem prioridade sobre a predefinição de qualidade."),
    IT("SiftExtraction / AlikedExtraction .max_num_features; se diverso da "
       "zero ha la precedenza sulla preimpostazione di qualità."),
    NL("SiftExtraction / AlikedExtraction .max_num_features; niet nul gaat "
       "voor op de kwaliteitsvoorinstelling."),
    RU("SiftExtraction / AlikedExtraction .max_num_features; ненулевое "
       "значение важнее пресета качества."),
    TR("SiftExtraction / AlikedExtraction .max_num_features; sıfırdan farklıysa "
       "kalite hazır ayarının önüne geçer."));

SS_MSG(colmap_max_image_size,
    EN("Max image size (0 = off)"),
    JA("画像サイズの上限（0 = 無効）"),
    ZH_HANS("图像尺寸上限（0 = 关闭）"),
    ZH_HANT("影像尺寸上限（0 = 關閉）"),
    KO("이미지 크기 상한(0 = 끔)"),
    DE("Maximale Bildgröße (0 = aus)"),
    FR("Taille d'image maximale (0 = désactivé)"),
    ES("Tamaño máximo de imagen (0 = desactivado)"),
    PT("Tamanho máximo de imagem (0 = desligado)"),
    IT("Dimensione massima dell'immagine (0 = disattivo)"),
    NL("Maximale beeldgrootte (0 = uit)"),
    RU("Предел размера изображения (0 = выкл.)"),
    TR("En büyük görüntü boyutu (0 = kapalı)"));
SS_MSG(colmap_max_image_size_help,
    EN("FeatureExtraction.max_image_size: downscale images beyond this for "
       "feature extraction."),
    JA("FeatureExtraction.max_image_size です。これを超える画像は特徴点の抽出時に"
       "縮小されます。"),
    ZH_HANS("即 FeatureExtraction.max_image_size：超过这个尺寸的图像会在提取特征时"
            "先缩小。"),
    ZH_HANT("即 FeatureExtraction.max_image_size：超過這個尺寸的影像會在擷取特徵時"
            "先縮小。"),
    KO("FeatureExtraction.max_image_size입니다. 이보다 큰 이미지는 특징점을 뽑을 때 "
       "축소합니다."),
    DE("FeatureExtraction.max_image_size: größere Bilder werden für die "
       "Merkmalssuche verkleinert."),
    FR("FeatureExtraction.max_image_size : les images plus grandes sont "
       "réduites pour l'extraction des points."),
    ES("FeatureExtraction.max_image_size: las imágenes más grandes se reducen "
       "para extraer los puntos característicos."),
    PT("FeatureExtraction.max_image_size: imagens maiores são reduzidas para a "
       "extração de pontos característicos."),
    IT("FeatureExtraction.max_image_size: le immagini più grandi vengono "
       "ridotte per l'estrazione dei punti."),
    NL("FeatureExtraction.max_image_size: grotere beelden worden verkleind om "
       "kenmerken te zoeken."),
    RU("FeatureExtraction.max_image_size: изображения крупнее уменьшаются перед "
       "поиском точек."),
    TR("FeatureExtraction.max_image_size: bundan büyük görüntüler öznitelik "
       "çıkarımı için küçültülür."));

SS_MSG(colmap_seq_overlap_help,
    EN("How many neighboring frames each frame is matched against (sequential "
       "matcher)."),
    JA("各フレームを前後いくつのフレームと照合するかです（逐次マッチャー）。"),
    ZH_HANS("每一帧与前后多少帧做匹配（顺序匹配器）。"),
    ZH_HANT("每一影格與前後多少影格做比對（循序比對器）。"),
    KO("각 프레임을 앞뒤 몇 개의 프레임과 매칭할지입니다(순차 매처)."),
    DE("Mit wie vielen Nachbarbildern jedes Bild verglichen wird (sequenzieller "
       "Matcher)."),
    FR("Nombre d'images voisines auxquelles chaque image est comparée "
       "(appariement séquentiel)."),
    ES("Con cuántos fotogramas vecinos se compara cada fotograma "
       "(emparejamiento secuencial)."),
    PT("Com quantos quadros vizinhos cada quadro é comparado (correspondência "
       "sequencial)."),
    IT("Con quanti fotogrammi vicini viene confrontato ogni fotogramma "
       "(matcher sequenziale)."),
    NL("Met hoeveel naburige beelden elk beeld wordt vergeleken (sequentiële "
       "matcher)."),
    RU("Со сколькими соседними кадрами сопоставляется каждый кадр "
       "(последовательное сопоставление)."),
    TR("Her karenin kaç komşu kareyle eşleştirileceği (sıralı eşleştirici)."));

SS_MSG(colmap_quadratic_overlap,
    EN("Quadratic overlap"),
    JA("二次的な重なり"),
    ZH_HANS("二次重叠"),
    ZH_HANT("二次重疊"),
    KO("이차 겹침"),
    DE("Quadratische Überlappung"),
    FR("Recouvrement quadratique"),
    ES("Solapamiento cuadrático"),
    PT("Sobreposição quadrática"),
    IT("Sovrapposizione quadratica"),
    NL("Kwadratische overlap"),
    RU("Квадратичное перекрытие"),
    TR("Karesel örtüşme"));
SS_MSG(colmap_quadratic_overlap_help,
    EN("Additionally match frame i against frames i +- 2^k (sequential "
       "matcher). Helps close loops in longer captures; enabled by default."),
    JA("フレーム i を i ± 2^k のフレームとも照合します（逐次マッチャー）。"
       "長い撮影でループを閉じやすくなります。既定で有効です。"),
    ZH_HANS("再把第 i 帧与第 i ± 2^k 帧做匹配（顺序匹配器）。较长的拍摄更容易闭环；"
            "默认开启。"),
    ZH_HANT("再把第 i 影格與第 i ± 2^k 影格做比對（循序比對器）。較長的拍攝更容易"
            "閉環；預設開啟。"),
    KO("프레임 i를 i ± 2^k 프레임과도 매칭합니다(순차 매처). 긴 촬영에서 루프를 "
       "닫는 데 도움이 됩니다. 기본값은 켬입니다."),
    DE("Bild i zusätzlich mit den Bildern i ± 2^k vergleichen (sequenzieller "
       "Matcher). Hilft, Schleifen in längeren Aufnahmen zu schließen; "
       "standardmäßig an."),
    FR("Comparer en plus l'image i aux images i ± 2^k (appariement "
       "séquentiel). Aide à fermer les boucles des prises longues ; activé par "
       "défaut."),
    ES("Comparar además el fotograma i con los fotogramas i ± 2^k "
       "(emparejamiento secuencial). Ayuda a cerrar bucles en tomas largas; "
       "activado por defecto."),
    PT("Comparar também o quadro i com os quadros i ± 2^k (correspondência "
       "sequencial). Ajuda a fechar laços em capturas longas; ligado por "
       "padrão."),
    IT("Confrontare anche il fotogramma i con i fotogrammi i ± 2^k (matcher "
       "sequenziale). Aiuta a chiudere gli anelli nelle riprese lunghe; attivo "
       "per impostazione predefinita."),
    NL("Beeld i ook vergelijken met de beelden i ± 2^k (sequentiële matcher). "
       "Helpt lussen in langere opnamen te sluiten; standaard aan."),
    RU("Дополнительно сопоставлять кадр i с кадрами i ± 2^k (последовательное "
       "сопоставление). Помогает замыкать петли в длинных съёмках; включено по "
       "умолчанию."),
    TR("i karesini ayrıca i ± 2^k kareleriyle de eşleştirir (sıralı "
       "eşleştirici). Uzun çekimlerde döngüleri kapatmaya yardım eder; "
       "varsayılan olarak açık."));

SS_MSG(colmap_lightglue,
    EN("LightGlue matching"),
    JA("LightGlue によるマッチング"),
    ZH_HANS("LightGlue 匹配"),
    ZH_HANT("LightGlue 比對"),
    KO("LightGlue 매칭"),
    DE("LightGlue-Zuordnung"),
    FR("Appariement LightGlue"),
    ES("Emparejamiento LightGlue"),
    PT("Correspondência LightGlue"),
    IT("Corrispondenze con LightGlue"),
    NL("LightGlue-matching"),
    RU("Сопоставление LightGlue"),
    TR("LightGlue eşleştirmesi"));
SS_MSG(colmap_lightglue_help,
    EN("Neural matcher: FeatureMatching.type *_LIGHTGLUE. More matches on difficult pairs; default for ALIKED, also supports SIFT."),
    JA("学習型照合：FeatureMatching.type *_LIGHTGLUE。難しいペアで一致が増えます。ALIKEDの既定で、SIFTにも対応します。"),
    ZH_HANS("神经匹配器：FeatureMatching.type *_LIGHTGLUE。困难图像对匹配更多；ALIKED 默认，也支持 SIFT。"),
    ZH_HANT("神經匹配器：FeatureMatching.type *_LIGHTGLUE。困難影像對匹配更多；ALIKED 預設，也支援 SIFT。"),
    KO("신경망 매칭: FeatureMatching.type *_LIGHTGLUE. 어려운 쌍에서 매칭이 늘어납니다. ALIKED 기본이며 SIFT도 지원합니다."),
    DE("Neuronaler Matcher: FeatureMatching.type *_LIGHTGLUE. Mehr Treffer bei schwierigen Paaren; Standard für ALIKED, auch SIFT."),
    FR("Appariement neuronal : FeatureMatching.type *_LIGHTGLUE. Plus de résultats sur paires difficiles ; défaut ALIKED, aussi SIFT."),
    ES("Emparejador neuronal: FeatureMatching.type *_LIGHTGLUE. Más aciertos en pares difíciles; por defecto con ALIKED, también SIFT."),
    PT("Correspondência neural: FeatureMatching.type *_LIGHTGLUE. Mais resultados em pares difíceis; padrão ALIKED, também SIFT."),
    IT("Abbinamento neurale: FeatureMatching.type *_LIGHTGLUE. Più risultati su coppie difficili; predefinito ALIKED, anche SIFT."),
    NL("Neurale matcher: FeatureMatching.type *_LIGHTGLUE. Meer treffers bij lastige paren; standaard voor ALIKED, ook SIFT."),
    RU("Нейросопоставление: FeatureMatching.type *_LIGHTGLUE. Больше совпадений сложных пар; по умолчанию для ALIKED, также SIFT."),
    TR("Sinir eşleştirici: FeatureMatching.type *_LIGHTGLUE. Zor çiftlerde daha çok eşleşme; ALIKED varsayılanı, SIFT de desteklenir."));

SS_MSG(colmap_affine_sift,
    EN("Affine SIFT + guided matching"),
    JA("アフィン SIFT ＋ ガイド付きマッチング"),
    ZH_HANS("仿射 SIFT ＋ 引导匹配"),
    ZH_HANT("仿射 SIFT ＋ 引導比對"),
    KO("어파인 SIFT + 유도 매칭"),
    DE("Affines SIFT + geführte Zuordnung"),
    FR("SIFT affine + appariement guidé"),
    ES("SIFT afín + emparejamiento guiado"),
    PT("SIFT afim + correspondência guiada"),
    IT("SIFT affine + corrispondenze guidate"),
    NL("Affiene SIFT + geleide matching"),
    RU("Аффинный SIFT + управляемое сопоставление"),
    TR("Afin SIFT + yönlendirmeli eşleştirme"));
SS_MSG(colmap_affine_sift_help,
    EN("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "slower but more robust matching."),
    JA("SiftExtraction.estimate_affine_shape と "
       "FeatureMatching.guided_matching です。遅くなりますが、マッチングが"
       "崩れにくくなります。"),
    ZH_HANS("即 SiftExtraction.estimate_affine_shape 与 "
            "FeatureMatching.guided_matching：更慢，但匹配更稳健。"),
    ZH_HANT("即 SiftExtraction.estimate_affine_shape 與 "
            "FeatureMatching.guided_matching：更慢，但比對更穩健。"),
    KO("SiftExtraction.estimate_affine_shape와 "
       "FeatureMatching.guided_matching입니다. 느려지지만 매칭이 더 튼튼해집니다."),
    DE("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "langsamer, aber robuster."),
    FR("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching : "
       "plus lent, mais plus robuste."),
    ES("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "más lento, pero más robusto."),
    PT("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "mais lento, mas mais robusto."),
    IT("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "più lento, ma più robusto."),
    NL("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "trager, maar robuuster."),
    RU("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "медленнее, но устойчивее."),
    TR("SiftExtraction.estimate_affine_shape + FeatureMatching.guided_matching: "
       "daha yavaş ama daha sağlam eşleştirme."));

SS_MSG(colmap_distortion_refinement,
    EN("Distortion refinement"),
    JA("歪みの最適化"),
    ZH_HANS("畸变优化"),
    ZH_HANT("畸變最佳化"),
    KO("왜곡 보정 최적화"),
    DE("Verzeichnung mitoptimieren"),
    FR("Affinage de la distorsion"),
    ES("Refinado de la distorsión"),
    PT("Refino da distorção"),
    IT("Affinamento della distorsione"),
    NL("Vertekening bijstellen"),
    RU("Уточнение дисторсии"),
    TR("Bozulmanın iyileştirilmesi"));
SS_MSG(colmap_extra_auto,
    EN("Auto"),           JA("自動"),          ZH_HANS("自动"),     ZH_HANT("自動"),
    KO("자동"),            DE("Automatisch"),  FR("Automatique"),  ES("Automático"),
    PT("Automático"),     IT("Automatico"),   NL("Automatisch"),  RU("Автоматически"),
    TR("Otomatik"));
SS_MSG(colmap_extra_during,
    EN("During mapping"),
    JA("マッピング中"),
    ZH_HANS("建图过程中"),
    ZH_HANT("建圖過程中"),
    KO("매핑 중"),
    DE("Während des Mappings"),
    FR("Pendant le mapping"),
    ES("Durante el mapeo"),
    PT("Durante o mapeamento"),
    IT("Durante il mapping"),
    NL("Tijdens het mappen"),
    RU("Во время реконструкции"),
    TR("Haritalama sırasında"));
SS_MSG(colmap_extra_final,
    EN("Final pass only"),
    JA("最後の仕上げだけ"),
    ZH_HANS("只在最后一遍"),
    ZH_HANT("只在最後一遍"),
    KO("마지막 단계에서만"),
    DE("Nur im letzten Durchgang"),
    FR("Seulement à la passe finale"),
    ES("Solo en la pasada final"),
    PT("Só na passagem final"),
    IT("Solo nella passata finale"),
    NL("Alleen in de laatste ronde"),
    RU("Только на финальном проходе"),
    TR("Yalnızca son geçişte"));
SS_MSG(colmap_distortion_refinement_help,
    EN("Choose when COLMAP optimizes lens distortion.\n- Final pass only: fixes distortion during mapping (Mapper.ba_refine_extra_params 0) for stable perspective fits.\n- Auto: final pass for perspective lenses, during mapping for fisheye."),
    JA("歪み係数をいつ最適化するかです。 「最後の仕上げだけ」ならマッピング中は固定し（Mapper.ba_refine_extra_params 0）、最後の仕上げで求めます。\n歪みの小さい透視レンズではこちらが安定します。 「自動」は透視モデルなら最後の仕上げだけ、魚眼ならマッピング中です。"),
    ZH_HANS("何时优化畸变系数。选「只在最后一遍」时，建图过程中保持固定（Mapper.ba_refine_extra_params 0），到最后一遍精修时再求解；对畸变较小的透视镜头更稳。\n「自动」：透视模型只在最后一遍，鱼眼在建图过程中。"),
    ZH_HANT("何時最佳化畸變係數。選「只在最後一遍」時，建圖過程中保持固定（Mapper.ba_refine_extra_params 0），到最後一遍精修時再求解；對畸變較小的透視鏡頭更穩。\n「自動」：透視模型只在最後一遍，魚眼在建圖過程中。"),
    KO("왜곡 계수를 언제 최적화할지입니다. ‘마지막 단계에서만’은 매핑 중에는 고정해 두고(Mapper.ba_refine_extra_params 0) 마지막 정밀화 단계에서 구합니다.\n- 왜곡이 작은 원근 렌즈에서는 이쪽이 더 안정적입니다.\n- ‘자동’은 원근 모델이면 마지막 단계에서만, 어안이면 매핑 중입니다."),
    DE("Wann die Verzeichnungskoeffizienten optimiert werden. „Nur im letzten Durchgang“ hält sie während des Mappings fest (Mapper.ba_refine_extra_params 0) -- bei perspektivischen Objektiven mit wenig Verzeichnung stabiler -- und bestimmt sie erst im letzten Feinschliff.\nAutomatisch: bei perspektivischen Modellen nur im letzten Durchgang, bei Fisheye während des Mappings."),
    FR("Quand les coefficients de distorsion sont optimisés. « Seulement à la passe finale » les garde fixes pendant le mapping (Mapper.ba_refine_extra_params 0) -- plus stable pour les objectifs perspectifs peu distordus -- et les retrouve à l'affinage final.\nAutomatique : passe finale seule pour les modèles perspectifs, pendant le mapping pour le fisheye."),
    ES("Cuándo se optimizan los coeficientes de distorsión. «Solo en la pasada final» los mantiene fijos durante el mapeo (Mapper.ba_refine_extra_params 0) -- más estable con objetivos perspectivos de poca distorsión -- y los recupera en el refinado final.\nAutomático: solo pasada final para los modelos perspectivos, durante el mapeo para el ojo de pez."),
    PT("Quando os coeficientes de distorção são otimizados. “Só na passagem final” mantém-nos fixos durante o mapeamento (Mapper.ba_refine_extra_params 0) -- mais estável com lentes perspectivas de pouca distorção -- e recupera-os no refino final.\nAutomático: só a passagem final para modelos perspectivos, durante o mapeamento para olho de peixe."),
    IT("Quando vengono ottimizzati i coefficienti di distorsione. «Solo nella passata finale» li tiene fissi durante il mapping (Mapper.ba_refine_extra_params 0) -- più stabile con obiettivi prospettici poco distorti -- e li ricava nell'affinamento finale.\nAutomatico: solo passata finale per i modelli prospettici, durante il mapping per il fisheye."),
    NL("Wanneer de vertekeningscoëfficiënten worden geoptimaliseerd. „Alleen in de laatste ronde” houdt ze vast tijdens het mappen (Mapper.ba_refine_extra_params 0) -- stabieler bij perspectivische lenzen met weinig vertekening -- en bepaalt ze pas in de laatste verfijning.\nAutomatisch: alleen de laatste ronde bij perspectivische modellen, tijdens het mappen bij fisheye."),
    RU("Когда оптимизируются коэффициенты дисторсии. «Только на финальном проходе» держит их фиксированными во время реконструкции (Mapper.ba_refine_extra_params 0) -- устойчивее для перспективных объективов с малой дисторсией -- и находит их на финальном уточнении.\nАвтоматически: только финальный проход для перспективных моделей, во время реконструкции для «рыбьего глаза»."),
    TR("Bozulma katsayılarının ne zaman iyileştirileceği. «Yalnızca son geçişte», haritalama boyunca onları sabit tutar (Mapper.ba_refine_extra_params 0) -- bozulması az perspektif objektiflerde daha kararlıdır -- ve son iyileştirme geçişinde bulur.\nOtomatik: perspektif modellerde yalnızca son geçiş, balıkgözünde haritalama sırasında."));

SS_MSG(colmap_min_matches,
    EN("Min matches per pair (0 = default)"),
    JA("ペアあたりの最小対応数（0 = 既定値）"),
    ZH_HANS("每对图像的最少匹配数（0 = 默认）"),
    ZH_HANT("每對影像的最少對應數（0 = 預設）"),
    KO("쌍당 최소 대응 수(0 = 기본값)"),
    DE("Mindestzahl Zuordnungen je Paar (0 = Standard)"),
    FR("Correspondances minimales par paire (0 = valeur par défaut)"),
    ES("Correspondencias mínimas por par (0 = valor predeterminado)"),
    PT("Correspondências mínimas por par (0 = valor padrão)"),
    IT("Corrispondenze minime per coppia (0 = valore predefinito)"),
    NL("Minimum aantal overeenkomsten per paar (0 = standaard)"),
    RU("Минимум соответствий на пару (0 = по умолчанию)"),
    TR("Çift başına en az eşleşme (0 = varsayılan)"));
SS_MSG(colmap_min_matches_help,
    EN("Mapper.min_num_matches (default 15): image pairs with fewer inlier matches are ignored by the mapper. Raise to suppress spurious registrations, lower for sparse overlap."),
    JA("Mapper.min_num_matches（既定 15）です。インライアの対応がこれより少ない画像ペアはマッパーが無視します。誤った登録を抑えたいときは上げ、重なりが少ない撮影では下げます。"),
    ZH_HANS("即 Mapper.min_num_matches（默认 15）：内点匹配少于此数的图像对会被建图阶段忽略。想压制错误的配准就调高，重叠很少时就调低。"),
    ZH_HANT("即 Mapper.min_num_matches（預設 15）：內點對應少於此數的影像對會被建圖階段忽略。想壓制錯誤的註冊就調高，重疊很少時就調低。"),
    KO("Mapper.min_num_matches(기본 15)입니다. 인라이어 대응이 이보다 적은 이미지 쌍은 매퍼가 무시합니다. 잘못된 등록을 줄이려면 올리고, 겹침이 적으면 내립니다."),
    DE("Mapper.min_num_matches (Standard 15): Bildpaare mit weniger Inlier-Zuordnungen ignoriert der Mapper.\nHöher unterdrückt falsche Registrierungen, niedriger hilft bei wenig Überlappung."),
    FR("Mapper.min_num_matches (défaut : 15) : ignore les paires avec moins de correspondances inliers. Augmenter contre les fausses poses ; baisser si peu de recouvrement."),
    ES("Mapper.min_num_matches (15 por defecto): el mapper ignora los pares de imágenes con menos correspondencias inlier.\nSúbelo para eliminar registros espurios, bájalo si hay poco solapamiento."),
    PT("Mapper.min_num_matches (padrão 15): o mapper ignora pares de imagens com menos correspondências inlier.\nAumente para eliminar registros espúrios, diminua quando houver pouca sobreposição."),
    IT("Mapper.min_num_matches (predefinito 15): il mapper ignora le coppie di immagini con meno corrispondenze inlier.\nAlzalo per eliminare registrazioni spurie, abbassalo se la sovrapposizione è scarsa."),
    NL("Mapper.min_num_matches (standaard 15): beeldparen met minder inlier-overeenkomsten negeert de mapper. Hoger onderdrukt valse registraties, lager helpt bij weinig overlap."),
    RU("Mapper.min_num_matches (по умолчанию 15): пары изображений с меньшим числом инлаерных соответствий маппер игнорирует.\nПовысьте, чтобы убрать ложные регистрации, понизьте при малом перекрытии."),
    TR("Mapper.min_num_matches (varsayılan 15): inlier eşleşmesi bundan az olan görüntü çiftlerini mapper yok sayar. Yanlış kayıtları bastırmak için yükseltin, örtüşme azsa düşürün."));

SS_MSG(colmap_repetitive,
    EN("Repetitive scenes"),
    JA("繰り返しの多いシーン"),
    ZH_HANS("重复结构的场景"),
    ZH_HANT("重複結構的場景"),
    KO("반복이 많은 장면"),
    DE("Sich wiederholende Szenen"),
    FR("Scènes répétitives"),
    ES("Escenas repetitivas"),
    PT("Cenas repetitivas"),
    IT("Scene ripetitive"),
    NL("Repetitieve scènes"),
    RU("Повторяющиеся сцены"),
    TR("Yinelenen sahneler"));
SS_MSG(colmap_repetitive_help,
    EN("Stricter matching and registration to stop similar rooms or facades merging incorrectly. 0 uses COLMAP defaults."),
    JA("似た部屋や外壁が誤って結合しないよう照合と登録を厳しくします。0はCOLMAPの既定値です。"),
    ZH_HANS("收紧匹配和注册，防止相似房间或立面被错误合并。0 使用 COLMAP 默认值。"),
    ZH_HANT("收緊匹配和註冊，防止相似房間或立面被錯誤合併。0 使用 COLMAP 預設值。"),
    KO("비슷한 방이나 외벽이 잘못 합쳐지지 않게 매칭과 등록을 엄격히 합니다. 0은 COLMAP 기본값입니다."),
    DE("Strengere Zuordnung und Registrierung gegen falsches Verbinden ähnlicher Räume oder Fassaden. 0 nutzt COLMAP-Vorgaben."),
    FR("Appariement et inscription plus stricts pour éviter de fusionner à tort des pièces ou façades semblables. 0 utilise les défauts COLMAP."),
    ES("Emparejamiento y registro más estrictos para no unir por error habitaciones o fachadas similares. 0 usa los valores COLMAP."),
    PT("Correspondência e registo mais estritos para não unir por erro salas ou fachadas semelhantes. 0 usa os padrões COLMAP."),
    IT("Abbinamento e registrazione più severi contro fusioni errate di stanze o facciate simili. 0 usa i valori COLMAP."),
    NL("Strengere koppeling en registratie tegen fout samenvoegen van gelijkende kamers of gevels. 0 gebruikt COLMAP-standaarden."),
    RU("Строже сопоставляет и регистрирует, не давая ошибочно склеивать похожие комнаты или фасады. 0 использует значения COLMAP."),
    TR("Benzer oda veya cephelerin yanlış birleşmesini engellemek için eşleştirme ve kayıt sıkılaşır. 0 COLMAP varsayılanıdır."));
SS_MSG(colmap_repetitive_level,
    EN("Repetitive level"),
    JA("繰り返しへの強さ"),
    ZH_HANS("重复结构强度"),
    ZH_HANT("重複結構強度"),
    KO("반복 대응 강도"),
    DE("Stufe der Wiederholung"),
    FR("Niveau de répétition"),
    ES("Nivel de repetición"),
    PT("Nível de repetição"),
    IT("Livello di ripetizione"),
    NL("Niveau van herhaling"),
    RU("Уровень повторяемости"),
    TR("Yinelenme düzeyi"));
SS_MSG(colmap_rep_off,
    EN("Off (COLMAP defaults)"),
    JA("なし（COLMAP の既定値）"),
    ZH_HANS("关闭（COLMAP 默认值）"),
    ZH_HANT("關閉（COLMAP 預設值）"),
    KO("끔(COLMAP 기본값)"),
    DE("Aus (COLMAP-Standard)"),
    FR("Désactivé (valeurs COLMAP par défaut)"),
    ES("Desactivado (valores de COLMAP por defecto)"),
    PT("Desligado (padrões do COLMAP)"),
    IT("Disattivo (valori predefiniti di COLMAP)"),
    NL("Uit (COLMAP-standaardwaarden)"),
    RU("Выкл. (значения COLMAP по умолчанию)"),
    TR("Kapalı (COLMAP varsayılanları)"));
SS_MSG(colmap_rep_low,
    EN("Low"),            JA("弱"),            ZH_HANS("低"),       ZH_HANT("低"),
    KO("낮음"),            DE("Niedrig"),      FR("Faible"),       ES("Bajo"),
    PT("Baixo"),          IT("Basso"),        NL("Laag"),         RU("Низкий"),
    TR("Düşük"));
SS_MSG(colmap_rep_medium,
    EN("Medium"),         JA("中"),            ZH_HANS("中"),       ZH_HANT("中"),
    KO("보통"),            DE("Mittel"),       FR("Moyen"),        ES("Medio"),
    PT("Médio"),          IT("Medio"),        NL("Gemiddeld"),    RU("Средний"),
    TR("Orta"));
SS_MSG(colmap_rep_high,
    EN("High"),           JA("強"),            ZH_HANS("高"),       ZH_HANT("高"),
    KO("높음"),            DE("Hoch"),         FR("Élevé"),        ES("Alto"),
    PT("Alto"),           IT("Alto"),         NL("Hoog"),         RU("Высокий"),
    TR("Yüksek"));
SS_MSG(colmap_rep_custom,
    EN("Custom"),         JA("カスタム"),       ZH_HANS("自定义"),   ZH_HANT("自訂"),
    KO("사용자 지정"),      DE("Benutzerdefiniert"), FR("Personnalisé"),
    ES("Personalizado"),  PT("Personalizado"), IT("Personalizzato"),
    NL("Aangepast"),      RU("Свой"),         TR("Özel"));
SS_MSG(colmap_repetitive_level_help,
    EN("- Low preserves registration; Medium suits multi-room interiors.\n- High handles strong repetition but may register fewer images with low overlap. Fills fields below."),
    JA("- 低は登録を保ち、中は複数の部屋がある屋内向けです。\n- 高は強い反復向けですが、重なりが少ないと登録画像が減ります。下の設定を入力します。"),
    ZH_HANS("- 低：保持注册率；中：适合多房间室内。\n- 高：应对强重复，但重叠不足时注册图像会减少。填入下方设置。"),
    ZH_HANT("- 低：保持註冊率；中：適合多房間室內。\n- 高：應對強重複，但重疊不足時註冊影像會減少。填入下方設定。"),
    KO("- 낮음은 등록률 유지, 중간은 여러 방의 실내에 적합합니다.\n- 높음은 강한 반복용이며 겹침이 적으면 등록이 줄 수 있습니다. 아래 값을 채웁니다."),
    DE("- Niedrig erhält die Registrierung; Mittel für Innenräume mit mehreren Zimmern.\n- Hoch gegen starke Wiederholung; bei wenig Überlappung weniger Bilder. Füllt die Felder unten."),
    FR("- Bas préserve l’inscription ; Moyen pour plusieurs pièces en intérieur.\n- Haut contre les fortes répétitions ; moins d’images si peu de recouvrement. Remplit les champs ci-dessous."),
    ES("- Bajo mantiene el registro; Medio para interiores de varias habitaciones.\n- Alto contra gran repetición; puede registrar menos con poco solape. Rellena los campos de abajo."),
    PT("- Baixo mantém o registo; Médio para interiores com várias salas.\n- Alto contra forte repetição; pode registar menos com pouca sobreposição. Preenche os campos abaixo."),
    IT("- Basso mantiene la registrazione; Medio per interni con più stanze.\n- Alto contro forti ripetizioni; meno immagini se poco sovrapposte. Compila i campi sotto."),
    NL("- Laag behoudt registratie; Midden voor interieurs met meerdere kamers.\n- Hoog tegen sterke herhaling; minder beelden bij weinig overlap. Vult de velden hieronder."),
    RU("- Низкий сохраняет регистрацию; Средний — для нескольких комнат.\n- Высокий против повторений; при малом перекрытии меньше снимков. Заполняет поля ниже."),
    TR("- Düşük kayıt oranını korur; Orta çok odalı iç mekânlar için.\n- Yüksek güçlü tekrarlar için; az örtüşmede daha az resim kaydedebilir. Alt alanları doldurur."));

SS_MSG(colmap_match_ratio,
    EN("Match ratio test (0 = default 0.8)"),
    JA("対応の比率テスト（0 = 既定の 0.8）"),
    ZH_HANS("匹配比率检验（0 = 默认 0.8）"),
    ZH_HANT("對應比率檢驗（0 = 預設 0.8）"),
    KO("대응 비율 검사(0 = 기본값 0.8)"),
    DE("Verhältnistest (0 = Standard 0.8)"),
    FR("Test du rapport (0 = 0.8 par défaut)"),
    ES("Prueba de razón (0 = 0.8 por defecto)"),
    PT("Teste de razão (0 = padrão 0.8)"),
    IT("Test del rapporto (0 = predefinito 0.8)"),
    NL("Verhoudingstest (0 = standaard 0.8)"),
    RU("Тест отношения (0 = по умолчанию 0.8)"),
    TR("Oran testi (0 = varsayılan 0.8)"));
SS_MSG(colmap_match_ratio_help,
    EN("SiftMatching.max_ratio: best/second-best match ratio, SIFT only. Lower is stricter; try 0.6–0.7 against false matches in repetitive textures."),
    JA("SiftMatching.max_ratio：最良と次点の一致比率。SIFT専用です。低いほど厳しく、反復模様の誤一致には0.6〜0.7を試してください。"),
    ZH_HANS("SiftMatching.max_ratio：最佳/次佳匹配比，仅用于 SIFT。越低越严格；重复纹理误匹配可试 0.6–0.7。"),
    ZH_HANT("SiftMatching.max_ratio：最佳/次佳匹配比，僅用於 SIFT。越低越嚴格；重複紋理誤匹配可試 0.6–0.7。"),
    KO("SiftMatching.max_ratio: 최선/차선 매칭 비율로 SIFT 전용입니다. 낮을수록 엄격하며 반복 무늬 오매칭에는 0.6~0.7을 시도하세요."),
    DE("SiftMatching.max_ratio: Verhältnis bester/zweitbester Treffer, nur SIFT. Niedriger ist strenger; bei Wiederholungsmustern 0.6–0.7 versuchen."),
    FR("SiftMatching.max_ratio : rapport meilleur/second résultat, SIFT seul. Plus bas est plus strict ; essayer 0.6–0.7 contre les faux liens répétitifs."),
    ES("SiftMatching.max_ratio: relación mejor/segundo acierto, solo SIFT. Menor es más estricto; prueba 0.6–0.7 contra falsos pares por repetición."),
    PT("SiftMatching.max_ratio: razão melhor/segundo resultado, só SIFT. Menor é mais estrito; tente 0.6–0.7 contra falsos pares por repetição."),
    IT("SiftMatching.max_ratio: rapporto migliore/secondo risultato, solo SIFT. Minore è più severo; provare 0.6–0.7 contro falsi abbinamenti ripetitivi."),
    NL("SiftMatching.max_ratio: verhouding beste/tweede treffer, alleen SIFT. Lager is strenger; probeer 0.6–0.7 bij foute koppelingen door herhaling."),
    RU("SiftMatching.max_ratio: отношение лучшего совпадения ко второму, только SIFT. Ниже — строже; при повторяющихся текстурах попробуйте 0.6–0.7."),
    TR("SiftMatching.max_ratio: en iyi/ikinci eşleşme oranı, yalnız SIFT. Düşük daha katıdır; tekrarlı dokularda yanlış eşleşmeye karşı 0.6–0.7 deneyin."));

SS_MSG(colmap_min_inliers_pair,
    EN("Min inliers per pair (0 = default 15)"),
    JA("ペアあたりの最小インライア数（0 = 既定の 15）"),
    ZH_HANS("每对图像的最少内点数（0 = 默认 15）"),
    ZH_HANT("每對影像的最少內點數（0 = 預設 15）"),
    KO("쌍당 최소 인라이어 수(0 = 기본값 15)"),
    DE("Mindestzahl Inlier je Paar (0 = Standard 15)"),
    FR("Inliers minimaux par paire (0 = 15 par défaut)"),
    ES("Inliers mínimos por par (0 = 15 por defecto)"),
    PT("Inliers mínimos por par (0 = padrão 15)"),
    IT("Inlier minimi per coppia (0 = predefinito 15)"),
    NL("Minimum aantal inliers per paar (0 = standaard 15)"),
    RU("Минимум инлаеров на пару (0 = по умолчанию 15)"),
    TR("Çift başına en az inlier (0 = varsayılan 15)"));
SS_MSG(colmap_min_inliers_pair_help,
    EN("TwoViewGeometry.min_num_inliers: rejects pairs below this geometric inlier count. Raise to 50–100 to exclude weak false links between similar areas."),
    JA("TwoViewGeometry.min_num_inliers：幾何インライア数が未満のペアを除外します。似た領域間の弱い誤一致を防ぐには50〜100に上げます。"),
    ZH_HANS("TwoViewGeometry.min_num_inliers：剔除几何内点数不足的图像对。升至 50–100 可过滤相似区域间证据较弱的假连接。"),
    ZH_HANT("TwoViewGeometry.min_num_inliers：剔除幾何內點數不足的影像對。升至 50–100 可過濾相似區域間證據較弱的假連結。"),
    KO("TwoViewGeometry.min_num_inliers: 기하 인라이어 수 미달 쌍을 버립니다. 비슷한 영역의 약한 오연결을 막으려면 50~100으로 높이세요."),
    DE("TwoViewGeometry.min_num_inliers: verwirft Paare unter dieser Inlierzahl. Auf 50–100 erhöhen gegen schwache falsche Links zwischen ähnlichen Bereichen."),
    FR("TwoViewGeometry.min_num_inliers : rejette les paires sous ce nombre d’inliers. Monter à 50–100 contre les liens faibles entre zones semblables."),
    ES("TwoViewGeometry.min_num_inliers: rechaza pares con menos inliers geométricos. Sube a 50–100 contra enlaces débiles falsos entre zonas similares."),
    PT("TwoViewGeometry.min_num_inliers: rejeita pares abaixo deste total de inliers. Suba a 50–100 contra ligações fracas falsas entre zonas semelhantes."),
    IT("TwoViewGeometry.min_num_inliers: scarta coppie sotto questo numero di inlier. Alzare a 50–100 contro legami deboli falsi tra aree simili."),
    NL("TwoViewGeometry.min_num_inliers: verwerpt paren onder dit aantal geometrische inliers. Verhoog naar 50–100 tegen zwakke foute links tussen gelijkende delen."),
    RU("TwoViewGeometry.min_num_inliers: отбрасывает пары ниже числа геометрических инлаеров. Поднимите до 50–100 против слабых ложных связей похожих зон."),
    TR("TwoViewGeometry.min_num_inliers: bu geometrik içnokta sayısından az olan çiftleri eler. Benzer alanlar arasında zayıf yanlış bağlara karşı 50–100 yapın."));

SS_MSG(colmap_min_inliers_reg,
    EN("Min inliers to register (0 = default 30)"),
    JA("登録に必要な最小インライア数（0 = 既定の 30）"),
    ZH_HANS("配准所需的最少内点数（0 = 默认 30）"),
    ZH_HANT("註冊所需的最少內點數（0 = 預設 30）"),
    KO("등록에 필요한 최소 인라이어 수(0 = 기본값 30)"),
    DE("Mindestzahl Inlier zum Registrieren (0 = Standard 30)"),
    FR("Inliers minimaux pour enregistrer (0 = 30 par défaut)"),
    ES("Inliers mínimos para registrar (0 = 30 por defecto)"),
    PT("Inliers mínimos para registrar (0 = padrão 30)"),
    IT("Inlier minimi per registrare (0 = predefinito 30)"),
    NL("Minimum aantal inliers om te registreren (0 = standaard 30)"),
    RU("Минимум инлаеров для регистрации (0 = по умолчанию 30)"),
    TR("Kayıt için en az inlier (0 = varsayılan 30)"));
SS_MSG(colmap_min_inliers_reg_help,
    EN("Mapper.abs_pose_min_num_inliers: minimum absolute-pose inliers to register an image into the model.\nRaise to 50-100 to stop images from registering onto the wrong (similar-looking) part of the scene."),
    JA("Mapper.abs_pose_min_num_inliers です。 画像をモデルに登録するのに必要な、絶対姿勢のインライアの最小数です。\n50〜100 に上げると、見た目の似た別の場所に画像が登録されてしまうのを防げます。"),
    ZH_HANS("即 Mapper.abs_pose_min_num_inliers：把一张图像配准进模型所需的绝对位姿内点下限。调到 50～100 可以避免图像被配准到场景中看起来相似的错误位置。"),
    ZH_HANT("即 Mapper.abs_pose_min_num_inliers：把一張影像註冊進模型所需的絕對姿態內點下限。調到 50～100 可以避免影像被註冊到場景中看起來相似的錯誤位置。"),
    KO("Mapper.abs_pose_min_num_inliers입니다. 이미지를 모델에 등록하는 데 필요한 절대 자세 인라이어의 최소 개수입니다.\n50~100으로 올리면 비슷해 보이는 엉뚱한 위치에 이미지가 등록되는 것을 막을 수 있습니다."),
    DE("Mapper.abs_pose_min_num_inliers: Mindestzahl Inlier der absoluten Pose, um ein Bild ins Modell aufzunehmen.\nAuf 50-100 anheben, damit Bilder nicht am falschen (ähnlich aussehenden) Ort der Szene landen."),
    FR("Mapper.abs_pose_min_num_inliers : nombre minimal d'inliers de pose absolue pour enregistrer une image dans le modèle.\nMonter à 50-100 pour empêcher qu'une image s'enregistre au mauvais endroit, d'aspect semblable."),
    ES("Mapper.abs_pose_min_num_inliers: mínimo de inliers de pose absoluta para registrar una imagen en el modelo.\nSúbelo a 50-100 para que las imágenes no se registren en la parte equivocada (de aspecto parecido) de la escena."),
    PT("Mapper.abs_pose_min_num_inliers: mínimo de inliers de pose absoluta para registrar uma imagem no modelo.\nAumente para 50-100 para impedir que imagens sejam registradas na parte errada (parecida) da cena."),
    IT("Mapper.abs_pose_min_num_inliers: inlier minimi per registrare una posa. Alzare a 50–100 per evitare registrazioni su parti simili ma sbagliate."),
    NL("Mapper.abs_pose_min_num_inliers: minimaal aantal inliers voor een pose. Verhoog naar 50–100 om registratie op verkeerde, gelijkende delen te voorkomen."),
    RU("Mapper.abs_pose_min_num_inliers: минимум инлаеров абсолютной позы, чтобы зарегистрировать снимок в модели.\nПоднимите до 50-100, чтобы снимки не регистрировались на похожем, но неверном участке сцены."),
    TR("Mapper.abs_pose_min_num_inliers: bir görüntüyü modele kaydetmek için gereken en az mutlak duruş inlier sayısı.\n50-100'e çıkarmak, görüntülerin sahnenin benzer görünen yanlış bölümüne kaydedilmesini engeller."));

SS_MSG(colmap_min_inlier_ratio,
    EN("Min inlier ratio to register (0 = default 0.25)"),
    JA("登録に必要な最小インライア比（0 = 既定の 0.25）"),
    ZH_HANS("配准所需的最小内点比例（0 = 默认 0.25）"),
    ZH_HANT("註冊所需的最小內點比例（0 = 預設 0.25）"),
    KO("등록에 필요한 최소 인라이어 비율(0 = 기본값 0.25)"),
    DE("Mindest-Inlier-Anteil zum Registrieren (0 = Standard 0.25)"),
    FR("Taux d'inliers minimal pour enregistrer (0 = 0.25 par défaut)"),
    ES("Proporción mínima de inliers para registrar (0 = 0.25 por defecto)"),
    PT("Proporção mínima de inliers para registrar (0 = padrão 0.25)"),
    IT("Frazione minima di inlier per registrare (0 = predefinito 0.25)"),
    NL("Minimale inlier-verhouding om te registreren (0 = standaard 0.25)"),
    RU("Минимальная доля инлаеров для регистрации (0 = по умолчанию 0.25)"),
    TR("Kayıt için en az inlier oranı (0 = varsayılan 0.25)"));
SS_MSG(colmap_min_inlier_ratio_help,
    EN("Mapper.abs_pose_min_inlier_ratio: minimum fraction of 2D-3D "
       "correspondences that must be pose inliers. Try 0.35-0.5 for stricter "
       "registration."),
    JA("Mapper.abs_pose_min_inlier_ratio です。2D-3D の対応のうち、姿勢の"
       "インライアでなければならない割合の下限です。登録を厳しくするなら "
       "0.35〜0.5 を試してください。"),
    ZH_HANS("即 Mapper.abs_pose_min_inlier_ratio：2D-3D 对应中必须是位姿内点的最小"
            "比例。想让配准更严格可以试 0.35～0.5。"),
    ZH_HANT("即 Mapper.abs_pose_min_inlier_ratio：2D-3D 對應中必須是姿態內點的最小"
            "比例。想讓註冊更嚴格可以試 0.35～0.5。"),
    KO("Mapper.abs_pose_min_inlier_ratio입니다. 2D-3D 대응 가운데 자세 인라이어여야 "
       "하는 최소 비율입니다. 등록을 더 엄격하게 하려면 0.35~0.5를 시도해 보세요."),
    DE("Mapper.abs_pose_min_inlier_ratio: Mindestanteil der 2D-3D-Zuordnungen, "
       "die Pose-Inlier sein müssen. Für strengere Registrierung 0.35-0.5 "
       "versuchen."),
    FR("Mapper.abs_pose_min_inlier_ratio : fraction minimale des "
       "correspondances 2D-3D qui doivent être des inliers de pose. Essayer "
       "0.35-0.5 pour un enregistrement plus strict."),
    ES("Mapper.abs_pose_min_inlier_ratio: fracción mínima de correspondencias "
       "2D-3D que deben ser inliers de pose. Prueba 0.35-0.5 para un registro "
       "más estricto."),
    PT("Mapper.abs_pose_min_inlier_ratio: fração mínima das correspondências "
       "2D-3D que precisam ser inliers de pose. Tente 0.35-0.5 para um registro "
       "mais rígido."),
    IT("Mapper.abs_pose_min_inlier_ratio: frazione minima delle corrispondenze "
       "2D-3D che devono essere inlier della posa. Prova 0.35-0.5 per una "
       "registrazione più severa."),
    NL("Mapper.abs_pose_min_inlier_ratio: minimale fractie van de "
       "2D-3D-overeenkomsten die pose-inliers moeten zijn. Probeer 0.35-0.5 "
       "voor strengere registratie."),
    RU("Mapper.abs_pose_min_inlier_ratio: минимальная доля 2D-3D соответствий, "
       "которые должны быть инлаерами позы. Для более строгой регистрации "
       "попробуйте 0.35-0.5."),
    TR("Mapper.abs_pose_min_inlier_ratio: 2B-3B karşılıklarının duruş inlier'ı "
       "olması gereken en düşük oranı. Daha katı kayıt için 0.35-0.5 deneyin."));

SS_MSG(colmap_max_reg_error,
    EN("Max registration error px (0 = default 12)"),
    JA("登録時の最大誤差（px、0 = 既定の 12）"),
    ZH_HANS("配准误差上限（像素，0 = 默认 12）"),
    ZH_HANT("註冊誤差上限（像素，0 = 預設 12）"),
    KO("등록 오차 상한(픽셀, 0 = 기본값 12)"),
    DE("Maximaler Registrierungsfehler px (0 = Standard 12)"),
    FR("Erreur d'enregistrement maximale px (0 = 12 par défaut)"),
    ES("Error máximo de registro px (0 = 12 por defecto)"),
    PT("Erro máximo de registro px (0 = padrão 12)"),
    IT("Errore massimo di registrazione px (0 = predefinito 12)"),
    NL("Maximale registratiefout px (0 = standaard 12)"),
    RU("Предел ошибки регистрации, px (0 = по умолчанию 12)"),
    TR("En büyük kayıt hatası px (0 = varsayılan 12)"));
SS_MSG(colmap_max_reg_error_help,
    EN("Mapper.abs_pose_max_error: reprojection error threshold (px) for absolute-pose RANSAC when registering images. Lower (6-8) = stricter; combine with the inlier thresholds above."),
    JA("Mapper.abs_pose_max_error です。 画像を登録するときの絶対姿勢 RANSAC の再投影誤差のしきい値（px）です。\n小さいほど（6〜8）厳しくなります。 上のインライアのしきい値と組み合わせて使ってください。"),
    ZH_HANS("即 Mapper.abs_pose_max_error：配准图像时绝对位姿 RANSAC 的重投影误差阈值（像素）。调小（6～8）更严格；配合上面的内点阈值一起用。"),
    ZH_HANT("即 Mapper.abs_pose_max_error：註冊影像時絕對姿態 RANSAC 的重投影誤差門檻（像素）。調小（6～8）更嚴格；搭配上面的內點門檻一起用。"),
    KO("Mapper.abs_pose_max_error입니다. 이미지를 등록할 때 절대 자세 RANSAC의 재투영 오차 임계값(픽셀)입니다. 작을수록(6~8) 엄격합니다. 위의 인라이어 임계값과 함께 쓰세요."),
    DE("Mapper.abs_pose_max_error: Rückprojektionsfehler (px) für Pose-RANSAC bei der Registrierung. Niedriger (6–8) ist strenger; mit den Inlier-Schwellen kombinieren."),
    FR("Mapper.abs_pose_max_error : seuil d'erreur de reprojection (px) du RANSAC de pose absolue lors de l'enregistrement.\nPlus bas (6-8) = plus strict ; à combiner avec les seuils d'inliers ci-dessus."),
    ES("Mapper.abs_pose_max_error: umbral de error de reproyección (px) del RANSAC de pose absoluta al registrar imágenes.\nMás bajo (6-8) es más estricto; combínalo con los umbrales de inliers de arriba."),
    PT("Mapper.abs_pose_max_error: limiar do erro de reprojeção (px) do RANSAC de pose absoluta ao registrar imagens.\nMais baixo (6-8) é mais rígido; combine com os limiares de inliers acima."),
    IT("Mapper.abs_pose_max_error: errore di riproiezione (px) per RANSAC della posa in registrazione. Più basso (6–8) è più severo; combinare con le soglie inlier."),
    NL("Mapper.abs_pose_max_error: herprojectiefout (px) voor pose-RANSAC bij registratie. Lager (6–8) is strenger; combineer met de inlier-drempels."),
    RU("Mapper.abs_pose_max_error: порог ошибки репроекции (px) для RANSAC абсолютной позы при регистрации снимков. Меньше (6-8) -- строже; используйте вместе с порогами инлаеров выше."),
    TR("Mapper.abs_pose_max_error: görüntüleri kaydederken mutlak duruş RANSAC'ının yeniden izdüşüm hatası eşiği (px).\nDüşük olan (6-8) daha katıdır; yukarıdaki inlier eşikleriyle birlikte kullanın."));

SS_MSG(colmap_gpu_ba,
    EN("GPU bundle adjustment"),
    JA("GPU バンドル調整"),
    ZH_HANS("GPU 光束法平差"),
    ZH_HANT("GPU 光束法平差"),
    KO("GPU 번들 조정"),
    DE("Bündelausgleichung auf der GPU"),
    FR("Ajustement de faisceaux sur GPU"),
    ES("Ajuste de haces en la GPU"),
    PT("Ajuste de feixes na GPU"),
    IT("Bundle adjustment su GPU"),
    NL("Bundelaanpassing op de GPU"),
    RU("Уравнивание связок на GPU"),
    TR("GPU'da demet düzeltmesi"));
SS_MSG(colmap_gpu_ba_help,
    EN("Mapper.ba_use_gpu."),
    JA("Mapper.ba_use_gpu です。"),
    ZH_HANS("即 Mapper.ba_use_gpu。"),
    ZH_HANT("即 Mapper.ba_use_gpu。"),
    KO("Mapper.ba_use_gpu입니다."),
    DE("Mapper.ba_use_gpu."),
    FR("Mapper.ba_use_gpu."),
    ES("Mapper.ba_use_gpu."),
    PT("Mapper.ba_use_gpu."),
    IT("Mapper.ba_use_gpu."),
    NL("Mapper.ba_use_gpu."),
    RU("Mapper.ba_use_gpu."),
    TR("Mapper.ba_use_gpu."));
SS_MSG(colmap_gpu_ba_fisheye,
    EN("Mapper.ba_use_gpu -- unavailable: COLMAP's GPU bundle adjustment does "
       "not support fisheye camera models yet."),
    JA("Mapper.ba_use_gpu — 使えません。COLMAP の GPU バンドル調整はまだ魚眼の"
       "カメラモデルに対応していません。"),
    ZH_HANS("即 Mapper.ba_use_gpu——不可用：COLMAP 的 GPU 光束法平差还不支持鱼眼"
            "相机模型。"),
    ZH_HANT("即 Mapper.ba_use_gpu——無法使用：COLMAP 的 GPU 光束法平差還不支援魚眼"
            "相機模型。"),
    KO("Mapper.ba_use_gpu — 쓸 수 없습니다. COLMAP의 GPU 번들 조정은 아직 어안 "
       "카메라 모델을 지원하지 않습니다."),
    DE("Mapper.ba_use_gpu -- nicht verfügbar: COLMAPs Bündelausgleichung auf "
       "der GPU unterstützt noch keine Fisheye-Kameramodelle."),
    FR("Mapper.ba_use_gpu -- indisponible : l'ajustement de faisceaux sur GPU "
       "de COLMAP ne gère pas encore les modèles de caméra fisheye."),
    ES("Mapper.ba_use_gpu -- no disponible: el ajuste de haces en GPU de COLMAP "
       "aún no admite modelos de cámara de ojo de pez."),
    PT("Mapper.ba_use_gpu -- indisponível: o ajuste de feixes na GPU do COLMAP "
       "ainda não aceita modelos de câmera olho de peixe."),
    IT("Mapper.ba_use_gpu -- non disponibile: il bundle adjustment su GPU di "
       "COLMAP non supporta ancora i modelli di camera fisheye."),
    NL("Mapper.ba_use_gpu -- niet beschikbaar: de bundelaanpassing op de GPU "
       "van COLMAP kent nog geen fisheye-cameramodellen."),
    RU("Mapper.ba_use_gpu -- недоступно: уравнивание связок на GPU в COLMAP пока "
       "не поддерживает модели камеры «рыбий глаз»."),
    TR("Mapper.ba_use_gpu -- kullanılamıyor: COLMAP'ın GPU demet düzeltmesi "
       "balıkgözü kamera modellerini henüz desteklemiyor."));

SS_MSG(colmap_merge_models,
    EN("Merge partial models"),
    JA("部分モデルを統合"),
    ZH_HANS("合并局部模型"),
    ZH_HANT("合併局部模型"),
    KO("부분 모델 병합"),
    DE("Teilmodelle zusammenführen"),
    FR("Fusionner les modèles partiels"),
    ES("Fusionar los modelos parciales"),
    PT("Mesclar os modelos parciais"),
    IT("Unire i modelli parziali"),
    NL("Deelmodellen samenvoegen"),
    RU("Объединять частичные модели"),
    TR("Kısmi modelleri birleştir"));
SS_MSG(colmap_merge_models_help,
    EN("Tries colmap model_merger on partial models; keeps a merge only if more images register. Otherwise training uses the largest model."),
    JA("部分モデルにcolmap model_mergerを試し、登録画像が増えた場合のみ結合を残します。それ以外は最大モデルで学習します。"),
    ZH_HANS("对部分模型尝试 colmap model_merger，仅保留注册图像更多的合并结果。否则训练使用最大模型。"),
    ZH_HANT("對部分模型嘗試 colmap model_merger，僅保留註冊影像更多的合併結果。否則訓練使用最大模型。"),
    KO("부분 모델에 colmap model_merger를 시도하며 등록 이미지가 늘 때만 병합을 유지합니다. 아니면 가장 큰 모델로 학습합니다."),
    DE("Versucht colmap model_merger für Teilmodelle; behält nur Zusammenschlüsse mit mehr Bildern. Sonst trainiert das größte Modell."),
    FR("Essaie colmap model_merger sur les modèles partiels ; garde la fusion si plus d’images sont inscrites. Sinon entraîne le plus grand modèle."),
    ES("Prueba colmap model_merger con modelos parciales; conserva la fusión solo si registra más imágenes. Si no, entrena el modelo mayor."),
    PT("Tenta colmap model_merger nos modelos parciais; guarda a fusão só se registar mais imagens. Senão, treina o maior modelo."),
    IT("Prova colmap model_merger sui modelli parziali; tiene la fusione solo se registra più immagini. Altrimenti allena il modello maggiore."),
    NL("Probeert colmap model_merger op deelmodellen; bewaart alleen fusies met meer geregistreerde beelden. Anders traint het grootste model."),
    RU("Пробует colmap model_merger для частей; сохраняет объединение лишь с большим числом снимков. Иначе обучение использует крупнейшую часть."),
    TR("Kısmi modellere colmap model_merger dener; birleşimi yalnız daha çok resim kaydederse tutar. Aksi hâlde en büyük modeli eğitir."));

SS_MSG(colmap_final_ba,
    EN("Final refinement pass"),
    JA("最後の仕上げ"),
    ZH_HANS("最后一遍精修"),
    ZH_HANT("最後一遍精修"),
    KO("마지막 정밀화 단계"),
    DE("Abschließender Feinschliff"),
    FR("Passe d'affinage finale"),
    ES("Pasada de refinado final"),
    PT("Passagem de refino final"),
    IT("Passata di affinamento finale"),
    NL("Laatste verfijningsronde"),
    RU("Финальный проход уточнения"),
    TR("Son iyileştirme geçişi"));
SS_MSG(colmap_final_ba_help,
    EN("Run bundle_adjuster after mapping on the largest (or merged) model, "
       "refining focal length, principal point, and distortion."),
    JA("マッピングのあと、最大の（または統合した）モデルに bundle_adjuster を"
       "かけて、焦点距離・主点・歪みを追い込みます。"),
    ZH_HANS("建图结束后，对最大的（或合并后的）模型运行 bundle_adjuster，进一步"
            "精修焦距、主点和畸变。"),
    ZH_HANT("建圖結束後，對最大的（或合併後的）模型執行 bundle_adjuster，進一步"
            "精修焦距、主點和畸變。"),
    KO("매핑이 끝난 뒤 가장 큰(또는 병합된) 모델에 bundle_adjuster를 돌려 초점 "
       "거리, 주점, 왜곡을 더 다듬습니다."),
    DE("Nach dem Mapping bundle_adjuster auf dem größten (oder vereinten) "
       "Modell laufen lassen und Brennweite, Hauptpunkt und Verzeichnung "
       "nachziehen."),
    FR("Lancer bundle_adjuster après le mapping sur le plus grand modèle (ou "
       "le modèle fusionné), pour affiner distance focale, point principal et "
       "distorsion."),
    ES("Ejecutar bundle_adjuster tras el mapeo sobre el modelo más grande (o el "
       "fusionado), afinando distancia focal, punto principal y distorsión."),
    PT("Executar o bundle_adjuster depois do mapeamento no maior modelo (ou no "
       "fundido), refinando distância focal, ponto principal e distorção."),
    IT("Eseguire bundle_adjuster dopo il mapping sul modello più grande (o su "
       "quello fuso), affinando focale, punto principale e distorsione."),
    NL("Na het mappen bundle_adjuster draaien op het grootste (of "
       "samengevoegde) model, en zo brandpuntsafstand, hoofdpunt en "
       "vertekening bijstellen."),
    RU("После реконструкции запустить bundle_adjuster на самой большой (или "
       "объединённой) модели, уточняя фокусное расстояние, главную точку и "
       "дисторсию."),
    TR("Haritalamadan sonra en büyük (ya da birleştirilmiş) modelde "
       "bundle_adjuster çalıştırıp odak uzaklığını, ana noktayı ve bozulmayı "
       "iyileştirir."));

SS_MSG(colmap_vocab_tree_hint,
    EN("vocabulary tree (auto find/download)"),
    JA("ボキャブラリツリー（自動で探す／取得する）"),
    ZH_HANS("词汇树（自动查找／下载）"),
    ZH_HANT("詞彙樹（自動尋找／下載）"),
    KO("어휘 트리(자동으로 찾기/내려받기)"),
    DE("Vokabularbaum (automatisch suchen/laden)"),
    FR("arbre de vocabulaire (recherche/téléchargement auto)"),
    ES("árbol de vocabulario (buscar/descargar automáticamente)"),
    PT("árvore de vocabulário (localizar/baixar automaticamente)"),
    IT("albero di vocabolario (ricerca/download automatici)"),
    NL("vocabulaireboom (automatisch zoeken/downloaden)"),
    RU("словарное дерево (найти/скачать автоматически)"),
    TR("sözcük ağacı (otomatik bul/indir)"));
SS_MSG(colmap_vocab_tree,
    EN("vocab tree"),
    JA("ボキャブラリツリー"),
    ZH_HANS("词汇树"),
    ZH_HANT("詞彙樹"),
    KO("어휘 트리"),
    DE("Vokabularbaum"),
    FR("arbre de vocabulaire"),
    ES("árbol de vocabulario"),
    PT("árvore de vocabulário"),
    IT("albero di vocabolario"),
    NL("vocabulaireboom"),
    RU("словарное дерево"),
    TR("sözcük ağacı"));

// ===========================================================================
// Tool locations
// ===========================================================================

SS_MSG(section_tool_locations,
    EN("Tool locations"), JA("外部ツールの場所"), ZH_HANS("工具位置"), ZH_HANT("工具位置"),
    KO("도구 위치"),      DE("Speicherorte der Werkzeuge"), FR("Emplacement des outils"),
    ES("Ubicación de las herramientas"), PT("Local das ferramentas"),
    IT("Percorsi degli strumenti"), NL("Locatie van hulpprogramma's"),
    RU("Расположение инструментов"), TR("Araç konumları"));

SS_MSG(colmap_executable,
    EN("colmap executable"),
    JA("colmap の実行ファイル"),
    ZH_HANS("colmap 可执行文件"),
    ZH_HANT("colmap 執行檔"),
    KO("colmap 실행 파일"),
    DE("colmap-Programmdatei"),
    FR("exécutable colmap"),
    ES("ejecutable de colmap"),
    PT("executável do colmap"),
    IT("eseguibile colmap"),
    NL("colmap-programma"),
    RU("исполняемый файл colmap"),
    TR("colmap çalıştırılabiliri"));

SS_MSG(ffmpeg_executable,
    EN("ffmpeg executable"),
    JA("ffmpeg の実行ファイル"),
    ZH_HANS("ffmpeg 可执行文件"),
    ZH_HANT("ffmpeg 執行檔"),
    KO("ffmpeg 실행 파일"),
    DE("ffmpeg-Programmdatei"),
    FR("exécutable ffmpeg"),
    ES("ejecutable de ffmpeg"),
    PT("executável do ffmpeg"),
    IT("eseguibile ffmpeg"),
    NL("ffmpeg-programma"),
    RU("исполняемый файл ffmpeg"),
    TR("ffmpeg çalıştırılabiliri"));

SS_MSG(ffmpeg_executable_help_fallback,
    EN("Only used when frame extraction falls back to ffmpeg."),
    JA("フレームの切り出しが ffmpeg にフォールバックしたときだけ使われます。"),
    ZH_HANS("只有当抽帧回退到 ffmpeg 时才会用到。"),
    ZH_HANT("只有當抽格回退到 ffmpeg 時才會用到。"),
    KO("프레임 추출이 ffmpeg으로 넘어갈 때만 쓰입니다."),
    DE("Wird nur benutzt, wenn die Bildextraktion auf ffmpeg zurückfällt."),
    FR("Utilisé seulement quand l'extraction des images retombe sur ffmpeg."),
    ES("Solo se usa cuando la extracción de fotogramas recurre a ffmpeg."),
    PT("Só é usado quando a extração de quadros recorre ao ffmpeg."),
    IT("Usato solo quando l'estrazione dei fotogrammi ripiega su ffmpeg."),
    NL("Wordt alleen gebruikt als het uithalen van beelden terugvalt op "
       "ffmpeg."),
    RU("Используется, только если извлечение кадров переходит на ffmpeg."),
    TR("Yalnızca kare çıkarma ffmpeg'e düştüğünde kullanılır."));

SS_MSG(ffmpeg_executable_help_always,
    EN("Used to extract frames from video."),
    JA("動画からフレームを切り出すのに使われます。"),
    ZH_HANS("用于从视频中抽取帧。"),
    ZH_HANT("用於從影片中抽取影格。"),
    KO("동영상에서 프레임을 뽑는 데 쓰입니다."),
    DE("Wird zum Extrahieren der Bilder aus dem Video benutzt."),
    FR("Sert à extraire les images de la vidéo."),
    ES("Se usa para extraer fotogramas del vídeo."),
    PT("Usado para extrair quadros do vídeo."),
    IT("Serve a estrarre i fotogrammi dal video."),
    NL("Wordt gebruikt om beelden uit de video te halen."),
    RU("Используется для извлечения кадров из видео."),
    TR("Videodan kare çıkarmak için kullanılır."));

// ===========================================================================
// The segmentation checkpoints (src/app/gui/ModelCache.cpp)
//
// Blurbs compare speeds rather than quote one machine's milliseconds; keep
// the ratios intact when translating.
// ===========================================================================

SS_MSG(model_sam3_label,
    EN("SAM 3 (most accurate for text prompts)"),
    JA("SAM 3（テキスト指定で最も正確）"),
    ZH_HANS("SAM 3（文字提示最准确）"),
    ZH_HANT("SAM 3（文字提示最準確）"),
    KO("SAM 3(텍스트 프롬프트에 가장 정확)"),
    DE("SAM 3 (am genauesten bei Texteingaben)"),
    FR("SAM 3 (le plus précis pour les invites textuelles)"),
    ES("SAM 3 (el más preciso con indicaciones de texto)"),
    PT("SAM 3 (o mais preciso com comandos de texto)"),
    IT("SAM 3 (il più preciso con il testo)"),
    NL("SAM 3 (nauwkeurigst voor tekstprompts)"),
    RU("SAM 3 (точнее всех по текстовым запросам)"),
    TR("SAM 3 (metin istemlerinde en doğru)"));

SS_MSG(model_sam3_blurb,
    EN("Direct text prompts, strongest detection. 707 MB, ~2 GB VRAM. One prompt costs ~3× SAM 2.1 + DINO; each extra phrase adds about as much time again."),
    JA("テキストを直接使え、最もよく検出します。707 MB、VRAM約2 GB。1指示はSAM 2.1＋DINOの約3倍、語句追加ごとに同程度の時間が増えます。"),
    ZH_HANS("直接理解文本，检测最强。707 MB，约 2 GB 显存。一个提示约为 SAM 2.1＋DINO 的 3 倍耗时；每加一个短语再增加相近耗时。"),
    ZH_HANT("直接理解文字，偵測最強。707 MB，約 2 GB 顯存。一個提示約為 SAM 2.1＋DINO 的 3 倍耗時；每加一個短語再增加相近耗時。"),
    KO("직접 텍스트 입력, 가장 강한 검출. 707 MB, VRAM 약 2 GB. 한 문구는 SAM 2.1+DINO의 약 3배이며 문구마다 비슷한 시간이 더 듭니다."),
    DE("Direkte Textprompts, stärkste Erkennung. 707 MB, ~2 GB VRAM. Ein Prompt: ~3× SAM 2.1 + DINO; jede weitere Phrase kostet etwa genauso viel zusätzlich."),
    FR("Texte direct, meilleure détection. 707 Mo, ~2 Go VRAM. Une consigne : ~3× SAM 2.1 + DINO ; chaque phrase ajoute à peu près autant de temps."),
    ES("Texto directo, mejor detección. 707 MB, ~2 GB VRAM. Un prompt: ~3× SAM 2.1 + DINO; cada frase extra añade un tiempo similar."),
    PT("Texto direto, melhor deteção. 707 MB, ~2 GB VRAM. Um pedido: ~3× SAM 2.1 + DINO; cada frase extra acrescenta tempo semelhante."),
    IT("Testo diretto, migliore rilevamento. 707 MB, ~2 GB VRAM. Un prompt: ~3× SAM 2.1 + DINO; ogni frase extra aggiunge un tempo simile."),
    NL("Directe tekstprompts, sterkste detectie. 707 MB, ~2 GB VRAM. Eén prompt: ~3× SAM 2.1 + DINO; elke extra zin kost ongeveer evenveel tijd erbij."),
    RU("Прямой текст, лучшее обнаружение. 707 МБ, ~2 ГБ VRAM. Один запрос: ~3× SAM 2.1 + DINO; каждая новая фраза добавляет примерно столько же времени."),
    TR("Doğrudan metin, en güçlü tespit. 707 MB, ~2 GB VRAM. Bir istem: SAM 2.1 + DINO’nun ~3 katı; her ek ifade yaklaşık aynı süreyi ekler."));

SS_MSG(model_sam3_f16_label,
    EN("SAM 3, full precision"),
    JA("SAM 3、フル精度"), ZH_HANS("SAM 3，全精度"), ZH_HANT("SAM 3，全精度"),
    KO("SAM 3, 전체 정밀도"), DE("SAM 3, volle Genauigkeit"),
    FR("SAM 3, pleine précision"), ES("SAM 3, precisión completa"),
    PT("SAM 3, precisão total"), IT("SAM 3, piena precisione"),
    NL("SAM 3, volledige precisie"), RU("SAM 3, полная точность"),
    TR("SAM 3, tam duyarlık"));

SS_MSG(model_sam3_f16_blurb,
    EN("The same model without file quantization. Slightly better masks, much "
       "bigger download, same speed."),
    JA("同じモデルのファイル量子化なし版です。マスクはわずかに良くなり、"
       "ダウンロードはずっと大きく、速度は同じです。"),
    ZH_HANS("同一个模型，不做文件量化。蒙版略好，下载大得多，速度相同。"),
    ZH_HANT("同一個模型，不做檔案量化。遮罩略好，下載大得多，速度相同。"),
    KO("같은 모델의 파일 양자화를 하지 않은 판입니다. 마스크가 조금 낫고, "
       "내려받기는 훨씬 크며, 속도는 같습니다."),
    DE("Dasselbe Modell ohne Dateiquantisierung. Etwas bessere Masken, viel "
       "größerer Download, gleiche Geschwindigkeit."),
    FR("Le même modèle sans quantification du fichier. Masques légèrement "
       "meilleurs, téléchargement bien plus lourd, même vitesse."),
    ES("El mismo modelo sin cuantización del archivo. Máscaras algo mejores, "
       "descarga mucho mayor, misma velocidad."),
    PT("O mesmo modelo sem quantização do arquivo. Máscaras um pouco "
       "melhores, download bem maior, mesma velocidade."),
    IT("Lo stesso modello senza quantizzazione del file. Maschere un po' "
       "migliori, scaricamento molto più grande, stessa velocità."),
    NL("Hetzelfde model zonder bestandskwantisatie. Iets betere maskers, veel "
       "grotere download, dezelfde snelheid."),
    RU("Та же модель без квантования файла. Маски чуть лучше, загрузка "
       "гораздо больше, скорость та же."),
    TR("Aynı modelin dosya nicemlemesi olmayan hâli. Maskeler biraz daha iyi, "
       "indirme çok daha büyük, hız aynı."));

SS_MSG(model_sam21_large_label,
    EN("SAM 2.1 Large"),  JA("SAM 2.1 Large"), ZH_HANS("SAM 2.1 Large"),
    ZH_HANT("SAM 2.1 Large"), KO("SAM 2.1 Large"), DE("SAM 2.1 Large"),
    FR("SAM 2.1 Large"),  ES("SAM 2.1 Large"), PT("SAM 2.1 Large"),
    IT("SAM 2.1 Large"),  NL("SAM 2.1 Large"), RU("SAM 2.1 Large"),
    TR("SAM 2.1 Large"));

SS_MSG(model_sam21_large_blurb,
    EN("The most accurate SAM 2.1, and the one to pick for thin structure -- "
       "railings, wires and cables, foliage. About 1.8x the time of Small. "
       "Apache-2.0."),
    JA("SAM 2.1 の中でいちばん正確で、手すり、電線やケーブル、葉のような細い"
       "構造にはこれを選んでください。Small のおよそ 1.8 倍の時間がかかり"
       "ます。Apache-2.0。"),
    ZH_HANS("SAM 2.1 中最准确的，栏杆、电线电缆、枝叶这类细结构就选它。耗时约为 "
            "Small 的 1.8 倍。Apache-2.0。"),
    ZH_HANT("SAM 2.1 中最準確的，欄杆、電線電纜、枝葉這類細結構就選它。耗時約為 "
            "Small 的 1.8 倍。Apache-2.0。"),
    KO("SAM 2.1 중 가장 정확하며 난간, 전선과 케이블, 잎사귀 같은 가느다란 구조에는 이것을 고르세요. Small의 약 1.8배 "
       "시간이 걸립니다. Apache-2.0."),
    DE("Das genaueste SAM 2.1 und die Wahl für feine Strukturen -- Geländer, "
       "Drähte und Kabel, Laub. Braucht etwa 1,8-mal so lange wie Small. "
       "Apache-2.0."),
    FR("Le plus précis des SAM 2.1 et celui à prendre pour les structures fines "
       "-- garde-corps, fils et câbles, feuillage. Environ 1,8 fois le temps de "
       "Small. Apache-2.0."),
    ES("El SAM 2.1 más preciso y el indicado para estructuras finas: "
       "barandillas, cables y tendidos, follaje. Unas 1,8 veces el tiempo de "
       "Small. Apache-2.0."),
    PT("O SAM 2.1 mais preciso e o indicado para estruturas finas: corrimãos, "
       "fios e cabos, folhagem. Cerca de 1,8 vez o tempo do Small. Apache-2.0."),
    IT("Il SAM 2.1 più preciso e quello da prendere per le strutture sottili: "
       "ringhiere, fili e cavi, fogliame. Circa 1,8 volte il tempo di Small. "
       "Apache-2.0."),
    NL("De nauwkeurigste SAM 2.1 en de keuze voor fijne structuur -- leuningen, "
       "draden en kabels, gebladerte. Ongeveer 1,8 keer de tijd van Small. "
       "Apache-2.0."),
    RU("Самая точная из SAM 2.1 и та, что нужна для тонких структур — перил, "
       "проводов и кабелей, листвы. Примерно в 1,8 раза дольше Small. "
       "Apache-2.0."),
    TR("En doğru SAM 2.1 ve ince yapılar için seçilecek olanı -- korkuluk, tel "
       "ve kablo, yaprak. Small'ın yaklaşık 1,8 katı süre. Apache-2.0."));

SS_MSG(model_sam21_baseplus_label,
    EN("SAM 2.1 Base+"),  JA("SAM 2.1 Base+"), ZH_HANS("SAM 2.1 Base+"),
    ZH_HANT("SAM 2.1 Base+"), KO("SAM 2.1 Base+"), DE("SAM 2.1 Base+"),
    FR("SAM 2.1 Base+"),  ES("SAM 2.1 Base+"), PT("SAM 2.1 Base+"),
    IT("SAM 2.1 Base+"),  NL("SAM 2.1 Base+"), RU("SAM 2.1 Base+"),
    TR("SAM 2.1 Base+"));

SS_MSG(model_sam21_baseplus_blurb,
    EN("Close to Large on most subjects in two thirds of its time. Apache-2.0."),
    JA("ほとんどの被写体で Large に近い結果を、その 3 分の 2 の時間で出"
       "します。Apache-2.0。"),
    ZH_HANS("在多数对象上接近 Large，耗时只有它的三分之二。Apache-2.0。"),
    ZH_HANT("在多數物件上接近 Large，耗時只有它的三分之二。Apache-2.0。"),
    KO("대부분의 피사체에서 Large에 가까운 결과를 그 3분의 2 시간에 냅니다. Apache-2.0."),
    DE("Bei den meisten Motiven nah an Large, in zwei Dritteln seiner Zeit. "
       "Apache-2.0."),
    FR("Proche de Large sur la plupart des sujets, en deux tiers de son temps. "
       "Apache-2.0."),
    ES("Cerca de Large en la mayoría de sujetos, en dos tercios de su tiempo. "
       "Apache-2.0."),
    PT("Perto do Large na maioria dos sujeitos, em dois terços do tempo dele. "
       "Apache-2.0."),
    IT("Vicino a Large su quasi tutti i soggetti, in due terzi del suo tempo. "
       "Apache-2.0."),
    NL("Dicht bij Large op de meeste onderwerpen, in twee derde van de tijd. "
       "Apache-2.0."),
    RU("На большинстве объектов близко к Large за две трети его времени. "
       "Apache-2.0."),
    TR("Çoğu öznede Large'a yakın, onun süresinin üçte ikisinde. Apache-2.0."));

SS_MSG(model_sam21_small_label,
    EN("SAM 2.1 Small"),  JA("SAM 2.1 Small"), ZH_HANS("SAM 2.1 Small"),
    ZH_HANT("SAM 2.1 Small"), KO("SAM 2.1 Small"), DE("SAM 2.1 Small"),
    FR("SAM 2.1 Small"),  ES("SAM 2.1 Small"), PT("SAM 2.1 Small"),
    IT("SAM 2.1 Small"),  NL("SAM 2.1 Small"), RU("SAM 2.1 Small"),
    TR("SAM 2.1 Small"));

SS_MSG(model_sam21_small_blurb,
    EN("The best speed-for-quality of the four: below Large a frame is mostly tracking, which does not care how big the backbone is. Apache-2.0."),
    JA("4つの中で速度対品質がいちばん良い選択です。Large 未満ではフレームの処理はほとんど追跡で、バックボーンの大きさはあまり効きません。Apache-2.0。"),
    ZH_HANS("四者中速度与质量的平衡最好：在 Large 以下，每帧的工作主要是跟踪，而跟踪并不在乎主干有多大。Apache-2.0。"),
    ZH_HANT("四者中速度與品質的平衡最好：在 Large 以下，每格的工作主要是追蹤，而追蹤並不在乎骨幹有多大。Apache-2.0。"),
    KO("넷 중 속도 대비 품질이 가장 좋습니다. Large 아래에서는 프레임 처리의 대부분이 추적이고, 추적은 백본 크기에 크게 좌우되지 않습니다. Apache-2.0."),
    DE("Das beste Verhältnis von Tempo zu Qualität der vier: unterhalb von Large ist ein Bild vor allem Nachverfolgung, und der ist die Größe des Rückgrats fast egal. Apache-2.0."),
    FR("Le meilleur rapport vitesse/qualité des quatre : en dessous de Large, le travail par image est surtout du suivi, qui se moque de la taille du réseau. Apache-2.0."),
    ES("La mejor relación velocidad-calidad. Por debajo de Large domina el seguimiento, cuyo coste no depende del tamaño de la red. Apache-2.0."),
    PT("A melhor relação velocidade/qualidade dos quatro: abaixo do Large, o trabalho por quadro é sobretudo rastreamento, que não liga para o tamanho da rede. Apache-2.0."),
    IT("Il miglior rapporto velocità/qualità dei quattro: sotto Large il lavoro per fotogramma è soprattutto inseguimento, a cui la dimensione della rete importa poco. Apache-2.0."),
    NL("De beste verhouding snelheid/kwaliteit van de vier: onder Large is het werk per beeld vooral volgen, en dat maalt niet om de grootte van het netwerk. Apache-2.0."),
    RU("Лучшее соотношение скорости и качества из четырёх: ниже Large работа над кадром — это в основном отслеживание, которому размер сети почти безразличен. Apache-2.0."),
    TR("Dördü arasında hız/kalite dengesi en iyi olanı: Large'ın altında kare başına iş çoğunlukla izlemedir ve izleme omurganın büyüklüğüne pek aldırmaz. Apache-2.0."));

SS_MSG(model_sam21_tiny_label,
    EN("SAM 2.1 Tiny"),
    JA("SAM 2.1 Tiny"),
    ZH_HANS("SAM 2.1 Tiny"),
    ZH_HANT("SAM 2.1 Tiny"),
    KO("SAM 2.1 Tiny"),
    DE("SAM 2.1 Tiny"),
    FR("SAM 2.1 Tiny"),
    ES("SAM 2.1 Tiny"),
    PT("SAM 2.1 Tiny"),
    IT("SAM 2.1 Tiny"),
    NL("SAM 2.1 Tiny"),
    RU("SAM 2.1 Tiny"),
    TR("SAM 2.1 Tiny"));

SS_MSG(model_sam21_tiny_blurb,
    EN("76 MB. Only about 4% quicker than Small, and it loses thin structure "
       "first -- take it for the download size, not the speed. Apache-2.0."),
    JA("76 MB。Small より 4% ほど速いだけですし、細い構造から先に失わ"
       "れます。速度ではなくダウンロードサイズのために選んでください。Apache-"
       "2.0。"),
    ZH_HANS("76 MB。只比 Small 快 4% 左右，而且最先丢失细结构——选它是为"
            "了下载体积，不是速度。Apache-2.0。"),
    ZH_HANT("76 MB。只比 Small 快 4% 左右，而且最先丟失細結構——選它是為"
            "了下載體積，不是速度。Apache-2.0。"),
    KO("76 MB. Small보다 4% 정도 빠를 뿐이고 가느다란 구조를 가장 먼저 잃습니다. 속도가 아니라 내려받기 크기 때문에 "
       "고르세요. Apache-2.0."),
    DE("76 MB. Nur etwa 4 % schneller als Small, und feine Strukturen gehen "
       "zuerst verloren -- wegen der Downloadgröße nehmen, nicht wegen des "
       "Tempos. Apache-2.0."),
    FR("76 Mo. À peine 4 % plus rapide que Small, et c'est lui qui perd les "
       "structures fines en premier -- à prendre pour la taille du "
       "téléchargement, pas pour la vitesse. Apache-2.0."),
    ES("76 MB. Apenas un 4 % más rápido que Small, y es el primero en perder las "
       "estructuras finas: tómelo por el tamaño de descarga, no por la "
       "velocidad. Apache-2.0."),
    PT("76 MB. Apenas 4% mais rápido que o Small, e é o primeiro a perder "
       "estruturas finas -- escolha pelo tamanho do download, não pela "
       "velocidade. Apache-2.0."),
    IT("76 MB. Appena il 4% più veloce di Small, ed è il primo a perdere le "
       "strutture sottili: lo prenda per la dimensione del download, non per la "
       "velocità. Apache-2.0."),
    NL("76 MB. Maar 4% sneller dan Small, en het verliest fijne structuur het "
       "eerst -- neem het om de downloadgrootte, niet om de snelheid. "
       "Apache-2.0."),
    RU("76 МБ. Быстрее Small всего на 4 % и первой теряет тонкие структуры — "
       "берите её ради размера загрузки, а не скорости. Apache-2.0."),
    TR("76 MB. Small'dan yalnızca %4 hızlı ve ince yapıyı ilk kaybeden o -- "
       "indirme boyutu için alın, hız için değil. Apache-2.0."));

// LEGAL -- human review in every language, see the block below.
SS_MSG(license_sam3_title,
    EN("SAM 3 License (Meta)"),
    JA("SAM 3 ライセンス（Meta）"),
    ZH_HANS("SAM 3 许可协议（Meta）"),
    ZH_HANT("SAM 3 授權條款（Meta）"),
    KO("SAM 3 라이선스(Meta)"),
    DE("SAM-3-Lizenz (Meta)"),
    FR("Licence SAM 3 (Meta)"),
    ES("Licencia de SAM 3 (Meta)"),
    PT("Licença do SAM 3 (Meta)"),
    IT("Licenza SAM 3 (Meta)"),
    NL("SAM 3-licentie (Meta)"),
    RU("Лицензия SAM 3 (Meta)"),
    TR("SAM 3 Lisansı (Meta)"));

SS_MSG(license_sam3_summary,
    EN("SAM 3 is Meta's model, not part of Spirula Studio, and it comes with "
       "its own licence -- which is not a standard one. It is free to use, "
       "including commercially, but only on Meta's terms, so we cannot ship "
       "it with the app or accept them for you.\n\n "
       "Please read it before continuing -- it is the actual "
       "agreement, not this summary of it."),
    JA("SAM 3 は Meta のモデルで、Spirula Studio の一部ではなく、独自の"
       "ライセンスが付いています。それは標準的なライセンスではありません。"
       "商用を含めて無償で使えますが、あくまで Meta の条件のもとでです。"
       "そのため、当アプリに同梱することも、条件への同意を代行することも"
       "できません。\n\n"
       "続ける前にお読みください。実際の契約はこの要約ではなくそちらです。"),
    ZH_HANS("SAM 3 是 Meta 的模型，不属于 Spirula Studio，并且带有它自己的"
            "许可协议——那不是一份标准协议。它可以免费使用，包括商业用途，"
            "但只在 Meta 的条件之下。因此我们既不能随应用一起分发它，"
            "也不能代你接受这些条件。\n\n"
            "请在继续之前阅读它——真正的协议是它，不是这段摘要。"),
    ZH_HANT("SAM 3 是 Meta 的模型，不屬於 Spirula Studio，並且帶有它自己的"
            "授權條款——那不是一份標準條款。它可以免費使用，包括商業用途，"
            "但只在 Meta 的條件之下。因此我們既不能隨應用一起散布它，"
            "也不能代你接受這些條件。\n\n"
            "請在繼續之前閱讀它——真正的協議是它，不是這段摘要。"),
    KO("SAM 3는 Meta의 모델로 Spirula Studio의 일부가 아니며, 자체 라이선스가 "
       "딸려 있습니다. 그것은 표준 라이선스가 아닙니다. 상업적 사용을 포함해 "
       "무료로 쓸 수 있지만 어디까지나 Meta의 조건 아래에서입니다. 그래서 저희는 "
       "이 모델을 앱과 함께 배포할 수도, 조건을 대신 수락할 수도 없습니다.\n\n"
       "계속하기 전에 읽어 주세요. 실제 계약은 이 요약이 아니라 그 문서입니다."),
    DE("SAM 3 ist Metas Modell, nicht Teil von Spirula Studio, und bringt "
       "eine eigene Lizenz mit -- keine übliche. Es ist kostenlos nutzbar, "
       "auch kommerziell, aber nur zu Metas Bedingungen; wir dürfen es daher "
       "weder mit der Anwendung ausliefern noch die Bedingungen für Sie "
       "annehmen.\n\n "
       "Bitte lesen Sie sie, bevor Sie fortfahren -- sie ist die eigentliche "
       "Vereinbarung, nicht diese Zusammenfassung."),
    FR("SAM 3 est le modèle de Meta, il ne fait pas partie de Spirula Studio "
       "et il vient avec sa propre licence -- qui n'est pas une licence "
       "standard. Il est gratuit à utiliser, y compris commercialement, mais "
       "uniquement aux conditions de Meta ; nous ne pouvons donc ni le livrer "
       "avec l'application ni les accepter à votre place.\n\n "
       "Merci de la lire avant de continuer : c'est elle l'accord véritable, "
       "pas ce résumé."),
    ES("SAM 3 es el modelo de Meta, no forma parte de Spirula Studio y viene "
       "con su propia licencia, que no es una licencia estándar. Su uso es "
       "gratuito, también comercial, pero solo en los términos de Meta, así "
       "que no podemos distribuirlo con la aplicación ni aceptarlos por "
       "usted.\n\n "
       "Léala antes de continuar: es ella el acuerdo real, no este resumen."),
    PT("O SAM 3 é o modelo da Meta, não faz parte do Spirula Studio e vem com "
       "a própria licença -- que não é uma licença padrão. É gratuito, "
       "inclusive para uso comercial, mas só nos termos da Meta, então não "
       "podemos distribuí-lo com o aplicativo nem aceitá-los por "
       "você.\n\nLeia-a antes de continuar: é ela o acordo de verdade, não "
       "este resumo."),
    IT("SAM 3 è il modello di Meta, non fa parte di Spirula Studio e ha una "
       "licenza propria, che non è una licenza standard. È gratuito, anche "
       "per uso commerciale, ma solo alle condizioni di Meta: non possiamo "
       "quindi distribuirlo con l'applicazione né accettarle al posto suo.\n\n "
       "La legga prima di proseguire: è lei l'accordo vero, non questo "
       "riassunto."),
    NL("SAM 3 is het model van Meta, hoort niet bij Spirula Studio en komt met "
       "een eigen licentie -- geen standaardlicentie. Het is gratis te "
       "gebruiken, ook commercieel, maar alleen op Meta's voorwaarden; we "
       "mogen het dus niet met de toepassing meeleveren en ze ook niet voor u "
       "aanvaarden.\n\nLees ze voordat u doorgaat: zij vormen de werkelijke "
       "overeenkomst, niet deze samenvatting."),
    RU("SAM 3 — модель Meta, она не входит в Spirula Studio и поставляется со "
       "своей лицензией, а она не стандартная. Пользоваться моделью можно "
       "бесплатно, в том числе коммерчески, но только на условиях Meta, "
       "поэтому мы не вправе ни поставлять её вместе с программой, ни "
       "принимать эти условия за вас.\n\n "
       "Прочитайте её, прежде чем продолжить: настоящее соглашение — это она, "
       "а не данная выжимка."),
    TR("SAM 3 Meta'nın modelidir, Spirula Studio'nun parçası değildir ve kendi "
       "lisansıyla gelir -- bu standart bir lisans değildir. Ticari kullanım "
       "dâhil ücretsizdir, ama yalnızca Meta'nın koşullarıyla; bu yüzden onu "
       "uygulamayla birlikte dağıtamayız ve koşulları sizin adınıza kabul "
       "edemeyiz.\n\nDevam etmeden önce lütfen okuyun -- gerçek sözleşme bu "
       "özet değil, o metindir."));

SS_MSG(license_sam2_title,
    EN("SAM 2.1 License (Meta, Apache-2.0)"),
    JA("SAM 2.1 ライセンス（Meta、Apache-2.0）"),
    ZH_HANS("SAM 2.1 许可协议（Meta，Apache-2.0）"),
    ZH_HANT("SAM 2.1 授權條款（Meta，Apache-2.0）"),
    KO("SAM 2.1 라이선스(Meta, Apache-2.0)"),
    DE("SAM-2.1-Lizenz (Meta, Apache-2.0)"),
    FR("Licence SAM 2.1 (Meta, Apache-2.0)"),
    ES("Licencia de SAM 2.1 (Meta, Apache-2.0)"),
    PT("Licença do SAM 2.1 (Meta, Apache-2.0)"),
    IT("Licenza SAM 2.1 (Meta, Apache-2.0)"),
    NL("SAM 2.1-licentie (Meta, Apache-2.0)"),
    RU("Лицензия SAM 2.1 (Meta, Apache-2.0)"),
    TR("SAM 2.1 Lisansı (Meta, Apache-2.0)"));

SS_MSG(license_sam2_summary,
    EN("SAM 2.1 is Meta's model, released under the Apache 2.0 licence. Downloaded separately to keep the app small."),
    JA("SAM 2.1 は Meta のモデルで、Apache 2.0 ライセンスで公開されています。 アプリを小さく保つため別途取得します。"),
    ZH_HANS("SAM 2.1 是 Meta 的模型，以 Apache 2.0 许可协议发布。 单独下载以减小应用体积。"),
    ZH_HANT("SAM 2.1 是 Meta 的模型，以 Apache 2.0 授權條款發布。 單獨下載以減小應用體積。"),
    KO("SAM 2.1은 Meta의 모델이며 Apache 2.0 라이선스로 공개되어 있습니다. 앱 크기를 줄이기 위해 별도로 받습니다."),
    DE("SAM 2.1 ist Metas Modell, veröffentlicht unter der Apache-2.0-Lizenz. Separater Download hält die App klein."),
    FR("SAM 2.1 est le modèle de Meta, publié sous licence Apache 2.0. Téléchargé séparément pour alléger l’application."),
    ES("SAM 2.1 es el modelo de Meta, publicado bajo la licencia Apache 2.0. Se descarga aparte para reducir la aplicación."),
    PT("O SAM 2.1 é o modelo da Meta, publicado sob a licença Apache 2.0. Descarregado à parte para reduzir a aplicação."),
    IT("SAM 2.1 è il modello di Meta, pubblicato con licenza Apache 2.0. Scaricato a parte per ridurre l’app."),
    NL("SAM 2.1 is het model van Meta, uitgebracht onder de Apache 2.0-licentie. Apart gedownload om de app klein te houden."),
    RU("SAM 2.1 — модель Meta, выпущенная под лицензией Apache 2.0. Загружается отдельно для уменьшения приложения."),
    TR("SAM 2.1 Meta'nın modelidir ve Apache 2.0 lisansıyla yayımlanmıştır. Uygulamayı küçük tutmak için ayrı indirilir."));

// ===========================================================================
// Model licence consent
//
// IRREVERSIBLE / LEGAL -- every message in this block gets human review in
// every language before shipping. A user is being asked to accept somebody
// else's terms; a translation that softens or overstates them is worse than
// no translation at all.
// ===========================================================================

SS_MSG(license_modal_title,
    EN("Model licence"), JA("モデルのライセンス"), ZH_HANS("模型许可协议"),
    ZH_HANT("模型授權條款"), KO("모델 라이선스"), DE("Modelllizenz"),
    FR("Licence du modèle"), ES("Licencia del modelo"),
    PT("Licença do modelo"), IT("Licenza del modello"), NL("Modellicentie"),
    RU("Лицензия модели"), TR("Model lisansı"));

SS_MSG(license_read,
    EN("Read the licence"),
    JA("ライセンスを読む"), ZH_HANS("阅读许可协议"), ZH_HANT("閱讀授權條款"),
    KO("라이선스 읽기"),   DE("Lizenz lesen"), FR("Lire la licence"),
    ES("Leer la licencia"), PT("Ler a licença"), IT("Leggi la licenza"),
    NL("Licentie lezen"),  RU("Прочитать лицензию"), TR("Lisansı oku"));

SS_MSG(license_copy_link,
    EN("Copy link"),     JA("リンクをコピー"), ZH_HANS("复制链接"),  ZH_HANT("複製連結"),
    KO("링크 복사"),      DE("Link kopieren"), FR("Copier le lien"),
    ES("Copiar el enlace"), PT("Copiar o link"), IT("Copia il link"),
    NL("Link kopiëren"), RU("Скопировать ссылку"), TR("Bağlantıyı kopyala"));

// {0} is a size like "707 MB".
SS_MSG(license_download_size,
    EN("Download: about {0}, kept for next time."),
    JA("ダウンロード: 約 {0}。次回以降は再利用されます。"),
    ZH_HANS("下载：约 {0}，之后会保留下来。"),
    ZH_HANT("下載：約 {0}，之後會保留下來。"),
    KO("내려받기: 약 {0}. 다음부터는 그대로 씁니다."),
    DE("Download: etwa {0}, bleibt für das nächste Mal erhalten."),
    FR("Téléchargement : environ {0}, conservé pour la prochaine fois."),
    ES("Descarga: unos {0}, se conserva para la próxima vez."),
    PT("Download: cerca de {0}, guardado para a próxima vez."),
    IT("Scaricamento: circa {0}, resta per la prossima volta."),
    NL("Download: ongeveer {0}, blijft bewaard voor de volgende keer."),
    RU("Загрузка: около {0}, сохраняется на будущее."),
    TR("İndirme: yaklaşık {0}, bir dahaki sefere saklanır."));

// {0} is the licence's name, e.g. "SAM 3 License (Meta)". The tick every family
// needs before Accept is enabled.
SS_MSG(license_accept_tick,
    EN("I have read and accept the terms of {0}"),
    JA("{0} の条件を読み、同意します"),
    ZH_HANS("我已阅读并接受 {0} 的条款"),
    ZH_HANT("我已閱讀並接受 {0} 的條款"),
    KO("{0}의 조건을 읽었고 이에 동의합니다"),
    DE("Ich habe die Bedingungen von {0} gelesen und nehme sie an"),
    FR("J'ai lu et j'accepte les conditions de {0}"),
    ES("He leído y acepto los términos de {0}"),
    PT("Li e aceito os termos de {0}"),
    IT("Ho letto e accetto i termini di {0}"),
    NL("Ik heb de voorwaarden van {0} gelezen en aanvaard ze"),
    RU("Я прочитал условия {0} и принимаю их"),
    TR("{0} koşullarını okudum ve kabul ediyorum"));

// What the user is told after cancelling a licence dialog that a download was
// waiting on.
SS_MSG(license_declined_download,
    EN("The licence was not accepted, so nothing was downloaded."),
    JA("ライセンスに同意していないため、何もダウンロードしていません。"),
    ZH_HANS("未接受许可协议，因此没有下载任何内容。"),
    ZH_HANT("未接受授權條款，因此沒有下載任何內容。"),
    KO("라이선스에 동의하지 않았으므로 아무것도 내려받지 않았습니다."),
    DE("Die Lizenz wurde nicht angenommen, deshalb wurde nichts heruntergeladen."),
    FR("La licence n'a pas été acceptée ; rien n'a donc été téléchargé."),
    ES("No se aceptó la licencia, así que no se descargó nada."),
    PT("A licença não foi aceita, então nada foi baixado."),
    IT("La licenza non è stata accettata, quindi non è stato scaricato nulla."),
    NL("De licentie is niet aanvaard, dus er is niets gedownload."),
    RU("Лицензия не принята, поэтому ничего не было загружено."),
    TR("Lisans kabul edilmedi, bu yüzden hiçbir şey indirilmedi."));

// The same, for a batch: it asks for every licence it needs before it starts.
SS_MSG(batch_licence_declined,
    EN("The batch was not started: a licence it needs was not accepted, so nothing "
       "was downloaded."),
    JA("バッチは開始されませんでした。必要なライセンスに同意していないため、"
       "何もダウンロードしていません。"),
    ZH_HANS("批处理未启动：未接受其所需的许可协议，因此没有下载任何内容。"),
    ZH_HANT("批次未啟動：未接受其所需的授權條款，因此沒有下載任何內容。"),
    KO("일괄 작업을 시작하지 않았습니다. 필요한 라이선스에 동의하지 않았으므로 "
       "아무것도 내려받지 않았습니다."),
    DE("Der Stapel wurde nicht gestartet: Eine benötigte Lizenz wurde nicht "
       "angenommen, deshalb wurde nichts heruntergeladen."),
    FR("Le lot n'a pas démarré : une licence requise n'a pas été acceptée ; "
       "rien n'a donc été téléchargé."),
    ES("No se inició el lote: no se aceptó una licencia que necesita, así que "
       "no se descargó nada."),
    PT("O lote não foi iniciado: uma licença necessária não foi aceita, então "
       "nada foi baixado."),
    IT("Il batch non è stato avviato: una licenza necessaria non è stata "
       "accettata, quindi non è stato scaricato nulla."),
    NL("De batch is niet gestart: een benodigde licentie is niet aanvaard, dus "
       "er is niets gedownload."),
    RU("Пакет не запущен: нужная лицензия не принята, поэтому ничего не было "
       "загружено."),
    TR("Toplu iş başlatılmadı: gereken bir lisans kabul edilmedi, bu yüzden "
       "hiçbir şey indirilmedi."));

SS_MSG(license_download,
    EN("Download"),      JA("ダウンロード"),  ZH_HANS("下载"),     ZH_HANT("下載"),
    KO("내려받기"),       DE("Herunterladen"), FR("Télécharger"), ES("Descargar"),
    PT("Baixar"),        IT("Scarica"),      NL("Downloaden"),   RU("Загрузить"),
    TR("İndir"));

// {0} is the licence URL.
SS_MSG(license_no_browser,
    EN("Could not open a browser. The licence is at {0} (copied to the "
       "clipboard)."),
    JA("ブラウザを開けませんでした。ライセンスは {0} にあります"
       "（クリップボードにコピーしました）。"),
    ZH_HANS("无法打开浏览器。许可协议在 {0}（已复制到剪贴板）。"),
    ZH_HANT("無法開啟瀏覽器。授權條款在 {0}（已複製到剪貼簿）。"),
    KO("브라우저를 열지 못했습니다. 라이선스는 {0}에 있습니다(클립보드에 "
       "복사했습니다)."),
    DE("Es ließ sich kein Browser öffnen. Die Lizenz steht unter {0} (in die "
       "Zwischenablage kopiert)."),
    FR("Impossible d'ouvrir un navigateur. La licence est à l'adresse {0} "
       "(copiée dans le presse-papiers)."),
    ES("No se pudo abrir un navegador. La licencia está en {0} (copiada al "
       "portapapeles)."),
    PT("Não foi possível abrir um navegador. A licença está em {0} (copiada "
       "para a área de transferência)."),
    IT("Non è stato possibile aprire un browser. La licenza si trova in {0} "
       "(copiata negli appunti)."),
    NL("Er kon geen browser worden geopend. De licentie staat op {0} "
       "(gekopieerd naar het klembord)."),
    RU("Не удалось открыть браузер. Лицензия находится по адресу {0} "
       "(скопирован в буфер обмена)."),
    TR("Bir tarayıcı açılamadı. Lisans şu adreste: {0} (panoya kopyalandı)."));

// ===========================================================================
// Log lines this screen writes
// ===========================================================================

// The pictures declare their colour space (an EXR's header, a TIFF's ICC
// profile), so the one under Advanced was filled in from it; {0} is the
// format, {1} the gamut it found.
SS_MSG(log_file_color_linear,
    EN("These are {0} images: reading them as linear {1}, from the file."),
    JA("{0} 画像です。ファイルの情報に従い、線形 {1} として読み込みます。"),
    ZH_HANS("这些是 {0} 图像：按文件所记录的线性 {1} 读取。"),
    ZH_HANT("這些是 {0} 影像：依檔案所記錄的線性 {1} 讀取。"),
    KO("{0} 이미지입니다. 파일에 기록된 대로 선형 {1}(으)로 읽습니다."),
    DE("Das sind {0}-Bilder: Sie werden laut Datei als lineares {1} gelesen."),
    FR("Ce sont des images {0} : elles sont lues comme {1} linéaire, "
       "d'après le fichier."),
    ES("Son imágenes {0}: se leen como {1} lineal, según el archivo."),
    PT("São imagens {0}: lidas como {1} linear, conforme o arquivo."),
    IT("Sono immagini {0}: vengono lette come {1} lineare, dal file."),
    NL("Dit zijn {0}-beelden: ze worden gelezen als lineair {1}, uit het bestand."),
    RU("Это снимки {0}: они читаются как линейный {1}, по данным файла."),
    TR("Bunlar {0} görüntüleri: dosyaya göre doğrusal {1} olarak okunuyor."));

SS_MSG(log_file_color_display,
    EN("These are {0} images: reading them as display-encoded {1}, from the file."),
    JA("{0} 画像です。ファイルの情報に従い、表示用エンコードの {1} として読み込みます。"),
    ZH_HANS("这些是 {0} 图像：按文件所记录的显示编码 {1} 读取。"),
    ZH_HANT("這些是 {0} 影像：依檔案所記錄的顯示編碼 {1} 讀取。"),
    KO("{0} 이미지입니다. 파일에 기록된 대로 디스플레이 인코딩된 {1}(으)로 읽습니다."),
    DE("Das sind {0}-Bilder: Sie werden laut Datei als anzeigecodiertes {1} gelesen."),
    FR("Ce sont des images {0} : elles sont lues comme {1} encodé pour "
       "l'affichage, d'après le fichier."),
    ES("Son imágenes {0}: se leen como {1} codificado para pantalla, según el "
       "archivo."),
    PT("São imagens {0}: lidas como {1} codificado para exibição, conforme o "
       "arquivo."),
    IT("Sono immagini {0}: vengono lette come {1} codificato per lo schermo, "
       "dal file."),
    NL("Dit zijn {0}-beelden: ze worden gelezen als weergavegecodeerd {1}, uit "
       "het bestand."),
    RU("Это снимки {0}: они читаются как экранно закодированный {1}, по данным "
       "файла."),
    TR("Bunlar {0} görüntüleri: dosyaya göre ekran kodlu {1} olarak okunuyor."));

SS_MSG(log_masks_attached,
    EN("Using {0} as the masks for the images beside it."),
    JA("{0} を、その隣にある画像のマスクとして使います。"),
    ZH_HANS("把 {0} 用作它旁边那些图像的蒙版。"),
    ZH_HANT("把 {0} 用作它旁邊那些影像的遮罩。"),
    KO("{0}을(를) 그 옆 이미지들의 마스크로 사용합니다."),
    DE("{0} wird als Maskenordner für die daneben liegenden Bilder benutzt."),
    FR("{0} est utilisé comme masques pour les images qui se trouvent à côté."),
    ES("Se usa {0} como máscaras de las imágenes que están junto a ella."),
    PT("Usando {0} como as máscaras das imagens ao lado."),
    IT("Si usa {0} come maschere per le immagini che le stanno accanto."),
    NL("{0} wordt gebruikt als maskers voor de beelden ernaast."),
    RU("{0} используется как маски для соседних снимков."),
    TR("{0}, yanındaki görüntülerin maskeleri olarak kullanılıyor."));

SS_MSG(log_masks_orphaned,
    EN("Ignored {0}: that is a folder of masks, and the images they belong to were not picked. Add the images folder -- its masks are found on their own."),
    JA("{0} は無視しました。マスクのフォルダですが、対応する画像が選ばれていません。画像のフォルダを追加してください。マスクは自動で見つかります。"),
    ZH_HANS("已忽略 {0}：那是一个蒙版文件夹，但它对应的图像没有被选中。请添加图像文件夹——它的蒙版会被自动找到。"),
    ZH_HANT("已忽略 {0}：那是一個遮罩資料夾，但它對應的影像沒有被選取。請新增影像資料夾——它的遮罩會被自動找到。"),
    KO("{0}은(는) 무시했습니다. 마스크 폴더인데 그것이 딸린 이미지가 선택되지 않았습니다. 이미지 폴더를 추가하세요. 마스크는 알아서 찾습니다."),
    DE("{0} wurde übergangen: das ist ein Maskenordner, und die zugehörigen Bilder wurden nicht gewählt.\nFügen Sie den Bilderordner hinzu -- seine Masken werden von selbst gefunden."),
    FR("{0} ignoré : dossier de masques sans les images associées. Ajoutez le dossier d’images ; ses masques seront trouvés automatiquement."),
    ES("Se ignoró {0}: es una carpeta de máscaras y no se eligieron las imágenes a las que pertenecen. Añada la carpeta de imágenes: sus máscaras se encuentran solas."),
    PT("{0} foi ignorada: é uma pasta de máscaras, e as imagens a que pertencem não foram escolhidas. Adicione a pasta de imagens -- as máscaras dela são encontradas sozinhas."),
    IT("{0} è stata ignorata: è una cartella di maschere e le immagini a cui appartengono non sono state scelte.\nAggiunga la cartella delle immagini: le sue maschere vengono trovate da sole."),
    NL("{0} is genegeerd: dat is een map met maskers, en de beelden waar ze bij horen zijn niet gekozen. Voeg de beeldmap toe -- de bijbehorende maskers worden vanzelf gevonden."),
    RU("{0} пропущена: это папка масок, а снимки, к которым они относятся, не выбраны. Добавьте папку со снимками — их маски находятся сами."),
    TR("{0} yok sayıldı: burası bir maske klasörü ve ait oldukları görüntüler seçilmedi. Görüntü klasörünü ekleyin -- maskeleri kendiliğinden bulunur."));

SS_MSG(log_clicks_dropped_input_gone,
    EN("Clicked objects dropped: {0}, the input they were drawn on, is no "
       "longer in the list."),
    JA("クリックした物体を破棄しました。それらを指定した入力 {0} が"
       "リストから外れたためです。"),
    ZH_HANS("已丢弃点选的物体：它们所依附的输入 {0} 已经不在列表里了。"),
    ZH_HANT("已丟棄點選的物體：它們所依附的輸入 {0} 已經不在清單裡了。"),
    KO("클릭한 물체를 버렸습니다. 그것들을 지정한 입력 {0}이(가) 목록에서 "
       "빠졌습니다."),
    DE("Angeklickte Objekte verworfen: {0}, die Eingabe, auf der sie "
       "eingezeichnet wurden, steht nicht mehr auf der Liste."),
    FR("Objets cliqués abandonnés : {0}, l'entrée sur laquelle ils avaient été "
       "tracés, ne figure plus dans la liste."),
    ES("Se descartaron los objetos marcados: {0}, la entrada sobre la que se "
       "marcaron, ya no está en la lista."),
    PT("Objetos clicados descartados: {0}, a entrada em que foram marcados, "
       "não está mais na lista."),
    IT("Oggetti cliccati scartati: {0}, l'ingresso su cui erano stati "
       "tracciati, non è più nell'elenco."),
    NL("Aangeklikte objecten vervallen: {0}, de invoer waarop ze waren gezet, "
       "staat niet meer in de lijst."),
    RU("Отмеченные объекты сброшены: вход {0}, на котором они были указаны, "
       "больше не в списке."),
    TR("Tıklanan nesneler bırakıldı: üzerlerinde işaretlendikleri {0} girdisi "
       "artık listede değil."));

SS_MSG(log_drop_no_images,
    EN("Dropped folder contains no dataset or images: {0}"),
    JA("ドロップされたフォルダにデータセットも画像もありません: {0}"),
    ZH_HANS("拖入的文件夹里既没有数据集也没有图像：{0}"),
    ZH_HANT("拖入的資料夾裡既沒有資料集也沒有影像：{0}"),
    KO("끌어다 놓은 폴더에 데이터셋도 이미지도 없습니다: {0}"),
    DE("Der abgelegte Ordner enthält weder Datensatz noch Bilder: {0}"),
    FR("Le dossier déposé ne contient ni jeu de données ni images : {0}"),
    ES("La carpeta arrastrada no contiene ni conjunto de datos ni imágenes: {0}"),
    PT("A pasta arrastada não contém conjunto de dados nem imagens: {0}"),
    IT("La cartella trascinata non contiene né set di dati né immagini: {0}"),
    NL("De neergezette map bevat geen dataset of beelden: {0}"),
    RU("В перетащенной папке нет ни набора данных, ни изображений: {0}"),
    TR("Bırakılan klasörde ne veri kümesi ne de görüntü var: {0}"));

SS_MSG(log_drop_unsupported,
    EN("Unsupported dropped file: {0}"),
    JA("対応していないファイルがドロップされました: {0}"),
    ZH_HANS("拖入了不支持的文件：{0}"),
    ZH_HANT("拖入了不支援的檔案：{0}"),
    KO("지원하지 않는 파일을 끌어다 놓았습니다: {0}"),
    DE("Abgelegte Datei wird nicht unterstützt: {0}"),
    FR("Fichier déposé non pris en charge : {0}"),
    ES("Archivo arrastrado no compatible: {0}"),
    PT("Arquivo arrastado sem suporte: {0}"),
    IT("File trascinato non supportato: {0}"),
    NL("Niet-ondersteund bestand neergezet: {0}"),
    RU("Перетащенный файл не поддерживается: {0}"),
    TR("Desteklenmeyen dosya bırakıldı: {0}"));

SS_MSG(log_drop_while_training,
    EN("Dropped input ignored: stop training first"),
    JA("ドロップされた入力を無視しました。先に学習を停止してください"),
    ZH_HANS("已忽略拖入的输入：请先停止训练"),
    ZH_HANT("已忽略拖入的輸入：請先停止訓練"),
    KO("끌어다 놓은 입력을 무시했습니다. 먼저 학습을 멈추세요"),
    DE("Abgelegte Eingabe übergangen: erst das Training anhalten"),
    FR("Entrée déposée ignorée : arrêtez d'abord l'entraînement"),
    ES("Entrada arrastrada ignorada: detenga primero el entrenamiento"),
    PT("Entrada arrastada ignorada: pare o treinamento primeiro"),
    IT("Ingresso trascinato ignorato: fermi prima l'addestramento"),
    NL("Neergezette invoer genegeerd: stop eerst de training"),
    RU("Перетащенный вход пропущен: сначала остановите обучение"),
    TR("Bırakılan girdi yok sayıldı: önce eğitimi durdurun"));

SS_MSG(log_dataset_settings_changed,
    EN("Dataset settings changed; reloading dataset"),
    JA("データセットの設定が変わったため、読み込み直します"),
    ZH_HANS("数据集设置已更改，正在重新加载数据集"),
    ZH_HANT("資料集設定已變更，正在重新載入資料集"),
    KO("데이터셋 설정이 바뀌어 데이터셋을 다시 불러옵니다"),
    DE("Datensatzeinstellungen geändert; Datensatz wird neu geladen"),
    FR("Réglages du jeu de données modifiés ; rechargement en cours"),
    ES("Cambiaron los ajustes del conjunto de datos; recargándolo"),
    PT("As configurações do conjunto de dados mudaram; recarregando"),
    IT("Impostazioni del set di dati cambiate; ricaricamento in corso"),
    NL("Datasetinstellingen gewijzigd; dataset wordt opnieuw geladen"),
    RU("Параметры набора данных изменились; перезагрузка"),
    TR("Veri kümesi ayarları değişti; veri kümesi yeniden yükleniyor"));

// ---------------------------------------------------------------------------
// Depth and normals
// ---------------------------------------------------------------------------

SS_MSG(step_geometry,
    EN("Depth"),         JA("深度"),          ZH_HANS("深度"),      ZH_HANT("深度"),
    KO("깊이"),           DE("Tiefe"),        FR("Profondeur"),   ES("Profundidad"),
    PT("Profundidade"),  IT("Profondità"),   NL("Diepte"),       RU("Глубина"),
    TR("Derinlik"));

SS_MSG(view_geometry,
    EN("Depth & normals"),
    JA("深度と法線"),      ZH_HANS("深度与法线"), ZH_HANT("深度與法線"),
    KO("깊이와 법선"),     DE("Tiefe und Normalen"),
    FR("Profondeur et normales"), ES("Profundidad y normales"),
    PT("Profundidade e normais"), IT("Profondità e normali"),
    NL("Diepte en normalen"), RU("Глубина и нормали"),
    TR("Derinlik ve normaller"));

SS_MSG(recon_reuse_rebuild,
    EN("A reconstruction is already in the output folder. Change any of these "
       "and the run builds it again; leave them and it is kept."),
    JA("出力フォルダにはすでに再構成結果があります。ここを変えると作り直し、"
       "変えなければそのまま残します。"),
    ZH_HANS("输出文件夹里已经有一份重建结果。改动这里的设置就会重新重建，"
            "不改就原样保留。"),
    ZH_HANT("輸出資料夾裡已經有一份重建結果。改動這裡的設定就會重新重建，"
            "不改就原樣保留。"),
    KO("출력 폴더에 이미 재구성 결과가 있습니다. 여기를 바꾸면 다시 만들고, "
       "그대로 두면 남겨 둡니다."),
    DE("Im Ausgabeordner liegt schon eine Rekonstruktion. Ändern Sie hier "
       "etwas, wird sie neu gebaut; sonst bleibt sie."),
    FR("Le dossier de sortie contient déjà une reconstruction. Modifiez un "
       "réglage ici et elle est refaite ; sinon elle est conservée."),
    ES("La carpeta de salida ya contiene una reconstrucción. Cambia algo aquí "
       "y se rehace; si no, se conserva."),
    PT("A pasta de saída já contém uma reconstrução. Mude algo aqui e ela é "
       "refeita; caso contrário, fica."),
    IT("La cartella di uscita contiene già una ricostruzione. Cambia qualcosa "
       "qui e viene rifatta; altrimenti resta."),
    NL("In de uitvoermap staat al een reconstructie. Verander hier iets en die "
       "wordt opnieuw gemaakt; anders blijft hij."),
    RU("В папке вывода уже есть реконструкция. Измените что-нибудь здесь — её "
       "построят заново; иначе она останется."),
    TR("Çıktı klasöründe zaten bir yeniden kurma var. Burada bir şey "
       "değiştirirseniz yeniden yapılır; değiştirmezseniz kalır."));

SS_MSG(recon_reuse_locked,
    EN("Existing reconstruction uses different settings and will stay unchanged. Enable “Reconstruct again” beside the output folder to rebuild it."),
    JA("既存再構成は異なる設定で作られており、変更しません。作り直すには出力フォルダ横の「再構成し直す」を有効にしてください。"),
    ZH_HANS("已有重建使用了不同设置，将保持不变。启用输出文件夹旁的“重新重建”可重做。"),
    ZH_HANT("已有重建使用了不同設定，將保持不變。啟用輸出資料夾旁的「重新重建」可重做。"),
    KO("기존 재구성은 다른 설정을 사용하며 변경되지 않습니다. 다시 만들려면 출력 폴더 옆의 “다시 재구성”을 켜세요."),
    DE("Die Rekonstruktion nutzt andere Einstellungen und bleibt unverändert. „Erneut rekonstruieren“ neben dem Ausgabeordner aktiviert den Neuaufbau."),
    FR("La reconstruction utilise d’autres réglages et reste inchangée. Activez « Reconstruire à nouveau » près du dossier de sortie pour la refaire."),
    ES("La reconstrucción usa otros ajustes y no cambiará. Activa «Reconstruir de nuevo» junto a la carpeta de salida para rehacerla."),
    PT("A reconstrução usa outras definições e fica inalterada. Ative «Reconstruir novamente» junto à pasta de saída para a refazer."),
    IT("La ricostruzione usa altre impostazioni e resta invariata. Attivare «Ricostruisci» accanto alla cartella di uscita per rifarla."),
    NL("De reconstructie gebruikt andere instellingen en blijft gelijk. Zet ‘Opnieuw reconstrueren’ naast de uitvoermap aan om opnieuw te bouwen."),
    RU("Реконструкция использует другие настройки и не изменится. Включите «Реконструировать заново» возле выходной папки для пересоздания."),
    TR("Mevcut model farklı ayarlar kullanır ve değişmez. Yeniden yapmak için çıktı klasörü yanındaki “Yeniden oluştur” seçeneğini açın."));

SS_MSG(reconstruct_again,
    EN("Reconstruct again"),
    JA("再構成をやり直す"), ZH_HANS("重新重建"),  ZH_HANT("重新重建"),
    KO("다시 재구성"),     DE("Neu rekonstruieren"),
    FR("Reconstruire à nouveau"), ES("Reconstruir de nuevo"),
    PT("Reconstruir de novo"), IT("Ricostruisci di nuovo"),
    NL("Opnieuw reconstrueren"), RU("Реконструировать заново"),
    TR("Yeniden kur"));

SS_MSG(reconstruct_again_help,
    EN("Rebuilds and replaces the cameras. Off only adds masks, depth and normals to an existing Spirula, COLMAP, Nerfstudio or Metashape dataset."),
    JA("カメラを作り直して置き換えます。オフでは既存のSpirula、COLMAP、Nerfstudio、Metashapeデータにマスク・深度・法線のみ追加します。"),
    ZH_HANS("重建并替换相机。关闭则仅向已有 Spirula、COLMAP、Nerfstudio 或 Metashape 数据集添加蒙版、深度及法线。"),
    ZH_HANT("重建並取代相機。關閉則僅向已有 Spirula、COLMAP、Nerfstudio 或 Metashape 資料集新增遮罩、深度及法線。"),
    KO("카메라를 다시 만들어 대체합니다. 끄면 기존 Spirula, COLMAP, Nerfstudio, Metashape 데이터에 마스크·깊이·법선만 추가합니다."),
    DE("Ersetzt die Kameras durch Neuaufbau. Aus ergänzt nur Masken, Tiefe und Normalen in vorhandenen Spirula-, COLMAP-, Nerfstudio- oder Metashape-Datensätzen."),
    FR("Reconstruit et remplace les caméras. Désactivé ajoute seulement masques, profondeur et normales au jeu Spirula, COLMAP, Nerfstudio ou Metashape existant."),
    ES("Reconstruye y sustituye cámaras. Desactivado solo añade máscaras, profundidad y normales al dataset Spirula, COLMAP, Nerfstudio o Metashape existente."),
    PT("Reconstrói e substitui câmaras. Desligado só acrescenta máscaras, profundidade e normais ao conjunto Spirula, COLMAP, Nerfstudio ou Metashape existente."),
    IT("Ricrea e sostituisce le camere. Spento aggiunge solo maschere, profondità e normali al dataset Spirula, COLMAP, Nerfstudio o Metashape esistente."),
    NL("Bouwt camera’s opnieuw en vervangt ze. Uit voegt alleen maskers, diepte en normalen toe aan een bestaande Spirula-, COLMAP-, Nerfstudio- of Metashape-dataset."),
    RU("Пересоздаёт и заменяет камеры. Выкл. лишь добавляет маски, глубину и нормали в существующий набор Spirula, COLMAP, Nerfstudio или Metashape."),
    TR("Kameraları yeniden yapıp değiştirir. Kapalıysa mevcut Spirula, COLMAP, Nerfstudio veya Metashape verisine yalnız maske, derinlik ve normal ekler."));

SS_MSG(update_dataset,
    EN("Update Dataset"),
    JA("データセットを更新"), ZH_HANS("更新数据集"), ZH_HANT("更新資料集"),
    KO("데이터셋 갱신"),    DE("Datensatz ergänzen"),
    FR("Compléter le jeu de données"), ES("Actualizar el conjunto de datos"),
    PT("Atualizar o conjunto de dados"), IT("Aggiorna il set di dati"),
    NL("Dataset bijwerken"), RU("Дополнить набор данных"),
    TR("Veri kümesini güncelle"));

// ---------------------------------------------------------------------------
// What a run will reuse and redo, listed above the button (DatasetPlan.h).
// Each state is a short verb phrase beside the step's own name.
// ---------------------------------------------------------------------------

SS_MSG(plan_step_model,
    EN("Reconstruction"),
    JA("再構成"),        ZH_HANS("重建"),      ZH_HANT("重建"),
    KO("재구성"),         DE("Rekonstruktion"), FR("Reconstruction"),
    ES("Reconstrucción"), PT("Reconstrução"), IT("Ricostruzione"),
    NL("Reconstructie"), RU("Реконструкция"), TR("Yeniden kurma"));

SS_MSG(plan_run,
    EN("Run"),
    JA("実行する"),      ZH_HANS("运行"),      ZH_HANT("執行"),
    KO("실행"),           DE("Ausführen"),    FR("Exécuter"),
    ES("Ejecutar"),      PT("Executar"),     IT("Eseguire"),
    NL("Uitvoeren"),     RU("Выполнить"),    TR("Çalıştır"));

SS_MSG(plan_finish,
    EN("Finish the interrupted run"),
    JA("中断した実行の続きをする"),
    ZH_HANS("接着做完中断的运行"),
    ZH_HANT("接著做完中斷的執行"),
    KO("중단된 실행 마저 하기"),
    DE("Den abgebrochenen Lauf zu Ende führen"),
    FR("Terminer l'exécution interrompue"),
    ES("Terminar la ejecución interrumpida"),
    PT("Terminar a execução interrompida"),
    IT("Finire l'esecuzione interrotta"),
    NL("De onderbroken run afmaken"),
    RU("Завершить прерванный запуск"),
    TR("Yarıda kalan çalıştırmayı bitir"));

SS_MSG(plan_add,
    EN("Add what is missing"),
    JA("足りない分を足す"),
    ZH_HANS("补上缺少的部分"),
    ZH_HANT("補上缺少的部分"),
    KO("빠진 것 채우기"),
    DE("Fehlendes ergänzen"),
    FR("Ajouter ce qui manque"),
    ES("Añadir lo que falta"),
    PT("Acrescentar o que falta"),
    IT("Aggiungere ciò che manca"),
    NL("Aanvullen wat ontbreekt"),
    RU("Добавить недостающее"),
    TR("Eksik olanı ekle"));

SS_MSG(plan_reuse,
    EN("Reuse"),
    JA("そのまま使う"),   ZH_HANS("沿用"),      ZH_HANT("沿用"),
    KO("그대로 사용"),     DE("Weiterverwenden"), FR("Réutiliser"),
    ES("Reutilizar"),    PT("Reutilizar"),   IT("Riutilizzare"),
    NL("Hergebruiken"),  RU("Использовать как есть"), TR("Yeniden kullan"));

SS_MSG(plan_reuse_masks_changed,
    EN("Reuse (built before the current masks)"),
    JA("そのまま使う（いまのマスクより前に作られたもの）"),
    ZH_HANS("沿用（是在当前蒙版之前做的）"),
    ZH_HANT("沿用（是在目前遮罩之前做的）"),
    KO("그대로 사용 (지금 마스크보다 먼저 만든 것)"),
    DE("Weiterverwenden (vor den jetzigen Masken gebaut)"),
    FR("Réutiliser (construite avant les masques actuels)"),
    ES("Reutilizar (construida antes de las máscaras actuales)"),
    PT("Reutilizar (construída antes das máscaras atuais)"),
    IT("Riutilizzare (costruita prima delle maschere attuali)"),
    NL("Hergebruiken (gebouwd vóór de huidige maskers)"),
    RU("Использовать как есть (построена до нынешних масок)"),
    TR("Yeniden kullan (şimdiki maskelerden önce yapıldı)"));

SS_MSG(plan_in_dataset,
    EN("Already in the dataset"),
    JA("すでにデータセットにある"),
    ZH_HANS("已经在数据集里"),
    ZH_HANT("已經在資料集裡"),
    KO("이미 데이터셋에 있음"),
    DE("Schon im Datensatz"),
    FR("Déjà dans le jeu de données"),
    ES("Ya está en el conjunto de datos"),
    PT("Já está no conjunto de dados"),
    IT("Già nel set di dati"),
    NL("Staat al in de dataset"),
    RU("Уже в наборе данных"),
    TR("Zaten veri kümesinde"));

SS_MSG(plan_unrecorded,
    EN("Keep (no record of how it was made)"),
    JA("残す（どう作られたかの記録がない）"),
    ZH_HANS("保留（没有它是怎么做出来的记录）"),
    ZH_HANT("保留（沒有它是怎麼做出來的紀錄）"),
    KO("남겨 둠 (어떻게 만들었는지 기록이 없음)"),
    DE("Behalten (kein Protokoll, wie es entstand)"),
    FR("Garder (aucune trace de sa fabrication)"),
    ES("Conservar (no consta cómo se hizo)"),
    PT("Manter (não há registo de como foi feito)"),
    IT("Tenere (nessuna traccia di come è stato fatto)"),
    NL("Behouden (niet vastgelegd hoe het gemaakt is)"),
    RU("Оставить (нет записи о том, как сделано)"),
    TR("Koru (nasıl yapıldığına dair kayıt yok)"));

SS_MSG(plan_keep,
    EN("Keep, though made with other settings"),
    JA("残す（別の設定で作られたもの）"),
    ZH_HANS("保留（虽然是用别的设置做的）"),
    ZH_HANT("保留（雖然是用別的設定做的）"),
    KO("남겨 둠 (다른 설정으로 만든 것)"),
    DE("Behalten, obwohl mit anderen Einstellungen gemacht"),
    FR("Garder, bien que fait avec d'autres réglages"),
    ES("Conservar, aunque se hizo con otros ajustes"),
    PT("Manter, embora feito com outras definições"),
    IT("Tenere, anche se fatto con altre impostazioni"),
    NL("Behouden, al is het met andere instellingen gemaakt"),
    RU("Оставить, хотя сделано с другими настройками"),
    TR("Koru, başka ayarlarla yapılmış olsa da"));

SS_MSG(plan_redo_requested,
    EN("Redo, as asked"),
    JA("指示どおりやり直す"),
    ZH_HANS("按要求重做"),
    ZH_HANT("按要求重做"),
    KO("요청대로 다시 하기"),
    DE("Neu machen, wie verlangt"),
    FR("Refaire, comme demandé"),
    ES("Rehacer, como se pidió"),
    PT("Refazer, como pedido"),
    IT("Rifare, come richiesto"),
    NL("Opnieuw doen, zoals gevraagd"),
    RU("Переделать, как просили"),
    TR("İstendiği gibi yeniden yap"));

SS_MSG(plan_redo_settings,
    EN("Redo: made with other settings"),
    JA("やり直す：別の設定で作られている"),
    ZH_HANS("重做：是用别的设置做的"),
    ZH_HANT("重做：是用別的設定做的"),
    KO("다시 하기: 다른 설정으로 만든 것"),
    DE("Neu machen: mit anderen Einstellungen gemacht"),
    FR("Refaire : fait avec d'autres réglages"),
    ES("Rehacer: se hizo con otros ajustes"),
    PT("Refazer: feito com outras definições"),
    IT("Rifare: fatto con altre impostazioni"),
    NL("Opnieuw doen: met andere instellingen gemaakt"),
    RU("Переделать: сделано с другими настройками"),
    TR("Yeniden yap: başka ayarlarla yapılmış"));

SS_MSG(plan_redo_frames,
    EN("Redo: the frames change"),
    JA("やり直す：フレームが変わる"),
    ZH_HANS("重做：帧会变"),
    ZH_HANT("重做：影格會變"),
    KO("다시 하기: 프레임이 바뀜"),
    DE("Neu machen: die Bilder ändern sich"),
    FR("Refaire : les images changent"),
    ES("Rehacer: cambian los fotogramas"),
    PT("Refazer: os quadros mudam"),
    IT("Rifare: cambiano i fotogrammi"),
    NL("Opnieuw doen: de beelden veranderen"),
    RU("Переделать: меняются кадры"),
    TR("Yeniden yap: kareler değişiyor"));

SS_MSG(plan_redo_model,
    EN("Redo: the reconstruction changes"),
    JA("やり直す：再構成が変わる"),
    ZH_HANS("重做：重建会变"),
    ZH_HANT("重做：重建會變"),
    KO("다시 하기: 재구성이 바뀜"),
    DE("Neu machen: die Rekonstruktion ändert sich"),
    FR("Refaire : la reconstruction change"),
    ES("Rehacer: cambia la reconstrucción"),
    PT("Refazer: a reconstrução muda"),
    IT("Rifare: cambia la ricostruzione"),
    NL("Opnieuw doen: de reconstructie verandert"),
    RU("Переделать: меняется реконструкция"),
    TR("Yeniden yap: yeniden kurma değişiyor"));

SS_MSG(plan_redo_stale,
    EN("Redo: made from older results"),
    JA("やり直す：古い結果から作られている"),
    ZH_HANS("重做：是从旧的结果做出来的"),
    ZH_HANT("重做：是從舊的結果做出來的"),
    KO("다시 하기: 예전 결과로 만든 것"),
    DE("Neu machen: aus älteren Ergebnissen gemacht"),
    FR("Refaire : fait à partir de résultats plus anciens"),
    ES("Rehacer: se hizo a partir de resultados anteriores"),
    PT("Refazer: feito a partir de resultados anteriores"),
    IT("Rifare: fatto da risultati precedenti"),
    NL("Opnieuw doen: gemaakt uit oudere resultaten"),
    RU("Переделать: сделано по более старым результатам"),
    TR("Yeniden yap: daha eski sonuçlardan yapılmış"));

SS_MSG(plan_redo_features,
    EN("Redo: the feature points change"),
    JA("やり直す：特徴点が変わる"),
    ZH_HANS("重做：特征点会变"),
    ZH_HANT("重做：特徵點會變"),
    KO("다시 하기: 특징점이 바뀜"),
    DE("Neu machen: die Merkmale ändern sich"),
    FR("Refaire : les points changent"),
    ES("Rehacer: cambian los puntos"),
    PT("Refazer: os pontos mudam"),
    IT("Rifare: cambiano i punti"),
    NL("Opnieuw doen: de kenmerken veranderen"),
    RU("Переделать: меняются признаки"),
    TR("Yeniden yap: öznitelikler değişiyor"));

SS_MSG(plan_redo_matches,
    EN("Redo: the matches change"),
    JA("やり直す：照合結果が変わる"),
    ZH_HANS("重做：匹配结果会变"),
    ZH_HANT("重做：比對結果會變"),
    KO("다시 하기: 정합 결과가 바뀜"),
    DE("Neu machen: die Zuordnungen ändern sich"),
    FR("Refaire : les appariements changent"),
    ES("Rehacer: cambian los emparejamientos"),
    PT("Refazer: os pareamentos mudam"),
    IT("Rifare: cambiano le corrispondenze"),
    NL("Opnieuw doen: de koppelingen veranderen"),
    RU("Переделать: меняются сопоставления"),
    TR("Yeniden yap: eşleşmeler değişiyor"));

SS_MSG(plan_redo_masked,
    EN("Redo for the images whose masks change"),
    JA("マスクが変わる画像だけやり直す"),
    ZH_HANS("只重做蒙版有变的图像"),
    ZH_HANT("只重做遮罩有變的影像"),
    KO("마스크가 바뀌는 이미지만 다시 하기"),
    DE("Neu machen für die Bilder, deren Masken sich ändern"),
    FR("Refaire pour les images dont les masques changent"),
    ES("Rehacer las imágenes cuyas máscaras cambian"),
    PT("Refazer as imagens cujas máscaras mudam"),
    IT("Rifare le immagini le cui maschere cambiano"),
    NL("Opnieuw doen voor de beelden waarvan de maskers veranderen"),
    RU("Переделать для снимков, у которых меняются маски"),
    TR("Maskesi değişen görüntüler için yeniden yap"));

SS_MSG(plan_redo_moved,
    EN("Redo: made from the images in another folder"),
    JA("やり直す：別のフォルダの画像から作られている"),
    ZH_HANS("重做：是用另一个文件夹的图像做的"),
    ZH_HANT("重做：是用另一個資料夾的影像做的"),
    KO("다시 하기: 다른 폴더의 이미지로 만든 것"),
    DE("Neu machen: aus den Bildern eines anderen Ordners gemacht"),
    FR("Refaire : fait à partir des images d'un autre dossier"),
    ES("Rehacer: se hizo con las imágenes de otra carpeta"),
    PT("Refazer: feito com as imagens de outra pasta"),
    IT("Rifare: fatto con le immagini di un'altra cartella"),
    NL("Opnieuw doen: gemaakt van de beelden in een andere map"),
    RU("Переделать: сделано по снимкам из другой папки"),
    TR("Yeniden yap: başka bir klasördeki görüntülerden yapılmış"));

SS_MSG(plan_reuse_if_same,
    EN("Reuse if made with these settings"),
    JA("この設定で作られていればそのまま使う"),
    ZH_HANS("如果是用这些设置做的就沿用"),
    ZH_HANT("如果是用這些設定做的就沿用"),
    KO("이 설정으로 만든 것이면 그대로 사용"),
    DE("Weiterverwenden, falls mit diesen Einstellungen gemacht"),
    FR("Réutiliser si fait avec ces réglages"),
    ES("Reutilizar si se hizo con estos ajustes"),
    PT("Reutilizar se foi feito com estas definições"),
    IT("Riutilizzare se fatto con queste impostazioni"),
    NL("Hergebruiken als het met deze instellingen gemaakt is"),
    RU("Использовать, если сделано с этими настройками"),
    TR("Bu ayarlarla yapıldıysa yeniden kullan"));

// The reconstruction's own stages, listed under it (DatasetPlan.h ModelPart).
SS_MSG(plan_part_features,
    EN("Feature extraction"),
    JA("特徴点の抽出"),   ZH_HANS("特征点提取"), ZH_HANT("特徵點提取"),
    KO("특징점 추출"),     DE("Merkmalsextraktion"), FR("Extraction des points"),
    ES("Extracción de puntos"), PT("Extração de pontos"), IT("Estrazione dei punti"),
    NL("Kenmerken extraheren"), RU("Извлечение признаков"), TR("Öznitelik çıkarma"));

SS_MSG(plan_part_mapping,
    EN("Mapping"),
    JA("マッピング"),     ZH_HANS("建图"),      ZH_HANT("建圖"),
    KO("매핑"),           DE("Kartierung"),   FR("Cartographie"),
    ES("Mapeo"),         PT("Mapeamento"),   IT("Mappatura"),
    NL("Kartering"),     RU("Картирование"), TR("Haritalama"));

SS_MSG(plan_part_align,
    EN("LiDAR alignment"),
    JA("LiDAR との位置合わせ"), ZH_HANS("与 LiDAR 对齐"), ZH_HANT("與 LiDAR 對齊"),
    KO("LiDAR 정렬"),      DE("LiDAR-Ausrichtung"), FR("Alignement LiDAR"),
    ES("Alineación con LiDAR"), PT("Alinhamento com LiDAR"), IT("Allineamento LiDAR"),
    NL("LiDAR-uitlijning"), RU("Совмещение с LiDAR"), TR("LiDAR hizalama"));

SS_MSG(plan_part_toggle_help,
    EN("Checked runs this stage; unchecked reuses earlier results. Enable a reused stage to rebuild it. All later stages run too."),
    JA("チェックでこの工程を実行し、外すと既存結果を使います。再利用中の工程もチェックで作り直せます。後の全工程も再実行します。"),
    ZH_HANS("勾选则运行此阶段，否则复用旧结果。勾选已复用阶段可重做；之后的所有阶段也会运行。"),
    ZH_HANT("勾選則執行此階段，否則重用舊結果。勾選已重用階段可重做；之後的所有階段也會執行。"),
    KO("체크하면 실행하고, 끄면 이전 결과를 재사용합니다. 재사용 단계도 켜면 다시 만듭니다. 이후 모든 단계도 실행됩니다."),
    DE("Aktiviert führt die Stufe aus, sonst werden Ergebnisse wiederverwendet. Aktivieren baut sie neu. Alle späteren Stufen laufen ebenfalls."),
    FR("Coché lance l’étape ; sinon réutilise ses résultats. Cocher une étape réutilisée la refait. Toutes les étapes suivantes sont aussi relancées."),
    ES("Marcado ejecuta esta etapa; sin marcar reutiliza resultados. Activa una etapa reutilizada para rehacerla. Todas las posteriores también se ejecutan."),
    PT("Marcado executa esta etapa; desmarcado reutiliza resultados. Ative uma etapa reutilizada para a refazer. Todas as seguintes também são executadas."),
    IT("Attivo esegue la fase, altrimenti riusa i risultati. Attivare una fase riusata la rifà. Vengono eseguite anche tutte le fasi successive."),
    NL("Aangevinkt voert de stap uit; anders worden resultaten hergebruikt. Aanvinken bouwt ze opnieuw. Alle volgende stappen worden ook uitgevoerd."),
    RU("Флажок запускает этап, иначе используются старые результаты. Включите повторно используемый этап для пересоздания. Все последующие тоже запускаются."),
    TR("İşaretliyse adımı çalıştırır; değilse eski sonuçları kullanır. Yeniden kullanılan adımı açmak yeniden yapar. Sonraki tüm adımlar da çalışır."));

SS_MSG(plan_lock_nothing,
    EN("Nothing finished is on disk for this stage, so it has to run."),
    JA("この段階の完成した結果がディスクにないので、実行が必要です。"),
    ZH_HANS("磁盘上没有这一阶段做完的结果，所以必须运行。"),
    ZH_HANT("磁碟上沒有這一階段做完的結果，所以必須執行。"),
    KO("이 단계의 끝난 결과가 디스크에 없어서 실행해야 합니다."),
    DE("Für diesen Schritt liegt nichts Fertiges auf der Festplatte, also muss er laufen."),
    FR("Rien de terminé n'est sur le disque pour cette étape : elle doit s'exécuter."),
    ES("No hay nada terminado en disco para esta etapa, así que debe ejecutarse."),
    PT("Não há nada terminado no disco para esta etapa, por isso tem de ser executada."),
    IT("Su disco non c'è niente di finito per questa fase, quindi va eseguita."),
    NL("Er staat voor deze stap niets afgerond op schijf, dus hij moet lopen."),
    RU("Для этого этапа на диске нет готового результата, поэтому он выполняется."),
    TR("Bu aşama için diskte bitmiş bir şey yok, bu yüzden çalışması gerekiyor."));

SS_MSG(plan_lock_frames,
    EN("The frames are made again, so the feature points of the old ones cannot "
       "be kept."),
    JA("フレームを作り直すので、古いフレームの特徴点は残せません。"),
    ZH_HANS("帧会重做，所以旧帧的特征点不能保留。"),
    ZH_HANT("影格會重做，所以舊影格的特徵點不能保留。"),
    KO("프레임을 다시 만들기 때문에 예전 프레임의 특징점은 남겨 둘 수 없습니다."),
    DE("Die Bilder werden neu gemacht, also lassen sich die Merkmale der alten "
       "nicht behalten."),
    FR("Les images sont refaites : les points des anciennes ne peuvent pas être "
       "gardés."),
    ES("Los fotogramas se rehacen, así que no se pueden conservar los puntos de "
       "los anteriores."),
    PT("Os quadros são refeitos, por isso os pontos dos antigos não podem ser "
       "mantidos."),
    IT("I fotogrammi vengono rifatti, quindi i punti di quelli vecchi non si "
       "possono tenere."),
    NL("De beelden worden opnieuw gemaakt, dus de kenmerken van de oude kunnen "
       "niet blijven."),
    RU("Кадры делаются заново, поэтому признаки старых кадров оставить нельзя."),
    TR("Kareler yeniden yapılıyor, bu yüzden eskilerin öznitelikleri korunamaz."));

SS_MSG(plan_lock_frontend,
    EN("These feature points are of another type than the one chosen, and its "
       "matcher cannot read them."),
    JA("これらの特徴点は選んだものと種類が違い、その照合器では読めません。"),
    ZH_HANS("这些特征点和所选的类型不同，它的匹配器读不了。"),
    ZH_HANT("這些特徵點和所選的類型不同，它的比對器讀不了。"),
    KO("이 특징점은 고른 것과 종류가 달라 그 정합기가 읽을 수 없습니다."),
    DE("Diese Merkmale sind von einer anderen Art als der gewählten, und deren "
       "Zuordnung kann sie nicht lesen."),
    FR("Ces points sont d'un autre type que celui choisi, et son apparieur ne "
       "peut pas les lire."),
    ES("Estos puntos son de otro tipo que el elegido, y su emparejador no puede "
       "leerlos."),
    PT("Estes pontos são de outro tipo que o escolhido, e o seu emparelhador não "
       "os consegue ler."),
    IT("Questi punti sono di un tipo diverso da quello scelto, e il suo "
       "abbinatore non li sa leggere."),
    NL("Deze kenmerken zijn van een ander soort dan het gekozen, en de koppelaar "
       "daarvan kan ze niet lezen."),
    RU("Эти признаки другого типа, чем выбранный, и его сопоставитель не может "
       "их прочитать."),
    TR("Bu öznitelikler seçilenden farklı türde ve onun eşleştiricisi bunları "
       "okuyamaz."));

SS_MSG(plan_lock_before,
    EN("The stage before it is made again, so this one has to run too."),
    JA("前の段階を作り直すので、この段階も実行が必要です。"),
    ZH_HANS("前一个阶段会重做，所以这一阶段也必须运行。"),
    ZH_HANT("前一個階段會重做，所以這一階段也必須執行。"),
    KO("앞 단계를 다시 만들기 때문에 이 단계도 실행해야 합니다."),
    DE("Der Schritt davor wird neu gemacht, also muss auch dieser laufen."),
    FR("L'étape d'avant est refaite : celle-ci doit donc s'exécuter aussi."),
    ES("La etapa anterior se rehace, así que esta también debe ejecutarse."),
    PT("A etapa anterior é refeita, por isso esta também tem de ser executada."),
    IT("La fase precedente viene rifatta, quindi anche questa va eseguita."),
    NL("De stap ervoor wordt opnieuw gedaan, dus deze moet ook lopen."),
    RU("Предыдущий этап делается заново, поэтому этот тоже выполняется."),
    TR("Önceki aşama yeniden yapılıyor, bu yüzden bu da çalışmalı."));

SS_MSG(plan_lock_lens,
    EN("A lens setting changed. The matches carry the lenses they were checked "
       "with, so keeping them would leave the change unused."),
    JA("レンズの設定が変わりました。照合結果は検証に使ったレンズを持っているので、"
       "残すと変更が使われません。"),
    ZH_HANS("镜头设置变了。匹配结果带着验证时用的镜头，保留它们就等于没用上这个改动。"),
    ZH_HANT("鏡頭設定變了。匹配結果帶著驗證時用的鏡頭，保留它們就等於沒用上這個改動。"),
    KO("렌즈 설정이 바뀌었습니다. 정합 결과는 검증에 쓴 렌즈를 담고 있어서, 남겨 "
       "두면 바꾼 설정이 쓰이지 않습니다."),
    DE("Eine Objektiveinstellung hat sich geändert. Die Paare tragen die Objektive, "
       "mit denen sie geprüft wurden; behalten bliebe die Änderung ungenutzt."),
    FR("Un réglage d'objectif a changé. Les appariements portent les objectifs avec "
       "lesquels ils ont été vérifiés : les garder laisserait le changement sans "
       "effet."),
    ES("Cambió un ajuste de lente. Los emparejamientos llevan las lentes con que se "
       "verificaron, así que conservarlos dejaría el cambio sin efecto."),
    PT("Mudou uma definição de lente. Os pareamentos levam as lentes com que foram "
       "verificados, por isso mantê-los deixaria a mudança sem efeito."),
    IT("È cambiata un'impostazione dell'obiettivo. Gli abbinamenti portano gli "
       "obiettivi con cui sono stati verificati: tenerli lascerebbe il cambiamento "
       "senza effetto."),
    NL("Een lensinstelling is veranderd. De koppelingen dragen de lenzen waarmee "
       "ze gecontroleerd zijn, dus houden zou de wijziging ongebruikt laten."),
    RU("Изменилась настройка объектива. Сопоставления хранят объективы, с которыми "
       "их проверяли, поэтому, оставив их, изменение не будет использовано."),
    TR("Bir lens ayarı değişti. Eşlemeler doğrulandıkları lensleri taşıyor; "
       "korunurlarsa değişiklik kullanılmamış olur."));

SS_MSG(plan_lock_mapping,
    EN("Mapping is the reconstruction itself and runs whenever it is made. To "
       "skip it, keep the reconstruction as it is."),
    JA("マッピングは再構成そのもので、再構成を作るときは必ず実行します。省くには"
       "再構成をそのまま残してください。"),
    ZH_HANS("建图就是重建本身，只要做重建就会运行。要跳过它，请保留现有的重建。"),
    ZH_HANT("建圖就是重建本身，只要做重建就會執行。要跳過它，請保留現有的重建。"),
    KO("매핑은 재구성 그 자체라서 재구성을 만들 때마다 실행됩니다. 건너뛰려면 "
       "재구성을 그대로 남겨 두세요."),
    DE("Die Kartierung ist die Rekonstruktion selbst und läuft, wann immer sie "
       "gemacht wird. Um sie zu überspringen, die Rekonstruktion behalten."),
    FR("La cartographie est la reconstruction elle-même et s'exécute chaque fois "
       "qu'elle est faite. Pour l'éviter, gardez la reconstruction telle quelle."),
    ES("El mapeo es la reconstrucción misma y se ejecuta siempre que se hace. "
       "Para saltarlo, conserve la reconstrucción tal como está."),
    PT("O mapeamento é a própria reconstrução e é executado sempre que ela é "
       "feita. Para o saltar, mantenha a reconstrução como está."),
    IT("La mappatura è la ricostruzione stessa e viene eseguita ogni volta che "
       "la si fa. Per saltarla, tieni la ricostruzione così com'è."),
    NL("Kartering is de reconstructie zelf en loopt telkens als die gemaakt "
       "wordt. Houd de reconstructie zoals ze is om haar over te slaan."),
    RU("Картирование и есть реконструкция и выполняется всякий раз, когда она "
       "делается. Чтобы пропустить его, оставьте реконструкцию как есть."),
    TR("Haritalama yeniden kurmanın kendisidir ve her yapıldığında çalışır. "
       "Atlamak için yeniden kurmayı olduğu gibi koruyun."));

SS_MSG(plan_keep_built,
    EN("Keep the existing frames and reconstruction"),
    JA("いまあるフレームと再構成を残す"),
    ZH_HANS("保留现有的帧和重建"),
    ZH_HANT("保留現有的影格和重建"),
    KO("지금 있는 프레임과 재구성 남겨 두기"),
    DE("Vorhandene Bilder und Rekonstruktion behalten"),
    FR("Garder les images et la reconstruction existantes"),
    ES("Conservar los fotogramas y la reconstrucción existentes"),
    PT("Manter os quadros e a reconstrução existentes"),
    IT("Tenere i fotogrammi e la ricostruzione esistenti"),
    NL("Bestaande beelden en reconstructie behouden"),
    RU("Оставить имеющиеся кадры и реконструкцию"),
    TR("Mevcut kareleri ve yeniden kurmayı koru"));

SS_MSG(plan_keep_built_help,
    EN("Existing results use different settings. Keep preserves them and only adds masks, depth and normals; otherwise rebuilds with current settings."),
    JA("既存結果は異なる設定で作成されています。保持するとマスク・深度・法線だけを追加し、保持しなければ現在の設定で作り直します。"),
    ZH_HANS("已有结果使用了不同设置。保留则仅添加蒙版、深度及法线；否则按当前设置重建。"),
    ZH_HANT("已有結果使用了不同設定。保留則僅新增遮罩、深度及法線；否則按目前設定重建。"),
    KO("기존 결과는 다른 설정으로 만들었습니다. 유지하면 마스크·깊이·법선만 추가하고, 아니면 현재 설정으로 다시 만듭니다."),
    DE("Vorhandene Ergebnisse haben andere Einstellungen. Behalten ergänzt nur Masken, Tiefe und Normalen; sonst wird mit aktuellen Einstellungen neu gebaut."),
    FR("Les résultats existants ont d’autres réglages. Garder ajoute seulement masques, profondeur et normales ; sinon refait avec les réglages actuels."),
    ES("Los resultados existentes usan otros ajustes. Conservar solo añade máscaras, profundidad y normales; si no, rehace con los ajustes actuales."),
    PT("Os resultados existentes usam outras definições. Manter só acrescenta máscaras, profundidade e normais; caso contrário refaz com as atuais."),
    IT("I risultati esistenti usano altre impostazioni. Mantieni aggiunge solo maschere, profondità e normali; altrimenti ricrea con le impostazioni attuali."),
    NL("Bestaande resultaten gebruiken andere instellingen. Behouden voegt alleen maskers, diepte en normalen toe; anders opnieuw met huidige instellingen."),
    RU("Имеющиеся результаты сделаны с другими настройками. Сохранение добавит лишь маски, глубину и нормали; иначе пересоздаст с текущими настройками."),
    TR("Mevcut sonuçlar farklı ayarlarla yapıldı. Tutmak yalnız maske, derinlik ve normal ekler; aksi hâlde güncel ayarlarla yeniden oluşturur."));

SS_MSG(plan_use_record,
    EN("Use the settings this dataset was made with"),
    JA("このデータセットを作ったときの設定に戻す"),
    ZH_HANS("改回做这个数据集时的设置"),
    ZH_HANT("改回做這個資料集時的設定"),
    KO("이 데이터셋을 만들 때의 설정으로 되돌리기"),
    DE("Einstellungen verwenden, mit denen dieser Datensatz gemacht wurde"),
    FR("Reprendre les réglages qui ont fait ce jeu de données"),
    ES("Usar los ajustes con que se hizo este conjunto de datos"),
    PT("Usar as definições com que este conjunto de dados foi feito"),
    IT("Usare le impostazioni con cui è stato fatto questo set di dati"),
    NL("De instellingen gebruiken waarmee deze dataset gemaakt is"),
    RU("Вернуть настройки, с которыми сделан этот набор"),
    TR("Bu veri kümesinin yapıldığı ayarları kullan"));

SS_MSG(plan_use_record_help,
    EN("Put back every setting the last run in this folder started with, so "
       "nothing on screen differs from what is on disk."),
    JA("このフォルダで最後に実行したときの設定をすべて戻し、画面とディスクの"
       "中身を一致させます。"),
    ZH_HANS("把这个文件夹上一次运行开始时的设置全部改回来，让屏幕上的设置和磁盘"
            "上的内容一致。"),
    ZH_HANT("把這個資料夾上一次執行開始時的設定全部改回來，讓螢幕上的設定和磁碟"
            "上的內容一致。"),
    KO("이 폴더에서 마지막으로 실행할 때의 설정을 모두 되돌려, 화면과 디스크의 "
       "내용이 같아지게 합니다."),
    DE("Stellt jede Einstellung wieder her, mit der der letzte Lauf in diesem "
       "Ordner begann, sodass nichts auf dem Bildschirm vom Inhalt der "
       "Festplatte abweicht."),
    FR("Remet chaque réglage avec lequel la dernière exécution dans ce dossier "
       "a commencé, pour que rien à l'écran ne diffère de ce qui est sur le "
       "disque."),
    ES("Restablece cada ajuste con el que empezó la última ejecución en esta "
       "carpeta, para que nada en pantalla difiera de lo que hay en disco."),
    PT("Repõe cada definição com que começou a última execução nesta pasta, "
       "para que nada no ecrã difira do que está no disco."),
    IT("Rimette ogni impostazione con cui è partita l'ultima esecuzione in "
       "questa cartella, così nulla a schermo differisce da ciò che è su "
       "disco."),
    NL("Zet elke instelling terug waarmee de laatste run in deze map begon, "
       "zodat niets op het scherm afwijkt van wat er op schijf staat."),
    RU("Возвращает все настройки, с которыми начался последний запуск в этой "
       "папке, чтобы на экране ничто не расходилось с тем, что на диске."),
    TR("Bu klasördeki son çalıştırmanın başladığı her ayarı geri koyar; "
       "böylece ekrandaki hiçbir şey diskteki içerikten farklı olmaz."));

SS_MSG(rebuild_title,
    EN("Redo finished steps?"),
    JA("終わった工程をやり直しますか？"),
    ZH_HANS("要重做已经完成的步骤吗？"),
    ZH_HANT("要重做已經完成的步驟嗎？"),
    KO("끝난 단계를 다시 할까요?"),
    DE("Fertige Schritte neu machen?"),
    FR("Refaire des étapes terminées ?"),
    ES("¿Rehacer pasos ya terminados?"),
    PT("Refazer passos já concluídos?"),
    IT("Rifare passi già conclusi?"),
    NL("Afgeronde stappen opnieuw doen?"),
    RU("Переделать завершённые шаги?"),
    TR("Biten adımlar yeniden yapılsın mı?"));

// {0} is the output folder.
SS_MSG(rebuild_confirm,
    EN("Some of what is already in {0} was made with settings that differ from "
       "the ones on screen, so this run would make it again:"),
    JA("{0} にすでにあるものの一部は画面と違う設定で作られているため、この実行で"
       "作り直すことになります。"),
    ZH_HANS("{0} 里已有的部分内容是用和屏幕上不同的设置做出来的，所以这次运行会"
            "重新做："),
    ZH_HANT("{0} 裡已有的部分內容是用和螢幕上不同的設定做出來的，所以這次執行會"
            "重新做："),
    KO("{0} 에 이미 있는 것 가운데 일부는 화면과 다른 설정으로 만들어져서, 이번 "
       "실행에서 다시 만듭니다:"),
    DE("Manches, was schon in {0} liegt, wurde mit anderen Einstellungen als den "
       "angezeigten gemacht; dieser Lauf würde es neu machen:"),
    FR("Une partie de ce qui se trouve déjà dans {0} a été faite avec des "
       "réglages différents de ceux affichés ; cette exécution la referait :"),
    ES("Parte de lo que ya está en {0} se hizo con ajustes distintos de los de "
       "la pantalla, así que esta ejecución lo rehará:"),
    PT("Parte do que já está em {0} foi feito com definições diferentes das do "
       "ecrã, por isso esta execução vai refazê-lo:"),
    IT("Una parte di ciò che è già in {0} è stata fatta con impostazioni diverse "
       "da quelle a schermo, quindi questa esecuzione la rifarebbe:"),
    NL("Een deel van wat al in {0} staat is gemaakt met andere instellingen dan "
       "die op het scherm, dus deze run zou het opnieuw maken:"),
    RU("Часть того, что уже лежит в {0}, сделана с настройками, отличными от "
       "тех, что на экране, поэтому этот запуск сделает это заново:"),
    TR("{0} içinde zaten olanların bir kısmı ekrandakilerden farklı ayarlarla "
       "yapıldı, bu yüzden bu çalıştırma onları yeniden yapacak:"));

SS_MSG(rebuild_go,
    EN("Redo them"),
    JA("やり直す"),       ZH_HANS("重做"),      ZH_HANT("重做"),
    KO("다시 하기"),       DE("Neu machen"),   FR("Les refaire"),
    ES("Rehacerlos"),    PT("Refazê-los"),   IT("Rifarli"),
    NL("Opnieuw doen"),  RU("Переделать"),   TR("Yeniden yap"));

SS_MSG(rebuild_keep,
    EN("Keep them, run the rest"),
    JA("残して、残りだけ実行"),
    ZH_HANS("保留它们，只跑其余的"),
    ZH_HANT("保留它們，只跑其餘的"),
    KO("남겨 두고 나머지만 실행"),
    DE("Behalten, den Rest ausführen"),
    FR("Les garder, exécuter le reste"),
    ES("Conservarlos y ejecutar el resto"),
    PT("Mantê-los e executar o resto"),
    IT("Tenerli, eseguire il resto"),
    NL("Behouden, de rest uitvoeren"),
    RU("Оставить, выполнить остальное"),
    TR("Koru, gerisini çalıştır"));

// {0} is the output folder.
SS_MSG(log_settings_restored,
    EN("Settings restored from the dataset in {0}"),
    JA("{0} のデータセットから設定を戻しました"),
    ZH_HANS("已从 {0} 的数据集恢复设置"),
    ZH_HANT("已從 {0} 的資料集恢復設定"),
    KO("{0} 의 데이터셋에서 설정을 되돌렸습니다"),
    DE("Einstellungen aus dem Datensatz in {0} übernommen"),
    FR("Réglages repris du jeu de données de {0}"),
    ES("Ajustes recuperados del conjunto de datos de {0}"),
    PT("Definições recuperadas do conjunto de dados em {0}"),
    IT("Impostazioni riprese dal set di dati in {0}"),
    NL("Instellingen overgenomen van de dataset in {0}"),
    RU("Настройки восстановлены из набора данных в {0}"),
    TR("Ayarlar {0} içindeki veri kümesinden geri yüklendi"));

SS_MSG(log_settings_unreadable,
    EN("The settings saved in {0} could not be read ({1}); the panel keeps "
       "its own"),
    JA("{0} に保存された設定を読み込めませんでした（{1}）。パネルは現在の設定のままです"),
    ZH_HANS("无法读取 {0} 中保存的设置（{1}）；面板保留当前设置"),
    ZH_HANT("無法讀取 {0} 中儲存的設定（{1}）；面板保留目前設定"),
    KO("{0} 에 저장된 설정을 읽을 수 없습니다({1}). 패널은 현재 설정을 유지합니다"),
    DE("Die in {0} gespeicherten Einstellungen ließen sich nicht lesen ({1}); "
       "das Panel behält seine eigenen"),
    FR("Les réglages enregistrés dans {0} sont illisibles ({1}) ; le panneau "
       "garde les siens"),
    ES("No se pudieron leer los ajustes guardados en {0} ({1}); el panel "
       "conserva los suyos"),
    PT("Não foi possível ler as definições guardadas em {0} ({1}); o painel "
       "mantém as suas"),
    IT("Impossibile leggere le impostazioni salvate in {0} ({1}); il pannello "
       "tiene le sue"),
    NL("De instellingen in {0} konden niet worden gelezen ({1}); het paneel "
       "houdt de eigen instellingen"),
    RU("Не удалось прочитать настройки, сохранённые в {0} ({1}); панель "
       "оставляет свои"),
    TR("{0} içinde kayıtlı ayarlar okunamadı ({1}); panel kendi ayarlarını "
       "korur"));

SS_MSG(rerun_geometry,
    EN("Depth and normals again"),
    JA("深度と法線をやり直す"), ZH_HANS("重算深度与法线"), ZH_HANT("重算深度與法線"),
    KO("깊이와 법선 다시"),   DE("Tiefe und Normalen neu"),
    FR("Refaire profondeur et normales"),
    ES("Rehacer profundidad y normales"),
    PT("Refazer profundidade e normais"),
    IT("Rifai profondità e normali"),
    NL("Diepte en normalen opnieuw"),
    RU("Глубина и нормали заново"),
    TR("Derinlik ve normaller yeniden"));

SS_MSG(rerun_geometry_help,
    EN("Estimate every map again, including the ones already on disk. The "
       "reconstruction is not touched: nothing downstream of these maps exists."),
    JA("すでにディスクにあるものも含め、すべてのマップを推定し直します。再構成には"
       "触れません。これらのマップの下流には何もないからです。"),
    ZH_HANS("重新估计所有图，包括磁盘上已有的。不动重建结果：这些图的下游没有任何"
            "东西。"),
    ZH_HANT("重新估計所有圖，包括磁碟上已有的。不動重建結果：這些圖的下游沒有任何"
            "東西。"),
    KO("디스크에 이미 있는 것까지 포함해 모든 맵을 다시 추정합니다. 재구성은 "
       "건드리지 않습니다. 이 맵들의 하류에는 아무것도 없습니다."),
    DE("Alle Karten neu schätzen, auch die schon vorhandenen. Die Rekonstruktion "
       "bleibt unangetastet: hinter diesen Karten kommt nichts mehr."),
    FR("Réestimer toutes les cartes, y compris celles déjà sur le disque. La "
       "reconstruction n'est pas touchée : rien ne dépend de ces cartes."),
    ES("Volver a estimar todos los mapas, incluidos los que ya están en disco. "
       "No se toca la reconstrucción: nada depende de estos mapas."),
    PT("Voltar a estimar todos os mapas, incluindo os que já estão no disco. A "
       "reconstrução não é tocada: nada depende destes mapas."),
    IT("Ristimare tutte le mappe, comprese quelle già su disco. La ricostruzione "
       "non viene toccata: da queste mappe non dipende nulla."),
    NL("Alle kaarten opnieuw schatten, ook die al op schijf staan. De "
       "reconstructie blijft ongemoeid: er hangt niets aan deze kaarten."),
    RU("Пересчитать все карты, включая уже лежащие на диске. Реконструкция не "
       "трогается: за этими картами ничего не следует."),
    TR("Diskte zaten bulunanlar dahil bütün haritaları yeniden kestir. Yeniden "
       "kurmaya dokunulmaz: bu haritaların ardında bir şey yoktur."));

SS_MSG(geom_unavailable,
    EN("This build cannot estimate depth and normals."),
    JA("このビルドでは深度と法線を推定できません。"),
    ZH_HANS("这个构建无法估计深度与法线。"),
    ZH_HANT("這個組建無法估計深度與法線。"),
    KO("이 빌드에서는 깊이와 법선을 추정할 수 없습니다."),
    DE("Diese Fassung kann Tiefe und Normalen nicht schätzen."),
    FR("Cette version ne peut pas estimer profondeur et normales."),
    ES("Esta compilación no puede estimar profundidad ni normales."),
    PT("Esta compilação não consegue estimar profundidade nem normais."),
    IT("Questa build non può stimare profondità e normali."),
    NL("Deze build kan diepte en normalen niet schatten."),
    RU("Эта сборка не умеет оценивать глубину и нормали."),
    TR("Bu yapı derinlik ve normalleri kestiremez."));

SS_MSG(geom_enable,
    EN("Estimate depth and normals"),
    JA("深度と法線を推定する"), ZH_HANS("估计深度与法线"), ZH_HANT("估計深度與法線"),
    KO("깊이와 법선 추정"),    DE("Tiefe und Normalen schätzen"),
    FR("Estimer profondeur et normales"),
    ES("Estimar profundidad y normales"),
    PT("Estimar profundidade e normais"),
    IT("Stima profondità e normali"),
    NL("Diepte en normalen schatten"),
    RU("Оценивать глубину и нормали"),
    TR("Derinlik ve normalleri kestir"));

SS_MSG(geom_enable_help,
    EN("Writes depth and normal maps for every reconstructed image to guide training on flat surfaces. About one second per image."),
    JA("再構成した各画像の深度・法線マップを書き、平面の学習を助けます。1画像あたり約1秒です。"),
    ZH_HANS("为每张已重建图像生成深度和法线图，帮助训练平面。每张约一秒。"),
    ZH_HANT("為每張已重建影像產生深度和法線圖，幫助訓練平面。每張約一秒。"),
    KO("재구성된 각 이미지의 깊이·법선 맵을 만들어 평면 학습을 돕습니다. 이미지당 약 1초입니다."),
    DE("Schreibt Tiefen- und Normalenkarten je rekonstruiertem Bild für ebene Flächen im Training. Etwa eine Sekunde pro Bild."),
    FR("Écrit profondeur et normales pour chaque image reconstruite afin de guider les surfaces planes à l’entraînement. Environ une seconde par image."),
    ES("Escribe profundidad y normales por imagen reconstruida para guiar superficies planas en entrenamiento. Cerca de un segundo por imagen."),
    PT("Grava profundidade e normais por imagem reconstruída para orientar superfícies planas no treino. Cerca de um segundo por imagem."),
    IT("Scrive profondità e normali per ogni immagine ricostruita per guidare le superfici piane nel training. Circa un secondo per immagine."),
    NL("Schrijft diepte- en normaalkaarten per gereconstrueerd beeld voor vlakke oppervlakken tijdens training. Ongeveer een seconde per beeld."),
    RU("Пишет глубину и нормали для каждого снимка реконструкции, помогая обучать плоские поверхности. Около секунды на снимок."),
    TR("Düz yüzeylerin eğitimini yönlendirmek için her oluşturulmuş resme derinlik ve normal haritası yazar. Resim başına yaklaşık bir saniye."));

SS_MSG(geom_model,
    EN("Geometry model"),
    JA("ジオメトリのモデル"), ZH_HANS("几何模型"),  ZH_HANT("幾何模型"),
    KO("기하 모델"),         DE("Geometriemodell"),
    FR("Modèle de géométrie"), ES("Modelo de geometría"),
    PT("Modelo de geometria"), IT("Modello di geometria"),
    NL("Geometriemodel"),    RU("Модель геометрии"),
    TR("Geometri modeli"));

SS_MSG(geom_model_moge_s,
    EN("MoGe-2 small"),
    JA("MoGe-2 スモール"), ZH_HANS("MoGe-2 小"), ZH_HANT("MoGe-2 小"),
    KO("MoGe-2 스몰"),     DE("MoGe-2 klein"),
    FR("MoGe-2 petit"),    ES("MoGe-2 pequeño"),
    PT("MoGe-2 pequeno"),  IT("MoGe-2 piccolo"),
    NL("MoGe-2 klein"),    RU("MoGe-2 малая"),
    TR("MoGe-2 küçük"));

SS_MSG(geom_model_moge_s_blurb,
    EN("141 MB, about 0.3 s an image. The quickest of the three; its normals "
       "are visibly coarser."),
    JA("141 MB、1 枚あたり約 0.3 秒。3 つの中で最も速いですが、法線は目に見えて"
       "粗くなります。"),
    ZH_HANS("141 MB，每张约 0.3 秒。三者中最快，但法线明显更粗。"),
    ZH_HANT("141 MB，每張約 0.3 秒。三者中最快，但法線明顯更粗。"),
    KO("141 MB, 장당 약 0.3 초. 셋 중 가장 빠르지만 법선이 눈에 띄게 거칩니다."),
    DE("141 MB, etwa 0,3 s je Bild. Das schnellste der drei; die Normalen sind "
       "sichtbar gröber."),
    FR("141 Mo, environ 0,3 s par image. Le plus rapide des trois ; ses "
       "normales sont visiblement plus grossières."),
    ES("141 MB, unos 0,3 s por imagen. El más rápido de los tres; sus normales "
       "son visiblemente más bastas."),
    PT("141 MB, cerca de 0,3 s por imagem. O mais rápido dos três; as suas "
       "normais são visivelmente mais grosseiras."),
    IT("141 MB, circa 0,3 s per immagine. Il più rapido dei tre; le sue normali "
       "sono visibilmente più grossolane."),
    NL("141 MB, ongeveer 0,3 s per beeld. De snelste van de drie; de normalen "
       "zijn zichtbaar grover."),
    RU("141 МБ, около 0,3 с на изображение. Самая быстрая из трёх; нормали "
       "заметно грубее."),
    TR("141 MB, görüntü başına yaklaşık 0,3 s. Üçünün en hızlısı; normalleri "
       "gözle görülür biçimde daha kabadır."));

SS_MSG(geom_model_moge_b,
    EN("MoGe-2 base"),
    JA("MoGe-2 ベース"), ZH_HANS("MoGe-2 中"), ZH_HANT("MoGe-2 中"),
    KO("MoGe-2 베이스"), DE("MoGe-2 mittel"),
    FR("MoGe-2 moyen"),  ES("MoGe-2 medio"),
    PT("MoGe-2 médio"),  IT("MoGe-2 medio"),
    NL("MoGe-2 middel"), RU("MoGe-2 средняя"),
    TR("MoGe-2 orta"));

SS_MSG(geom_model_moge_b_blurb,
    EN("419 MB, about 0.4 s an image. The default: metric depth, and the sky "
       "written as no ground truth rather than as a wall."),
    JA("419 MB、1 枚あたり約 0.4 秒。既定値です。深度は実寸で、空は壁ではなく"
       "「正解なし」として書き出されます。"),
    ZH_HANS("419 MB，每张约 0.4 秒。默认项：深度为真实尺度，天空写成“无真值”"
            "而不是一堵墙。"),
    ZH_HANT("419 MB，每張約 0.4 秒。預設項：深度為真實尺度，天空寫成「無真值」"
            "而不是一堵牆。"),
    KO("419 MB, 장당 약 0.4 초. 기본값입니다. 깊이는 실측 단위이고 하늘은 벽이 "
       "아니라 정답 없음으로 기록됩니다."),
    DE("419 MB, etwa 0,4 s je Bild. Die Vorgabe: metrische Tiefe, und der "
       "Himmel wird als fehlende Referenz statt als Wand geschrieben."),
    FR("419 Mo, environ 0,4 s par image. Le choix par défaut : profondeur "
       "métrique, et le ciel écrit comme absence de vérité plutôt qu'en mur."),
    ES("419 MB, unos 0,4 s por imagen. La opción por defecto: profundidad "
       "métrica, y el cielo escrito como sin referencia en vez de como muro."),
    PT("419 MB, cerca de 0,4 s por imagem. A predefinição: profundidade "
       "métrica, e o céu escrito como sem referência em vez de como parede."),
    IT("419 MB, circa 0,4 s per immagine. Il valore predefinito: profondità "
       "metrica, e il cielo scritto come assenza di riferimento anziché muro."),
    NL("419 MB, ongeveer 0,4 s per beeld. De standaard: metrische diepte, en "
       "de lucht geschreven als ontbrekende referentie in plaats van als muur."),
    RU("419 МБ, около 0,4 с на изображение. Значение по умолчанию: глубина в "
       "метрах, а небо пишется как отсутствие эталона, а не как стена."),
    TR("419 MB, görüntü başına yaklaşık 0,4 s. Varsayılan: metrik derinlik ve "
       "gökyüzü duvar yerine referans yok olarak yazılır."));

SS_MSG(geom_model_moge_l,
    EN("MoGe-2 large"),
    JA("MoGe-2 ラージ"), ZH_HANS("MoGe-2 大"), ZH_HANT("MoGe-2 大"),
    KO("MoGe-2 라지"),   DE("MoGe-2 groß"),
    FR("MoGe-2 grand"),  ES("MoGe-2 grande"),
    PT("MoGe-2 grande"), IT("MoGe-2 grande"),
    NL("MoGe-2 groot"),  RU("MoGe-2 большая"),
    TR("MoGe-2 büyük"));

SS_MSG(geom_model_moge_l_blurb,
    EN("1.3 GB of download and 630 MB on the card, about 0.7 s an image, for "
       "the sharpest MoGe-2 maps."),
    JA("ダウンロード 1.3 GB、カード上は 630 MB、1 枚あたり約 0.7 秒。MoGe-2 で"
       "最も鮮明なマップになります。"),
    ZH_HANS("下载 1.3 GB，显存占 630 MB，每张约 0.7 秒，贴图是 MoGe-2 中最清晰的。"),
    ZH_HANT("下載 1.3 GB，顯存佔 630 MB，每張約 0.7 秒，貼圖是 MoGe-2 中最清晰的。"),
    KO("내려받기 1.3 GB, 카드에서 630 MB, 장당 약 0.7 초로 MoGe-2 중 가장 "
       "선명한 맵을 냅니다."),
    DE("1,3 GB Download und 630 MB auf der Karte, etwa 0,7 s je Bild, für die "
       "schärfsten Karten von MoGe-2."),
    FR("1,3 Go à télécharger et 630 Mo sur la carte, environ 0,7 s par image, "
       "pour les cartes MoGe-2 les plus nettes."),
    ES("1,3 GB de descarga y 630 MB en la tarjeta, unos 0,7 s por imagen, para "
       "los mapas más nítidos de MoGe-2."),
    PT("1,3 GB de transferência e 630 MB na placa, cerca de 0,7 s por imagem, "
       "para os mapas mais nítidos do MoGe-2."),
    IT("1,3 GB da scaricare e 630 MB sulla scheda, circa 0,7 s per immagine, "
       "per le mappe più nitide di MoGe-2."),
    NL("1,3 GB download en 630 MB op de kaart, ongeveer 0,7 s per beeld, voor "
       "de scherpste kaarten van MoGe-2."),
    RU("1,3 ГБ загрузки и 630 МБ на карте, около 0,7 с на изображение, ради "
       "самых резких карт MoGe-2."),
    TR("1,3 GB indirme ve kartta 630 MB, görüntü başına yaklaşık 0,7 s; "
       "MoGe-2'nin en keskin haritaları."));

SS_MSG(geom_model_moge3_l,
    EN("MoGe-3 large"),
    JA("MoGe-3 ラージ"), ZH_HANS("MoGe-3 大"), ZH_HANT("MoGe-3 大"),
    KO("MoGe-3 라지"),   DE("MoGe-3 groß"),
    FR("MoGe-3 grand"),  ES("MoGe-3 grande"),
    PT("MoGe-3 grande"), IT("MoGe-3 grande"),
    NL("MoGe-3 groot"),  RU("MoGe-3 большая"),
    TR("MoGe-3 büyük"));

SS_MSG(geom_model_moge3_l_blurb,
    EN("Download: 1.5 GB; VRAM: 760 MB. Normals cost like MoGe-2 large. Depth refinement sharpens 3D edges at ~2× time and 1.2 GB extra VRAM."),
    JA("取得1.5 GB、VRAM 760 MB。法線はMoGe-2 largeと同程度です。深度の3D補正は輪郭を鮮明にし、時間約2倍、VRAM追加1.2 GBです。"),
    ZH_HANS("下载 1.5 GB，显存 760 MB。仅法线成本接近 MoGe-2 large。深度的 3D 优化使边缘更清晰，约需 2 倍时间和额外 1.2 GB 显存。"),
    ZH_HANT("下載 1.5 GB，顯存 760 MB。僅法線成本接近 MoGe-2 large。深度的 3D 最佳化使邊緣更清晰，約需 2 倍時間和額外 1.2 GB 顯存。"),
    KO("다운로드 1.5 GB, VRAM 760 MB. 법선 비용은 MoGe-2 large 수준입니다. 깊이 3D 보정은 경계를 선명하게 하며 약 2배 시간과 VRAM 1.2 GB가 추가됩니다."),
    DE("Download: 1.5 GB; VRAM: 760 MB. Normalen kosten wie MoGe-2 large. 3D-Tiefenverfeinerung schärft Kanten bei ~2× Zeit und 1.2 GB mehr VRAM."),
    FR("Téléchargement : 1.5 Go ; VRAM : 760 Mo. Normales comme MoGe-2 large. Affinage 3D : bords nets, ~2× temps et 1.2 Go VRAM de plus."),
    ES("Descarga: 1.5 GB; VRAM: 760 MB. Normales como MoGe-2 large. Refinado 3D: bordes nítidos, ~2× tiempo y 1.2 GB VRAM extra."),
    PT("Transferência: 1.5 GB; VRAM: 760 MB. Normais como MoGe-2 large. Refino 3D: bordas nítidas, ~2× tempo e 1.2 GB VRAM extra."),
    IT("Download: 1.5 GB; VRAM: 760 MB. Normali come MoGe-2 large. Affinamento 3D: bordi nitidi, ~2× tempo e 1.2 GB VRAM in più."),
    NL("Download: 1.5 GB; VRAM: 760 MB. Normalen kosten als MoGe-2 large. 3D-diepteverfijning: scherpere randen, ~2× tijd en 1.2 GB extra VRAM."),
    RU("Загрузка: 1.5 ГБ; VRAM: 760 МБ. Нормали как MoGe-2 large. Уточнение глубины в 3D: резче края, ~2× времени и ещё 1.2 ГБ VRAM."),
    TR("İndirme: 1.5 GB; VRAM: 760 MB. Normaller MoGe-2 large kadar. 3D derinlik iyileştirme: keskin kenarlar, ~2× süre ve ek 1.2 GB VRAM."));

SS_MSG(geom_model_small,
    EN("Metric3D v2 small"),
    JA("Metric3D v2 スモール"), ZH_HANS("Metric3D v2 小"), ZH_HANT("Metric3D v2 小"),
    KO("Metric3D v2 스몰"),   DE("Metric3D v2 klein"),
    FR("Metric3D v2 petit"), ES("Metric3D v2 pequeño"),
    PT("Metric3D v2 pequeno"), IT("Metric3D v2 piccolo"),
    NL("Metric3D v2 klein"), RU("Metric3D v2 малая"),
    TR("Metric3D v2 küçük"));

SS_MSG(geom_model_small_blurb,
    EN("72 MB, about 0.12 s an image. Enough to try the idea out; the normals "
       "are visibly coarser."),
    JA("72 MB、1 枚あたり約 0.12 秒。試すには十分ですが、法線は目に見えて粗く"
       "なります。"),
    ZH_HANS("72 MB，每张约 0.12 秒。用来试试足够，但法线明显更粗。"),
    ZH_HANT("72 MB，每張約 0.12 秒。用來試試足夠，但法線明顯更粗。"),
    KO("72 MB, 장당 약 0.12 초. 시험해 보기에는 충분하지만 법선이 눈에 띄게 "
       "거칩니다."),
    DE("72 MB, etwa 0,12 s je Bild. Zum Ausprobieren genug; die Normalen sind "
       "sichtbar gröber."),
    FR("72 Mo, environ 0,12 s par image. Assez pour essayer ; les normales sont "
       "visiblement plus grossières."),
    ES("72 MB, unos 0,12 s por imagen. Basta para probar; las normales son "
       "visiblemente más bastas."),
    PT("72 MB, cerca de 0,12 s por imagem. Chega para experimentar; as normais "
       "são visivelmente mais grosseiras."),
    IT("72 MB, circa 0,12 s per immagine. Basta per provare; le normali sono "
       "visibilmente più grossolane."),
    NL("72 MB, ongeveer 0,12 s per beeld. Genoeg om het te proberen; de normalen "
       "zijn zichtbaar grover."),
    RU("72 МБ, около 0,12 с на изображение. Хватит, чтобы попробовать; нормали "
       "заметно грубее."),
    TR("72 MB, görüntü başına yaklaşık 0,12 s. Denemek için yeter; normaller "
       "gözle görülür biçimde kabadır."));

SS_MSG(geom_model_large,
    EN("Metric3D v2 large"),
    JA("Metric3D v2 ラージ"), ZH_HANS("Metric3D v2 大"), ZH_HANT("Metric3D v2 大"),
    KO("Metric3D v2 라지"),  DE("Metric3D v2 groß"),
    FR("Metric3D v2 grand"), ES("Metric3D v2 grande"),
    PT("Metric3D v2 grande"), IT("Metric3D v2 grande"),
    NL("Metric3D v2 groot"), RU("Metric3D v2 большая"),
    TR("Metric3D v2 büyük"));

SS_MSG(geom_model_large_blurb,
    EN("790 MB, about 0.8 s an image. What the reference pipeline runs, and the "
       "right choice for almost every capture."),
    JA("790 MB、1 枚あたり約 0.8 秒。参照実装が使うもので、ほとんどの撮影ではこれ"
       "が正解です。"),
    ZH_HANS("790 MB，每张约 0.8 秒。参考实现用的就是它，几乎所有拍摄都该选这个。"),
    ZH_HANT("790 MB，每張約 0.8 秒。參考實作用的就是它，幾乎所有拍攝都該選這個。"),
    KO("790 MB, 장당 약 0.8 초. 참조 구현이 쓰는 것이며 거의 모든 촬영에 알맞습니다."),
    DE("790 MB, etwa 0,8 s je Bild. Was die Referenzpipeline nutzt, und für fast "
       "jede Aufnahme die richtige Wahl."),
    FR("790 Mo, environ 0,8 s par image. Ce qu'utilise le pipeline de référence, "
       "et le bon choix pour presque toute prise."),
    ES("790 MB, unos 0,8 s por imagen. Es lo que usa la tubería de referencia y "
       "la elección adecuada para casi toda toma."),
    PT("790 MB, cerca de 0,8 s por imagem. É o que o pipeline de referência usa "
       "e a escolha certa para quase toda captura."),
    IT("790 MB, circa 0,8 s per immagine. È ciò che usa la pipeline di "
       "riferimento e la scelta giusta per quasi ogni ripresa."),
    NL("790 MB, ongeveer 0,8 s per beeld. Wat de referentiepijplijn gebruikt, en "
       "voor bijna elke opname de juiste keuze."),
    RU("790 МБ, около 0,8 с на изображение. То, что использует эталонный "
       "конвейер, и верный выбор почти для любой съёмки."),
    TR("790 MB, görüntü başına yaklaşık 0,8 s. Referans işlem hattının "
       "kullandığı ve neredeyse her çekim için doğru seçim."));

SS_MSG(geom_model_giant,
    EN("Metric3D v2 giant"),
    JA("Metric3D v2 ジャイアント"), ZH_HANS("Metric3D v2 巨型"),
    ZH_HANT("Metric3D v2 巨型"), KO("Metric3D v2 자이언트"),
    DE("Metric3D v2 riesig"), FR("Metric3D v2 géant"),
    ES("Metric3D v2 gigante"), PT("Metric3D v2 gigante"),
    IT("Metric3D v2 gigante"), NL("Metric3D v2 reusachtig"),
    RU("Metric3D v2 гигантская"), TR("Metric3D v2 devasa"));

SS_MSG(geom_model_giant_blurb,
    EN("2.6 GB of download and 2.6 GB on the card, about 2 s an image, for a "
       "modest gain over large."),
    JA("ダウンロード 2.6 GB、カード上も 2.6 GB、1 枚あたり約 2 秒。ラージからの"
       "伸びはわずかです。"),
    ZH_HANS("下载 2.6 GB，显存也占 2.6 GB，每张约 2 秒，相比“大”只有小幅提升。"),
    ZH_HANT("下載 2.6 GB，顯存也佔 2.6 GB，每張約 2 秒，相比「大」只有小幅提升。"),
    KO("내려받기 2.6 GB, 카드에서도 2.6 GB, 장당 약 2 초이며 라지 대비 향상은 "
       "크지 않습니다."),
    DE("2,6 GB Download und 2,6 GB auf der Karte, etwa 2 s je Bild, für einen "
       "bescheidenen Gewinn gegenüber groß."),
    FR("2,6 Go à télécharger et 2,6 Go sur la carte, environ 2 s par image, pour "
       "un gain modeste sur le grand."),
    ES("2,6 GB de descarga y 2,6 GB en la tarjeta, unos 2 s por imagen, para una "
       "ganancia modesta sobre el grande."),
    PT("2,6 GB de transferência e 2,6 GB na placa, cerca de 2 s por imagem, para "
       "um ganho modesto sobre o grande."),
    IT("2,6 GB da scaricare e 2,6 GB sulla scheda, circa 2 s per immagine, per "
       "un guadagno modesto sul grande."),
    NL("2,6 GB download en 2,6 GB op de kaart, ongeveer 2 s per beeld, voor een "
       "bescheiden winst boven groot."),
    RU("2,6 ГБ загрузки и 2,6 ГБ на карте, около 2 с на изображение, ради "
       "скромного выигрыша над большой."),
    TR("2,6 GB indirme ve kartta 2,6 GB, görüntü başına yaklaşık 2 s; büyüğe "
       "göre kazanç ölçülüdür."));

SS_MSG(geom_model_marigold,
    EN("Marigold v2 (finest, slow)"),
    JA("Marigold v2（最も精細・低速）"), ZH_HANS("Marigold v2（最精细，较慢）"),
    ZH_HANT("Marigold v2（最精細，較慢）"), KO("Marigold v2 (가장 정밀, 느림)"),
    DE("Marigold v2 (feinste, langsam)"), FR("Marigold v2 (le plus fin, lent)"),
    ES("Marigold v2 (el más fino, lento)"), PT("Marigold v2 (o mais fino, lento)"),
    IT("Marigold v2 (il più fine, lento)"), NL("Marigold v2 (fijnste, traag)"),
    RU("Marigold v2 (самая детальная, медленная)"), TR("Marigold v2 (en ince, yavaş)"));

SS_MSG(geom_model_marigold_blurb,
    EN("20B-parameter diffusion model for detailed depth and normals. About 10 s per image on an RTX 5070.\n- MoGe-2 base sets depth scale and finds sky.\n- Installation streams 45 GB and keeps 11 GB."),
    JA("深度と法線向けに調整された 200 億パラメータの拡散モデルです。ここで最も精細で、ずば抜けて遅く、RTX 5070 で両方のマップに 1 枚約 10 秒かかります。\n- 深度の尺度と空の判定には MoGe-2 ベースを使います。 45 GB をストリーミングして一度だけ組み立て、11 GB を残します。"),
    ZH_HANS("为深度和法线微调的 200 亿参数扩散模型：这里细节最精细，也远比其他慢，在 RTX 5070 上两种图合计每张约 10 秒。\n深度的尺度和天空由 MoGe-2 中确定。只需流式读取 45 GB 构建一次，保留 11 GB。"),
    ZH_HANT("為深度和法線微調的 200 億參數擴散模型：這裡細節最精細，也遠比其他慢，在 RTX 5070 上兩種圖合計每張約 10 秒。\n深度的尺度和天空由 MoGe-2 中確定。只需串流讀取 45 GB 建置一次，保留 11 GB。"),
    KO("깊이와 법선용으로 조정된 200억 매개변수 확산 모델입니다. 여기서 가장 정밀하고 단연 가장 느려서, RTX 5070 에서 두 맵에 장당 약 10 초가 걸립니다.\n- 깊이의 축척과 하늘은 MoGe-2 베이스가 정합니다. 45 GB 를 스트리밍해 한 번 만들고 11 GB 를 남깁니다."),
    DE("Ein Diffusionsmodell mit 20 Milliarden Parametern, auf Tiefe und Normalen abgestimmt: das feinste Detail hier und mit Abstand das langsamste, etwa 10 s je Bild für beide Karten auf einer RTX 5070.\n- MoGe-2 mittel legt den Maßstab der Tiefe fest und findet den Himmel. Einmal aus 45 GB gestreamt gebaut; belegt 11 GB."),
    FR("Un modèle de diffusion de 20 milliards de paramètres adapté à la profondeur et aux normales : le détail le plus fin ici, et de loin le plus lent, environ 10 s par image pour les deux cartes sur une RTX 5070.\n- MoGe-2 moyen fixe l'échelle de sa profondeur et trouve le ciel. Construit une fois à partir de 45 Go lus en flux ;\n- en garde 11 Go."),
    ES("Un modelo de difusión de 20 mil millones de parámetros ajustado para profundidad y normales: el detalle más fino aquí y, con diferencia, el más lento, unos 10 s por imagen para ambos mapas en una RTX 5070.\nMoGe-2 medio fija la escala de su profundidad y detecta el cielo. Se construye una vez a partir de 45 GB transmitidos; ocupa 11 GB."),
    PT("Um modelo de difusão de 20 mil milhões de parâmetros afinado para profundidade e normais: o detalhe mais fino aqui e, de longe, o mais lento, cerca de 10 s por imagem para os dois mapas numa RTX 5070.\n- O MoGe-2 médio fixa a escala da profundidade e encontra o céu. Construído uma vez a partir de 45 GB transmitidos;\n- ocupa 11 GB."),
    IT("Un modello di diffusione da 20 miliardi di parametri adattato a profondità e normali: il dettaglio più fine qui e di gran lunga il più lento, circa 10 s per immagine per entrambe le mappe su una RTX 5070.\n- MoGe-2 medio fissa la scala della profondità e trova il cielo.\n- Costruito una volta da 45 GB trasmessi; ne tiene 11 GB."),
    NL("Een diffusiemodel met 20 miljard parameters, afgestemd op diepte en normalen:\n- het fijnste detail hier en verreweg het traagst, ongeveer 10 s per afbeelding voor beide kaarten op een RTX 5070.\n- MoGe-2 middel bepaalt de schaal van zijn diepte en vindt de lucht. Eenmalig gebouwd uit 45 GB gestreamd; houdt 11 GB."),
    RU("Диффузионная модель на 20 миллиардов параметров, настроенная на глубину и нормали: самая детальная здесь и самая медленная — около 10 с на изображение для обеих карт на RTX 5070.\n- Масштаб глубины и небо определяет MoGe-2 средняя.\n- Собирается один раз из 45 ГБ, загружаемых потоком; занимает 11 ГБ."),
    TR("Derinlik ve normaller için ayarlanmış 20 milyar parametreli bir difüzyon modeli: buradaki en ince ayrıntı ve açık ara en yavaşı;\n- RTX 5070'te iki harita için görüntü başına yaklaşık 10 sn.\n- Derinliğin ölçeğini ve gökyüzünü MoGe-2 orta belirler. Akıtılan 45 GB'tan bir kez oluşturulur; 11 GB yer kaplar."));

SS_MSG(geom_install_disk,
    EN("Keeps {0} under {1}; that disk has {2} free."),
    JA("{1} に {0} を残します。そのディスクの空きは {2} です。"),
    ZH_HANS("将在 {1} 下保留 {0}；该磁盘剩余 {2}。"),
    ZH_HANT("將在 {1} 下保留 {0}；該磁碟剩餘 {2}。"),
    KO("{1} 에 {0} 를 남깁니다. 그 디스크의 여유 공간은 {2} 입니다."),
    DE("Belegt {0} unter {1}; dort sind {2} frei."),
    FR("Garde {0} sous {1} ; ce disque a {2} de libre."),
    ES("Ocupa {0} en {1}; ese disco tiene {2} libres."),
    PT("Ocupa {0} em {1}; esse disco tem {2} livres."),
    IT("Occupa {0} in {1}; quel disco ha {2} liberi."),
    NL("Houdt {0} onder {1}; die schijf heeft {2} vrij."),
    RU("Займёт {0} в {1}; на этом диске свободно {2}."),
    TR("{1} altında {0} yer kaplar; o diskte {2} boş."));

SS_MSG(geom_install_no_disk,
    EN("Not enough disk space: it keeps {0} under {1}, plus {2} spare, and that "
       "disk has {3} free."),
    JA("ディスクの空きが足りません。{1} に {0} を残し、さらに {2} の予備が要りますが、"
       "そのディスクの空きは {3} です。"),
    ZH_HANS("磁盘空间不足：需在 {1} 下保留 {0}，另加 {2} 余量，而该磁盘只剩 {3}。"),
    ZH_HANT("磁碟空間不足：需在 {1} 下保留 {0}，另加 {2} 餘量，而該磁碟只剩 {3}。"),
    KO("디스크 공간이 부족합니다. {1} 에 {0} 와 여분 {2} 가 필요한데 그 디스크의 여유 "
       "공간은 {3} 입니다."),
    DE("Zu wenig Speicherplatz: Es belegt {0} unter {1}, dazu {2} Reserve, und "
       "dort sind {3} frei."),
    FR("Pas assez d'espace disque : il garde {0} sous {1}, plus {2} de réserve, "
       "et ce disque a {3} de libre."),
    ES("No hay espacio suficiente: ocupa {0} en {1}, más {2} de reserva, y ese "
       "disco tiene {3} libres."),
    PT("Espaço em disco insuficiente: ocupa {0} em {1}, mais {2} de reserva, e "
       "esse disco tem {3} livres."),
    IT("Spazio su disco insufficiente: occupa {0} in {1}, più {2} di riserva, e "
       "quel disco ha {3} liberi."),
    NL("Te weinig schijfruimte: het houdt {0} onder {1}, plus {2} reserve, en "
       "die schijf heeft {3} vrij."),
    RU("Недостаточно места: нужно {0} в {1} и ещё {2} в запасе, а на этом диске "
       "свободно {3}."),
    TR("Yeterli disk alanı yok: {1} altında {0} ve {2} yedek gerekiyor, o diskte "
       "{3} boş."));

SS_MSG(geom_get_model,
    EN("Get the geometry model"),
    JA("ジオメトリのモデルを取得"), ZH_HANS("获取几何模型"), ZH_HANT("取得幾何模型"),
    KO("기하 모델 받기"),         DE("Geometriemodell holen"),
    FR("Obtenir le modèle de géométrie"),
    ES("Obtener el modelo de geometría"),
    PT("Obter o modelo de geometria"),
    IT("Ottieni il modello di geometria"),
    NL("Geometriemodel ophalen"), RU("Получить модель геометрии"),
    TR("Geometri modelini getir"));

SS_MSG(geom_model_ready,
    EN("Geometry model ready."),
    JA("ジオメトリのモデルの準備ができました。"),
    ZH_HANS("几何模型已就绪。"), ZH_HANT("幾何模型已就緒。"),
    KO("기하 모델이 준비되었습니다."), DE("Geometriemodell bereit."),
    FR("Modèle de géométrie prêt."), ES("Modelo de geometría listo."),
    PT("Modelo de geometria pronto."), IT("Modello di geometria pronto."),
    NL("Geometriemodel gereed."), RU("Модель геометрии готова."),
    TR("Geometri modeli hazır."));

SS_MSG(geom_model_first,
    EN("the geometry model has not been downloaded yet"),
    JA("ジオメトリのモデルがまだダウンロードされていません"),
    ZH_HANS("还没有下载几何模型"), ZH_HANT("還沒有下載幾何模型"),
    KO("기하 모델을 아직 내려받지 않았습니다"),
    DE("das Geometriemodell ist noch nicht heruntergeladen"),
    FR("le modèle de géométrie n'est pas encore téléchargé"),
    ES("el modelo de geometría todavía no está descargado"),
    PT("o modelo de geometria ainda não foi transferido"),
    IT("il modello di geometria non è ancora scaricato"),
    NL("het geometriemodel is nog niet gedownload"),
    RU("модель геометрии ещё не скачана"),
    TR("geometri modeli henüz indirilmedi"));

SS_MSG(geom_write_normals,
    EN("Normal maps"),
    JA("法線マップ"),     ZH_HANS("法线图"),    ZH_HANT("法線圖"),
    KO("법선 맵"),        DE("Normalenkarten"), FR("Cartes de normales"),
    ES("Mapas de normales"), PT("Mapas de normais"), IT("Mappe di normali"),
    NL("Normaalkaarten"), RU("Карты нормалей"), TR("Normal haritaları"));

SS_MSG(geom_write_depth,
    EN("Depth maps"),
    JA("深度マップ"),     ZH_HANS("深度图"),    ZH_HANT("深度圖"),
    KO("깊이 맵"),        DE("Tiefenkarten"),  FR("Cartes de profondeur"),
    ES("Mapas de profundidad"), PT("Mapas de profundidade"),
    IT("Mappe di profondità"), NL("Dieptekaarten"), RU("Карты глубины"),
    TR("Derinlik haritaları"));

SS_MSG(geom_nothing_to_write,
    EN("Neither map is selected, so this step would write nothing."),
    JA("どちらのマップも選ばれていないので、この工程は何も書き出しません。"),
    ZH_HANS("两种图都没勾选，这一步不会写出任何东西。"),
    ZH_HANT("兩種圖都沒勾選，這一步不會寫出任何東西。"),
    KO("어느 맵도 선택되지 않아 이 단계는 아무것도 쓰지 않습니다."),
    DE("Keine der beiden Karten ist gewählt, dieser Schritt schriebe also nichts."),
    FR("Aucune des deux cartes n'est cochée : cette étape n'écrirait rien."),
    ES("No hay ningún mapa seleccionado, así que este paso no escribiría nada."),
    PT("Nenhum dos mapas está selecionado, por isso este passo não escreveria nada."),
    IT("Nessuna delle due mappe è selezionata, quindi questo passo non scriverebbe nulla."),
    NL("Geen van beide kaarten is aangevinkt, dus deze stap zou niets schrijven."),
    RU("Ни одна карта не выбрана, поэтому этот шаг ничего не запишет."),
    TR("İki harita da seçili değil, bu adım hiçbir şey yazmaz."));

SS_MSG(geom_try,
    EN("Try it on one frame..."),
    JA("1 フレームで試す…"), ZH_HANS("在一帧上试一下…"), ZH_HANT("在一格上試一下…"),
    KO("한 프레임에서 시험…"), DE("An einem Bild ausprobieren …"),
    FR("Essayer sur une image…"), ES("Probar en un fotograma…"),
    PT("Testar num fotograma…"), IT("Prova su un fotogramma…"),
    NL("Op één beeld uitproberen…"), RU("Проверить на одном кадре…"),
    TR("Tek karede dene…"));

SS_MSG(geom_try_help,
    EN("The same network and the same camera handling the run uses, on one picture.\nIt also says how long one image takes, which is the only honest way to know what the whole capture will cost."),
    JA("実行時と同じネットワーク・同じカメラ処理を、1 枚の絵に対して走らせます。\n1 枚あたりの所要時間も出るので、撮影全体にどれだけかかるかを正しく見積もれます。"),
    ZH_HANS("用与正式运行完全相同的网络和相机处理，跑一张图。\n它还会给出单张耗时，这是估算整段拍摄要花多久的唯一可靠办法。"),
    ZH_HANT("用與正式執行完全相同的網路和相機處理，跑一張圖。\n它還會給出單張耗時，這是估算整段拍攝要花多久的唯一可靠辦法。"),
    KO("실행 때와 똑같은 신경망과 똑같은 카메라 처리를 그림 한 장에 돌립니다. 장당 걸리는 시간도 알려 주므로 촬영 전체의 비용을 정확히 가늠할 수 있습니다."),
    DE("Dasselbe Netz und dieselbe Kamerabehandlung wie im Lauf, an einem Bild.\nEs nennt auch die Zeit je Bild -- die einzige ehrliche Art zu wissen, was die ganze Aufnahme kostet."),
    FR("Le même réseau et le même traitement de caméra que l'exécution, sur une image.\nIl donne aussi le temps par image, seule façon honnête de savoir ce que coûtera toute la prise."),
    ES("La misma red y el mismo tratamiento de cámara que la ejecución, sobre una imagen.\nTambién indica el tiempo por imagen, la única forma honesta de saber qué costará toda la toma."),
    PT("A mesma rede e o mesmo tratamento de câmara que a execução, sobre uma imagem.\nTambém diz o tempo por imagem, a única forma honesta de saber o que custará toda a captura."),
    IT("La stessa rete e lo stesso trattamento della fotocamera dell'esecuzione, su un'immagine.\nDice anche il tempo per immagine, l'unico modo onesto di sapere quanto costerà l'intera ripresa."),
    NL("Hetzelfde netwerk en dezelfde camerabehandeling als de run, op één beeld.\nHet noemt ook de tijd per beeld, de enige eerlijke manier om te weten wat de hele opname kost."),
    RU("Та же сеть и та же обработка камеры, что и в запуске, на одном снимке.\nЗаодно называет время на изображение — единственный честный способ узнать, во что обойдётся вся съёмка."),
    TR("Çalıştırmadaki ağın ve kamera işleyişinin aynısı, tek bir resim üzerinde.\nGörüntü başına süreyi de söyler; bütün çekimin neye mal olacağını bilmenin tek dürüst yolu budur."));

SS_MSG(geom_advanced,
    EN("Advanced geometry"),
    JA("ジオメトリの詳細設定"), ZH_HANS("几何高级设置"), ZH_HANT("幾何進階設定"),
    KO("기하 고급 설정"),      DE("Erweiterte Geometrie"),
    FR("Géométrie avancée"),  ES("Geometría avanzada"),
    PT("Geometria avançada"), IT("Geometria avanzata"),
    NL("Geavanceerde geometrie"), RU("Дополнительно о геометрии"),
    TR("Gelişmiş geometri"));

SS_MSG(geom_max_size,
    EN("Inference size"),
    JA("推論サイズ"),     ZH_HANS("推理尺寸"),  ZH_HANT("推論尺寸"),
    KO("추론 크기"),      DE("Inferenzgröße"), FR("Taille d'inférence"),
    ES("Tamaño de inferencia"), PT("Tamanho de inferência"),
    IT("Dimensione d'inferenza"), NL("Inferentiegrootte"),
    RU("Размер вывода"), TR("Çıkarım boyutu"));

SS_MSG(geom_num_tokens,
    EN("Tokens (MoGe)"),
    JA("トークン数 (MoGe)"), ZH_HANS("词元数 (MoGe)"), ZH_HANT("詞元數 (MoGe)"),
    KO("토큰 수 (MoGe)"),   DE("Tokens (MoGe)"),  FR("Jetons (MoGe)"),
    ES("Tokens (MoGe)"),    PT("Tokens (MoGe)"),
    IT("Token (MoGe)"),     NL("Tokens (MoGe)"),
    RU("Токены (MoGe)"),    TR("Belirteçler (MoGe)"));

SS_MSG(geom_normal_format,
    EN("Normal map format"),
    JA("法線マップの形式"), ZH_HANS("法线图格式"), ZH_HANT("法線圖格式"),
    KO("법선 맵 형식"),    DE("Format der Normalenkarten"),
    FR("Format des normales"), ES("Formato de las normales"),
    PT("Formato das normais"), IT("Formato delle normali"),
    NL("Formaat normaalkaarten"), RU("Формат карт нормалей"),
    TR("Normal haritası biçimi"));

SS_MSG(geom_jpeg_quality,
    EN("JPEG quality"),
    JA("JPEG 品質"),      ZH_HANS("JPEG 质量"), ZH_HANT("JPEG 品質"),
    KO("JPEG 품질"),      DE("JPEG-Qualität"), FR("Qualité JPEG"),
    ES("Calidad JPEG"),   PT("Qualidade JPEG"), IT("Qualità JPEG"),
    NL("JPEG-kwaliteit"), RU("Качество JPEG"), TR("JPEG kalitesi"));

SS_MSG(geom_depth_units,
    EN("Depth units"),
    JA("深度の単位"),     ZH_HANS("深度单位"),  ZH_HANT("深度單位"),
    KO("깊이 단위"),      DE("Tiefeneinheit"), FR("Unités de profondeur"),
    ES("Unidades de profundidad"), PT("Unidades de profundidade"),
    IT("Unità di profondità"), NL("Diepte-eenheden"), RU("Единицы глубины"),
    TR("Derinlik birimi"));

SS_MSG(geom_split,
    EN("Split wide frames"),
    JA("広い画角を分割"),  ZH_HANS("拆分广角画面"), ZH_HANT("拆分廣角畫面"),
    KO("넓은 화각 분할"),  DE("Weite Bilder zerlegen"),
    FR("Découper les images larges"), ES("Dividir cuadros amplios"),
    PT("Dividir quadros largos"), IT("Dividi i fotogrammi ampi"),
    NL("Brede beelden splitsen"), RU("Разбивать широкие кадры"),
    TR("Geniş kareleri böl"));

SS_MSG(geom_face_res,
    EN("Face resolution"),
    JA("面の解像度"),      ZH_HANS("拆分面分辨率"), ZH_HANT("拆分面解析度"),
    KO("면 해상도"),       DE("Flächenauflösung"),
    FR("Résolution des faces"), ES("Resolución de las caras"),
    PT("Resolução das faces"), IT("Risoluzione delle facce"),
    NL("Vlakresolutie"),   RU("Разрешение граней"), TR("Yüz çözünürlüğü"));

SS_MSG(geom_ray_depth,
    EN("Store ray depth"),
    JA("光線深度で保存"),  ZH_HANS("保存光线深度"), ZH_HANT("儲存光線深度"),
    KO("광선 깊이로 저장"), DE("Strahltiefe speichern"),
    FR("Enregistrer la profondeur radiale"),
    ES("Guardar profundidad radial"), PT("Guardar profundidade radial"),
    IT("Salva la profondità radiale"), NL("Straaldiepte opslaan"),
    RU("Хранить лучевую глубину"), TR("Işın derinliğini sakla"));

SS_MSG(geom_overwrite,
    EN("Recompute maps that already exist"),
    JA("すでにあるマップも作り直す"),
    ZH_HANS("重新计算已有的图"), ZH_HANT("重新計算已有的圖"),
    KO("이미 있는 맵도 다시 계산"),
    DE("Vorhandene Karten neu berechnen"),
    FR("Recalculer les cartes déjà présentes"),
    ES("Recalcular los mapas que ya existen"),
    PT("Recalcular os mapas que já existem"),
    IT("Ricalcola le mappe già presenti"),
    NL("Bestaande kaarten opnieuw berekenen"),
    RU("Пересчитывать уже существующие карты"),
    TR("Zaten var olan haritaları yeniden hesapla"));

// ---- the preview panel ----

SS_MSG(geom_preview_title,
    EN("Depth and normals on one frame"),
    JA("1 フレームの深度と法線"),
    ZH_HANS("单帧的深度与法线"), ZH_HANT("單格的深度與法線"),
    KO("한 프레임의 깊이와 법선"),
    DE("Tiefe und Normalen an einem Bild"),
    FR("Profondeur et normales sur une image"),
    ES("Profundidad y normales en un fotograma"),
    PT("Profundidade e normais num fotograma"),
    IT("Profondità e normali su un fotogramma"),
    NL("Diepte en normalen op één beeld"),
    RU("Глубина и нормали на одном кадре"),
    TR("Tek karede derinlik ve normaller"));

SS_MSG(geom_preview_legend,
    EN("A normal map is right when flat things read as one flat colour and "
       "edges are crisp. Black is where no face of the frame reached."),
    JA("平らな面が 1 色に読め、稜線がはっきりしていれば法線は正しく出ています。"
       "黒はどの面も届かなかった場所です。"),
    ZH_HANS("平面读起来是一整片同色、边缘清晰，法线就是对的。黑色是画面任何一个面"
            "都没覆盖到的地方。"),
    ZH_HANT("平面讀起來是一整片同色、邊緣清晰，法線就是對的。黑色是畫面任何一個面"
            "都沒覆蓋到的地方。"),
    KO("평평한 면이 한 가지 색으로 읽히고 모서리가 또렷하면 법선이 맞은 것입니다. "
       "검은색은 어느 면도 닿지 않은 곳입니다."),
    DE("Eine Normalenkarte stimmt, wenn Flächen als eine einzige Farbe lesen und "
       "Kanten scharf sind. Schwarz ist, wohin keine Fläche des Bildes reichte."),
    FR("Une carte de normales est juste quand les surfaces planes se lisent d'une "
       "seule couleur et que les arêtes sont nettes. Le noir est là où aucune "
       "face de l'image n'a atteint."),
    ES("Un mapa de normales es correcto cuando las superficies planas se leen de "
       "un solo color y las aristas son nítidas. El negro es donde no llegó "
       "ninguna cara del cuadro."),
    PT("Um mapa de normais está certo quando as superfícies planas se leem de uma "
       "só cor e as arestas são nítidas. O preto é onde nenhuma face do quadro "
       "chegou."),
    IT("Una mappa di normali è giusta quando le superfici piane si leggono di un "
       "solo colore e gli spigoli sono netti. Il nero è dove nessuna faccia del "
       "fotogramma è arrivata."),
    NL("Een normaalkaart klopt als vlakke dingen als één kleur lezen en randen "
       "scherp zijn. Zwart is waar geen enkel vlak van het beeld kwam."),
    RU("Карта нормалей верна, когда плоское читается одним цветом, а рёбра "
       "чёткие. Чёрное — куда не дотянулась ни одна грань кадра."),
    TR("Bir normal haritası, düz yüzeyler tek renk okunuyor ve kenarlar keskinse "
       "doğrudur. Siyah, karenin hiçbir yüzünün ulaşmadığı yerdir."));

SS_MSG(geom_preview_reading,
    EN("reading the dataset..."),
    JA("データセットを読み込んでいます..."),
    ZH_HANS("正在读取数据集……"), ZH_HANT("正在讀取資料集……"),
    KO("데이터셋을 읽는 중..."), DE("Datensatz wird gelesen..."),
    FR("lecture du jeu de données..."), ES("leyendo el conjunto de datos..."),
    PT("a ler o conjunto de dados..."), IT("lettura del set di dati..."),
    NL("dataset wordt gelezen..."), RU("чтение набора данных..."),
    TR("veri kümesi okunuyor..."));

SS_MSG(geom_preview_running,
    EN("estimating depth and normals..."),
    JA("深度と法線を推定しています..."),
    ZH_HANS("正在估计深度与法线……"), ZH_HANT("正在估計深度與法線……"),
    KO("깊이와 법선을 추정하는 중..."),
    DE("Tiefe und Normalen werden geschätzt..."),
    FR("estimation de la profondeur et des normales..."),
    ES("estimando profundidad y normales..."),
    PT("a estimar profundidade e normais..."),
    IT("stima di profondità e normali..."),
    NL("diepte en normalen worden geschat..."),
    RU("оценка глубины и нормалей..."),
    TR("derinlik ve normaller kestiriliyor..."));

SS_MSG(geom_preview_from_dataset,
    EN("Using the cameras of the reconstruction in the output folder. Images: {0}"),
    JA("出力フォルダの再構成結果のカメラを使っています。画像: {0}"),
    ZH_HANS("正在使用输出文件夹中重建结果的相机。图像: {0}"),
    ZH_HANT("正在使用輸出資料夾中重建結果的相機。影像: {0}"),
    KO("출력 폴더에 있는 재구성 결과의 카메라를 씁니다. 이미지: {0}"),
    DE("Es werden die Kameras der Rekonstruktion im Ausgabeordner benutzt. Bilder: {0}"),
    FR("Utilise les caméras de la reconstruction du dossier de sortie. Images : {0}"),
    ES("Se usan las cámaras de la reconstrucción de la carpeta de salida. Imágenes: {0}"),
    PT("A usar as câmaras da reconstrução na pasta de saída. Imagens: {0}"),
    IT("Si usano le fotocamere della ricostruzione nella cartella di uscita. Immagini: {0}"),
    NL("Gebruikt de camera's van de reconstructie in de uitvoermap. Beelden: {0}"),
    RU("Используются камеры реконструкции из папки вывода. Изображений: {0}"),
    TR("Çıktı klasöründeki yeniden kurmanın kameraları kullanılıyor. Görüntü: {0}"));

SS_MSG(geom_preview_assumed_lens,
    EN("Nothing is reconstructed yet, so this assumes the lens named on the screen ({0}) with no distortion. The run itself uses the reconstruction's own cameras."),
    JA("まだ再構成が無いので、画面で指定したレンズ（{0}）を歪み無しと仮定しています。実行時は再構成結果のカメラを使います。"),
    ZH_HANS("目前还没有重建结果，所以这里假定画面上选的镜头（{0}）且无畸变。正式运行时会使用重建结果自己的相机。"),
    ZH_HANT("目前還沒有重建結果，所以這裡假定畫面上選的鏡頭（{0}）且無畸變。正式執行時會使用重建結果自己的相機。"),
    KO("아직 재구성이 없어서 화면에서 고른 렌즈({0})를 왜곡 없이 가정합니다. 실제 실행은 재구성 결과의 카메라를 씁니다."),
    DE("Ohne Rekonstruktion wird das gewählte Objektiv ({0}) ohne Verzeichnung angenommen. Der Lauf nutzt die rekonstruierten Kameras."),
    FR("Rien n'est encore reconstruit : on suppose l'objectif indiqué à l'écran ({0}) sans distorsion. L'exécution, elle, utilise les caméras de la reconstruction."),
    ES("Todavía no hay nada reconstruido, así que se supone el objetivo indicado en pantalla ({0}) sin distorsión. La ejecución usa las cámaras de la reconstrucción."),
    PT("Ainda não há nada reconstruído, por isso assume-se a objetiva indicada no ecrã ({0}) sem distorção. A execução usa as câmaras da reconstrução."),
    IT("Non c'è ancora nulla di ricostruito, quindi si assume l'obiettivo indicato a schermo ({0}) senza distorsione. L'esecuzione usa le fotocamere della ricostruzione."),
    NL("Er is nog niets gereconstrueerd, dus dit veronderstelt de op het scherm genoemde lens ({0}) zonder vervorming. De run zelf gebruikt de camera's van de reconstructie."),
    RU("Реконструкции ещё нет, поэтому берётся объектив, названный на экране ({0}), без дисторсии. Сам запуск использует камеры реконструкции."),
    TR("Yeniden oluşturma henüz yoksa seçilen mercek ({0}) bozulmasız varsayılır. Asıl işlem yeniden oluşturulan kameraları kullanır."));

SS_MSG(geom_preview_nothing,
    EN("no picture to work on"),
    JA("対象になる絵がありません"),
    ZH_HANS("没有可用来处理的图"), ZH_HANT("沒有可用來處理的圖"),
    KO("작업할 그림이 없습니다"), DE("kein Bild zum Arbeiten"),
    FR("aucune image sur laquelle travailler"),
    ES("no hay ninguna imagen sobre la que trabajar"),
    PT("não há imagem sobre a qual trabalhar"),
    IT("nessuna immagine su cui lavorare"),
    NL("geen beeld om mee te werken"),
    RU("нет изображения для работы"),
    TR("üzerinde çalışılacak resim yok"));

SS_MSG(geom_preview_cost,
    EN("Faces: {0} at {1}x{2}, time per image: {3} ms"),
    JA("面: {0}（{1}x{2}）、1 枚あたり: {3} ms"),
    ZH_HANS("面: {0}（{1}x{2}）, 每张耗时: {3} ms"),
    ZH_HANT("面: {0}（{1}x{2}）, 每張耗時: {3} ms"),
    KO("면: {0} ({1}x{2}), 장당 시간: {3} ms"),
    DE("Flächen: {0} zu {1}x{2}, Zeit je Bild: {3} ms"),
    FR("Faces : {0} en {1}x{2}, temps par image : {3} ms"),
    ES("Caras: {0} a {1}x{2}, tiempo por imagen: {3} ms"),
    PT("Faces: {0} a {1}x{2}, tempo por imagem: {3} ms"),
    IT("Facce: {0} a {1}x{2}, tempo per immagine: {3} ms"),
    NL("Vlakken: {0} op {1}x{2}, tijd per beeld: {3} ms"),
    RU("Граней: {0} по {1}x{2}, время на изображение: {3} мс"),
    TR("Yüz: {0}, {1}x{2}, görüntü başına süre: {3} ms"));

SS_MSG(geom_preview_total,
    EN("Images: {0}, estimated total: {1}"),
    JA("画像: {0}、全体の見込み: {1}"),
    ZH_HANS("图像: {0}, 预计总耗时: {1}"),
    ZH_HANT("影像: {0}, 預計總耗時: {1}"),
    KO("이미지: {0}, 전체 예상: {1}"),
    DE("Bilder: {0}, geschätzt insgesamt: {1}"),
    FR("Images : {0}, total estimé : {1}"),
    ES("Imágenes: {0}, total estimado: {1}"),
    PT("Imagens: {0}, total estimado: {1}"),
    IT("Immagini: {0}, totale stimato: {1}"),
    NL("Beelden: {0}, geschat totaal: {1}"),
    RU("Изображений: {0}, всего ориентировочно: {1}"),
    TR("Görüntü: {0}, tahmini toplam: {1}"));

SS_MSG(geom_view_photo,
    EN("Photo"),          JA("写真"),          ZH_HANS("照片"),      ZH_HANT("照片"),
    KO("사진"),            DE("Foto"),         FR("Photo"),        ES("Foto"),
    PT("Foto"),           IT("Foto"),         NL("Foto"),         RU("Фото"),
    TR("Fotoğraf"));

SS_MSG(geom_view_normals,
    EN("Normals"),        JA("法線"),          ZH_HANS("法线"),      ZH_HANT("法線"),
    KO("법선"),            DE("Normalen"),     FR("Normales"),     ES("Normales"),
    PT("Normais"),        IT("Normali"),      NL("Normalen"),     RU("Нормали"),
    TR("Normaller"));

SS_MSG(geom_view_depth,
    EN("Depth"),          JA("深度"),          ZH_HANS("深度"),      ZH_HANT("深度"),
    KO("깊이"),            DE("Tiefe"),        FR("Profondeur"),   ES("Profundidad"),
    PT("Profundidade"),   IT("Profondità"),   NL("Diepte"),       RU("Глубина"),
    TR("Derinlik"));

SS_MSG(preset_ds_general,
    EN("General"),
    JA("汎用"),
    ZH_HANS("通用"),
    ZH_HANT("通用"),
    KO("일반"),
    DE("Allgemein"),
    FR("Général"),
    ES("General"),
    PT("Geral"),
    IT("Generale"),
    NL("Algemeen"),
    RU("Общий"),
    TR("Genel"));

SS_MSG(preset_ds_general_help,
    EN("The settings a capture starts on. The reconstruction reads the inputs "
       "and decides the rest for itself."),
    JA("撮影を読み込んだときの既定の設定です。残りは入力を見て再構成が自分で決めます。"),
    ZH_HANS("载入一次拍摄时的默认设置。其余的由重建读取输入后自行决定。"),
    ZH_HANT("載入一次拍攝時的預設設定。其餘由重建讀取輸入後自行決定。"),
    KO("촬영을 불러왔을 때의 기본 설정입니다. 나머지는 재구성이 입력을 보고 스스로 정합니다."),
    DE("Die Einstellungen, mit denen eine Aufnahme startet. Den Rest "
       "entscheidet die Rekonstruktion anhand der Eingaben selbst."),
    FR("Les réglages avec lesquels une prise de vue démarre. La reconstruction "
       "lit les entrées et décide du reste elle-même."),
    ES("Los ajustes con los que empieza una captura. La reconstrucción lee las "
       "entradas y decide el resto por sí misma."),
    PT("As definições com que uma captura começa. A reconstrução lê as "
       "entradas e decide o resto sozinha."),
    IT("Le impostazioni con cui parte un'acquisizione. La ricostruzione legge "
       "gli input e decide il resto da sé."),
    NL("De instellingen waarmee een opname begint. De reconstructie leest de "
       "invoer en bepaalt de rest zelf."),
    RU("Настройки, с которых начинается съёмка. Остальное реконструкция решает "
       "сама, посмотрев на входные данные."),
    TR("Bir çekimin başladığı ayarlar. Gerisini yeniden oluşturma, girdilere "
       "bakarak kendisi belirler."));

SS_MSG(preset_ds_360,
    EN("360 camera"),
    JA("360 度カメラ"),
    ZH_HANS("360 相机"),
    ZH_HANT("360 相機"),
    KO("360 카메라"),
    DE("360-Kamera"),
    FR("Caméra 360"),
    ES("Cámara 360"),
    PT("Câmara 360"),
    IT("Fotocamera 360"),
    NL("360-camera"),
    RU("Камера 360"),
    TR("360 kamera"));

SS_MSG(preset_ds_360_help,
    EN("For consumer 360° cameras: fisheye lenses, or panorama for 2:1 frames. Masks out people and bags, including the camera operator."),
    JA("一般向け360度カメラ用です。魚眼、または2:1画像ならパノラマを使い、撮影者を含む人物やバッグをマスクします。"),
    ZH_HANS("适用于消费级 360° 相机：鱼眼镜头，2:1 帧则用全景模型。排除人物、包袋及拍摄者。"),
    ZH_HANT("適用於消費級 360° 相機：魚眼鏡頭，2:1 影格則用全景模型。排除人物、包袋及拍攝者。"),
    KO("일반 360° 카메라용입니다. 어안 또는 2:1 프레임에 파노라마를 사용하고 촬영자를 포함한 사람과 가방을 마스킹합니다."),
    DE("Für Consumer-360°-Kameras: Fischauge oder Panorama bei 2:1-Bildern. Maskiert Menschen und Taschen einschließlich des Kameraträgers."),
    FR("Pour caméras 360° grand public : fisheye ou panorama pour les images 2:1. Masque personnes et sacs, y compris l’opérateur."),
    ES("Para cámaras 360° de consumo: ojo de pez o panorama para fotogramas 2:1. Enmascara personas y bolsas, incluido quien lleva la cámara."),
    PT("Para câmaras 360° de consumo: olho de peixe ou panorama para fotogramas 2:1. Mascara pessoas e sacos, incluindo quem leva a câmara."),
    IT("Per camere 360° consumer: fisheye o panorama per fotogrammi 2:1. Maschera persone e borse, incluso l’operatore."),
    NL("Voor consumenten-360°-camera’s: fisheye of panorama bij 2:1-beelden. Maskeert mensen en tassen, inclusief de cameradrager."),
    RU("Для бытовых камер 360°: рыбий глаз или панорама для кадров 2:1. Маскирует людей и сумки, включая оператора."),
    TR("Tüketici 360° kameraları için: balıkgözü, 2:1 karelerde panorama. Kamerayı taşıyan dâhil insanları ve çantaları maskeler."));

SS_MSG(preset_ds_internet,
    EN("Photos from everywhere"),
    JA("いろいろな出所の写真"),
    ZH_HANS("来源各异的照片"),
    ZH_HANT("來源各異的照片"),
    KO("여기저기서 모은 사진"),
    DE("Fotos aus aller Herkunft"),
    FR("Photos d'origines diverses"),
    ES("Fotos de procedencias diversas"),
    PT("Fotos de origens diversas"),
    IT("Foto di provenienze diverse"),
    NL("Foto's van overal"),
    RU("Фотографии из разных источников"),
    TR("Çeşitli kaynaklardan fotoğraflar"));

SS_MSG(preset_ds_internet_help,
    EN("For unrelated cameras: per-image lenses, learned features and matching, final-pass distortion refinement, depth and normals."),
    JA("異なるカメラの写真用です。画像別レンズ、学習特徴点と照合、最終段階の歪み調整、深度・法線を使います。"),
    ZH_HANS("适用于不同相机的照片：每图独立镜头、学习特征与匹配、最终阶段优化畸变、深度和法线。"),
    ZH_HANT("適用於不同相機的照片：每圖獨立鏡頭、學習特徵與匹配、最終階段最佳化畸變、深度和法線。"),
    KO("서로 다른 카메라용입니다. 이미지별 렌즈, 학습 특징·매칭, 최종 단계 왜곡 보정, 깊이·법선을 사용합니다."),
    DE("Für verschiedene Kameras: Objektiv je Bild, gelernte Merkmale und Zuordnung, Verzeichnungsanpassung am Ende, Tiefe und Normalen."),
    FR("Pour des caméras différentes : objectifs par image, caractéristiques et appariement appris, distorsion en passe finale, profondeur et normales."),
    ES("Para cámaras distintas: lentes por imagen, características y emparejamiento aprendidos, distorsión en pase final, profundidad y normales."),
    PT("Para câmaras distintas: lentes por imagem, características e correspondência aprendidas, distorção na passagem final, profundidade e normais."),
    IT("Per camere diverse: lenti per immagine, caratteristiche e abbinamento appresi, distorsione nel passo finale, profondità e normali."),
    NL("Voor verschillende camera’s: lens per beeld, geleerde kenmerken en koppeling, vervorming in de eindronde, diepte en normalen."),
    RU("Для разных камер: объектив на снимок, обученные признаки и сопоставление, дисторсия в финальном проходе, глубина и нормали."),
    TR("Farklı kameralar için: resim başına mercek, öğrenilmiş özellik ve eşleştirme, son geçişte bozulma iyileştirme, derinlik ve normaller."));

// ---------------------------------------------------------------------------
// name -> text, for app/gui/DatasetPreset.h's kDatasetPresets. Same shape as
// i18n/catalog/Train.h's table, and the picker static_asserts the two lists
// are the same length.
// ---------------------------------------------------------------------------

struct DatasetPresetText {
    const char* name;
    const Msg* label;
    const Msg* help;
};

inline constexpr DatasetPresetText kDatasetPresetText[] = {
    {"general",         &preset_ds_general,  &preset_ds_general_help},
    {"360-camera",      &preset_ds_360,      &preset_ds_360_help},
    {"internet-photos", &preset_ds_internet, &preset_ds_internet_help},
};
inline constexpr size_t kNumDatasetPresetText =
    sizeof(kDatasetPresetText) / sizeof(kDatasetPresetText[0]);

// Null for a name with no entry -- callers fall back to the name itself, so a
// preset added to DatasetPreset.h without text here still works.
inline const DatasetPresetText* preset_text(const char* name) {
    for (const DatasetPresetText& p : kDatasetPresetText)
        if (std::strcmp(p.name, name) == 0) return &p;
    return nullptr;
}


SS_MSG(source_sequential,
    EN("Sequential"),    JA("撮影順"),       ZH_HANS("按顺序"),    ZH_HANT("按順序"),
    KO("순서대로"),       DE("Sequenziell"),  FR("Séquentiel"),    ES("Secuencial"),
    PT("Sequencial"),    IT("Sequenziale"),  NL("Sequentieel"),   RU("По порядку"),
    TR("Sıralı"));

SS_MSG(source_sequential_help,
    EN("Places neighboring frames first using file order. Enable for repetitive scenes such as corridors; otherwise content matching is usually more accurate."),
    JA("ファイル順で隣り合うフレームを先に配置します。廊下など反復模様のある場面で有効にしてください。それ以外は内容による照合の方が通常は正確です。"),
    ZH_HANS("按文件顺序优先放置相邻帧。适用于走廊等重复场景；其他场景通常按图像内容匹配更准确。"),
    ZH_HANT("按檔案順序優先放置相鄰影格。適用於走廊等重複場景；其他場景通常按影像內容匹配更準確。"),
    KO("파일 순서의 인접 프레임부터 배치합니다. 복도처럼 반복되는 장면에서 켜세요. 그 외에는 내용 기반 매칭이 대체로 더 정확합니다."),
    DE("Platziert benachbarte Bilder zuerst nach Dateireihenfolge. Für wiederholte Strukturen wie Flure aktivieren; sonst ist Inhaltsabgleich meist genauer."),
    FR("Place d’abord les vues voisines selon l’ordre des fichiers. Utile dans les couloirs répétitifs ; sinon, le contenu donne souvent de meilleurs résultats."),
    ES("Coloca primero los fotogramas vecinos según el orden de archivos. Útil en escenas repetitivas como pasillos; en otras suele ser mejor el contenido."),
    PT("Coloca primeiro os fotogramas vizinhos pela ordem dos ficheiros. Útil em cenas repetitivas como corredores; nas outras, o conteúdo costuma ser mais preciso."),
    IT("Posiziona prima i fotogrammi vicini in ordine di file. Utile per scene ripetitive, come corridoi; altrimenti il confronto per contenuto è più preciso."),
    NL("Plaatst naburige beelden eerst op bestandsvolgorde. Gebruik bij herhaling, zoals gangen; anders is koppelen op beeldinhoud meestal nauwkeuriger."),
    RU("Сначала размещает соседние кадры по порядку файлов. Полезно для повторяющихся сцен, например коридоров; иначе точнее сопоставлять по содержимому."),
    TR("Önce dosya sırasındaki komşu kareleri yerleştirir. Koridor gibi tekrarlı sahnelerde açın; diğerlerinde içerik eşleştirme genelde daha doğrudur."));

// ---- Grounding DINO (a TextDetector for SAM 2.1), and BiRefNet ----

SS_MSG(model_gdino_tiny_label,
    EN("Grounding DINO Tiny"),
    JA("Grounding DINO Tiny"),
    ZH_HANS("Grounding DINO Tiny"),
    ZH_HANT("Grounding DINO Tiny"),
    KO("Grounding DINO Tiny"),
    DE("Grounding DINO Tiny"),
    FR("Grounding DINO Tiny"),
    ES("Grounding DINO Tiny"),
    PT("Grounding DINO Tiny"),
    IT("Grounding DINO Tiny"),
    NL("Grounding DINO Tiny"),
    RU("Grounding DINO Tiny"),
    TR("Grounding DINO Tiny"));

SS_MSG(model_gdino_tiny_blurb,
    EN("Finds text-prompted boxes per frame for SAM 2.1. With Base+, about one-third of SAM 3’s time. 690 MB, Apache-2.0."),
    JA("各フレームでテキストから枠を検出しSAM 2.1へ渡します。Base+ではSAM 3の約3分の1の時間です。690 MB、Apache-2.0。"),
    ZH_HANS("逐帧按文本检测框，交给 SAM 2.1。配合 Base+ 耗时约为 SAM 3 的三分之一。690 MB，Apache-2.0。"),
    ZH_HANT("逐影格按文字偵測框，交給 SAM 2.1。配合 Base+ 耗時約為 SAM 3 的三分之一。690 MB，Apache-2.0。"),
    KO("프레임마다 텍스트로 박스를 찾아 SAM 2.1에 전달합니다. Base+와 함께면 SAM 3 시간의 약 1/3입니다. 690 MB, Apache-2.0."),
    DE("Findet Text-Boxen je Bild für SAM 2.1. Mit Base+ etwa ein Drittel der Zeit von SAM 3. 690 MB, Apache-2.0."),
    FR("Trouve les boîtes par texte et par image pour SAM 2.1. Avec Base+, environ un tiers du temps de SAM 3. 690 Mo, Apache-2.0."),
    ES("Detecta cajas por texto en cada fotograma para SAM 2.1. Con Base+, tarda un tercio que SAM 3. 690 MB, Apache-2.0."),
    PT("Deteta caixas por texto em cada fotograma para SAM 2.1. Com Base+, demora um terço de SAM 3. 690 MB, Apache-2.0."),
    IT("Trova riquadri da testo per fotogramma per SAM 2.1. Con Base+, circa un terzo del tempo di SAM 3. 690 MB, Apache-2.0."),
    NL("Vindt tekstgestuurde kaders per beeld voor SAM 2.1. Met Base+ ongeveer een derde van de tijd van SAM 3. 690 MB, Apache-2.0."),
    RU("Находит рамки по тексту на каждом кадре для SAM 2.1. С Base+ около трети времени SAM 3. 690 МБ, Apache-2.0."),
    TR("Her karede metinle kutular bulup SAM 2.1’e verir. Base+ ile SAM 3’ün yaklaşık üçte biri kadar sürer. 690 MB, Apache-2.0."));

SS_MSG(model_gdino_base_label,
    EN("Grounding DINO Base"),
    JA("Grounding DINO Base"),
    ZH_HANS("Grounding DINO Base"),
    ZH_HANT("Grounding DINO Base"),
    KO("Grounding DINO Base"),
    DE("Grounding DINO Base"),
    FR("Grounding DINO Base"),
    ES("Grounding DINO Base"),
    PT("Grounding DINO Base"),
    IT("Grounding DINO Base"),
    NL("Grounding DINO Base"),
    RU("Grounding DINO Base"),
    TR("Grounding DINO Base"));

SS_MSG(model_gdino_base_blurb,
    EN("Larger detector, lang-segment-anything’s default. Finds more small objects, slightly slower than Tiny. 933 MB, Apache-2.0."),
    JA("大型の検出器でlang-segment-anythingの既定です。小さな物も多く見つけ、Tinyより少し低速です。933 MB、Apache-2.0。"),
    ZH_HANS("大型检测器，lang-segment-anything 默认。更可靠地发现小物体，比 Tiny 稍慢。933 MB，Apache-2.0。"),
    ZH_HANT("大型偵測器，lang-segment-anything 預設。更可靠地發現小物體，比 Tiny 稍慢。933 MB，Apache-2.0。"),
    KO("큰 검출기로 lang-segment-anything 기본값입니다. 작은 객체를 더 찾으며 Tiny보다 조금 느립니다. 933 MB, Apache-2.0."),
    DE("Größerer Detektor, Standard bei lang-segment-anything. Findet mehr Kleines, etwas langsamer als Tiny. 933 MB, Apache-2.0."),
    FR("Grand détecteur, défaut de lang-segment-anything. Trouve plus de petits objets, un peu plus lent que Tiny. 933 Mo, Apache-2.0."),
    ES("Detector mayor, predeterminado de lang-segment-anything. Encuentra más objetos pequeños, algo más lento que Tiny. 933 MB, Apache-2.0."),
    PT("Detetor maior, padrão do lang-segment-anything. Encontra mais objetos pequenos, um pouco mais lento que Tiny. 933 MB, Apache-2.0."),
    IT("Rilevatore maggiore, predefinito di lang-segment-anything. Trova più oggetti piccoli, poco più lento di Tiny. 933 MB, Apache-2.0."),
    NL("Grotere detector, standaard van lang-segment-anything. Vindt meer kleine objecten, iets trager dan Tiny. 933 MB, Apache-2.0."),
    RU("Крупный детектор, стандарт lang-segment-anything. Находит больше мелких объектов, чуть медленнее Tiny. 933 МБ, Apache-2.0."),
    TR("Büyük dedektör, lang-segment-anything varsayılanı. Daha çok küçük nesne bulur, Tiny’den biraz yavaştır. 933 MB, Apache-2.0."));

SS_MSG(model_birefnet_label,
    EN("BiRefNet (most accurate for main subject)"),
    JA("BiRefNet（主な被写体で最も正確）"),
    ZH_HANS("BiRefNet（主体分割最准确）"),
    ZH_HANT("BiRefNet（主體分割最準確）"),
    KO("BiRefNet(주 피사체에 가장 정확)"),
    DE("BiRefNet (am genauesten für das Hauptmotiv)"),
    FR("BiRefNet (le plus précis pour le sujet principal)"),
    ES("BiRefNet (el más preciso para el sujeto principal)"),
    PT("BiRefNet (o mais preciso para o objeto principal)"),
    IT("BiRefNet (il più preciso per il soggetto principale)"),
    NL("BiRefNet (nauwkeurigst voor het hoofdonderwerp)"),
    RU("BiRefNet (точнее всех для главного объекта)"),
    TR("BiRefNet (ana özne için en doğru)"));

SS_MSG(model_birefnet_blurb,
    EN("Finds the main subject of every frame by itself -- for object captures on a turntable or held in hand.\nNo text and no clicks; about twice the time of BiRefNet Lite. 444 MB, MIT."),
    JA("各フレームの主な被写体を自動で見つけます。 ターンテーブルに載せたり手に持ったりして撮る物体向けです。\nテキストもクリックも不要、BiRefNet Lite のおよそ 2 倍の時間がかかります。 444 MB、MIT。"),
    ZH_HANS("自动找出每一帧的主体——适合放在转台上或拿在手里拍摄的物体。不需要文字，也不需要点击；耗时约为 BiRefNet Lite 的两倍。444 MB，MIT。"),
    ZH_HANT("自動找出每一格的主體——適合放在轉台上或拿在手裡拍攝的物體。不需要文字，也不需要點擊；耗時約為 BiRefNet Lite 的兩倍。444 MB，MIT。"),
    KO("모든 프레임의 주 피사체를 스스로 찾습니다. 턴테이블에 올리거나 손에 들고 찍는 물체용입니다. 텍스트도 클릭도 필요 없고 BiRefNet Lite의 약 두 배 시간이 걸립니다. 444 MB, MIT."),
    DE("Findet das Hauptmotiv jedes Bildes von selbst -- für Objekte auf dem Drehteller oder in der Hand.\nKein Text, keine Klicks; etwa doppelt so lange wie BiRefNet Lite. 444 MB, MIT."),
    FR("Trouve seul le sujet principal de chaque image -- pour les objets filmés sur un plateau tournant ou tenus en main.\nNi texte ni clics ; environ deux fois le temps de BiRefNet Lite. 444 Mo, MIT."),
    ES("Detecta el sujeto principal sin texto ni clics, para objetos en mano o en plataforma giratoria. Tarda el doble que Lite. 444 MB, MIT."),
    PT("Encontra sozinho o objeto principal de cada quadro -- para objetos numa base giratória ou segurados na mão.\nSem texto e sem cliques; cerca do dobro do tempo do BiRefNet Lite. 444 MB, MIT."),
    IT("Trova da solo il soggetto principale di ogni fotogramma: per oggetti su un piatto girevole o tenuti in mano.\nNiente testo né clic; circa il doppio del tempo di BiRefNet Lite. 444 MB, MIT."),
    NL("Vindt uit zichzelf het hoofdonderwerp van elk beeld -- voor objecten op een draaitafel of in de hand.\nGeen tekst en geen klikken; ongeveer twee keer de tijd van BiRefNet Lite. 444 MB, MIT."),
    RU("Сам находит главный объект каждого кадра — для предметов на поворотном столе или в руке. Без текста и щелчков; примерно вдвое дольше BiRefNet Lite. 444 МБ, MIT."),
    TR("Her karenin ana öznesini kendiliğinden bulur: döner tablada ya da elde çekilen nesneler için.\nMetin de tıklama da gerekmez; BiRefNet Lite'ın yaklaşık iki katı süre. 444 MB, MIT."));

SS_MSG(model_birefnet_lite_label,
    EN("BiRefNet Lite (main subject, no prompt)"),
    JA("BiRefNet Lite（主な被写体、プロンプト不要）"),
    ZH_HANS("BiRefNet Lite（主体，无需提示）"),
    ZH_HANT("BiRefNet Lite（主體，無需提示）"),
    KO("BiRefNet Lite(주 피사체, 프롬프트 불필요)"),
    DE("BiRefNet Lite (Hauptmotiv, ohne Prompt)"),
    FR("BiRefNet Lite (sujet principal, sans consigne)"),
    ES("BiRefNet Lite (sujeto principal, sin indicación)"),
    PT("BiRefNet Lite (objeto principal, sem comando)"),
    IT("BiRefNet Lite (soggetto principale, senza prompt)"),
    NL("BiRefNet Lite (hoofdonderwerp, zonder prompt)"),
    RU("BiRefNet Lite (главный объект, без запроса)"),
    TR("BiRefNet Lite (ana özne, istem yok)"));

SS_MSG(model_birefnet_lite_blurb,
    EN("The smaller BiRefNet: twice as fast, with slightly less certain edges. "
       "178 MB, MIT."),
    JA("小さい BiRefNet。2 倍速く、輪郭はやや曖昧です。178 MB、MI"
       "T。"),
    ZH_HANS("较小的 BiRefNet：快一倍，边缘稍欠准确。178 MB，MIT。"),
    ZH_HANT("較小的 BiRefNet：快一倍，邊緣稍欠準確。178 MB，MIT。"),
    KO("작은 BiRefNet: 두 배 빠르고 윤곽은 조금 덜 확실합니다. 178 MB, MIT."),
    DE("Das kleinere BiRefNet: doppelt so schnell, mit etwas unsichereren "
       "Kanten. 178 MB, MIT."),
    FR("Le BiRefNet plus petit : deux fois plus rapide, avec des bords un peu "
       "moins sûrs. 178 Mo, MIT."),
    ES("El BiRefNet pequeño: el doble de rápido, con bordes algo menos seguros. "
       "178 MB, MIT."),
    PT("O BiRefNet menor: duas vezes mais rápido, com bordas um pouco menos "
       "precisas. 178 MB, MIT."),
    IT("Il BiRefNet più piccolo: veloce il doppio, con bordi un po' meno sicuri. "
       "178 MB, MIT."),
    NL("De kleinere BiRefNet: twee keer zo snel, met iets minder zekere randen. "
       "178 MB, MIT."),
    RU("Меньший BiRefNet: вдвое быстрее, края чуть менее уверенные. 178 МБ, MIT."),
    TR("Küçük BiRefNet: iki kat hızlı, kenarları biraz daha az kesin. 178 MB, "
       "MIT."));

SS_MSG(license_gdino_title,
    EN("Grounding DINO licence (Apache-2.0)"),
    JA("Grounding DINO のライセンス（Apache-2.0）"),
    ZH_HANS("Grounding DINO 许可协议（Apache-2.0）"),
    ZH_HANT("Grounding DINO 授權條款（Apache-2.0）"),
    KO("Grounding DINO 라이선스(Apache-2.0)"),
    DE("Lizenz von Grounding DINO (Apache-2.0)"),
    FR("Licence de Grounding DINO (Apache-2.0)"),
    ES("Licencia de Grounding DINO (Apache-2.0)"),
    PT("Licença do Grounding DINO (Apache-2.0)"),
    IT("Licenza di Grounding DINO (Apache-2.0)"),
    NL("Licentie van Grounding DINO (Apache-2.0)"),
    RU("Лицензия Grounding DINO (Apache-2.0)"),
    TR("Grounding DINO lisansı (Apache-2.0)"));

SS_MSG(license_gdino_summary,
    EN("Grounding DINO (IDEA Research) is released under the Apache 2.0 licence. Downloaded separately to keep the app small."),
    JA("Grounding DINO（IDEA Research）は Apache 2.0 ライセンスで公開されています。 アプリを小さく保つため別途取得します。"),
    ZH_HANS("Grounding DINO（IDEA Research）以 Apache 2.0 许可协议发布。 单独下载以减小应用体积。"),
    ZH_HANT("Grounding DINO（IDEA Research）以 Apache 2.0 授權條款發布。 單獨下載以減小應用體積。"),
    KO("Grounding DINO(IDEA Research)는 Apache 2.0 라이선스로 공개되어 있습니다. 앱 크기를 줄이기 위해 별도로 받습니다."),
    DE("Grounding DINO (IDEA Research) steht unter der Apache-2.0-Lizenz. Separater Download hält die App klein."),
    FR("Grounding DINO (IDEA Research) est publié sous licence Apache 2.0. Téléchargé séparément pour alléger l’application."),
    ES("Grounding DINO (IDEA Research) se publica con licencia Apache 2.0. Se descarga aparte para reducir la aplicación."),
    PT("O Grounding DINO (IDEA Research) é publicado sob a licença Apache 2.0. Descarregado à parte para reduzir a aplicação."),
    IT("Grounding DINO (IDEA Research) è pubblicato con licenza Apache 2.0. Scaricato a parte per ridurre l’app."),
    NL("Grounding DINO (IDEA Research) is uitgebracht onder de Apache 2.0-licentie. Apart gedownload om de app klein te houden."),
    RU("Grounding DINO (IDEA Research) выпущен под лицензией Apache 2.0. Загружается отдельно для уменьшения приложения."),
    TR("Grounding DINO (IDEA Research) Apache 2.0 lisansıyla yayımlanır. Uygulamayı küçük tutmak için ayrı indirilir."));

SS_MSG(license_birefnet_title,
    EN("BiRefNet License (MIT)"),
    JA("BiRefNet ライセンス（MIT）"),
    ZH_HANS("BiRefNet 许可协议（MIT）"),
    ZH_HANT("BiRefNet 授權條款（MIT）"),
    KO("BiRefNet 라이선스(MIT)"),
    DE("BiRefNet-Lizenz (MIT)"),
    FR("Licence BiRefNet (MIT)"),
    ES("Licencia de BiRefNet (MIT)"),
    PT("Licença do BiRefNet (MIT)"),
    IT("Licenza BiRefNet (MIT)"),
    NL("BiRefNet-licentie (MIT)"),
    RU("Лицензия BiRefNet (MIT)"),
    TR("BiRefNet Lisansı (MIT)"));

SS_MSG(license_birefnet_summary,
    EN("BiRefNet (Peng Zheng et al.) is released under the MIT licence. Downloaded separately to keep the app small."),
    JA("BiRefNet（Peng Zheng ほか）は MIT ライセンスで公開されています。 アプリを小さく保つため別途取得します。"),
    ZH_HANS("BiRefNet（Peng Zheng 等）以 MIT 许可协议发布。 单独下载以减小应用体积。"),
    ZH_HANT("BiRefNet（Peng Zheng 等）以 MIT 授權條款發布。 單獨下載以減小應用體積。"),
    KO("BiRefNet(Peng Zheng 외)은 MIT 라이선스로 공개되어 있습니다. 앱 크기를 줄이기 위해 별도로 받습니다."),
    DE("BiRefNet (Peng Zheng u. a.) steht unter der MIT-Lizenz. Separater Download hält die App klein."),
    FR("BiRefNet (Peng Zheng et al.) est publié sous licence MIT. Téléchargé séparément pour alléger l’application."),
    ES("BiRefNet (Peng Zheng et al.) se publica con licencia MIT. Se descarga aparte para reducir la aplicación."),
    PT("O BiRefNet (Peng Zheng et al.) é publicado sob a licença MIT. Descarregado à parte para reduzir a aplicação."),
    IT("BiRefNet (Peng Zheng et al.) è pubblicato con licenza MIT. Scaricato a parte per ridurre l’app."),
    NL("BiRefNet (Peng Zheng e.a.) is uitgebracht onder de MIT-licentie. Apart gedownload om de app klein te houden."),
    RU("BiRefNet (Peng Zheng и др.) выпущен под лицензией MIT. Загружается отдельно для уменьшения приложения."),
    TR("BiRefNet (Peng Zheng ve ark.) MIT lisansıyla yayımlanır. Uygulamayı küçük tutmak için ayrı indirilir."));

// {0} is the settings file.
SS_MSG(license_not_saved,
    EN("Could not save your acceptance to {0}, so the licence is not accepted."),
    JA("同意を {0} に保存できなかったため、ライセンスには同意していない扱いです。"),
    ZH_HANS("无法把你的接受记录保存到 {0}，因此该许可协议尚未被接受。"),
    ZH_HANT("無法把你的接受記錄儲存到 {0}，因此該授權條款尚未被接受。"),
    KO("동의를 {0}에 저장하지 못해 라이선스에 동의하지 않은 것으로 처리됩니다."),
    DE("Die Zustimmung ließ sich nicht in {0} speichern, die Lizenz gilt daher "
       "nicht als angenommen."),
    FR("Impossible d'enregistrer votre acceptation dans {0} ; la licence n'est "
       "donc pas acceptée."),
    ES("No se pudo guardar tu aceptación en {0}, así que la licencia no está "
       "aceptada."),
    PT("Não foi possível salvar sua aceitação em {0}, então a licença não está "
       "aceita."),
    IT("Impossibile salvare la tua accettazione in {0}, quindi la licenza non "
       "risulta accettata."),
    NL("Je aanvaarding kon niet worden opgeslagen in {0}, dus de licentie is niet "
       "aanvaard."),
    RU("Не удалось сохранить ваше согласие в {0}, поэтому лицензия не считается "
       "принятой."),
    TR("Kabulünüz {0} dosyasına kaydedilemedi, bu yüzden lisans kabul edilmiş "
       "sayılmaz."));

// {0} is the licence family, e.g. "sam3".
SS_MSG(license_not_accepted_download,
    EN("The {0} licence has not been accepted, so nothing was downloaded."),
    JA("{0} のライセンスに同意していないため、何もダウンロードしていません。"),
    ZH_HANS("尚未接受 {0} 许可协议，因此没有下载任何内容。"),
    ZH_HANT("尚未接受 {0} 授權條款，因此沒有下載任何內容。"),
    KO("{0} 라이선스에 동의하지 않았으므로 아무것도 내려받지 않았습니다."),
    DE("Die Lizenz {0} wurde nicht angenommen, deshalb wurde nichts heruntergeladen."),
    FR("La licence {0} n'a pas été acceptée ; rien n'a donc été téléchargé."),
    ES("No se ha aceptado la licencia {0}, así que no se descargó nada."),
    PT("A licença {0} não foi aceita, então nada foi baixado."),
    IT("La licenza {0} non è stata accettata, quindi non è stato scaricato nulla."),
    NL("De licentie {0} is niet aanvaard, dus er is niets gedownload."),
    RU("Лицензия {0} не принята, поэтому ничего не было загружено."),
    TR("{0} lisansı kabul edilmedi, bu yüzden hiçbir şey indirilmedi."));

SS_MSG(license_accept,
    EN("Accept"), JA("同意する"), ZH_HANS("接受"), ZH_HANT("接受"), KO("동의"),
    DE("Annehmen"), FR("Accepter"), ES("Aceptar"), PT("Aceitar"), IT("Accetta"),
    NL("Aanvaarden"), RU("Принять"), TR("Kabul et"));

SS_MSG(license_full_text,
    EN("Full text"), JA("全文"), ZH_HANS("全文"), ZH_HANT("全文"), KO("전문"),
    DE("Vollständiger Text"), FR("Texte intégral"), ES("Texto completo"),
    PT("Texto completo"), IT("Testo completo"), NL("Volledige tekst"),
    RU("Полный текст"), TR("Tam metin"));

SS_MSG(mask_subject_note,
    EN("This model finds the main subject of each frame by itself: no prompt and no "
       "clicks are needed."),
    JA("このモデルは各フレームの主な被写体を自動で見つけます。プロンプトもクリックも"
       "不要です。"),
    ZH_HANS("这个模型会自动找出每一帧的主体：不需要提示词，也不需要点击。"),
    ZH_HANT("這個模型會自動找出每一格的主體：不需要提示詞，也不需要點擊。"),
    KO("이 모델은 각 프레임의 주 피사체를 스스로 찾습니다. 프롬프트도 클릭도 필요 "
       "없습니다."),
    DE("Dieses Modell findet das Hauptmotiv jedes Bildes von selbst: kein Prompt und "
       "keine Klicks nötig."),
    FR("Ce modèle trouve seul le sujet principal de chaque image : ni consigne ni clic "
       "ne sont nécessaires."),
    ES("Este modelo encuentra por sí solo el sujeto principal de cada fotograma: no "
       "hacen falta indicaciones ni clics."),
    PT("Este modelo encontra sozinho o objeto principal de cada quadro: não precisa de "
       "comando nem de cliques."),
    IT("Questo modello trova da solo il soggetto principale di ogni fotogramma: non "
       "servono prompt né clic."),
    NL("Dit model vindt uit zichzelf het hoofdonderwerp van elk beeld: geen prompt en "
       "geen klikken nodig."),
    RU("Эта модель сама находит главный объект каждого кадра: ни запрос, ни щелчки не "
       "нужны."),
    TR("Bu model her karenin ana öznesini kendiliğinden bulur: istem de tıklama da "
       "gerekmez."));

SS_MSG(mask_subject_keep,
    EN("Keep the subject"),
    JA("被写体を残す"),
    ZH_HANS("保留主体"),
    ZH_HANT("保留主體"),
    KO("피사체 남기기"),
    DE("Motiv behalten"),
    FR("Garder le sujet"),
    ES("Conservar el sujeto"),
    PT("Manter o objeto"),
    IT("Tenere il soggetto"),
    NL("Onderwerp behouden"),
    RU("Оставить объект"),
    TR("Özneyi koru"));

SS_MSG(mask_subject_remove,
    EN("Remove the subject"),
    JA("被写体を取り除く"),
    ZH_HANS("移除主体"),
    ZH_HANT("移除主體"),
    KO("피사체 제거"),
    DE("Motiv entfernen"),
    FR("Retirer le sujet"),
    ES("Quitar el sujeto"),
    PT("Remover o objeto"),
    IT("Togliere il soggetto"),
    NL("Onderwerp verwijderen"),
    RU("Убрать объект"),
    TR("Özneyi kaldır"));

SS_MSG(mask_subject_polarity_help,
    EN("Keep the subject for an object capture: everything around it is ignored. "
       "Remove it when it is the thing in the way."),
    JA("物体の撮影では被写体を残します。周りはすべて無視されます。被写体が邪魔物の"
       "ときは取り除きます。"),
    ZH_HANS("拍摄物体时保留主体：周围的一切都会被忽略。主体本身碍事时就把它移除。"),
    ZH_HANT("拍攝物體時保留主體：周圍的一切都會被忽略。主體本身礙事時就把它移除。"),
    KO("물체 촬영에서는 피사체를 남깁니다. 주변은 모두 무시됩니다. 피사체가 방해물일 "
       "때는 제거합니다."),
    DE("Für eine Objektaufnahme das Motiv behalten: alles darum herum wird ignoriert. "
       "Entfernen, wenn es selbst im Weg ist."),
    FR("Gardez le sujet pour la capture d'un objet : tout ce qui l'entoure est ignoré. "
       "Retirez-le quand c'est lui qui gêne."),
    ES("Conserve el sujeto al capturar un objeto: todo lo que lo rodea se ignora. "
       "Quítelo cuando sea él lo que estorba."),
    PT("Mantenha o objeto ao capturar um objeto: tudo ao redor é ignorado. Remova-o "
       "quando ele é o que atrapalha."),
    IT("Tenete il soggetto per la ripresa di un oggetto: tutto ciò che lo circonda "
       "viene ignorato. Toglietelo quando è lui a dare fastidio."),
    NL("Behoud het onderwerp bij een objectopname: alles eromheen wordt genegeerd. "
       "Verwijder het als het zelf in de weg zit."),
    RU("Оставьте объект при съёмке предмета: всё вокруг будет проигнорировано. "
       "Уберите его, когда мешает он сам."),
    TR("Nesne çekiminde özneyi koruyun: çevresindeki her şey yok sayılır. Engel olan "
       "özneyse onu kaldırın."));

}  // namespace dataset
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
