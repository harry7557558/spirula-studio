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
    EN("Colour primaries the photographs were captured in. Reconstruction, AI "
       "masking and depth/normal estimation convert them to sRGB first, which "
       "is what those detectors and models were trained on."),
    JA("写真が撮影された色域です。再構成・AI マスキング・深度/法線推定は先に "
       "sRGB へ変換します。検出器もモデルもそれで学習されています。"),
    ZH_HANS("照片拍摄时使用的色域。重建、AI 遮罩与深度/法线估计会先转换为 sRGB，"
            "这些检测器和模型都以此训练。"),
    ZH_HANT("照片拍攝時使用的色域。重建、AI 遮罩與深度/法線估計會先轉換為 sRGB，"
            "這些偵測器與模型都以此訓練。"),
    KO("사진이 촬영된 색역입니다. 재구성, AI 마스킹, 깊이/법선 추정은 먼저 sRGB 로 "
       "변환하며, 그 검출기와 모델이 그것으로 학습되었습니다."),
    DE("Farbprimärvalenzen der Aufnahmen. Rekonstruktion, KI-Maskierung und "
       "Tiefen-/Normalenschätzung wandeln sie zuerst nach sRGB, worauf diese "
       "Detektoren und Modelle trainiert wurden."),
    FR("Primaires de couleur des photographies. La reconstruction, le masquage "
       "par IA et l'estimation de profondeur/normales les passent d'abord en "
       "sRGB, ce sur quoi ces détecteurs et modèles ont été entraînés."),
    ES("Primarios de color de las fotografías. La reconstrucción, el "
       "enmascarado por IA y la estimación de profundidad/normales las pasan "
       "antes a sRGB, que es con lo que se entrenaron esos detectores y "
       "modelos."),
    PT("Primárias de cor das fotografias. A reconstrução, o mascaramento por IA "
       "e a estimativa de profundidade/normais passam-nas antes a sRGB, que é "
       "com o que esses detectores e modelos foram treinados."),
    IT("Primarie di colore delle fotografie. Ricostruzione, mascheratura con IA "
       "e stima di profondità/normali le portano prima a sRGB, su cui quei "
       "rilevatori e modelli sono stati addestrati."),
    NL("Kleurprimairen van de foto's. Reconstructie, AI-maskering en diepte-/"
       "normaalschatting zetten ze eerst om naar sRGB, waarop die detectoren en "
       "modellen zijn getraind."),
    RU("Основные цвета снимков. Реконструкция, ИИ-маскирование и оценка "
       "глубины/нормалей сначала переводят их в sRGB — на этом обучались эти "
       "детекторы и модели."),
    TR("Fotoğrafların çekildiği renk birincilleri. Yeniden oluşturma, yapay "
       "zekâ maskeleme ve derinlik/normal kestirimi onları önce sRGB'ye "
       "çevirir; bu algılayıcılar ve modeller bununla eğitildi."));

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

// Item 0 of BOTH colour-space pickers: an EXR declares its own, and this is
// what says "do not override it".
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
    EN("Whether the pictures hold scene-linear light (EXR, linear 16-bit) or "
       "ordinary display-encoded values. Read from an EXR's own header unless "
       "you set it here."),
    JA("写真がシーンリニアの光（EXR、リニア 16 ビット）か、通常の表示用に"
       "エンコードされた値かです。ここで設定しない限り、EXR のヘッダーから"
       "読み取ります。"),
    ZH_HANS("照片存的是场景线性光（EXR、线性 16 位），还是普通的显示编码数值。"
            "除非在此设置，否则取自 EXR 自己的文件头。"),
    ZH_HANT("照片存的是場景線性光（EXR、線性 16 位元），還是普通的顯示編碼數值。"
            "除非在此設定，否則取自 EXR 自己的檔頭。"),
    KO("사진이 장면 선형 광(EXR, 선형 16비트)인지 보통의 디스플레이 인코딩 "
       "값인지입니다. 여기서 설정하지 않으면 EXR 헤더에서 읽습니다."),
    DE("Ob die Bilder szenenlineares Licht (EXR, lineare 16 Bit) oder gewöhnliche "
       "anzeigecodierte Werte enthalten. Wird aus dem Kopf einer EXR gelesen, "
       "solange Sie es hier nicht setzen."),
    FR("Si les images contiennent de la lumière scène-linéaire (EXR, 16 bits "
       "linéaire) ou des valeurs encodées pour l'affichage. Lu dans l'en-tête "
       "d'un EXR tant que vous ne le réglez pas ici."),
    ES("Si las fotos guardan luz escena-lineal (EXR, 16 bits lineal) o valores "
       "corrientes codificados para pantalla. Se lee de la cabecera de un EXR "
       "mientras no lo fije aquí."),
    PT("Se as fotos guardam luz cena-linear (EXR, 16 bits linear) ou valores "
       "comuns codificados para exibição. Lido do cabeçalho de um EXR enquanto "
       "não o definir aqui."),
    IT("Se le foto contengono luce scena-lineare (EXR, 16 bit lineare) o comuni "
       "valori codificati per la visualizzazione. Letto dall'intestazione di un "
       "EXR finché non lo imposti qui."),
    NL("Of de foto's scène-lineair licht (EXR, lineair 16-bits) bevatten of "
       "gewone voor weergave gecodeerde waarden. Wordt uit de kop van een EXR "
       "gelezen zolang u het hier niet instelt."),
    RU("Хранят ли снимки сцен-линейный свет (EXR, линейные 16 бит) или обычные "
       "значения с кодировкой для дисплея. Читается из заголовка EXR, пока вы не "
       "зададите это здесь."),
    TR("Fotoğrafların sahne-doğrusal ışık (EXR, doğrusal 16 bit) mi yoksa "
       "sıradan ekran için kodlanmış değerler mi tuttuğu. Burada ayarlamadığınız "
       "sürece bir EXR'nin başlığından okunur."));

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
    EN("Off leaves the sparse point cloud in sRGB, which the trainer converts "
       "with \"Convert seed point colours\". On writes it in the photographs' "
       "own space, for training with that conversion off."),
    JA("オフなら疎な点群は sRGB のままで、学習側の「シード点の色を変換」が処理"
       "します。オンなら写真と同じ空間で書き出し、その変換をオフにして学習します。"),
    ZH_HANS("关闭时稀疏点云保持 sRGB，由训练端的\"转换种子点颜色\"处理；开启则以"
            "照片自身的空间写出，训练时关掉该转换。"),
    ZH_HANT("關閉時稀疏點雲保持 sRGB，由訓練端的「轉換種子點顏色」處理；開啟則以"
            "照片自身的空間寫出，訓練時關掉該轉換。"),
    KO("끄면 희소 점 구름은 sRGB 로 남고 학습 쪽의 \"시드 점 색 변환\"이 처리합니다. "
       "켜면 사진과 같은 공간으로 써서, 그 변환을 끄고 학습합니다."),
    DE("Aus lässt die dünne Punktwolke in sRGB, was das Training mit "
       "\"Startpunktfarben umrechnen\" erledigt. An schreibt sie im Raum der "
       "Fotos, für ein Training mit ausgeschalteter Umrechnung."),
    FR("Désactivé laisse le nuage épars en sRGB, que l'entraînement traite avec "
       "« Convertir les couleurs des points de départ ». Activé l'écrit dans "
       "l'espace des photographies, pour un entraînement sans cette conversion."),
    ES("Desactivado deja la nube dispersa en sRGB, de lo que se encarga "
       "«Convertir los colores de los puntos iniciales» en el entrenamiento. "
       "Activado la escribe en el espacio de las fotografías, para entrenar sin "
       "esa conversión."),
    PT("Desligado deixa a nuvem esparsa em sRGB, do que trata «Converter as "
       "cores dos pontos iniciais» no treino. Ligado escreve-a no espaço das "
       "fotografias, para treinar sem essa conversão."),
    IT("Spento lascia la nuvola sparsa in sRGB, di cui si occupa «Converti i "
       "colori dei punti iniziali» nell'addestramento. Attivo la scrive nello "
       "spazio delle fotografie, per addestrare con quella conversione spenta."),
    NL("Uit laat de dunne puntenwolk in sRGB, waar \"Kleuren van startpunten "
       "omzetten\" in de training voor zorgt. Aan schrijft haar in de ruimte van "
       "de foto's, om te trainen met die omzetting uit."),
    RU("Выключено оставляет разреженное облако в sRGB, чем занимается "
       "«Преобразовать цвета начальных точек» при обучении. Включено пишет его в "
       "пространстве фотографий — для обучения с выключенным преобразованием."),
    TR("Kapalı, seyrek nokta bulutunu sRGB'de bırakır; bunu eğitimdeki "
       "\"Başlangıç noktası renklerini dönüştür\" halleder. Açık, onu "
       "fotoğrafların uzayında yazar; o dönüşüm kapalıyken eğitmek için."));

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
    EN("The frames as they are written, then the match map, then the model "
       "being built -- whichever the running step is on. A mask is the one "
       "thing a counter cannot tell you about: \"1400 images masked\" says "
       "nothing about whether the prompt caught what you meant. Masked-out "
       "areas are tinted red, exactly as in Try the mask."),
    JA("書き出されるフレーム、照合マップ、組み上がっていくモデルを、"
       "そのとき動いている工程に合わせて表示します。マスクだけは数字ではわかりません。"
       "「1400 枚にマスクを作成」と出ていても、狙ったものを捉えられたかはわかりません。"
       "隠される部分は「マスクを試す」と同じように赤く染まります。"),
    ZH_HANS("依次显示正在写出的画面、匹配图和正在搭起来的模型 —— 跟着当前运行的步骤走。"
            "蒙版是计数说明不了的：显示“已给 1400 张图做蒙版”，并不告诉你提示词有没有"
            "抓到你想要的东西。被遮住的区域会染成红色，和“试一下蒙版”里一样。"),
    ZH_HANT("依次顯示正在寫出的畫面、比對圖和正在搭起來的模型 —— 跟著目前執行的步驟走。"
            "遮罩是計數說明不了的：顯示「已為 1400 張影像做遮罩」，並不告訴你提示詞有沒有"
            "抓到你想要的東西。被遮住的區域會染成紅色，和「試一下遮罩」裡一樣。"),
    KO("써 나가는 프레임, 정합 지도, 쌓여 가는 모델을 지금 도는 단계에 맞춰 "
       "보여줍니다. 마스크는 숫자로 알 수 없는 하나입니다. \"1400장 마스크 완료\"라고 "
       "해도 프롬프트가 원하던 것을 잡았는지는 알 수 없습니다. 가려지는 부분은 "
       "\"마스크 시험\"과 똑같이 붉게 물듭니다."),
    DE("Die Bilder, während sie geschrieben werden, dann die Zuordnungskarte, "
       "dann das entstehende Modell -- je nachdem, welcher Schritt gerade "
       "läuft. Eine Maske ist das Einzige, worüber ein Zähler nichts sagt: "
       "\"1400 Bilder maskiert\" verrät nicht, ob der Text getroffen hat, was "
       "gemeint war. Ausmaskierte Bereiche sind rot getönt, genau wie in "
       "\"Maske ausprobieren\"."),
    FR("Les images au fur et à mesure, puis la carte d'appariement, puis le "
       "modèle en construction -- selon l'étape en cours. Un masque est la "
       "seule chose qu'un compteur ne dit pas : \"1400 images masquées\" "
       "n'indique pas si la description a attrapé ce que vous vouliez. Les "
       "zones masquées sont teintées en rouge, comme dans \"Essayer le "
       "masque\"."),
    ES("Los fotogramas según se escriben, luego el mapa de emparejado, luego el "
       "modelo que se va armando, según el paso en curso. Una máscara es lo "
       "único que un contador no cuenta: \"1400 imágenes enmascaradas\" no dice "
       "si la descripción atrapó lo que querías. Las zonas tapadas salen "
       "teñidas de rojo, igual que en \"Probar la máscara\"."),
    PT("Os quadros conforme são escritos, depois o mapa de pareamento, depois o "
       "modelo sendo montado -- conforme a etapa em curso. Uma máscara é a "
       "única coisa que um contador não conta: \"1400 imagens mascaradas\" não "
       "diz se a descrição pegou o que você queria. As áreas tapadas ficam "
       "tingidas de vermelho, como em \"Testar a máscara\"."),
    IT("I fotogrammi mentre vengono scritti, poi la mappa dei confronti, poi il "
       "modello che si sta costruendo, secondo il passo in corso. Una maschera "
       "è l'unica cosa che un contatore non dice: \"1400 immagini mascherate\" "
       "non dice se la descrizione ha preso ciò che volevi. Le zone coperte "
       "sono tinte di rosso, come in \"Prova la maschera\"."),
    NL("De beelden terwijl ze worden weggeschreven, dan de koppelkaart, dan het "
       "model dat wordt opgebouwd -- afhankelijk van de lopende stap. Een "
       "masker is het enige waarover een teller niets zegt: \"1400 "
       "afbeeldingen gemaskeerd\" vertelt niet of de omschrijving ving wat je "
       "bedoelde. Weggemaskeerde delen kleuren rood, net als in \"Masker "
       "uitproberen\"."),
    RU("Кадры по мере записи, затем карта сопоставлений, затем собираемая "
       "модель -- смотря какой шаг идёт. Маска -- единственное, о чём счётчик "
       "ничего не говорит: \"замаскировано 1400 изображений\" не сообщает, "
       "поймал ли запрос то, что вы имели в виду. Скрытые области подкрашены "
       "красным, как и в \"Проверить маску\"."),
    TR("Kareler yazılırken, sonra eşleşme haritası, sonra kurulmakta olan model "
       "-- hangi adım çalışıyorsa o. Maske, bir sayacın anlatamayacağı tek "
       "şeydir: \"1400 görüntü maskelendi\" ifadesi, metnin istediğinizi "
       "yakalayıp yakalamadığını söylemez. Maskelenen alanlar, \"Maskeyi "
       "dene\" bölümündeki gibi kırmızıya boyanır."));

// ---------------------------------------------------------------------------
// What the preview panel shows: frames, the match matrix, the model
// ---------------------------------------------------------------------------

SS_MSG(reel_follow,
    EN("Latest"),        JA("最新"),          ZH_HANS("最新"),      ZH_HANT("最新"),
    KO("최신"),           DE("Neueste"),      FR("Dernière"),     ES("La última"),
    PT("A última"),      IT("L'ultima"),     NL("Nieuwste"),     RU("Последний"),
    TR("En yeni"));

SS_MSG(reel_follow_help,
    EN("Keep up with the step as it works. Turn this off, or drag the slider "
       "back, to look at a picture it has already been past -- the run carries "
       "on either way."),
    JA("進行に合わせて最新の一枚を表示し続けます。これを切るか、スライダーを戻すと、"
       "すでに通り過ぎた画像を見られます。どちらでも処理は止まりません。"),
    ZH_HANS("跟着这一步的进度显示最新的一张。关掉它，或者把滑块往回拖，"
            "就能看已经处理过的图像；无论哪种，运行都不会停。"),
    ZH_HANT("跟著這一步的進度顯示最新的一張。關掉它，或者把滑桿往回拖，"
            "就能看已經處理過的影像；無論哪種，執行都不會停。"),
    KO("작업이 진행되는 대로 가장 최근 장면을 보여 줍니다. 이것을 끄거나 슬라이더를 "
       "뒤로 끌면 이미 지나간 이미지를 볼 수 있고, 어느 쪽이든 실행은 계속됩니다."),
    DE("Mit dem Schritt mitgehen und immer das neueste Bild zeigen. Ausschalten "
       "oder den Regler zurückziehen, um ein bereits verarbeitetes Bild "
       "anzusehen -- der Lauf geht so oder so weiter."),
    FR("Suivre l'étape et montrer toujours la dernière image. Décochez, ou "
       "ramenez le curseur en arrière, pour revoir une image déjà traitée : le "
       "traitement continue dans les deux cas."),
    ES("Seguir el paso y mostrar siempre la imagen más reciente. Desactívelo, o "
       "arrastre el control hacia atrás, para ver una imagen ya procesada: la "
       "ejecución sigue igual."),
    PT("Acompanhar a etapa e mostrar sempre a imagem mais recente. Desligue, ou "
       "arraste o controle para trás, para ver uma imagem já processada -- a "
       "execução continua de qualquer forma."),
    IT("Segue il passo e mostra sempre l'immagine più recente. Disattivalo, o "
       "riporta indietro il cursore, per rivedere un'immagine già elaborata: "
       "l'esecuzione prosegue comunque."),
    NL("Loopt mee met de stap en toont steeds het nieuwste beeld. Zet het uit, "
       "of sleep de schuif terug, om een al verwerkt beeld te bekijken -- de "
       "verwerking gaat hoe dan ook door."),
    RU("Показывать самый свежий кадр по ходу шага. Снимите галочку или "
       "перетащите ползунок назад, чтобы посмотреть уже пройденное "
       "изображение -- работа при этом не прерывается."),
    TR("Adım ilerledikçe en yeni görüntüyü gösterir. Kapatın ya da kaydırıcıyı "
       "geri çekin; böylece çoktan geçilmiş bir görüntüye bakabilirsiniz, "
       "çalışma yine de sürer."));

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
    EN("Which images were matched to which, brightest where the most points "
       "survived. A capture shot as a walk gives a bright diagonal; if the walk "
       "came back on itself, the corners light up too. A diagonal with dark "
       "corners is a loop that did not close, which is what splits a "
       "reconstruction in two."),
    JA("どの画像どうしが照合できたかを示します。残った点が多いほど明るくなります。"
       "歩きながら撮ると対角線が明るくなり、元の場所まで戻ってくると四隅も光ります。"
       "対角線だけで四隅が暗いのは、輪が閉じていない状態です。"
       "再構成が二つに割れるのはこれが原因です。"),
    ZH_HANS("显示哪些图像互相匹配上了，留下的点越多越亮。边走边拍会出现明亮的对角线；"
            "如果走回了原处，四角也会亮起来。只有对角线而四角发暗，说明回环没有闭合，"
            "重建裂成两半就是这么来的。"),
    ZH_HANT("顯示哪些影像互相比對上了，留下的點越多越亮。邊走邊拍會出現明亮的對角線；"
            "如果走回了原處，四角也會亮起來。只有對角線而四角發暗，說明回環沒有閉合，"
            "重建裂成兩半就是這麼來的。"),
    KO("어떤 이미지끼리 맞춰졌는지 보여줍니다. 남은 점이 많을수록 밝습니다. "
       "걸으면서 찍으면 밝은 대각선이 생기고, 제자리로 돌아오면 네 귀퉁이도 "
       "밝아집니다. 대각선만 있고 귀퉁이가 어두우면 고리가 닫히지 않은 것이고, "
       "재구성이 둘로 갈라지는 원인이 바로 이것입니다."),
    DE("Welche Bilder einander zugeordnet wurden, am hellsten dort, wo die "
       "meisten Punkte übrig blieben. Eine im Gehen gefilmte Aufnahme ergibt "
       "eine helle Diagonale; kam der Weg auf sich selbst zurück, leuchten auch "
       "die Ecken. Eine Diagonale mit dunklen Ecken ist eine Schleife, die sich "
       "nicht geschlossen hat -- und genau das zerteilt eine Rekonstruktion."),
    FR("Quelles images ont été appariées entre elles, le plus clair là où le "
       "plus de points ont survécu. Une prise faite en marchant donne une "
       "diagonale claire ; si le trajet est revenu sur lui-même, les coins "
       "s'allument aussi. Une diagonale aux coins sombres est une boucle non "
       "fermée, ce qui coupe une reconstruction en deux."),
    ES("Qué imágenes se emparejaron con cuáles; más claro donde sobrevivieron "
       "más puntos. Una toma hecha caminando da una diagonal clara; si el "
       "recorrido volvió sobre sí mismo, también se encienden las esquinas. Una "
       "diagonal con esquinas oscuras es un bucle que no cerró, que es lo que "
       "parte en dos una reconstrucción."),
    PT("Quais imagens foram pareadas com quais, mais claro onde sobraram mais "
       "pontos. Uma captura feita andando dá uma diagonal clara; se o percurso "
       "voltou sobre si mesmo, os cantos também acendem. Uma diagonal com "
       "cantos escuros é um laço que não fechou, e é isso que parte uma "
       "reconstrução em duas."),
    IT("Quali immagini sono state confrontate con quali, più chiaro dove sono "
       "rimasti più punti. Una ripresa fatta camminando dà una diagonale "
       "chiara; se il percorso è tornato su se stesso si accendono anche gli "
       "angoli. Una diagonale con gli angoli scuri è un anello che non si è "
       "chiuso, ed è ciò che spezza in due una ricostruzione."),
    NL("Welke beelden aan welke zijn gekoppeld, het helderst waar de meeste "
       "punten overbleven. Een opname die al lopend is gemaakt geeft een "
       "heldere diagonaal; kwam de route op zichzelf terug, dan lichten de "
       "hoeken ook op. Een diagonaal met donkere hoeken is een lus die niet "
       "sloot, en dat is wat een reconstructie in tweeën breekt."),
    RU("Какие изображения сопоставились с какими; ярче там, где уцелело больше "
       "точек. Съёмка на ходу даёт яркую диагональ; если путь вернулся к "
       "началу, загораются и углы. Диагональ с тёмными углами -- это незамкнутая "
       "петля, и именно она разрывает реконструкцию надвое."),
    TR("Hangi görüntülerin hangileriyle eşleştiği; en çok nokta kalan yerde en "
       "parlak. Yürüyerek yapılan bir çekim parlak bir köşegen verir; yol "
       "kendine döndüyse köşeler de yanar. Köşeleri karanlık bir köşegen, "
       "kapanmamış bir halkadır ve bir yeniden kurmayı ikiye bölen de budur."));

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
    EN("This folder already holds part of a run. Pick a step to throw away and "
       "do again; everything before it is kept. A bad mask prompt costs the "
       "masking pass, not the extraction as well."),
    JA("このフォルダーには前回の途中結果が残っています。やり直す工程を選んでください。"
       "その前の結果はそのまま使います。マスクの指定を間違えても、やり直すのは"
       "マスクだけで、フレームの書き出しからにはなりません。"),
    ZH_HANS("这个文件夹里已经有上次运行的一部分结果。选一步丢掉重做，它之前的都保留。"
            "蒙版提示词写错了，只需重做蒙版，不必连抽帧一起重来。"),
    ZH_HANT("這個資料夾裡已經有上次執行的一部分結果。選一步丟掉重做，它之前的都保留。"
            "遮罩提示詞寫錯了，只需重做遮罩，不必連抽格一起重來。"),
    KO("이 폴더에는 지난 실행의 일부가 남아 있습니다. 버리고 다시 할 단계를 "
       "고르세요. 그 앞의 결과는 그대로 씁니다. 마스크 문구를 잘못 써도 "
       "다시 하는 것은 마스크뿐이고 프레임 추출까지는 아닙니다."),
    DE("In diesem Ordner liegt schon ein Teil eines Laufs. Wählen Sie den "
       "Schritt, der verworfen und neu gemacht wird; alles davor bleibt. Ein "
       "schlechter Masken-Text kostet den Maskendurchgang, nicht auch die "
       "Bildausgabe."),
    FR("Ce dossier contient déjà une partie d'une exécution. Choisissez "
       "l'étape à jeter et à refaire ; tout ce qui précède est conservé. Une "
       "mauvaise description de masque coûte la passe de masquage, pas aussi "
       "l'extraction."),
    ES("Esta carpeta ya guarda parte de una ejecución. Elige el paso que se "
       "tira y se rehace; todo lo anterior se conserva. Una descripción de "
       "máscara equivocada cuesta la pasada de máscaras, no también la "
       "extracción."),
    PT("Esta pasta já guarda parte de uma execução. Escolha a etapa a jogar "
       "fora e refazer; tudo antes dela fica. Uma descrição de máscara errada "
       "custa a passagem de máscaras, não também a extração."),
    IT("Questa cartella contiene già parte di un'esecuzione. Scegli il passo "
       "da buttare e rifare; tutto ciò che viene prima resta. Una descrizione "
       "di maschera sbagliata costa la passata di mascheratura, non anche "
       "l'estrazione."),
    NL("In deze map staat al een deel van een run. Kies de stap die wordt "
       "weggegooid en opnieuw gedaan; alles ervoor blijft staan. Een verkeerde "
       "maskeromschrijving kost de maskeerronde, niet ook het uitpakken."),
    RU("В этой папке уже лежит часть запуска. Выберите шаг, который надо "
       "выбросить и сделать заново; всё до него остаётся. Неудачный запрос для "
       "маски стоит прохода масок, а не ещё и извлечения кадров."),
    TR("Bu klasörde bir çalışmanın bir bölümü zaten duruyor. Atılıp yeniden "
       "yapılacak adımı seçin; ondan öncesi kalır. Kötü bir maske metni yalnız "
       "maskeleme geçişine mal olur, kare çıkarmaya da değil."));

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
    EN("Only part of the capture reconstructed -- it will train, but expect "
       "gaps. More overlap between shots, or a higher quality setting, "
       "usually fixes it."),
    JA("撮影の一部しか再構成できませんでした。学習は行えますが、欠けが出ます。"
       "撮影どうしの重なりを増やすか、品質設定を上げると直ることが多いです。"),
    ZH_HANS("只重建出了拍摄内容的一部分——仍然可以训练，但会有缺口。"
            "增加拍摄之间的重叠，或者提高质量设置，通常就能解决。"),
    ZH_HANT("只重建出了拍攝內容的一部分——仍然可以訓練，但會有缺口。"
            "增加拍攝之間的重疊，或者提高品質設定，通常就能解決。"),
    KO("촬영분의 일부만 재구성되었습니다. 학습은 되지만 빈 곳이 생깁니다. "
       "촬영끼리 더 많이 겹치게 하거나 품질 설정을 올리면 대개 해결됩니다."),
    DE("Nur ein Teil der Aufnahme wurde rekonstruiert -- trainieren lässt es "
       "sich, aber mit Lücken. Mehr Überlappung zwischen den Aufnahmen oder "
       "eine höhere Qualitätsstufe hilft meist."),
    FR("Seule une partie de la prise a été reconstruite : elle s'entraînera, "
       "mais avec des trous. Plus de recouvrement entre les prises, ou un "
       "réglage de qualité plus élevé, corrige généralement le problème."),
    ES("Solo se reconstruyó parte de la captura: se puede entrenar, pero "
       "habrá huecos. Más solapamiento entre tomas, o una calidad más alta, "
       "suele arreglarlo."),
    PT("Só parte da captura foi reconstruída: dá para treinar, mas haverá "
       "falhas. Mais sobreposição entre as tomadas, ou uma qualidade mais "
       "alta, costuma resolver."),
    IT("È stata ricostruita solo una parte della ripresa: si può addestrare, "
       "ma con dei vuoti. Più sovrapposizione tra gli scatti, o una qualità "
       "più alta, di solito risolve."),
    NL("Slechts een deel van de opname is gereconstrueerd -- trainen kan, "
       "maar met gaten. Meer overlap tussen de opnamen, of een hogere "
       "kwaliteitsinstelling, helpt meestal."),
    RU("Восстановилась лишь часть съёмки — обучать можно, но с пробелами. "
       "Обычно помогает большее перекрытие между кадрами или более высокая "
       "настройка качества."),
    TR("Çekimin yalnızca bir bölümü yeniden oluşturuldu -- eğitilebilir ama "
       "boşluklar olacak. Çekimler arasında daha çok örtüşme ya da daha "
       "yüksek bir kalite ayarı genelde sorunu çözer."));

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
    EN("Add another clip to this dataset. Several videos reconstruct together "
       "as one scene: each gets its own folder of frames, and its own camera, "
       "so they may come from different lenses. Click several files in the "
       "dialog to take them all, or drop them onto this window."),
    JA("このデータセットにクリップをもう1本追加します。複数の動画は1つの"
       "シーンとしてまとめて再構成されます。それぞれにフレーム用のフォルダと"
       "カメラが割り当てられるので、レンズが違っていてもかまいません。"
       "ダイアログで複数のファイルを選ぶか、このウィンドウにドロップして"
       "ください。"),
    ZH_HANS("再往这个数据集里加一段视频。多段视频会作为同一个场景一起重建："
            "每段有自己的帧文件夹和自己的相机，所以它们可以来自不同镜头。"
            "在对话框里点选多个文件，或者把它们拖到这个窗口。"),
    ZH_HANT("再往這個資料集裡加一段影片。多段影片會作為同一個場景一起重建："
            "每段有自己的影格資料夾和自己的相機，所以它們可以來自不同鏡頭。"
            "在對話框裡點選多個檔案，或者把它們拖到這個視窗。"),
    KO("이 데이터셋에 클립을 하나 더 추가합니다. 여러 동영상은 하나의 장면으로 "
       "함께 재구성됩니다. 각각 프레임 폴더와 카메라를 따로 가지므로 서로 다른 "
       "렌즈여도 괜찮습니다. 대화 상자에서 여러 파일을 클릭하거나 이 창에 "
       "끌어다 놓으세요."),
    DE("Diesem Datensatz einen weiteren Clip hinzufügen. Mehrere Videos werden "
       "gemeinsam als eine Szene rekonstruiert: jedes bekommt einen eigenen "
       "Bilderordner und eine eigene Kamera, sie dürfen also von "
       "verschiedenen Objektiven stammen. Im Dialog mehrere Dateien anklicken "
       "oder sie in dieses Fenster ziehen."),
    FR("Ajouter une autre séquence à ce jeu de données. Plusieurs vidéos sont "
       "reconstruites ensemble comme une seule scène : chacune a son dossier "
       "d'images et sa propre caméra, elles peuvent donc venir d'objectifs "
       "différents. Cliquez plusieurs fichiers dans la boîte de dialogue, ou "
       "déposez-les sur cette fenêtre."),
    ES("Añadir otro clip a este conjunto de datos. Varios vídeos se "
       "reconstruyen juntos como una sola escena: cada uno tiene su carpeta "
       "de fotogramas y su propia cámara, así que pueden venir de objetivos "
       "distintos. Haga clic en varios archivos en el diálogo, o arrástrelos "
       "a esta ventana."),
    PT("Adicionar outro clipe a este conjunto de dados. Vários vídeos são "
       "reconstruídos juntos como uma única cena: cada um ganha sua pasta de "
       "quadros e sua própria câmera, então podem vir de lentes diferentes. "
       "Clique em vários arquivos na caixa de diálogo, ou arraste-os para "
       "esta janela."),
    IT("Aggiunge un'altra clip a questo set di dati. Più video vengono "
       "ricostruiti insieme come un'unica scena: ciascuno ha la sua cartella "
       "di fotogrammi e la sua fotocamera, quindi possono venire da obiettivi "
       "diversi. Clicchi più file nella finestra di dialogo, oppure li "
       "trascini su questa finestra."),
    NL("Nog een clip aan deze dataset toevoegen. Meerdere video's worden samen "
       "als één scène gereconstrueerd: elke krijgt een eigen map met beelden "
       "en een eigen camera, dus ze mogen van verschillende lenzen komen. "
       "Klik meerdere bestanden aan in het dialoogvenster, of sleep ze naar "
       "dit venster."),
    RU("Добавить в этот набор ещё один ролик. Несколько видео восстанавливаются "
       "вместе как одна сцена: у каждого своя папка кадров и своя камера, так "
       "что объективы могут быть разными. Выберите в диалоге несколько файлов "
       "или перетащите их в это окно."),
    TR("Bu veri kümesine bir klip daha ekleyin. Birden çok video tek bir sahne "
       "olarak birlikte yeniden oluşturulur: her birinin kendi kare klasörü "
       "ve kendi kamerası olur, dolayısıyla farklı objektiflerden gelebilirler. "
       "İletişim kutusunda birkaç dosyaya tıklayın ya da onları bu pencereye "
       "bırakın."));

SS_MSG(add_photos,
    EN("Add photos..."), JA("写真を追加…"),   ZH_HANS("添加照片…"), ZH_HANT("新增相片…"),
    KO("사진 추가…"),     DE("Fotos hinzufügen …"), FR("Ajouter des photos…"),
    ES("Añadir fotos…"), PT("Adicionar fotos…"), IT("Aggiungi fotografie…"),
    NL("Foto's toevoegen…"), RU("Добавить фотографии…"), TR("Fotoğraf ekle…"));

SS_MSG(add_photos_help,
    EN("Add a folder of photos. On its own it is read where it is; alongside "
       "another input its images are linked into the dataset, because the "
       "reconstruction reads one folder tree."),
    JA("写真のフォルダを追加します。単独ならその場所のまま読み込みます。"
       "ほかの入力と一緒の場合は、再構成が1つのフォルダツリーを読むため、"
       "画像はデータセットにリンクされます。"),
    ZH_HANS("添加一个照片文件夹。只有它时就地读取；和其他输入放在一起时，"
            "它的图像会被链接进数据集，因为重建只读取一棵文件夹树。"),
    ZH_HANT("新增一個相片資料夾。只有它時就地讀取；和其他輸入放在一起時，"
            "它的影像會被連結進資料集，因為重建只讀取一棵資料夾樹。"),
    KO("사진 폴더를 추가합니다. 하나뿐이면 있는 자리에서 그대로 읽고, 다른 "
       "입력과 함께라면 재구성이 하나의 폴더 트리만 읽기 때문에 이미지가 "
       "데이터셋으로 링크됩니다."),
    DE("Einen Fotoordner hinzufügen. Allein wird er dort gelesen, wo er liegt; "
       "neben einer anderen Eingabe werden seine Bilder in den Datensatz "
       "verlinkt, weil die Rekonstruktion einen einzigen Ordnerbaum liest."),
    FR("Ajouter un dossier de photos. Seul, il est lu là où il se trouve ; à "
       "côté d'une autre entrée, ses images sont liées dans le jeu de "
       "données, car la reconstruction ne lit qu'une seule arborescence."),
    ES("Añadir una carpeta de fotos. Sola, se lee donde está; junto a otra "
       "entrada, sus imágenes se enlazan al conjunto de datos, porque la "
       "reconstrucción lee un único árbol de carpetas."),
    PT("Adicionar uma pasta de fotos. Sozinha, ela é lida onde está; ao lado "
       "de outra entrada, suas imagens são vinculadas ao conjunto de dados, "
       "porque a reconstrução lê uma única árvore de pastas."),
    IT("Aggiunge una cartella di fotografie. Da sola viene letta dov'è; "
       "insieme a un altro ingresso le sue immagini vengono collegate nel set "
       "di dati, perché la ricostruzione legge un solo albero di cartelle."),
    NL("Een fotomap toevoegen. Alleen wordt die gelezen waar hij staat; naast "
       "een andere invoer worden de beelden in de dataset gelinkt, omdat de "
       "reconstructie één mappenboom leest."),
    RU("Добавить папку с фотографиями. В одиночку она читается там, где лежит; "
       "рядом с другим входом её снимки связываются с набором данных, потому "
       "что реконструкция читает одно дерево папок."),
    TR("Bir fotoğraf klasörü ekleyin. Tek başınaysa bulunduğu yerde okunur; "
       "başka bir girdiyle birlikteyse görüntüleri veri kümesine bağlanır, "
       "çünkü yeniden oluşturma tek bir klasör ağacı okur."));

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
    EN("Masks already made for these images:\n{0}\n\nThey are used as they "
       "are -- nothing is segmented for this input."),
    JA("これらの画像用にすでに用意されているマスクです:\n{0}\n\n"
       "そのまま使われ、この入力に対してセグメンテーションは行いません。"),
    ZH_HANS("已经为这些图像准备好的蒙版：\n{0}\n\n它们会被原样使用——"
            "不会对这个输入再做分割。"),
    ZH_HANT("已經為這些影像準備好的遮罩：\n{0}\n\n它們會被原樣使用——"
            "不會對這個輸入再做分割。"),
    KO("이 이미지들에 이미 만들어진 마스크입니다:\n{0}\n\n그대로 쓰이며, 이 "
       "입력에 대해서는 분할을 하지 않습니다."),
    DE("Für diese Bilder liegen bereits Masken vor:\n{0}\n\nSie werden "
       "unverändert genutzt -- für diese Eingabe wird nichts segmentiert."),
    FR("Des masques existent déjà pour ces images :\n{0}\n\nIls sont utilisés "
       "tels quels : rien n'est segmenté pour cette entrée."),
    ES("Ya hay máscaras hechas para estas imágenes:\n{0}\n\nSe usan tal cual: "
       "no se segmenta nada para esta entrada."),
    PT("Já existem máscaras feitas para estas imagens:\n{0}\n\nElas são usadas "
       "como estão -- nada é segmentado para esta entrada."),
    IT("Per queste immagini esistono già delle maschere:\n{0}\n\nVengono usate "
       "così come sono: per questo ingresso non si segmenta nulla."),
    NL("Er zijn al maskers voor deze beelden:\n{0}\n\nZe worden gebruikt zoals "
       "ze zijn -- voor deze invoer wordt niets gesegmenteerd."),
    RU("Для этих изображений уже есть маски:\n{0}\n\nОни используются как "
       "есть — для этого входа ничего не сегментируется."),
    TR("Bu görüntüler için hazır maskeler var:\n{0}\n\nOlduğu gibi "
       "kullanılırlar -- bu girdi için bir şey bölütlenmez."));

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
    EN("This folder holds a previous (possibly interrupted) run. Checked, the "
       "finished parts are reused -- extracted frames, masks, features and "
       "matches -- and only what is missing runs. Unchecked, a folder with "
       "none of that is required; nothing is ever deleted automatically."),
    JA("このフォルダには前回の（途中で止まったかもしれない）実行が残っています。"
       "チェックすると、切り出したフレーム、マスク、特徴、マッチなど完了済みの"
       "部分を再利用し、足りないところだけを実行します。外した場合は、それらが"
       "何もないフォルダが必要です。自動で削除されるものはありません。"),
    ZH_HANS("这个文件夹里有上一次（可能被中断的）运行。勾选后会复用已完成的部分"
            "——抽出的帧、蒙版、特征和匹配——只跑缺的那些。不勾选则需要一个"
            "不含这些内容的文件夹；任何东西都不会被自动删除。"),
    ZH_HANT("這個資料夾裡有上一次（可能被中斷的）執行。勾選後會重用已完成的部分"
            "——抽出的影格、遮罩、特徵和配對——只跑缺的那些。不勾選則需要一個"
            "不含這些內容的資料夾；任何東西都不會被自動刪除。"),
    KO("이 폴더에는 지난번(중간에 멈췄을 수도 있는) 실행이 남아 있습니다. "
       "체크하면 추출한 프레임, 마스크, 특징점, 매칭 등 끝난 부분을 재사용하고 "
       "빠진 것만 실행합니다. 체크를 해제하면 그런 것이 하나도 없는 폴더가 "
       "필요합니다. 무엇도 자동으로 지워지지 않습니다."),
    DE("In diesem Ordner liegt ein früherer (womöglich abgebrochener) Lauf. "
       "Angehakt werden die fertigen Teile weiterverwendet -- extrahierte "
       "Bilder, Masken, Merkmale und Zuordnungen -- und nur das Fehlende läuft. "
       "Nicht angehakt wird ein Ordner ohne all das verlangt; gelöscht wird "
       "nie etwas von selbst."),
    FR("Ce dossier contient une exécution précédente (peut-être interrompue). "
       "Coché, les parties terminées sont réutilisées -- images extraites, "
       "masques, points caractéristiques et appariements -- et seul ce qui "
       "manque est calculé. Décoché, un dossier vierge est exigé ; rien n'est "
       "jamais supprimé automatiquement."),
    ES("Esta carpeta contiene una ejecución anterior (quizá interrumpida). "
       "Marcado, se reutilizan las partes terminadas -- fotogramas extraídos, "
       "máscaras, características y correspondencias -- y solo se calcula lo "
       "que falta. Sin marcar, se exige una carpeta sin nada de eso; nunca se "
       "borra nada automáticamente."),
    PT("Esta pasta contém uma execução anterior (talvez interrompida). "
       "Marcado, as partes concluídas são reaproveitadas -- quadros "
       "extraídos, máscaras, características e correspondências -- e só o que "
       "falta é calculado. Desmarcado, exige-se uma pasta sem nada disso; "
       "nada é apagado automaticamente."),
    IT("Questa cartella contiene un'esecuzione precedente (forse "
       "interrotta). Selezionato, le parti già finite vengono riusate -- "
       "fotogrammi estratti, maschere, caratteristiche e corrispondenze -- e "
       "si calcola solo ciò che manca. Deselezionato, serve una cartella "
       "priva di tutto questo; nulla viene mai cancellato da solo."),
    NL("Deze map bevat een eerdere (mogelijk afgebroken) run. Aangevinkt "
       "worden de afgeronde delen hergebruikt -- uitgehaalde beelden, "
       "maskers, kenmerken en overeenkomsten -- en draait alleen wat "
       "ontbreekt. Uitgevinkt is een map zonder dat alles vereist; er wordt "
       "nooit iets automatisch verwijderd."),
    RU("В этой папке лежит прошлый (возможно, прерванный) запуск. С флажком "
       "готовые части используются повторно — извлечённые кадры, маски, "
       "особые точки и соответствия — и считается только недостающее. Без "
       "флажка требуется папка, где ничего этого нет; автоматически ничего "
       "никогда не удаляется."),
    TR("Bu klasörde önceki bir (belki yarıda kalmış) çalıştırma var. "
       "İşaretliyken bitmiş parçalar yeniden kullanılır -- çıkarılan kareler, "
       "maskeler, öznitelikler ve eşleşmeler -- ve yalnızca eksik olan "
       "çalışır. İşaretsizken bunların hiçbirini içermeyen bir klasör gerekir; "
       "hiçbir şey kendiliğinden silinmez."));

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
    EN("Working resolution, how many features are found per image, and how "
       "many image pairs are compared. Higher finds more cameras in difficult "
       "scenes and takes longer."),
    JA("作業解像度、1枚あたりに検出する特徴の数、比較する画像ペアの数をまとめて"
       "決めます。上げるほど難しいシーンでもカメラが見つかりますが、時間は"
       "長くなります。"),
    ZH_HANS("工作分辨率、每张图像找多少特征，以及比较多少对图像。调高在困难场景"
            "里能找到更多相机，但更耗时。"),
    ZH_HANT("工作解析度、每張影像找多少特徵，以及比較多少對影像。調高在困難場景"
            "裡能找到更多相機，但更耗時。"),
    KO("작업 해상도, 이미지당 찾는 특징점 수, 비교하는 이미지 쌍의 수를 함께 "
       "정합니다. 높일수록 어려운 장면에서도 카메라를 더 많이 찾지만 시간이 더 "
       "걸립니다."),
    DE("Arbeitsauflösung, wie viele Merkmale je Bild gefunden werden und wie "
       "viele Bildpaare verglichen werden. Höher findet in schwierigen Szenen "
       "mehr Kameras und dauert länger."),
    FR("Résolution de travail, nombre de points caractéristiques par image et "
       "nombre de paires d'images comparées. Plus haut trouve plus de caméras "
       "dans les scènes difficiles et prend plus de temps."),
    ES("Resolución de trabajo, cuántas características se buscan por imagen y "
       "cuántos pares de imágenes se comparan. Más alto encuentra más cámaras "
       "en escenas difíciles y tarda más."),
    PT("Resolução de trabalho, quantas características são encontradas por "
       "imagem e quantos pares de imagens são comparados. Mais alto encontra "
       "mais câmeras em cenas difíceis e demora mais."),
    IT("Risoluzione di lavoro, quante caratteristiche si cercano per immagine "
       "e quante coppie di immagini si confrontano. Più alto trova più "
       "fotocamere nelle scene difficili e richiede più tempo."),
    NL("Werkresolutie, hoeveel kenmerken per beeld worden gevonden en hoeveel "
       "beeldparen worden vergeleken. Hoger vindt meer camera's in lastige "
       "scènes en duurt langer."),
    RU("Рабочее разрешение, сколько особых точек ищется на снимке и сколько "
       "пар снимков сравнивается. Выше — больше найденных камер в сложных "
       "сценах и дольше."),
    TR("Çalışma çözünürlüğü, görüntü başına kaç öznitelik bulunacağı ve kaç "
       "görüntü çiftinin karşılaştırılacağı. Yüksek olan zor sahnelerde daha "
       "çok kamera bulur ve daha uzun sürer."));

SS_MSG(quality_help_colmap,
    EN("Feature count used for matching (4k / 8k / 16k). Higher finds more "
       "cameras in difficult scenes but matching is O(n^2) in feature count."),
    JA("マッチングに使う特徴の数です（4k / 8k / 16k）。増やすほど難しいシーンでも"
       "カメラが見つかりますが、マッチングの計算量は特徴数の2乗です。"),
    ZH_HANS("用于匹配的特征数量（4k / 8k / 16k）。调高在困难场景里能找到更多相机，"
            "但匹配的复杂度是特征数的平方。"),
    ZH_HANT("用於配對的特徵數量（4k / 8k / 16k）。調高在困難場景裡能找到更多相機，"
            "但配對的複雜度是特徵數的平方。"),
    KO("매칭에 쓰는 특징점 수입니다(4k / 8k / 16k). 높이면 어려운 장면에서 "
       "카메라를 더 찾지만 매칭 비용은 특징점 수의 제곱입니다."),
    DE("Zahl der Merkmale für die Zuordnung (4k / 8k / 16k). Höher findet in "
       "schwierigen Szenen mehr Kameras, doch die Zuordnung wächst quadratisch "
       "mit der Merkmalszahl."),
    FR("Nombre de points caractéristiques utilisés pour l'appariement "
       "(4k / 8k / 16k). Plus haut trouve plus de caméras dans les scènes "
       "difficiles, mais l'appariement est en O(n²) du nombre de points."),
    ES("Número de características usadas para el emparejamiento "
       "(4k / 8k / 16k). Más alto encuentra más cámaras en escenas difíciles, "
       "pero el emparejamiento es O(n²) en el número de características."),
    PT("Número de características usadas na correspondência (4k / 8k / 16k). "
       "Mais alto encontra mais câmeras em cenas difíceis, mas a "
       "correspondência é O(n²) no número de características."),
    IT("Numero di caratteristiche usate per la corrispondenza "
       "(4k / 8k / 16k). Più alto trova più fotocamere nelle scene difficili, "
       "ma la corrispondenza è O(n²) nel numero di caratteristiche."),
    NL("Aantal kenmerken voor het matchen (4k / 8k / 16k). Hoger vindt meer "
       "camera's in lastige scènes, maar matchen is O(n²) in het aantal "
       "kenmerken."),
    RU("Число особых точек для сопоставления (4k / 8k / 16k). Больше — больше "
       "найденных камер в сложных сценах, но сопоставление растёт как квадрат "
       "числа точек."),
    TR("Eşleştirmede kullanılan öznitelik sayısı (4k / 8k / 16k). Yüksek olan "
       "zor sahnelerde daha çok kamera bulur ama eşleştirme öznitelik "
       "sayısına göre O(n²)'dir."));

SS_MSG(camera_lens,
    EN("Camera / lens"), JA("カメラ／レンズ"), ZH_HANS("相机／镜头"), ZH_HANT("相機／鏡頭"),
    KO("카메라 / 렌즈"), DE("Kamera / Objektiv"), FR("Caméra / objectif"),
    ES("Cámara / objetivo"), PT("Câmera / lente"), IT("Fotocamera / obiettivo"),
    NL("Camera / lens"), RU("Камера / объектив"), TR("Kamera / objektif"));

SS_MSG(camera_lens_help,
    EN("The lens distortion the reconstruction fits. OpenCV suits nearly "
       "every phone and camera. Pick a fisheye model for a fisheye camera or "
       "a 360 rig -- a fisheye reconstructed as a normal lens comes out "
       "badly, and nothing detects that for you. Pinhole is only for images "
       "that are already undistorted."),
    JA("再構成が当てはめるレンズ歪みモデルです。OpenCV はほぼすべてのスマホ・"
       "カメラに合います。魚眼カメラや360度リグでは魚眼モデルを選んで"
       "ください。魚眼を通常レンズとして再構成すると結果は悪くなり、それを"
       "自動で検出する仕組みはありません。Pinhole は歪み補正済みの画像専用です。"),
    ZH_HANS("重建要拟合的镜头畸变模型。OpenCV 几乎适用于所有手机和相机。"
            "鱼眼相机或 360 相机组请选鱼眼模型——把鱼眼当普通镜头重建结果会很差，"
            "而且没有任何机制会替你发现。Pinhole 只用于已经去畸变的图像。"),
    ZH_HANT("重建要擬合的鏡頭變形模型。OpenCV 幾乎適用於所有手機和相機。"
            "魚眼相機或 360 相機組請選魚眼模型——把魚眼當普通鏡頭重建結果會很差，"
            "而且沒有任何機制會替你發現。Pinhole 只用於已經去變形的影像。"),
    KO("재구성이 맞출 렌즈 왜곡 모델입니다. OpenCV는 거의 모든 휴대폰과 카메라에 "
       "맞습니다. 어안 카메라나 360 리그라면 어안 모델을 고르세요. 어안을 일반 "
       "렌즈로 재구성하면 결과가 나빠지는데, 그걸 대신 알아채 주는 장치는 "
       "없습니다. Pinhole은 이미 왜곡을 보정한 이미지에만 씁니다."),
    DE("Das Verzeichnungsmodell, das die Rekonstruktion anpasst. OpenCV passt "
       "zu fast jedem Telefon und jeder Kamera. Für eine Fischaugenkamera oder "
       "ein 360-Rig ein Fischaugenmodell wählen -- ein als Normalobjektiv "
       "rekonstruiertes Fischauge wird schlecht, und niemand merkt das für "
       "Sie. Pinhole ist nur für bereits entzerrte Bilder."),
    FR("Le modèle de distorsion que la reconstruction ajuste. OpenCV convient "
       "à presque tous les téléphones et appareils. Choisissez un modèle "
       "fisheye pour une caméra fisheye ou un rig 360 : un fisheye "
       "reconstruit comme un objectif normal donne un mauvais résultat, et "
       "rien ne le détecte pour vous. Pinhole ne sert qu'aux images déjà "
       "corrigées."),
    ES("El modelo de distorsión que ajusta la reconstrucción. OpenCV vale "
       "para casi todos los teléfonos y cámaras. Elija un modelo de ojo de "
       "pez para una cámara de ojo de pez o un equipo 360: un ojo de pez "
       "reconstruido como objetivo normal sale mal, y nada lo detecta por "
       "usted. Pinhole solo sirve para imágenes ya corregidas."),
    PT("O modelo de distorção que a reconstrução ajusta. OpenCV serve para "
       "quase todo telefone e câmera. Escolha um modelo olho de peixe para "
       "uma câmera olho de peixe ou um conjunto 360: um olho de peixe "
       "reconstruído como lente normal sai ruim, e nada detecta isso por "
       "você. Pinhole só serve para imagens já corrigidas."),
    IT("Il modello di distorsione che la ricostruzione adatta. OpenCV va bene "
       "per quasi ogni telefono e fotocamera. Per una fotocamera fisheye o un "
       "rig 360 scelga un modello fisheye: un fisheye ricostruito come "
       "obiettivo normale viene male, e nulla se ne accorge al posto suo. "
       "Pinhole serve solo per immagini già corrette."),
    NL("Het vervormingsmodel dat de reconstructie past. OpenCV past bij bijna "
       "elke telefoon en camera. Kies een fisheye-model voor een "
       "fisheye-camera of een 360-rig -- een fisheye die als gewone lens "
       "wordt gereconstrueerd komt er slecht uit, en niets merkt dat voor u "
       "op. Pinhole is alleen voor al ontvormde beelden."),
    RU("Модель искажений объектива, которую подгоняет реконструкция. OpenCV "
       "подходит почти любому телефону и фотоаппарату. Для камеры «рыбий "
       "глаз» или 360-рига выберите модель фишай: фишай, восстановленный как "
       "обычный объектив, выходит плохо, и заметить это за вас некому. "
       "Pinhole — только для уже исправленных изображений."),
    TR("Yeniden oluşturmanın uyduracağı objektif bozulma modeli. OpenCV "
       "neredeyse her telefona ve kameraya uyar. Balıkgözü kamera veya 360 "
       "düzeneği için balıkgözü modeli seçin -- normal objektif gibi yeniden "
       "oluşturulan bir balıkgözü kötü çıkar ve bunu sizin yerinize fark eden "
       "bir şey yoktur. Pinhole yalnızca bozulması giderilmiş görüntüler "
       "içindir."));

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
    EN("Each input's images go into their own folder and get their own "
       "camera, so each can have its own lens model and starting focal "
       "length. A 360 file's two lens tracks share the row -- they are the "
       "same lens twice -- but are still solved as two cameras."),
    JA("入力ごとに画像は専用のフォルダに入り、カメラも別々になります。そのため"
       "レンズモデルと初期焦点距離を入力ごとに設定できます。360度ファイルの"
       "2つのレンズトラックは同じレンズが2つなので1行にまとまりますが、"
       "解かれるときは2台のカメラとして扱われます。"),
    ZH_HANS("每个输入的图像各进各的文件夹，也各有各的相机，所以镜头模型和初始"
            "焦距都可以分别设置。360 文件的两条镜头轨道共用一行——它们是同一"
            "只镜头的两份——但求解时仍算两台相机。"),
    ZH_HANT("每個輸入的影像各進各的資料夾，也各有各的相機，所以鏡頭模型和初始"
            "焦距都可以分別設定。360 檔案的兩條鏡頭軌道共用一列——它們是同一"
            "顆鏡頭的兩份——但求解時仍算兩台相機。"),
    KO("입력마다 이미지가 각자의 폴더에 들어가고 카메라도 따로 생기므로, 렌즈 "
       "모델과 시작 초점거리를 각각 정할 수 있습니다. 360 파일의 두 렌즈 트랙은 "
       "같은 렌즈가 둘이라 한 줄을 함께 쓰지만, 풀 때는 두 대의 카메라로 "
       "다룹니다."),
    DE("Die Bilder jeder Eingabe kommen in einen eigenen Ordner und bekommen "
       "eine eigene Kamera, also darf jede ihr eigenes Objektivmodell und ihre "
       "eigene Startbrennweite haben. Die zwei Objektivspuren einer "
       "360-Datei teilen sich die Zeile -- es ist zweimal dasselbe Objektiv "
       "-- werden aber trotzdem als zwei Kameras gelöst."),
    FR("Les images de chaque entrée vont dans leur propre dossier et "
       "reçoivent leur propre caméra ; chacune peut donc avoir son modèle "
       "d'objectif et sa focale de départ. Les deux pistes d'un fichier 360 "
       "partagent la ligne -- c'est deux fois le même objectif -- mais sont "
       "quand même résolues comme deux caméras."),
    ES("Las imágenes de cada entrada van a su propia carpeta y reciben su "
       "propia cámara, así que cada una puede tener su modelo de objetivo y "
       "su focal inicial. Las dos pistas de un archivo 360 comparten la fila "
       "-- es el mismo objetivo dos veces -- pero se resuelven igualmente "
       "como dos cámaras."),
    PT("As imagens de cada entrada vão para a própria pasta e ganham a "
       "própria câmera, então cada uma pode ter seu modelo de lente e sua "
       "distância focal inicial. As duas trilhas de um arquivo 360 dividem a "
       "linha -- é a mesma lente duas vezes -- mas mesmo assim são resolvidas "
       "como duas câmeras."),
    IT("Le immagini di ogni ingresso vanno in una cartella propria e ricevono "
       "una fotocamera propria, così ciascuna può avere il suo modello di "
       "obiettivo e la sua focale iniziale. Le due tracce di un file 360 "
       "condividono la riga -- è lo stesso obiettivo due volte -- ma vengono "
       "comunque risolte come due fotocamere."),
    NL("De beelden van elke invoer gaan in een eigen map en krijgen een eigen "
       "camera, dus elk mag zijn eigen lensmodel en beginbrandpuntsafstand "
       "hebben. De twee lenssporen van een 360-bestand delen de regel -- het "
       "is tweemaal dezelfde lens -- maar worden toch als twee camera's "
       "opgelost."),
    RU("Снимки каждого входа попадают в свою папку и получают свою камеру, так "
       "что у каждого может быть своя модель объектива и своё начальное "
       "фокусное расстояние. Две дорожки 360-файла делят одну строку — это "
       "один и тот же объектив дважды, — но решаются всё равно как две камеры."),
    TR("Her girdinin görüntüleri kendi klasörüne gider ve kendi kamerasını "
       "alır, dolayısıyla her biri kendi objektif modeline ve başlangıç odak "
       "uzaklığına sahip olabilir. Bir 360 dosyasının iki objektif izi aynı "
       "satırı paylaşır -- aynı objektifin iki kopyasıdır -- ama yine de iki "
       "kamera olarak çözülür."));

SS_MSG(focal_x_width,
    EN("x width"),       JA("×幅"),          ZH_HANS("× 宽度"),   ZH_HANT("× 寬度"),
    KO("× 너비"),         DE("× Breite"),     FR("× largeur"),    ES("× ancho"),
    PT("× largura"),     IT("× larghezza"),  NL("× breedte"),    RU("× ширина"),
    TR("× genişlik"));

SS_MSG(focal_x_width_help,
    EN("Starting focal length for this input, as a fraction of its image "
       "width (fx = fy = factor x width) -- the width is only known once the "
       "frames exist, which is why it is not in pixels here. 0 reads EXIF and "
       "falls back to a guess from the image size. Worth setting for a "
       "fisheye, where a bad guess can stop the reconstruction from starting "
       "at all; an Insta360 X5 is ~0.269, which .insv files are filled in "
       "with."),
    JA("この入力の初期焦点距離を、画像幅に対する割合で指定します"
       "（fx = fy = 係数 × 幅）。幅はフレームができて初めて分かるため、ここでは"
       "ピクセルで指定できません。0 なら EXIF を読み、なければ画像サイズから"
       "推定します。魚眼では推定が外れると再構成がそもそも始まらないことがある"
       "ので、指定する価値があります。Insta360 X5 はおよそ 0.269 で、.insv では"
       "自動で入ります。"),
    ZH_HANS("这个输入的初始焦距，按图像宽度的比例给出（fx = fy = 系数 × 宽度）。"
            "宽度要等帧生成后才知道，所以这里不用像素。填 0 会读 EXIF，读不到"
            "就按图像尺寸估计。鱼眼值得设一下：估错可能让重建根本起不来。"
            "Insta360 X5 约为 0.269，.insv 文件会自动填上。"),
    ZH_HANT("這個輸入的初始焦距，按影像寬度的比例給出（fx = fy = 係數 × 寬度）。"
            "寬度要等影格產生後才知道，所以這裡不用像素。填 0 會讀 EXIF，讀不到"
            "就按影像尺寸估計。魚眼值得設一下：估錯可能讓重建根本起不來。"
            "Insta360 X5 約為 0.269，.insv 檔會自動填上。"),
    KO("이 입력의 시작 초점거리를 이미지 너비에 대한 비율로 지정합니다"
       "(fx = fy = 계수 × 너비). 너비는 프레임이 생겨야 알 수 있어서 여기서는 "
       "픽셀로 지정하지 않습니다. 0이면 EXIF를 읽고, 없으면 이미지 크기로 "
       "추정합니다. 어안에서는 추정이 어긋나면 재구성이 아예 시작되지 않을 수 "
       "있어 설정할 값어치가 있습니다. Insta360 X5는 약 0.269이며 .insv 파일에는 "
       "자동으로 채워집니다."),
    DE("Startbrennweite für diese Eingabe, als Bruchteil ihrer Bildbreite "
       "(fx = fy = Faktor × Breite) -- die Breite steht erst fest, wenn die "
       "Bilder da sind, deshalb hier nicht in Pixeln. 0 liest EXIF und fällt "
       "auf eine Schätzung aus der Bildgröße zurück. Bei einem Fischauge "
       "lohnt es sich, denn eine schlechte Schätzung kann die Rekonstruktion "
       "ganz verhindern; eine Insta360 X5 liegt bei etwa 0,269, womit "
       ".insv-Dateien gefüllt werden."),
    FR("Focale de départ pour cette entrée, en fraction de la largeur d'image "
       "(fx = fy = facteur × largeur) -- la largeur n'est connue qu'une fois "
       "les images extraites, d'où l'absence de pixels ici. 0 lit l'EXIF et "
       "retombe sur une estimation d'après la taille d'image. Utile pour un "
       "fisheye, où une mauvaise estimation peut empêcher la reconstruction "
       "de démarrer ; une Insta360 X5 vaut environ 0,269, valeur inscrite "
       "d'office pour les fichiers .insv."),
    ES("Focal inicial de esta entrada, como fracción del ancho de imagen "
       "(fx = fy = factor × ancho): el ancho no se conoce hasta que existen "
       "los fotogramas, por eso aquí no va en píxeles. 0 lee el EXIF y "
       "recurre a una estimación por el tamaño de imagen. Vale la pena "
       "fijarla en un ojo de pez, donde una mala estimación puede impedir que "
       "la reconstrucción arranque; una Insta360 X5 ronda 0,269, valor que se "
       "rellena solo para archivos .insv."),
    PT("Distância focal inicial desta entrada, como fração da largura da "
       "imagem (fx = fy = fator × largura) -- a largura só é conhecida depois "
       "que os quadros existem, por isso aqui não é em pixels. 0 lê o EXIF e "
       "recorre a uma estimativa pelo tamanho da imagem. Vale definir num "
       "olho de peixe, onde um palpite ruim pode impedir a reconstrução de "
       "começar; uma Insta360 X5 fica em ~0,269, valor preenchido "
       "automaticamente para arquivos .insv."),
    IT("Focale iniziale per questo ingresso, come frazione della larghezza "
       "dell'immagine (fx = fy = fattore × larghezza): la larghezza si conosce "
       "solo quando i fotogrammi esistono, ecco perché qui non è in pixel. 0 "
       "legge l'EXIF e ripiega su una stima dalla dimensione dell'immagine. "
       "Conviene impostarla per un fisheye, dove una stima sbagliata può "
       "impedire del tutto l'avvio della ricostruzione; una Insta360 X5 sta "
       "attorno a 0,269, valore che i file .insv ricevono da soli."),
    NL("Beginbrandpuntsafstand voor deze invoer, als fractie van de "
       "beeldbreedte (fx = fy = factor × breedte) -- de breedte is pas bekend "
       "als de beelden er zijn, vandaar geen pixels hier. 0 leest EXIF en valt "
       "terug op een schatting uit de beeldgrootte. De moeite waard bij een "
       "fisheye, waar een slechte schatting de reconstructie helemaal kan "
       "blokkeren; een Insta360 X5 zit rond 0,269, waarmee .insv-bestanden "
       "worden ingevuld."),
    RU("Начальное фокусное расстояние для этого входа — как доля ширины "
       "изображения (fx = fy = коэффициент × ширина). Ширина известна только "
       "после появления кадров, поэтому здесь не пиксели. 0 читает EXIF, а при "
       "его отсутствии оценивает по размеру изображения. Для фишая задать "
       "стоит: плохая догадка может вовсе не дать реконструкции начаться; у "
       "Insta360 X5 это примерно 0,269, и для .insv значение подставляется "
       "само."),
    TR("Bu girdi için başlangıç odak uzaklığı, görüntü genişliğinin bir kesri "
       "olarak (fx = fy = katsayı × genişlik) -- genişlik ancak kareler "
       "oluştuğunda bilindiğinden burada piksel cinsinden verilmez. 0, EXIF'i "
       "okur ve bulamazsa görüntü boyutundan tahmin eder. Balıkgözünde "
       "ayarlamaya değer: kötü bir tahmin yeniden oluşturmanın hiç "
       "başlamamasına yol açabilir. Insta360 X5 için yaklaşık 0,269'dur ve "
       ".insv dosyalarına kendiliğinden yazılır."));

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
    EN("How lens parameters are shared. \"Shared\" when everything was shot "
       "with one camera at one zoom. \"Per folder\" for a multi-camera rig "
       "organized one subfolder per camera -- a multi-track 360 video "
       "switches to this on its own. \"Per image\" when zoom or focus changed "
       "between shots."),
    JA("レンズパラメータをどう共有するかです。すべて同じカメラ・同じズームで"
       "撮ったなら「共有カメラ1台」。カメラごとにサブフォルダを分けたマルチ"
       "カメラ装置なら「フォルダごと」で、複数トラックの360度動画では自動的に"
       "これになります。ショットごとにズームやピントが変わったなら「画像ごと」。"),
    ZH_HANS("镜头参数如何共享。全部用同一台相机、同一焦段拍的选“共用一台相机”。"
            "按相机分子文件夹的多相机装置选“每个文件夹一台”——多轨 360 视频会"
            "自动切到这一项。若各次拍摄之间变过焦距或对焦，选“每张图像一台”。"),
    ZH_HANT("鏡頭參數如何共用。全部用同一台相機、同一焦段拍的選「共用一台相機」。"
            "按相機分子資料夾的多相機裝置選「每個資料夾一台」——多軌 360 影片會"
            "自動切到這一項。若各次拍攝之間變過焦距或對焦，選「每張影像一台」。"),
    KO("렌즈 파라미터를 어떻게 공유할지입니다. 전부 같은 카메라·같은 줌으로 "
       "찍었다면 '공유 카메라 하나'. 카메라마다 하위 폴더를 나눈 다중 카메라 "
       "장치라면 '폴더마다 하나'이고, 다중 트랙 360 동영상은 알아서 이쪽으로 "
       "바뀝니다. 촬영 사이에 줌이나 초점이 바뀌었다면 '이미지마다 하나'."),
    DE("Wie Objektivparameter geteilt werden. „Eine gemeinsame Kamera“, wenn "
       "alles mit einer Kamera bei einer Brennweite aufgenommen wurde. „Je "
       "Ordner“ für ein Mehrkamera-Rig mit einem Unterordner pro Kamera -- "
       "ein mehrspuriges 360-Video schaltet von selbst darauf um. „Je Bild“, "
       "wenn Zoom oder Fokus zwischen den Aufnahmen wechselten."),
    FR("Comment les paramètres d'objectif sont partagés. « Une caméra "
       "partagée » si tout a été pris avec un seul appareil à une seule "
       "focale. « Par dossier » pour un rig multi-caméras avec un sous-dossier "
       "par caméra -- une vidéo 360 multipiste bascule là-dessus toute seule. "
       "« Par image » si le zoom ou la mise au point a changé entre les "
       "prises."),
    ES("Cómo se comparten los parámetros de objetivo. «Una cámara compartida» "
       "si todo se tomó con una cámara a un mismo zoom. «Por carpeta» para un "
       "equipo multicámara con una subcarpeta por cámara: un vídeo 360 "
       "multipista cambia solo a esta opción. «Por imagen» si el zoom o el "
       "enfoque cambiaron entre tomas."),
    PT("Como os parâmetros de lente são compartilhados. “Uma câmera "
       "compartilhada” se tudo foi feito com uma câmera num mesmo zoom. “Por "
       "pasta” para um conjunto multicâmera com uma subpasta por câmera -- um "
       "vídeo 360 multipista muda sozinho para isso. “Por imagem” se o zoom "
       "ou o foco mudou entre as tomadas."),
    IT("Come si condividono i parametri dell'obiettivo. «Una fotocamera "
       "condivisa» se è stato ripreso tutto con una fotocamera a uno stesso "
       "zoom. «Per cartella» per un rig multi-fotocamera con una sottocartella "
       "per fotocamera: un video 360 multitraccia passa da solo a questa "
       "voce. «Per immagine» se zoom o messa a fuoco sono cambiati tra gli "
       "scatti."),
    NL("Hoe lensparameters worden gedeeld. ‘Eén gedeelde camera’ als alles met "
       "één camera bij één zoomstand is opgenomen. ‘Per map’ voor een "
       "meercamera-opstelling met een submap per camera -- een 360-video met "
       "meerdere sporen schakelt hier vanzelf op over. ‘Per beeld’ als zoom of "
       "scherpstelling tussen opnamen veranderde."),
    RU("Как разделяются параметры объектива. «Одна общая камера» — если всё "
       "снято одним аппаратом на одном зуме. «По папке» — для многокамерной "
       "установки, где каждой камере отведена подпапка; многодорожечное "
       "360-видео переключается на это само. «По снимку» — если между кадрами "
       "менялись зум или фокус."),
    TR("Objektif parametrelerinin nasıl paylaşılacağı. Her şey tek kamerayla "
       "ve tek yakınlaştırmayla çekildiyse “tek ortak kamera”. Kamera başına "
       "bir alt klasöre ayrılmış çoklu kamera düzeneği için “klasör başına” -- "
       "çok izli bir 360 video kendiliğinden buna geçer. Çekimler arasında "
       "yakınlaştırma veya odak değiştiyse “görüntü başına”."));

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
    EN("Which pairs of images are compared. Automatic is right almost always: "
       "neighbouring frames for short video, every pair below 100 images, GPU "
       "pre-selection above that."),
    JA("どの画像の組み合わせを比較するかです。ほとんどの場合「自動」で正しく、"
       "短い動画なら隣接フレーム、100 枚未満ならすべての組み合わせ、それ以上"
       "なら GPU による事前選択が使われます。"),
    ZH_HANS("比较哪些图像配对。“自动”几乎总是对的：短视频用相邻帧，不足 100 张时"
            "比较所有配对，超过则用 GPU 预筛选。"),
    ZH_HANT("比較哪些影像配對。「自動」幾乎總是對的：短影片用相鄰影格，不足 100 張時"
            "比較所有配對，超過則用 GPU 預篩選。"),
    KO("어떤 이미지 쌍을 비교할지입니다. 거의 언제나 '자동'이 맞습니다. 짧은 "
       "동영상은 이웃 프레임, 100장 미만이면 모든 쌍, 그보다 많으면 GPU 사전 "
       "선별을 씁니다."),
    DE("Welche Bildpaare verglichen werden. Automatisch ist fast immer "
       "richtig: benachbarte Bilder bei kurzem Video, jedes Paar unter 100 "
       "Bildern, darüber GPU-Vorauswahl."),
    FR("Quelles paires d'images sont comparées. Automatique a presque "
       "toujours raison : images voisines pour une vidéo courte, toutes les "
       "paires en dessous de 100 images, présélection GPU au-delà."),
    ES("Qué pares de imágenes se comparan. Automático acierta casi siempre: "
       "fotogramas vecinos en vídeo corto, todos los pares por debajo de 100 "
       "imágenes y preselección en GPU por encima."),
    PT("Quais pares de imagens são comparados. Automático acerta quase "
       "sempre: quadros vizinhos em vídeo curto, todos os pares abaixo de 100 "
       "imagens e pré-seleção na GPU acima disso."),
    IT("Quali coppie di immagini vengono confrontate. Automatico è quasi "
       "sempre giusto: fotogrammi vicini per un video breve, tutte le coppie "
       "sotto le 100 immagini, preselezione su GPU oltre."),
    NL("Welke beeldparen worden vergeleken. Automatisch klopt bijna altijd: "
       "naburige beelden bij korte video, elk paar onder de 100 beelden, "
       "GPU-voorselectie daarboven."),
    RU("Какие пары снимков сравниваются. «Автоматически» почти всегда верно: "
       "соседние кадры для короткого видео, все пары при менее чем 100 "
       "снимках и предварительный отбор на GPU сверх того."),
    TR("Hangi görüntü çiftlerinin karşılaştırılacağı. Otomatik neredeyse her "
       "zaman doğrudur: kısa videoda komşu kareler, 100 görüntünün altında "
       "her çift, üstünde GPU ön seçimi."));

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
    EN("How image pairs are matched. Exhaustive tries every pair (best "
       "quality, fine up to a few hundred images). Sequential matches temporal "
       "neighbors (video). Vocabulary tree scales to thousands of unordered "
       "photos."),
    JA("画像ペアのマッチング方法です。総当たりはすべての組み合わせを試します"
       "（品質は最良で、数百枚までなら問題ありません）。逐次は時間的に隣り合う"
       "フレームを照合します（動画向け）。ボキャブラリツリーは順不同の数千枚"
       "規模まで対応できます。"),
    ZH_HANS("图像配对的匹配方式。穷举会试遍所有配对（质量最好，几百张以内没问题）。"
            "顺序匹配时间上相邻的帧（视频用）。词汇树可以扩展到数千张无序照片。"),
    ZH_HANT("影像配對的比對方式。窮舉會試遍所有配對（品質最好，幾百張以內沒問題）。"
            "循序比對時間上相鄰的影格（影片用）。詞彙樹可以擴展到數千張無序相片。"),
    KO("이미지 쌍을 어떻게 매칭할지입니다. 전수는 모든 쌍을 시도합니다(품질이 "
       "가장 좋고 수백 장까지는 괜찮습니다). 순차는 시간적으로 이웃한 프레임을 "
       "맞춥니다(동영상용). 어휘 트리는 순서 없는 수천 장까지 감당합니다."),
    DE("Wie Bildpaare zugeordnet werden. Vollständig probiert jedes Paar "
       "(beste Qualität, bis zu einigen hundert Bildern unproblematisch). "
       "Sequenziell ordnet zeitliche Nachbarn zu (Video). Der Vokabularbaum "
       "skaliert auf Tausende ungeordneter Fotos."),
    FR("Comment les paires d'images sont appariées. Exhaustif essaie chaque "
       "paire (meilleure qualité, sans problème jusqu'à quelques centaines "
       "d'images). Séquentiel apparie les voisins temporels (vidéo). L'arbre "
       "de vocabulaire monte à des milliers de photos sans ordre."),
    ES("Cómo se emparejan los pares de imágenes. Exhaustivo prueba todos los "
       "pares (mejor calidad, sin problema hasta unos cientos de imágenes). "
       "Secuencial empareja vecinos temporales (vídeo). El árbol de "
       "vocabulario escala a miles de fotos sin orden."),
    PT("Como os pares de imagens são correspondidos. Exaustivo tenta todos os "
       "pares (melhor qualidade, tranquilo até algumas centenas de imagens). "
       "Sequencial casa vizinhos temporais (vídeo). A árvore de vocabulário "
       "escala para milhares de fotos sem ordem."),
    IT("Come vengono messe in corrispondenza le coppie di immagini. Esaustivo "
       "prova ogni coppia (qualità migliore, tranquillo fino a qualche "
       "centinaio di immagini). Sequenziale abbina i vicini temporali "
       "(video). L'albero di vocabolario arriva a migliaia di foto senza "
       "ordine."),
    NL("Hoe beeldparen worden gematcht. Uitputtend probeert elk paar (beste "
       "kwaliteit, tot enkele honderden beelden prima). Sequentieel matcht "
       "tijdelijke buren (video). De vocabulaireboom schaalt naar duizenden "
       "ongeordende foto's."),
    RU("Как сопоставляются пары снимков. Полный перебор пробует все пары "
       "(лучшее качество, до нескольких сотен снимков вполне нормально). "
       "Последовательное сопоставляет соседей по времени (видео). Словарное "
       "дерево тянет тысячи неупорядоченных фотографий."),
    TR("Görüntü çiftlerinin nasıl eşleştirileceği. Kapsamlı her çifti dener "
       "(en iyi kalite, birkaç yüz görüntüye kadar sorunsuz). Sıralı zamansal "
       "komşuları eşleştirir (video). Sözcük ağacı sırasız binlerce fotoğrafa "
       "ölçeklenir."));

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
    EN("Retrieve visually similar non-neighbour frames through the vocabulary "
       "tree and match them, closing loops when the camera revisits a spot. "
       "SIFT features only."),
    JA("ボキャブラリツリーで見た目の似た非隣接フレームを探して照合し、カメラが"
       "同じ場所に戻ったときにループを閉じます。SIFT 特徴でのみ使えます。"),
    ZH_HANS("通过词汇树找出视觉上相似但不相邻的帧并加以匹配，在相机回到同一位置时"
            "闭合回环。仅适用于 SIFT 特征。"),
    ZH_HANT("透過詞彙樹找出視覺上相似但不相鄰的影格並加以比對，在相機回到同一位置時"
            "閉合迴環。僅適用於 SIFT 特徵。"),
    KO("어휘 트리로 시각적으로 비슷한 비이웃 프레임을 찾아 매칭해, 카메라가 같은 "
       "자리로 돌아왔을 때 루프를 닫습니다. SIFT 특징점에서만 됩니다."),
    DE("Über den Vokabularbaum optisch ähnliche, nicht benachbarte Bilder "
       "finden und zuordnen, damit sich Schleifen schließen, wenn die Kamera "
       "an einen Ort zurückkehrt. Nur mit SIFT-Merkmalen."),
    FR("Retrouver via l'arbre de vocabulaire des images non voisines mais "
       "visuellement proches et les apparier, ce qui ferme la boucle quand la "
       "caméra repasse au même endroit. Uniquement avec les points SIFT."),
    ES("Recuperar mediante el árbol de vocabulario fotogramas no vecinos pero "
       "visualmente parecidos y emparejarlos, cerrando el bucle cuando la "
       "cámara vuelve a un mismo punto. Solo con características SIFT."),
    PT("Recuperar pela árvore de vocabulário quadros não vizinhos mas "
       "visualmente parecidos e casá-los, fechando o laço quando a câmera "
       "volta ao mesmo ponto. Só com características SIFT."),
    IT("Recuperare tramite l'albero di vocabolario fotogrammi non vicini ma "
       "visivamente simili e abbinarli, chiudendo il giro quando la "
       "fotocamera ripassa da uno stesso punto. Solo con caratteristiche "
       "SIFT."),
    NL("Via de vocabulaireboom visueel gelijkende niet-naburige beelden "
       "opzoeken en matchen, zodat de lus sluit als de camera een plek weer "
       "aandoet. Alleen met SIFT-kenmerken."),
    RU("Находить через словарное дерево внешне похожие несоседние кадры и "
       "сопоставлять их, замыкая петлю, когда камера возвращается на то же "
       "место. Только с признаками SIFT."),
    TR("Sözcük ağacı üzerinden görsel olarak benzeyen komşu olmayan kareleri "
       "bulup eşleştirir; böylece kamera aynı noktaya döndüğünde çevrim "
       "kapanır. Yalnızca SIFT öznitelikleriyle."));

SS_MSG(loop_closure_help_builtin,
    EN("Sequential matching only pairs each frame with its temporal "
       "neighbours, so a capture that walks around a subject and returns has "
       "nothing joining the two ends -- one weak step then splits the "
       "reconstruction into pieces. This also matches frames that look alike "
       "wherever they fall in the video, which closes the loop. Costs a "
       "pair-selection pass and roughly twice the matching time; enabled by "
       "default. Under \"Automatic\" it only applies below 100 frames -- "
       "above that, matching is content-based already."),
    JA("逐次マッチングは各フレームを時間的な隣とだけ組にするため、被写体の"
       "まわりを一周して戻ってくる撮影では両端をつなぐものがありません。"
       "弱いつなぎ目が1か所あるだけで再構成はばらばらに割れてしまいます。"
       "これを有効にすると、動画のどこにあっても見た目の似たフレームどうしを"
       "照合するので、ループが閉じます。ペア選択のパスが1回増え、マッチング"
       "時間はおよそ2倍になります。既定で有効です。「自動」では 100 フレーム"
       "未満のときだけ適用されます。それ以上ではマッチングがすでに内容ベース"
       "だからです。"),
    ZH_HANS("顺序匹配只把每一帧和它时间上的邻居配对，所以绕着被摄物走一圈再回来的"
            "拍摄，两端之间没有任何连接——只要有一处连接较弱，重建就会碎成几块。"
            "开启后还会匹配视频中任何位置上看起来相似的帧，从而闭合回环。代价是"
            "多一遍配对筛选和大约两倍的匹配时间；默认开启。在“自动”下只在不足"
            "100 帧时生效——超过之后匹配本来就是基于内容的。"),
    ZH_HANT("循序比對只把每一影格和它時間上的鄰居配對，所以繞著被攝物走一圈再回來的"
            "拍攝，兩端之間沒有任何連接——只要有一處連接較弱，重建就會碎成幾塊。"
            "開啟後還會比對影片中任何位置上看起來相似的影格，從而閉合迴環。代價是"
            "多一遍配對篩選和大約兩倍的比對時間；預設開啟。在「自動」下只在不足"
            "100 影格時生效——超過之後比對本來就是基於內容的。"),
    KO("순차 매칭은 각 프레임을 시간적으로 이웃한 것과만 짝지으므로, 피사체 "
       "주위를 한 바퀴 돌아 돌아오는 촬영에서는 양 끝을 잇는 것이 없습니다. "
       "약한 연결이 한 군데만 있어도 재구성이 조각으로 갈라집니다. 이 옵션은 "
       "동영상 어디에 있든 비슷해 보이는 프레임끼리도 매칭해 루프를 닫습니다. "
       "쌍 선별 패스 한 번과 대략 두 배의 매칭 시간이 들며 기본으로 켜져 "
       "있습니다. '자동'에서는 100프레임 미만일 때만 적용됩니다. 그보다 많으면 "
       "매칭이 이미 내용 기반입니다."),
    DE("Sequenzielle Zuordnung paart jedes Bild nur mit seinen zeitlichen "
       "Nachbarn; bei einer Aufnahme, die um ein Motiv herumgeht und "
       "zurückkommt, verbindet also nichts die beiden Enden -- eine einzige "
       "schwache Stelle zerlegt die Rekonstruktion dann in Teile. Dies ordnet "
       "zusätzlich ähnlich aussehende Bilder zu, egal wo sie im Video liegen, "
       "und schließt so die Schleife. Kostet einen Durchgang zur Paarauswahl "
       "und etwa die doppelte Zuordnungszeit; standardmäßig an. Unter "
       "„Automatisch“ greift es nur unter 100 Bildern -- darüber ist die "
       "Zuordnung ohnehin inhaltsbasiert."),
    FR("L'appariement séquentiel ne relie chaque image qu'à ses voisines dans "
       "le temps ; une prise qui fait le tour d'un sujet et revient n'a donc "
       "rien qui joigne les deux extrémités -- un seul maillon faible et la "
       "reconstruction se scinde. Ceci apparie en plus les images qui se "
       "ressemblent où qu'elles soient dans la vidéo, ce qui ferme la boucle. "
       "Coûte une passe de sélection de paires et environ le double du temps "
       "d'appariement ; activé par défaut. En « Automatique », ne s'applique "
       "qu'en dessous de 100 images : au-delà, l'appariement est déjà fondé "
       "sur le contenu."),
    ES("El emparejamiento secuencial solo une cada fotograma con sus vecinos "
       "temporales, así que una captura que rodea un sujeto y vuelve no tiene "
       "nada que una los dos extremos: un solo eslabón débil parte la "
       "reconstrucción en trozos. Esto empareja además fotogramas parecidos "
       "estén donde estén en el vídeo, cerrando el bucle. Cuesta una pasada "
       "de selección de pares y aproximadamente el doble de tiempo de "
       "emparejamiento; activado por defecto. En «Automático» solo se aplica "
       "por debajo de 100 fotogramas: por encima, el emparejamiento ya es por "
       "contenido."),
    PT("A correspondência sequencial só liga cada quadro aos seus vizinhos no "
       "tempo, então uma captura que dá a volta num objeto e retorna não tem "
       "nada unindo as duas pontas -- um único elo fraco parte a reconstrução "
       "em pedaços. Isto casa também quadros parecidos onde quer que estejam "
       "no vídeo, fechando o laço. Custa uma passagem de seleção de pares e "
       "cerca do dobro do tempo de correspondência; ligado por padrão. Em "
       "“Automático” só vale abaixo de 100 quadros: acima disso, a "
       "correspondência já é por conteúdo."),
    IT("La corrispondenza sequenziale accoppia ogni fotogramma solo con i "
       "vicini nel tempo, quindi una ripresa che gira attorno a un soggetto e "
       "torna indietro non ha nulla che unisca i due capi: basta un anello "
       "debole e la ricostruzione si spezza. Questo abbina anche fotogrammi "
       "simili ovunque cadano nel video, chiudendo il giro. Costa una passata "
       "di selezione delle coppie e circa il doppio del tempo di "
       "corrispondenza; attivo di default. In «Automatico» vale solo sotto i "
       "100 fotogrammi: oltre, la corrispondenza è già basata sul contenuto."),
    NL("Sequentieel matchen koppelt elk beeld alleen aan zijn buren in de "
       "tijd, dus bij een opname die om een onderwerp heen loopt en terugkomt "
       "verbindt niets de twee uiteinden -- één zwakke schakel en de "
       "reconstructie valt uiteen. Dit matcht daarnaast beelden die op elkaar "
       "lijken, waar ze ook in de video vallen, en sluit zo de lus. Kost een "
       "extra selectieronde en ongeveer het dubbele van de matchtijd; staat "
       "standaard aan. Onder ‘Automatisch’ geldt het alleen onder 100 "
       "beelden -- daarboven is het matchen al inhoudsgebaseerd."),
    RU("Последовательное сопоставление связывает каждый кадр только с "
       "соседями по времени, поэтому у съёмки, которая обходит объект и "
       "возвращается, два конца ничем не соединены — достаточно одного "
       "слабого звена, и реконструкция распадается на куски. Здесь вдобавок "
       "сопоставляются похожие кадры, где бы они ни оказались в видео, и петля "
       "замыкается. Стоит одного прохода отбора пар и примерно вдвое большего "
       "времени сопоставления; включено по умолчанию. При «Автоматически» "
       "применяется только до 100 кадров — выше сопоставление и так идёт по "
       "содержимому."),
    TR("Sıralı eşleştirme her kareyi yalnızca zamandaki komşularıyla "
       "eşleştirir; bir öznenin çevresini dolaşıp geri dönen bir çekimde iki "
       "ucu birleştiren hiçbir şey olmaz -- tek bir zayıf halka bile yeniden "
       "oluşturmayı parçalara böler. Bu seçenek ayrıca videoda nerede olursa "
       "olsun birbirine benzeyen kareleri de eşleştirerek çevrimi kapatır. Bir "
       "çift seçme geçişi ve kabaca iki katı eşleştirme süresi gerektirir; "
       "varsayılan olarak açıktır. “Otomatik”te yalnızca 100 karenin altında "
       "geçerlidir -- üstünde eşleştirme zaten içerik temellidir."));

SS_MSG(frames_per_second,
    EN("Frames per second"),
    JA("1秒あたりのフレーム数"),
    ZH_HANS("每秒帧数"),
    ZH_HANT("每秒影格數"),
    KO("초당 프레임 수"),
    DE("Bilder pro Sekunde"),
    FR("Images par seconde"),
    ES("Fotogramas por segundo"),
    PT("Quadros por segundo"),
    IT("Fotogrammi al secondo"),
    NL("Beelden per seconde"),
    RU("Кадров в секунду"),
    TR("Saniyedeki kare"));

SS_MSG(frames_per_second_help,
    EN("How many frames to keep per second of video. 1-3 is right for a slow "
       "walkthrough; more only helps if the camera moved fast. Applies to "
       "every video in the list."),
    JA("動画1秒あたり何フレーム残すかです。ゆっくり歩いて撮ったなら 1〜3 が"
       "適切で、それ以上が効くのはカメラが速く動いたときだけです。リスト内の"
       "すべての動画に適用されます。"),
    ZH_HANS("每秒视频保留多少帧。慢慢走着拍的话 1-3 就合适；更高只有在相机移动"
            "很快时才有用。对列表中的所有视频都生效。"),
    ZH_HANT("每秒影片保留多少影格。慢慢走著拍的話 1-3 就合適；更高只有在相機移動"
            "很快時才有用。對清單中的所有影片都生效。"),
    KO("동영상 1초당 몇 프레임을 남길지입니다. 천천히 걸으며 찍었다면 1~3이 "
       "알맞고, 그보다 높이는 건 카메라가 빠르게 움직였을 때만 도움이 됩니다. "
       "목록의 모든 동영상에 적용됩니다."),
    DE("Wie viele Bilder je Sekunde Video behalten werden. 1-3 passt für "
       "einen langsamen Rundgang; mehr hilft nur, wenn die Kamera schnell "
       "bewegt wurde. Gilt für jedes Video in der Liste."),
    FR("Combien d'images conserver par seconde de vidéo. 1 à 3 convient à une "
       "déambulation lente ; davantage n'aide que si la caméra bougeait vite. "
       "S'applique à toutes les vidéos de la liste."),
    ES("Cuántos fotogramas conservar por segundo de vídeo. De 1 a 3 va bien "
       "para un recorrido lento; más solo ayuda si la cámara se movía rápido. "
       "Se aplica a todos los vídeos de la lista."),
    PT("Quantos quadros manter por segundo de vídeo. De 1 a 3 serve para um "
       "percurso lento; mais só ajuda se a câmera se moveu rápido. Vale para "
       "todos os vídeos da lista."),
    IT("Quanti fotogrammi tenere per ogni secondo di video. Da 1 a 3 va bene "
       "per una camminata lenta; di più serve solo se la fotocamera si "
       "muoveva in fretta. Vale per tutti i video dell'elenco."),
    NL("Hoeveel beelden per seconde video bewaard blijven. 1-3 past bij een "
       "rustige rondgang; meer helpt alleen als de camera snel bewoog. Geldt "
       "voor elke video in de lijst."),
    RU("Сколько кадров оставлять на секунду видео. 1-3 подходит для "
       "неторопливого обхода; больше помогает, только если камера двигалась "
       "быстро. Применяется ко всем видео в списке."),
    TR("Videonun her saniyesinden kaç karenin tutulacağı. Yavaş bir gezinti "
       "için 1-3 uygundur; daha fazlası yalnızca kamera hızlı hareket ettiyse "
       "işe yarar. Listedeki bütün videolara uygulanır."));

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
    EN("Describe what should not be part of the scene -- people walking "
       "through, parked cars, your own shadow -- and it is cut out of both "
       "the camera solve and the training. This is the single biggest quality "
       "win on a capture with anything moving in it. Inputs that arrived with "
       "masks of their own keep them; this is for the rest."),
    JA("シーンに含めたくないもの、たとえば通りがかりの人、駐車中の車、自分の影"
       "などを書いてください。カメラの推定と学習の両方から取り除かれます。"
       "動くものが写っている撮影では、これがいちばん効く品質改善です。"
       "すでにマスクが付いている入力はそのまま使われ、これはそれ以外に"
       "適用されます。"),
    ZH_HANS("写出不该属于场景的东西——路过的行人、停着的车、你自己的影子——"
            "它们会同时从相机求解和训练中被剔除。对于画面里有任何移动物体的"
            "拍摄，这是提升质量最有效的一招。自带蒙版的输入会保留自己的蒙版，"
            "这里针对的是其余的。"),
    ZH_HANT("寫出不該屬於場景的東西——路過的行人、停著的車、你自己的影子——"
            "它們會同時從相機求解和訓練中被剔除。對於畫面裡有任何移動物體的"
            "拍攝，這是提升品質最有效的一招。自帶遮罩的輸入會保留自己的遮罩，"
            "這裡針對的是其餘的。"),
    KO("장면에 들어가면 안 되는 것을 적으세요. 지나가는 사람, 주차된 차, 자기 "
       "그림자 같은 것들이며, 카메라 계산과 학습 양쪽에서 잘려 나갑니다. 움직이는 "
       "것이 있는 촬영에서는 이것이 품질을 가장 크게 끌어올립니다. 이미 마스크가 "
       "딸려 온 입력은 그것을 그대로 쓰고, 이 설정은 나머지에 적용됩니다."),
    DE("Beschreiben Sie, was nicht zur Szene gehören soll -- durchlaufende "
       "Personen, parkende Autos, der eigene Schatten -- und es fällt sowohl "
       "aus der Kameraberechnung als auch aus dem Training heraus. Bei einer "
       "Aufnahme mit irgendetwas Bewegtem ist das der größte einzelne "
       "Qualitätsgewinn. Eingaben, die eigene Masken mitbrachten, behalten "
       "sie; dies gilt dem Rest."),
    FR("Décrivez ce qui ne doit pas faire partie de la scène -- passants, "
       "voitures garées, votre propre ombre -- et cela est retiré à la fois du "
       "calcul des caméras et de l'entraînement. Sur une prise où quelque "
       "chose bouge, c'est le plus gros gain de qualité possible. Les entrées "
       "arrivées avec leurs propres masques les gardent ; ceci vaut pour les "
       "autres."),
    ES("Describa lo que no debe formar parte de la escena -- gente que pasa, "
       "coches aparcados, su propia sombra -- y se elimina tanto del cálculo "
       "de cámaras como del entrenamiento. En una captura con algo en "
       "movimiento, es la mayor mejora de calidad posible. Las entradas que "
       "llegaron con sus propias máscaras las conservan; esto vale para el "
       "resto."),
    PT("Descreva o que não deve fazer parte da cena -- pessoas passando, "
       "carros estacionados, sua própria sombra -- e isso é retirado tanto do "
       "cálculo das câmeras quanto do treinamento. Numa captura com qualquer "
       "coisa em movimento, é o maior ganho de qualidade que existe. Entradas "
       "que chegaram com máscaras próprias as mantêm; isto vale para as "
       "demais."),
    IT("Descriva ciò che non deve far parte della scena -- passanti, "
       "automobili in sosta, la propria ombra -- e verrà tolto sia dal calcolo "
       "delle fotocamere sia dall'addestramento. In una ripresa con qualcosa "
       "in movimento è il singolo guadagno di qualità più grande. Gli ingressi "
       "arrivati con maschere proprie le conservano; questo vale per gli "
       "altri."),
    NL("Beschrijf wat geen deel van de scène hoort te zijn -- voorbijgangers, "
       "geparkeerde auto's, je eigen schaduw -- en het valt zowel uit de "
       "cameraberekening als uit de training weg. Bij een opname waarin iets "
       "beweegt is dit de grootste kwaliteitswinst die er is. Invoeren die met "
       "eigen maskers kwamen, houden die; dit geldt voor de rest."),
    RU("Опишите, чего в сцене быть не должно, — прохожих, припаркованных машин, "
       "собственной тени, — и это будет исключено и из расчёта камер, и из "
       "обучения. Для съёмки, где хоть что-то движется, это самый большой "
       "выигрыш в качестве. Входы, пришедшие со своими масками, их сохраняют; "
       "это для остальных."),
    TR("Sahnenin parçası olmaması gerekeni yazın -- geçen insanlar, park etmiş "
       "arabalar, kendi gölgeniz -- ve bu hem kamera çözümünden hem de "
       "eğitimden çıkarılır. İçinde hareketli bir şey olan çekimlerde kaliteyi "
       "en çok artıran tek şey budur. Kendi maskeleriyle gelen girdiler "
       "onları korur; bu ayar geri kalanlar içindir."));

SS_MSG(mask_model,
    EN("Model"),         JA("モデル"),        ZH_HANS("模型"),     ZH_HANT("模型"),
    KO("모델"),           DE("Modell"),       FR("Modèle"),       ES("Modelo"),
    PT("Modelo"),        IT("Modello"),      NL("Model"),        RU("Модель"),
    TR("Model"));

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
    EN("Objects you pointed at in \"Try the mask\". Each is followed from the "
       "frame you clicked it on, using the model's video memory, so you do not "
       "have to click every frame."),
    JA("「マスクを試す」で指し示した物体です。クリックしたフレームを起点に、"
       "モデルの動画メモリを使って追跡されるので、毎フレームでクリックする"
       "必要はありません。"),
    ZH_HANS("你在“试一下蒙版”里点选的物体。每个都从你点它的那一帧开始，"
            "借助模型的视频记忆一路跟踪，不必逐帧点击。"),
    ZH_HANT("你在「試一下遮罩」裡點選的物體。每個都從你點它的那一影格開始，"
            "藉助模型的影片記憶一路追蹤，不必逐格點擊。"),
    KO("[마스크 시험해 보기]에서 지목한 물체들입니다. 각각 클릭한 프레임에서부터 "
       "모델의 비디오 메모리로 따라가므로 프레임마다 클릭할 필요가 없습니다."),
    DE("Objekte, auf die Sie in „Maske ausprobieren“ gezeigt haben. Jedes wird "
       "ab dem angeklickten Bild über das Videogedächtnis des Modells "
       "weiterverfolgt, Sie müssen also nicht jedes Bild anklicken."),
    FR("Les objets désignés dans « Essayer le masque ». Chacun est suivi à "
       "partir de l'image où vous l'avez cliqué, grâce à la mémoire vidéo du "
       "modèle : inutile de cliquer sur chaque image."),
    ES("Los objetos que señaló en «Probar la máscara». Cada uno se sigue desde "
       "el fotograma donde hizo clic, usando la memoria de vídeo del modelo, "
       "así que no hace falta pulsar en cada fotograma."),
    PT("Os objetos que você apontou em “Testar a máscara”. Cada um é seguido a "
       "partir do quadro em que você clicou, usando a memória de vídeo do "
       "modelo, então não é preciso clicar em cada quadro."),
    IT("Gli oggetti indicati in «Prova la maschera». Ciascuno viene seguito a "
       "partire dal fotogramma su cui ha cliccato, grazie alla memoria video "
       "del modello: non serve cliccare su ogni fotogramma."),
    NL("De objecten die je in ‘Masker uitproberen’ hebt aangewezen. Elk wordt "
       "vanaf het aangeklikte beeld gevolgd met het videogeheugen van het "
       "model, dus je hoeft niet elk beeld aan te klikken."),
    RU("Объекты, которые вы указали в «Проверить маску». Каждый отслеживается "
       "от кадра, где вы по нему щёлкнули, через видеопамять модели, так что "
       "щёлкать на каждом кадре не нужно."),
    TR("“Maskeyi dene”de işaret ettiğiniz nesneler. Her biri tıkladığınız "
       "kareden başlayarak modelin video belleğiyle izlenir, yani her kareye "
       "tıklamanız gerekmez."));

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
    EN("How sure the model must be before something counts as a match. Lower "
       "catches more -- the half-hidden person at the edge of the frame -- and "
       "starts masking things you did not name; higher keeps only the obvious "
       "ones."),
    JA("何かを一致と見なすまでに、モデルがどれだけ確信している必要があるかです。"
       "低くすると拾う範囲が広がり、画面の端で半分隠れた人まで取れますが、"
       "指定していないものまでマスクし始めます。高くすると明らかなものだけが"
       "残ります。"),
    ZH_HANS("模型要有多确信才算命中。调低会找到更多——画面边缘半遮住的人也能取到"
            "——但也会开始蒙住你没点名的东西；调高则只留下明显的。"),
    ZH_HANT("模型要有多確信才算命中。調低會找到更多——畫面邊緣半遮住的人也能取到"
            "——但也會開始遮住你沒點名的東西；調高則只留下明顯的。"),
    KO("무언가를 일치로 인정하기까지 모델이 얼마나 확신해야 하는지입니다. "
       "낮추면 화면 끝에 반쯤 가린 사람까지 더 많이 잡지만, 지정하지 않은 "
       "것까지 마스킹하기 시작합니다. 높이면 확실한 것만 남습니다."),
    DE("Wie sicher das Modell sein muss, damit etwas als Treffer zählt. "
       "Niedriger fängt mehr ein -- die halb verdeckte Person am Bildrand -- "
       "und maskiert auch Dinge, die Sie nicht genannt haben; höher behält nur "
       "das Offensichtliche."),
    FR("À quel point le modèle doit être sûr pour qu'une chose compte comme "
       "une correspondance. Plus bas attrape davantage -- la personne à moitié "
       "cachée au bord de l'image -- et se met à masquer ce que vous n'avez "
       "pas nommé ; plus haut ne garde que l'évident."),
    ES("Cuánta seguridad necesita el modelo para dar algo por acertado. Más "
       "bajo capta más -- la persona medio tapada en el borde del fotograma -- "
       "y empieza a enmascarar cosas que no nombró; más alto deja solo lo "
       "evidente."),
    PT("Quanta certeza o modelo precisa ter para algo contar como acerto. Mais "
       "baixo capta mais -- a pessoa meio escondida na beira do quadro -- e "
       "passa a mascarar coisas que você não nomeou; mais alto deixa só o "
       "evidente."),
    IT("Quanto deve essere sicuro il modello perché qualcosa conti come "
       "corrispondenza. Più bassa prende di più -- la persona mezza nascosta "
       "al bordo del fotogramma -- e inizia a mascherare cose che non ha "
       "nominato; più alta lascia solo l'evidente."),
    NL("Hoe zeker het model moet zijn voordat iets als treffer telt. Lager "
       "pakt meer op -- de half verscholen persoon aan de rand van het beeld "
       "-- en gaat ook dingen maskeren die je niet noemde; hoger houdt alleen "
       "het overduidelijke."),
    RU("Насколько модель должна быть уверена, чтобы счесть что-то совпадением. "
       "Ниже -- берётся больше, вплоть до наполовину скрытого человека у края "
       "кадра, но маскируется и то, что вы не называли; выше -- остаётся "
       "только очевидное."),
    TR("Bir şeyin eşleşme sayılması için modelin ne kadar emin olması "
       "gerektiği. Düşürmek daha çoğunu yakalar -- karenin kenarındaki yarı "
       "gizli kişiyi de -- ama adını koymadığınız şeyleri de maskelemeye "
       "başlar; yükseltmek yalnızca bariz olanları bırakır."));

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
    EN("When two detections of the same phrase overlap by more than this, only "
       "the stronger one is kept. The default is strict, which thins out a "
       "crowd: raise it when people standing close together are left "
       "unmasked."),
    JA("同じ語句の検出どうしがこれ以上重なった場合、強いほうだけを残します。"
       "既定値は厳しめで、人が密集した場面では取りこぼしが出ます。近くに"
       "立っている人がマスクされないときは値を上げてください。"),
    ZH_HANS("同一个短语的两个检测重叠超过这个比例时，只保留更强的那个。默认值"
            "偏严，人多的场面会少标出一些；靠得很近的人没被蒙住时，把它调高。"),
    ZH_HANT("同一個語句的兩個偵測重疊超過這個比例時，只保留較強的那個。預設值"
            "偏嚴，人多的場面會少標出一些；靠得很近的人沒被遮住時，把它調高。"),
    KO("같은 문구의 검출 둘이 이보다 많이 겹치면 강한 쪽만 남깁니다. 기본값은 "
       "엄격한 편이라 사람이 몰린 장면에서는 일부가 빠집니다. 가까이 선 사람이 "
       "마스킹되지 않으면 값을 올리세요."),
    DE("Überlappen sich zwei Treffer derselben Formulierung stärker als das, "
       "bleibt nur der stärkere. Der Standard ist streng und dünnt eine "
       "Menschenmenge aus: erhöhen Sie ihn, wenn dicht beieinanderstehende "
       "Personen unmaskiert bleiben."),
    FR("Quand deux détections de la même formulation se recouvrent plus que "
       "cela, seule la plus forte est gardée. La valeur par défaut est stricte "
       "et éclaircit une foule : augmentez-la si des personnes serrées l'une "
       "contre l'autre restent non masquées."),
    ES("Cuando dos detecciones de la misma expresión se solapan más que esto, "
       "solo se queda la más fuerte. El valor por defecto es estricto y aclara "
       "una multitud: súbalo si personas muy juntas quedan sin enmascarar."),
    PT("Quando duas detecções da mesma expressão se sobrepõem mais do que "
       "isso, fica só a mais forte. O padrão é estrito e rareia uma multidão: "
       "aumente-o se pessoas bem próximas ficarem sem máscara."),
    IT("Quando due rilevamenti della stessa frase si sovrappongono più di "
       "così, resta solo il più forte. Il valore predefinito è severo e dirada "
       "una folla: lo alzi se persone vicine tra loro restano senza maschera."),
    NL("Als twee treffers van dezelfde omschrijving elkaar meer dan dit "
       "overlappen, blijft alleen de sterkste over. De standaard is streng en "
       "dunt een menigte uit: verhoog hem als mensen die dicht bij elkaar "
       "staan ongemaskeerd blijven."),
    RU("Если два обнаружения одной и той же фразы перекрываются сильнее этого, "
       "остаётся только более уверенное. Значение по умолчанию строгое и "
       "прореживает толпу: поднимите его, если стоящие вплотную люди остаются "
       "без маски."),
    TR("Aynı ifadeye ait iki algılama bundan fazla örtüşürse yalnızca güçlü "
       "olan kalır. Varsayılan katıdır ve kalabalığı seyreltir: birbirine "
       "yakın duran kişiler maskelenmeden kalıyorsa yükseltin."));

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
    EN("Longest side the masking works at. The masks come back at the "
       "original resolution either way, so a smaller value only saves work per "
       "frame and coarsens the mask edges. 0 takes the frame at its own size."),
    JA("マスク処理を行うときの画像の長辺です。マスクは元の解像度に戻して"
       "書き出されるので、小さくすると1フレームあたりの処理が軽くなり、"
       "マスクの境目が粗くなります。0 で元のサイズのまま扱います。"),
    ZH_HANS("做蒙版时图像长边的像素数。蒙版最后都会放回原分辨率，所以调小只是"
            "每帧更省，边缘更糙。填 0 表示按原尺寸处理。"),
    ZH_HANT("做遮罩時影像長邊的像素數。遮罩最後都會放回原解析度，所以調小只是"
            "每格更省，邊緣更粗糙。填 0 表示按原尺寸處理。"),
    KO("마스킹을 수행할 때 이미지 긴 변의 픽셀 수입니다. 마스크는 어차피 원래 "
       "해상도로 되돌려 저장되므로, 줄이면 프레임당 작업만 가벼워지고 "
       "가장자리가 거칠어집니다. 0이면 원래 크기 그대로 씁니다."),
    DE("Längste Seite, mit der die Maskierung arbeitet. Die Masken kommen "
       "ohnehin in der ursprünglichen Auflösung heraus, ein kleinerer Wert "
       "spart also nur Arbeit je Bild und macht die Maskenkanten gröber. 0 "
       "nimmt das Bild in seiner eigenen Größe."),
    FR("Plus grand côté sur lequel le masquage travaille. Les masques "
       "ressortent de toute façon à la résolution d'origine : une valeur plus "
       "petite ne fait qu'alléger le travail par image et grossir les bords du "
       "masque. 0 prend l'image telle quelle."),
    ES("Lado mayor con el que trabaja el enmascarado. Las máscaras salen en la "
       "resolución original de todos modos, así que un valor menor solo ahorra "
       "trabajo por fotograma y engrosa los bordes. 0 toma la imagen tal cual."),
    PT("Maior lado com que o mascaramento trabalha. As máscaras saem na "
       "resolução original de qualquer forma, então um valor menor só alivia o "
       "trabalho por quadro e engrossa as bordas. 0 usa a imagem como está."),
    IT("Lato più lungo su cui lavora la mascheratura. Le maschere escono "
       "comunque alla risoluzione originale, quindi un valore più piccolo "
       "alleggerisce solo il lavoro per fotogramma e ingrossa i bordi. 0 "
       "prende l'immagine com'è."),
    NL("Langste zijde waarmee het maskeren werkt. De maskers komen er hoe dan "
       "ook in de oorspronkelijke resolutie uit, dus een kleinere waarde "
       "scheelt alleen werk per beeld en maakt de maskerranden grover. 0 neemt "
       "het beeld op zijn eigen grootte."),
    RU("Длинная сторона, с которой работает маскирование. Маски всё равно "
       "выходят в исходном разрешении, так что меньшее значение лишь экономит "
       "работу на кадр и огрубляет края маски. 0 -- брать кадр как есть."),
    TR("Maskelemenin çalıştığı en uzun kenar. Maskeler yine özgün "
       "çözünürlükte çıkar, yani küçük bir değer sadece kare başına işi "
       "azaltır ve maske kenarlarını kabalaştırır. 0, kareyi olduğu gibi "
       "alır."));

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
    EN("Follow each object from frame to frame with the model's video memory, "
       "instead of segmenting every frame on its own. It keeps an object the "
       "model loses sight of for a frame or two, and costs one extra pass per "
       "object per frame -- a shot full of them is several times slower. "
       "Clicked objects use it whatever this says."),
    JA("フレームごとに別々に切り出すのではなく、モデルの動画メモリを使って"
       "物体をフレームからフレームへ追いかけます。少しの間見失った物体も"
       "残せますが、物体ひとつにつき毎フレーム余分な推論が一回かかるので、"
       "写っている物体が多いと何倍も遅くなります。クリックした物体は、"
       "この設定にかかわらず常に使います。"),
    ZH_HANS("借助模型的视频记忆把每个物体从一帧跟到下一帧，而不是逐帧单独分割。"
            "短暂看不见的物体也能保住，但每个物体每帧都要多跑一次模型，"
            "画面里物体一多就会慢上好几倍。点选的物体无论这里怎么设都会用它。"),
    ZH_HANT("藉助模型的影片記憶把每個物體從一影格追到下一影格，而不是逐格"
            "單獨分割。短暫看不見的物體也能保住，但每個物體每格都要多跑一次"
            "模型，畫面裡物體一多就會慢上好幾倍。點選的物體無論這裡怎麼設"
            "都會用它。"),
    KO("프레임마다 따로 분할하지 않고, 모델의 비디오 메모리로 각 물체를 "
       "프레임에서 프레임으로 따라갑니다. 잠깐 놓친 물체도 유지되지만, 물체 "
       "하나마다 프레임마다 모델을 한 번씩 더 돌리므로 물체가 많은 촬영은 몇 "
       "배로 느려집니다. 클릭한 물체는 이 설정과 상관없이 항상 사용합니다."),
    DE("Jedes Objekt mit dem Videogedächtnis des Modells von Bild zu Bild "
       "weiterverfolgen, statt jedes Bild für sich zu segmentieren. Das hält "
       "ein Objekt, das für ein, zwei Bilder verloren geht, kostet aber einen "
       "zusätzlichen Durchlauf je Objekt und Bild -- eine Aufnahme voller "
       "Objekte wird um ein Vielfaches langsamer. Angeklickte Objekte nutzen "
       "es in jedem Fall."),
    FR("Suivre chaque objet d'une image à l'autre grâce à la mémoire vidéo du "
       "modèle, au lieu de segmenter chaque image isolément. Cela conserve un "
       "objet perdu de vue pendant une image ou deux, mais coûte une passe "
       "supplémentaire par objet et par image : une prise pleine d'objets "
       "devient plusieurs fois plus lente. Les objets cliqués l'utilisent quoi "
       "qu'il en soit."),
    ES("Seguir cada objeto de un fotograma al siguiente con la memoria de "
       "vídeo del modelo, en lugar de segmentar cada fotograma por separado. "
       "Mantiene un objeto que se pierde de vista uno o dos fotogramas, pero "
       "cuesta una pasada más por objeto y fotograma: una toma llena de ellos "
       "se vuelve varias veces más lenta. Los objetos marcados con clic lo "
       "usan de todos modos."),
    PT("Seguir cada objeto de um quadro para o seguinte com a memória de vídeo "
       "do modelo, em vez de segmentar cada quadro sozinho. Mantém um objeto "
       "que some por um ou dois quadros, mas custa uma passagem a mais por "
       "objeto e por quadro: uma tomada cheia deles fica várias vezes mais "
       "lenta. Os objetos clicados usam isso de qualquer forma."),
    IT("Seguire ogni oggetto da un fotogramma all'altro con la memoria video "
       "del modello, invece di segmentare ogni fotogramma per conto suo. "
       "Mantiene un oggetto perso di vista per uno o due fotogrammi, ma costa "
       "una passata in più per oggetto e per fotogramma: una ripresa piena di "
       "oggetti diventa parecchie volte più lenta. Gli oggetti cliccati la "
       "usano comunque."),
    NL("Elk object met het videogeheugen van het model van beeld naar beeld "
       "volgen, in plaats van elk beeld apart te segmenteren. Dat houdt een "
       "object vast dat een beeld of twee uit zicht raakt, maar kost een extra "
       "doorloop per object per beeld: een opname vol objecten wordt vele "
       "malen trager. Aangeklikte objecten gebruiken het hoe dan ook."),
    RU("Вести каждый объект от кадра к кадру через видеопамять модели, а не "
       "сегментировать каждый кадр отдельно. Объект, пропавший из виду на "
       "кадр-другой, тогда не теряется, но каждый объект на каждом кадре "
       "стоит лишнего прохода модели: съёмка, полная объектов, идёт в "
       "несколько раз дольше. Объекты, отмеченные щелчком, используют её в "
       "любом случае."),
    TR("Her kareyi tek başına bölütlemek yerine, her nesneyi modelin video "
       "belleğiyle kareden kareye izler. Bir iki kare gözden kaybolan nesneyi "
       "korur, ama nesne başına her karede fazladan bir geçiş demektir: nesne "
       "dolu bir çekim birkaç kat yavaşlar. Tıklanan nesneler bu ayardan "
       "bağımsız olarak bunu kullanır."));

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
    EN("Look for new objects only every Nth frame and let the memory carry the "
       "ones already found in between. Bigger is faster and slower to notice "
       "something that walks into the shot. 1 = every frame."),
    JA("新しい物体を探すのは N フレームに 1 回だけにして、その間は見つけ済みの"
       "ものをメモリで持ち越します。大きくすると速くなりますが、途中で入って"
       "きたものが見つかるのは遅くなります。1 で毎フレーム検出します。"),
    ZH_HANS("只每隔 N 帧找一次新物体，中间已经找到的靠记忆带过去。调大更快，"
            "但中途走进画面的东西会晚一些才被发现。填 1 表示每帧都检测。"),
    ZH_HANT("只每隔 N 格找一次新物體，中間已經找到的靠記憶帶過去。調大更快，"
            "但中途走進畫面的東西會晚一些才被發現。填 1 表示每格都偵測。"),
    KO("새 물체는 N 프레임마다 한 번만 찾고, 그 사이에는 이미 찾은 것을 "
       "메모리로 이어 갑니다. 크게 잡으면 빨라지지만 도중에 들어온 것을 늦게 "
       "알아차립니다. 1이면 매 프레임 검출합니다."),
    DE("Nur jedes N-te Bild nach neuen Objekten durchsuchen und die bereits "
       "gefundenen dazwischen vom Gedächtnis tragen lassen. Größer ist "
       "schneller und bemerkt später, was ins Bild läuft. 1 = jedes Bild."),
    FR("Ne chercher de nouveaux objets qu'une image sur N ; entre-temps, la "
       "mémoire porte ceux déjà trouvés. Plus grand est plus rapide et "
       "remarque plus tard ce qui entre dans le champ. 1 = chaque image."),
    ES("Buscar objetos nuevos solo cada N fotogramas; entre medias, la memoria "
       "lleva los ya encontrados. Más grande es más rápido y tarda más en "
       "notar lo que entra en cuadro. 1 = cada fotograma."),
    PT("Procurar objetos novos só a cada N quadros; no intervalo, a memória "
       "carrega os já encontrados. Maior é mais rápido e demora mais a notar o "
       "que entra em cena. 1 = todos os quadros."),
    IT("Cercare nuovi oggetti solo ogni N fotogrammi; nel mezzo la memoria "
       "porta quelli già trovati. Più grande è più veloce e nota più tardi ciò "
       "che entra in campo. 1 = ogni fotogramma."),
    NL("Alleen elk N-de beeld op nieuwe objecten doorzoeken; daartussen draagt "
       "het geheugen de al gevonden objecten. Groter is sneller en merkt later "
       "op wat het beeld in loopt. 1 = elk beeld."),
    RU("Искать новые объекты лишь раз в N кадров, а между ними вести уже "
       "найденные памятью. Больше -- быстрее, но позже заметит то, что вошло в "
       "кадр. 1 -- каждый кадр."),
    TR("Yeni nesneleri yalnızca N karede bir ara; arada bulunmuş olanları "
       "bellek taşır. Büyütmek hızlandırır ama kareye gireni daha geç fark "
       "eder. 1 = her kare."));

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
    EN("How many past frames each tracked object remembers, at most. Tracking "
       "costs about the same multiple of this, so 2 or 3 is much faster than "
       "the model's own 7 -- at the price of losing an object that stayed "
       "hidden longer. 0 = the model's own."),
    JA("追跡している物体ひとつが覚えておく過去のフレーム数の上限です。追跡の"
       "処理量はこれにほぼ比例するので、2 や 3 にすればモデル本来の 7 より"
       "ずっと速くなります。そのぶん長く隠れていた物体は見失います。"
       "0 でモデルの既定値になります。"),
    ZH_HANS("每个被跟踪的物体最多记住多少个过去的帧。跟踪的开销大致与它成正比，"
            "所以设成 2 或 3 会比模型自带的 7 快得多，代价是被挡住太长时间"
            "的物体会跟丢。填 0 用模型自己的值。"),
    ZH_HANT("每個被追蹤的物體最多記住多少個過去的影格。追蹤的開銷大致與它成"
            "正比，所以設成 2 或 3 會比模型自帶的 7 快得多，代價是被擋住"
            "太長時間的物體會追丟。填 0 用模型自己的值。"),
    KO("추적 중인 물체 하나가 기억하는 지난 프레임의 최대 개수입니다. 추적 "
       "비용이 여기에 거의 비례하므로 2나 3으로 두면 모델 기본값 7보다 훨씬 "
       "빠릅니다. 대신 오래 가려져 있던 물체는 놓칩니다. 0이면 모델의 "
       "기본값입니다."),
    DE("Wie viele vergangene Bilder sich ein verfolgtes Objekt höchstens "
       "merkt. Der Aufwand des Verfolgens ist dazu proportional, 2 oder 3 ist "
       "also deutlich schneller als die 7 des Modells -- um den Preis, ein "
       "länger verdecktes Objekt zu verlieren. 0 = der Wert des Modells."),
    FR("Combien d'images passées un objet suivi retient au plus. Le coût du "
       "suivi y est proportionnel : 2 ou 3 est bien plus rapide que les 7 du "
       "modèle, au prix d'un objet perdu s'il est resté caché plus longtemps. "
       "0 = la valeur du modèle."),
    ES("Cuántos fotogramas pasados recuerda como mucho cada objeto seguido. El "
       "coste del seguimiento es proporcional a esto, así que 2 o 3 es mucho "
       "más rápido que los 7 del modelo, a cambio de perder un objeto que "
       "estuvo tapado más tiempo. 0 = el valor del modelo."),
    PT("Quantos quadros passados cada objeto rastreado guarda, no máximo. O "
       "custo do rastreamento é proporcional a isso, então 2 ou 3 é bem mais "
       "rápido que os 7 do modelo, ao preço de perder um objeto escondido por "
       "mais tempo. 0 = o valor do modelo."),
    IT("Quanti fotogrammi passati ricorda al massimo ogni oggetto inseguito. "
       "Il costo dell'inseguimento è proporzionale, quindi 2 o 3 è molto più "
       "veloce dei 7 del modello, al prezzo di perdere un oggetto rimasto "
       "nascosto più a lungo. 0 = il valore del modello."),
    NL("Hoeveel eerdere beelden een gevolgd object hoogstens onthoudt. De "
       "kosten van het volgen zijn hieraan evenredig, dus 2 of 3 is veel "
       "sneller dan de 7 van het model -- ten koste van een object dat langer "
       "verstopt zat. 0 = de waarde van het model."),
    RU("Сколько прошедших кадров помнит каждый отслеживаемый объект. Стоимость "
       "отслеживания этому пропорциональна, так что 2-3 заметно быстрее "
       "модельных 7 -- ценой объекта, скрытого дольше. 0 -- значение модели."),
    TR("İzlenen her nesnenin en çok kaç geçmiş kareyi hatırladığı. İzlemenin "
       "maliyeti bununla orantılıdır, bu yüzden 2 ya da 3 modelin kendi 7 "
       "değerinden çok daha hızlıdır; bedeli, daha uzun süre gizlenen bir "
       "nesneyi kaybetmektir. 0 = modelin kendi değeri."));

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
    EN("A `masks` folder beside or under the photos is picked up on its own, "
       "because that is where a prepared capture keeps them. Turn this off "
       "for a dataset whose `masks` folder belongs to something else -- a "
       "different set of views, or an experiment you are not reproducing."),
    JA("写真のとなりや下にある `masks` フォルダーは自動で拾います。用意済みの"
       "データセットはそこに置くからです。その `masks` が別のもの――別の視点の"
       "集まりや、いま再現しようとしていない実験――に属している場合は、"
       "これを外してください。"),
    ZH_HANS("照片旁边或下面的 `masks` 文件夹会被自动采用，因为准备好的数据集"
            "就放在那里。如果那个 `masks` 属于别的东西——另一组视角，或者你"
            "并不打算复现的实验——请关掉这一项。"),
    ZH_HANT("照片旁邊或下面的 `masks` 資料夾會被自動採用，因為準備好的資料集"
            "就放在那裡。如果那個 `masks` 屬於別的東西——另一組視角，或者你"
            "並不打算重現的實驗——請關掉這一項。"),
    KO("사진 옆이나 아래의 `masks` 폴더는 자동으로 사용됩니다. 준비된 촬영본이 "
       "마스크를 두는 자리이기 때문입니다. 그 `masks`가 다른 것에 속한다면"
       "(다른 시점 묶음이거나, 지금 재현하려는 것이 아닌 실험이라면) 이 "
       "항목을 끄세요."),
    DE("Ein `masks`-Ordner neben oder unter den Fotos wird von selbst "
       "übernommen -- dort legt eine vorbereitete Aufnahme sie ab. Schalten "
       "Sie das aus, wenn dieser Ordner zu etwas anderem gehört: zu einem "
       "anderen Satz Ansichten oder zu einem Versuch, den Sie nicht "
       "nachstellen."),
    FR("Un dossier `masks` à côté ou sous les photos est repris tout seul, "
       "car c'est là qu'une prise de vue préparée les garde. Décochez pour un "
       "jeu de données dont le dossier `masks` appartient à autre chose : un "
       "autre ensemble de vues, ou une expérience que vous ne reproduisez "
       "pas."),
    ES("Una carpeta `masks` junto a las fotos o debajo de ellas se toma sola, "
       "porque ahí es donde las guarda una captura preparada. Desactívelo si "
       "esa carpeta pertenece a otra cosa: a otro conjunto de vistas, o a un "
       "experimento que no está reproduciendo."),
    PT("Uma pasta `masks` ao lado das fotos ou abaixo delas é adotada "
       "sozinha, porque é ali que uma captura preparada as guarda. Desligue "
       "isto se essa pasta pertencer a outra coisa: a outro conjunto de "
       "vistas, ou a um experimento que você não está reproduzindo."),
    IT("Una cartella `masks` accanto alle foto o sotto di esse viene presa da "
       "sola, perché è lì che una ripresa preparata le tiene. Lo disattivi se "
       "quella cartella appartiene ad altro: a un altro insieme di viste, o a "
       "un esperimento che non sta riproducendo."),
    NL("Een map `masks` naast of onder de foto's wordt vanzelf overgenomen -- "
       "daar bewaart een voorbereide opname ze. Zet dit uit voor een dataset "
       "waarvan die map bij iets anders hoort: een andere reeks aanzichten, "
       "of een proef die u niet nabootst."),
    RU("Папка `masks` рядом с фотографиями или под ними подхватывается сама — "
       "именно там подготовленная съёмка их держит. Снимите галочку, если эта "
       "папка относится к чему-то другому: к другому набору видов или к "
       "опыту, который вы не повторяете."),
    TR("Fotoğrafların yanındaki ya da altındaki bir `masks` klasörü kendinden "
       "alınır; hazırlanmış bir çekim maskeleri orada tutar. O klasör başka "
       "bir şeye aitse -- başka bir görünüm kümesine ya da yeniden "
       "üretmediğiniz bir denemeye -- bunu kapatın."));

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
    EN("The usual choice: an ordinary lens, corrected for barrel and "
       "pincushion distortion. Phones, compacts, SLRs, and action cameras "
       "that are not in a fisheye mode."),
    JA("ふつうはこれです。通常のレンズを、樽型・糸巻き型のゆがみを含めて"
       "補正します。スマートフォン、コンパクト、一眼、魚眼モードでない"
       "アクションカメラ向けです。"),
    ZH_HANS("一般就选它：普通镜头，校正桶形和枕形畸变。手机、卡片机、单反，"
            "以及没有开鱼眼模式的运动相机。"),
    ZH_HANT("一般就選它：普通鏡頭，校正桶形和枕形變形。手機、輕便相機、單眼，"
            "以及沒有開魚眼模式的運動相機。"),
    KO("보통은 이것입니다. 일반 렌즈를 통형·실패형 왜곡까지 보정합니다. "
       "휴대폰, 콤팩트, DSLR, 어안 모드가 아닌 액션캠에 씁니다."),
    DE("Die übliche Wahl: ein gewöhnliches Objektiv, korrigiert um "
       "tonnen- und kissenförmige Verzeichnung. Telefone, Kompakte, "
       "Spiegelreflex und Actionkameras, die nicht im Fischaugenmodus sind."),
    FR("Le choix habituel : un objectif ordinaire, corrigé de la distorsion "
       "en barillet et en coussinet. Téléphones, compacts, reflex et caméras "
       "d'action qui ne sont pas en mode fisheye."),
    ES("La opción habitual: un objetivo normal, corregido de distorsión de "
       "barril y de cojín. Teléfonos, compactas, réflex y cámaras de acción "
       "que no estén en modo ojo de pez."),
    PT("A escolha habitual: uma lente comum, corrigida de distorção de barril "
       "e de almofada. Telefones, compactas, reflex e câmeras de ação que não "
       "estejam em modo olho de peixe."),
    IT("La scelta abituale: un obiettivo comune, corretto dalla distorsione a "
       "barile e a cuscinetto. Telefoni, compatte, reflex e action cam che "
       "non siano in modalità fisheye."),
    NL("De gewone keuze: een normaal objectief, gecorrigeerd voor ton- en "
       "kussenvormige vertekening. Telefoons, compacts, spiegelreflex en "
       "actiecamera's die niet in een fisheye-stand staan."),
    RU("Обычный выбор: обыкновенный объектив с поправкой на бочкообразную и "
       "подушкообразную дисторсию. Телефоны, компакты, зеркальные камеры и "
       "экшн-камеры не в режиме «рыбий глаз»."),
    TR("Alışılmış seçim: sıradan bir objektif, fıçı ve yastık bozulmasına "
       "göre düzeltilir. Telefonlar, kompaktlar, SLR'ler ve balıkgözü "
       "kipinde olmayan aksiyon kameraları."));

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

SS_MSG(lens_fisheye_kb,
    EN("Fisheye (Kannala-Brandt)"),
    JA("魚眼（Kannala-Brandt）"),
    ZH_HANS("鱼眼（Kannala-Brandt）"),
    ZH_HANT("魚眼（Kannala-Brandt）"),
    KO("어안(Kannala-Brandt)"),
    DE("Fisheye (Kannala-Brandt)"),
    FR("Fisheye (Kannala-Brandt)"),
    ES("Ojo de pez (Kannala-Brandt)"),
    PT("Olho de peixe (Kannala-Brandt)"),
    IT("Fisheye (Kannala-Brandt)"),
    NL("Fisheye (Kannala-Brandt)"),
    RU("Фишай (Каннала — Брандт)"),
    TR("Balıkgözü (Kannala-Brandt)"));

SS_MSG(lens_fisheye_kb_help,
    EN("A fisheye lens, up to about 180 degrees across. The standard model "
       "for one fisheye, and for the wide modes of most action cameras."),
    JA("およそ180度までの魚眼レンズです。単一の魚眼、および多くのアクション"
       "カメラの広角モードの標準的なモデルです。"),
    ZH_HANS("视场约到 180 度的鱼眼镜头。单个鱼眼的标准模型，多数运动相机的广角"
            "模式也是这个。"),
    ZH_HANT("視場約到 180 度的魚眼鏡頭。單個魚眼的標準模型，多數運動相機的廣角"
            "模式也是這個。"),
    KO("대략 180도까지의 어안 렌즈입니다. 어안 하나에 대한 표준 모델이며, 대부분 "
       "액션캠의 광각 모드도 이것입니다."),
    DE("Ein Fischaugenobjektiv bis etwa 180 Grad. Das Standardmodell für ein "
       "einzelnes Fischauge und für die Weitwinkelmodi der meisten "
       "Actionkameras."),
    FR("Un objectif fisheye, jusqu'à environ 180 degrés. Le modèle standard "
       "pour un fisheye, et pour les modes grand-angle de la plupart des "
       "caméras d'action."),
    ES("Un objetivo ojo de pez, de hasta unos 180 grados. El modelo estándar "
       "para un ojo de pez, y para los modos angulares de casi todas las "
       "cámaras de acción."),
    PT("Uma lente olho de peixe, de até cerca de 180 graus. O modelo padrão "
       "para um olho de peixe, e para os modos grande-angulares da maioria "
       "das câmeras de ação."),
    IT("Un obiettivo fisheye, fino a circa 180 gradi. Il modello standard per "
       "un fisheye singolo e per le modalità grandangolari di quasi tutte le "
       "action cam."),
    NL("Een fisheye-objectief, tot ongeveer 180 graden. Het standaardmodel "
       "voor één fisheye, en voor de groothoekstanden van de meeste "
       "actiecamera's."),
    RU("Объектив «рыбий глаз» примерно до 180 градусов. Стандартная модель "
       "для одного фишая и для широкоугольных режимов большинства "
       "экшн-камер."),
    TR("Yaklaşık 180 dereceye kadar bir balıkgözü objektif. Tek balıkgözü "
       "için ve çoğu aksiyon kamerasının geniş açı kipleri için standart "
       "model."));

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
    EN("A fisheye with extra distortion parameters. This is the one to pick "
       "for very wide fisheye cameras, such as the lenses of a typical 360 "
       "camera before stitching."),
    JA("ゆがみのパラメータを追加で持つ魚眼です。画角の非常に広い魚眼カメラ、"
       "たとえばステッチ前の一般的な360度カメラのレンズにはこれを選びます。"),
    ZH_HANS("带有额外畸变参数的鱼眼。视角非常大的鱼眼相机就选这个，例如常见 "
            "360 相机拼接之前的镜头。"),
    ZH_HANT("帶有額外變形參數的魚眼。視角非常大的魚眼相機就選這個，例如常見 "
            "360 相機拼接之前的鏡頭。"),
    KO("왜곡 파라미터를 더 가진 어안입니다. 화각이 아주 넓은 어안 카메라, "
       "이를테면 이어 붙이기 전의 일반적인 360도 카메라 렌즈에는 이것을 "
       "고릅니다."),
    DE("Ein Fischauge mit zusätzlichen Verzeichnungsparametern. Das ist die "
       "Wahl für sehr weitwinklige Fischaugenkameras, etwa die Objektive "
       "einer üblichen 360-Kamera vor dem Zusammenfügen."),
    FR("Un fisheye avec des paramètres de distorsion supplémentaires. C'est "
       "le choix pour les caméras fisheye très ouvertes, par exemple les "
       "objectifs d'une caméra 360 courante avant assemblage."),
    ES("Un ojo de pez con parámetros de distorsión adicionales. Es el que se "
       "elige para cámaras de ojo de pez muy angulares, como los objetivos de "
       "una cámara 360 corriente antes de unir las imágenes."),
    PT("Um olho de peixe com parâmetros de distorção a mais. É o que se "
       "escolhe para câmeras olho de peixe muito abertas, como as lentes de "
       "uma câmera 360 comum antes da costura."),
    IT("Un fisheye con parametri di distorsione in più. È quello da scegliere "
       "per fotocamere fisheye molto aperte, per esempio gli obiettivi di una "
       "comune fotocamera a 360 gradi prima della cucitura."),
    NL("Een fisheye met extra vertekeningsparameters. Dit is de keuze voor "
       "zeer wijde fisheye-camera's, zoals de objectieven van een gewone "
       "360-camera vóór het aaneennaaien."),
    RU("Фишай с дополнительными параметрами дисторсии. Это выбор для очень "
       "широких фишай-камер — например, объективов обычной 360-камеры до "
       "сшивки."),
    TR("Fazladan bozulma parametreleri olan bir balıkgözü. Çok geniş açılı "
       "balıkgözü kameralar için bu seçilir; örneğin sıradan bir 360 "
       "kameranın, birleştirmeden önceki objektifleri."));

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
    EN("One image covering the whole sphere, twice as wide as it is tall -- "
       "what a 360 camera writes once its own software has stitched its two "
       "lenses together. Not for the raw two-circle frames themselves."),
    JA("全天球を1枚に収めた、横が縦の2倍の画像です。360度カメラが自前の"
       "ソフトで2つのレンズを合成したあとに書き出すものです。円が2つ並んだ"
       "生の画面には使いません。"),
    ZH_HANS("整个球面装在一张图里，宽是高的两倍——360 相机用自带软件把两个镜头"
            "拼接之后导出的就是这种。不要用于还是两个圆的原始画面。"),
    ZH_HANT("整個球面裝在一張圖裡，寬是高的兩倍——360 相機用自帶軟體把兩個鏡頭"
            "拼接之後匯出的就是這種。不要用於還是兩個圓的原始畫面。"),
    KO("온 구면을 한 장에 담은, 가로가 세로의 두 배인 이미지입니다. 360도 "
       "카메라가 자체 소프트웨어로 두 렌즈를 이어 붙인 뒤 내보내는 것이 이것"
       "입니다. 원이 두 개인 원본 화면에는 쓰지 않습니다."),
    DE("Ein Bild, das die ganze Kugel abdeckt und doppelt so breit wie hoch "
       "ist -- das, was eine 360-Kamera schreibt, nachdem ihre eigene "
       "Software die beiden Objektive zusammengefügt hat. Nicht für die rohen "
       "Bilder mit den zwei Kreisen."),
    FR("Une image qui couvre toute la sphère, deux fois plus large que haute "
       "-- ce qu'écrit une caméra 360 une fois que son logiciel a assemblé "
       "ses deux objectifs. Pas pour les images brutes à deux cercles."),
    ES("Una imagen que cubre toda la esfera, el doble de ancha que de alta: "
       "lo que escribe una cámara 360 cuando su propio programa ya ha unido "
       "los dos objetivos. No para los fotogramas en bruto de dos círculos."),
    PT("Uma imagem que cobre toda a esfera, duas vezes mais larga que alta -- "
       "o que uma câmera 360 grava depois de o seu próprio programa costurar "
       "as duas lentes. Não para os quadros crus de dois círculos."),
    IT("Un'immagine che copre tutta la sfera, larga il doppio dell'altezza: "
       "quello che scrive una fotocamera a 360 gradi dopo che il suo "
       "programma ha cucito i due obiettivi. Non per i fotogrammi grezzi a "
       "due cerchi."),
    NL("Eén beeld dat de hele bol beslaat, twee keer zo breed als hoog -- wat "
       "een 360-camera wegschrijft zodra haar eigen software de twee "
       "objectieven aaneen heeft genaaid. Niet voor de rauwe beelden met twee "
       "cirkels."),
    RU("Одно изображение на всю сферу, вдвое шире, чем выше, — то, что "
       "360-камера записывает после того, как её собственная программа сшила "
       "два объектива. Не для исходных кадров с двумя кругами."),
    TR("Tüm küreyi kaplayan, eni boyunun iki katı olan tek bir görüntü -- 360 "
       "kameranın, kendi yazılımı iki objektifi birleştirdikten sonra "
       "yazdığı şey. İki daireli ham kareler için değil."));

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
    EN("This capture records two fisheye circles per frame, not a stitched "
       "panorama. Pick a fisheye model -- Fisheye (thin prism) is the one "
       "these cameras fit."),
    JA("この素材は1フレームに魚眼の円を2つ記録しており、合成済みのパノラマ"
       "ではありません。魚眼のモデルを選んでください。これらのカメラには"
       "「魚眼（薄プリズム）」が合います。"),
    ZH_HANS("这份素材每帧记录两个鱼眼圆，并不是拼接好的全景。请选鱼眼模型——"
            "这类相机适合“鱼眼（薄棱镜）”。"),
    ZH_HANT("這份素材每格記錄兩個魚眼圓，並不是拼接好的全景。請選魚眼模型——"
            "這類相機適合「魚眼（薄稜鏡）」。"),
    KO("이 촬영본은 한 프레임에 어안 원을 두 개 기록하며, 이어 붙인 파노라마가 "
       "아닙니다. 어안 모델을 고르세요. 이런 카메라에는 '어안(얇은 프리즘)'이 "
       "맞습니다."),
    DE("Diese Aufnahme zeichnet zwei Fischaugenkreise je Bild auf, kein "
       "zusammengefügtes Panorama. Wählen Sie ein Fischaugenmodell -- "
       "Fisheye (dünnes Prisma) passt zu diesen Kameras."),
    FR("Cette prise enregistre deux cercles fisheye par image, pas un "
       "panorama assemblé. Choisissez un modèle fisheye : Fisheye (prisme "
       "mince) est celui qui convient à ces caméras."),
    ES("Esta toma graba dos círculos de ojo de pez por fotograma, no un "
       "panorama unido. Elija un modelo de ojo de pez: Ojo de pez (prisma "
       "delgado) es el que encaja con estas cámaras."),
    PT("Esta captura grava dois círculos olho de peixe por quadro, não um "
       "panorama costurado. Escolha um modelo olho de peixe: Olho de peixe "
       "(prisma fino) é o que serve para essas câmeras."),
    IT("Questa ripresa registra due cerchi fisheye per fotogramma, non un "
       "panorama cucito. Scelga un modello fisheye: Fisheye (prisma sottile) "
       "è quello adatto a queste fotocamere."),
    NL("Deze opname legt per beeld twee fisheye-cirkels vast, geen "
       "aaneengenaaid panorama. Kies een fisheye-model -- Fisheye (dun "
       "prisma) past bij deze camera's."),
    RU("Эта съёмка записывает по два круга «рыбьего глаза» на кадр, а не "
       "сшитую панораму. Выберите модель фишая — этим камерам подходит "
       "«Фишай (тонкая призма)»."),
    TR("Bu çekim kare başına iki balıkgözü dairesi kaydeder, birleştirilmiş "
       "bir panorama değil. Bir balıkgözü modeli seçin -- bu kameralara "
       "Balıkgözü (ince prizma) uyar."));

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
    EN("\"Remove\" is for distractors -- people, cars, the photographer's "
       "shadow. \"Keep only\" is for object captures, where everything but the "
       "subject should be ignored."),
    JA("「取り除く」は邪魔物、たとえば人、車、撮影者の影に使います。"
       "「だけを残す」は物体の撮影用で、被写体以外をすべて無視したいときに"
       "使います。"),
    ZH_HANS("“移除”用于干扰物——行人、汽车、摄影者的影子。“只保留”用于物体拍摄，"
            "此时除主体外的一切都该被忽略。"),
    ZH_HANT("「移除」用於干擾物——行人、汽車、攝影者的影子。「只保留」用於物體拍攝，"
            "此時除主體外的一切都該被忽略。"),
    KO("'제거'는 지나가는 사람, 자동차, 촬영자의 그림자 같은 방해물용입니다. "
       "'만 남기기'는 물체 촬영용으로, 피사체 말고는 전부 무시하고 싶을 때 "
       "씁니다."),
    DE("„Entfernen“ ist für Störendes -- Passanten, Autos, der eigene "
       "Schatten. „Nur behalten“ ist für Objektaufnahmen, bei denen alles "
       "außer dem Motiv ignoriert werden soll."),
    FR("« Retirer » sert pour les gêneurs -- passants, voitures, l'ombre du "
       "photographe. « Ne garder que » sert aux prises d'objet, où tout sauf "
       "le sujet doit être ignoré."),
    ES("«Quitar» es para elementos molestos: transeúntes, coches, la sombra "
       "del fotógrafo. «Conservar solo» es para capturas de objetos, donde "
       "todo salvo el sujeto debe ignorarse."),
    PT("“Remover” é para elementos indesejados: pessoas passando, carros, a "
       "sombra do fotógrafo. “Manter só” é para capturas de objetos, em que "
       "tudo além do sujeito deve ser ignorado."),
    IT("«Rimuovere» serve per gli elementi di disturbo: passanti, automobili, "
       "l'ombra del fotografo. «Tenere solo» serve alle riprese di oggetti, "
       "dove tutto tranne il soggetto va ignorato."),
    NL("‘Verwijderen’ is voor stoorelementen -- voorbijgangers, auto's, de "
       "schaduw van de fotograaf. ‘Alleen houden’ is voor objectopnamen, "
       "waarbij alles behalve het onderwerp genegeerd moet worden."),
    RU("«Убрать» — для помех: прохожих, машин, тени фотографа. «Оставить "
       "только» — для съёмки предметов, когда всё, кроме объекта, нужно "
       "игнорировать."),
    TR("“Kaldır” istenmeyenler içindir -- geçen insanlar, arabalar, "
       "fotoğrafçının gölgesi. “Yalnızca tut” nesne çekimleri içindir; orada "
       "özne dışındaki her şey yok sayılmalıdır."));

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
    EN("Plain words for the things to take out of the reconstruction, "
       "separated by semicolons. Anything that moved, reflected, or was not "
       "part of the scene is a good candidate."),
    JA("再構成から外したいものをふつうの言葉でセミコロン区切りに書きます。"
       "動いたもの、映り込んだもの、シーンの一部ではなかったものが候補です。"),
    ZH_HANS("用平常的词语写出要从重建中拿掉的东西，以分号分隔。凡是移动过的、"
            "有反射的、本来就不属于场景的，都是合适的候选。"),
    ZH_HANT("用平常的詞語寫出要從重建中拿掉的東西，以分號分隔。凡是移動過的、"
            "有反射的、本來就不屬於場景的，都是合適的候選。"),
    KO("재구성에서 빼고 싶은 것들을 평범한 낱말로 세미콜론으로 구분해 적으세요. "
       "움직였던 것, 비쳤던 것, 원래 장면의 일부가 아니었던 것이 좋은 "
       "후보입니다."),
    DE("Einfache Wörter für das, was aus der Rekonstruktion heraus soll, durch "
       "Semikolon getrennt. Alles, was sich bewegte, spiegelte oder nicht zur "
       "Szene gehörte, ist ein guter Kandidat."),
    FR("Des mots ordinaires pour ce qu'il faut sortir de la reconstruction, "
       "séparés par des points-virgules. Tout ce qui bougeait, se reflétait ou "
       "ne faisait pas partie de la scène est un bon candidat."),
    ES("Palabras corrientes para lo que hay que sacar de la reconstrucción, "
       "separadas por punto y coma. Todo lo que se movía, se reflejaba o no "
       "formaba parte de la escena es buen candidato."),
    PT("Palavras simples para o que deve sair da reconstrução, separadas por "
       "ponto e vírgula. Tudo o que se movia, refletia ou não fazia parte da "
       "cena é bom candidato."),
    IT("Parole comuni per ciò che va tolto dalla ricostruzione, separate da "
       "punto e virgola. Tutto ciò che si muoveva, si rifletteva o non faceva "
       "parte della scena è un buon candidato."),
    NL("Gewone woorden voor wat uit de reconstructie moet, gescheiden door "
       "puntkomma's. Alles wat bewoog, weerspiegelde of geen deel van de scène "
       "was, is een goede kandidaat."),
    RU("Обычные слова для того, что нужно вынести из реконструкции, через "
       "точку с запятой. Хорошие кандидаты — всё, что двигалось, отражалось "
       "или не относилось к сцене."),
    TR("Yeniden oluşturmadan çıkarılacak şeyler için noktalı virgülle ayrılmış "
       "sıradan sözcükler. Hareket eden, yansıyan ya da sahnenin parçası "
       "olmayan her şey iyi bir adaydır."));

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
    EN("One object per thing you want. SAM finds a single object per prompt, "
       "so clicking a person and then a car with the same object selected "
       "gives one mask that fits neither -- open a second object instead. "
       "Clicks belong to the frame you made them on: scrub to a later frame "
       "and click again to correct an object that has drifted."),
    JA("欲しいものごとに物体を1つ用意します。SAM は1つのプロンプトにつき1つの"
       "物体しか見つけないので、同じ物体を選んだまま人と車をクリックすると、"
       "どちらにも合わない1つのマスクになります。代わりに物体を追加して"
       "ください。クリックはそれを行ったフレームに属します。追跡がずれた"
       "物体は、後のフレームまで送ってからもう一度クリックすると直せます。"),
    ZH_HANS("你想要的每样东西各占一个物体。SAM 每条提示只找一个物体，所以在同一"
            "个物体上先点人再点车，会得到一个两边都不合的蒙版——请另开一个物体。"
            "点击属于你点它的那一帧：如果某个物体跟丢了，拖到后面的帧再点一次"
            "就能纠正。"),
    ZH_HANT("你想要的每樣東西各佔一個物體。SAM 每條提示只找一個物體，所以在同一"
            "個物體上先點人再點車，會得到一個兩邊都不合的遮罩——請另開一個物體。"
            "點擊屬於你點它的那一影格：如果某個物體跟丟了，拖到後面的影格再點一次"
            "就能糾正。"),
    KO("원하는 것마다 물체를 하나씩 두세요. SAM은 프롬프트 하나당 물체 하나만 "
       "찾으므로, 같은 물체를 고른 채 사람과 자동차를 차례로 클릭하면 둘 다 맞지 "
       "않는 마스크 하나가 나옵니다. 대신 물체를 하나 더 여세요. 클릭은 그것을 "
       "찍은 프레임에 속합니다. 추적이 어긋난 물체는 뒤쪽 프레임으로 옮겨 다시 "
       "클릭하면 바로잡을 수 있습니다."),
    DE("Ein Objekt je Sache, die Sie wollen. SAM findet pro Eingabe genau ein "
       "Objekt; klickt man also bei demselben ausgewählten Objekt erst eine "
       "Person und dann ein Auto an, entsteht eine Maske, die zu keinem von "
       "beiden passt -- stattdessen ein zweites Objekt anlegen. Klicks gehören "
       "zu dem Bild, auf dem sie gemacht wurden: zu einem späteren Bild "
       "spulen und erneut klicken, um ein abgedriftetes Objekt zu korrigieren."),
    FR("Un objet par chose voulue. SAM ne trouve qu'un seul objet par invite : "
       "cliquer une personne puis une voiture avec le même objet sélectionné "
       "donne un masque qui ne convient ni à l'une ni à l'autre -- ouvrez "
       "plutôt un deuxième objet. Les clics appartiennent à l'image où ils "
       "ont été faits : avancez à une image ultérieure et recliquez pour "
       "corriger un objet qui a dérivé."),
    ES("Un objeto por cada cosa que quiera. SAM encuentra un solo objeto por "
       "indicación, así que marcar una persona y luego un coche con el mismo "
       "objeto seleccionado da una máscara que no encaja con ninguno: abra un "
       "segundo objeto. Los clics pertenecen al fotograma en que se hicieron: "
       "avance a un fotograma posterior y vuelva a marcar para corregir un "
       "objeto que se ha desviado."),
    PT("Um objeto para cada coisa que você quer. O SAM encontra um único "
       "objeto por comando, então clicar numa pessoa e depois num carro com o "
       "mesmo objeto selecionado dá uma máscara que não serve para nenhum dos "
       "dois -- abra um segundo objeto. Os cliques pertencem ao quadro em que "
       "foram feitos: avance para um quadro posterior e clique de novo para "
       "corrigir um objeto que se desviou."),
    IT("Un oggetto per ogni cosa che le serve. SAM trova un solo oggetto per "
       "richiesta, quindi cliccare una persona e poi un'automobile con lo "
       "stesso oggetto selezionato dà una maschera che non va bene per "
       "nessuno dei due: apra piuttosto un secondo oggetto. I clic "
       "appartengono al fotogramma su cui sono stati fatti: vada a un "
       "fotogramma successivo e clicchi di nuovo per correggere un oggetto che "
       "è andato alla deriva."),
    NL("Eén object per ding dat je wilt. SAM vindt per prompt maar één object, "
       "dus eerst een persoon en dan een auto aanklikken met hetzelfde object "
       "geselecteerd geeft één masker dat bij geen van beide past -- open in "
       "plaats daarvan een tweede object. Klikken horen bij het beeld waarop "
       "je ze zette: spoel naar een later beeld en klik opnieuw om een object "
       "te corrigeren dat is afgedwaald."),
    RU("По одному объекту на каждую нужную вещь. SAM находит по одному объекту "
       "на запрос, так что если при выбранном объекте щёлкнуть сначала "
       "человека, а потом машину, выйдет одна маска, не подходящая ни тому, ни "
       "другому — заведите второй объект. Щелчки принадлежат тому кадру, где "
       "сделаны: перемотайте на более поздний кадр и щёлкните снова, чтобы "
       "поправить сбившийся объект."),
    TR("İstediğiniz her şey için bir nesne. SAM istem başına tek bir nesne "
       "bulur; aynı nesne seçiliyken önce bir insana sonra bir arabaya "
       "tıklarsanız ikisine de uymayan tek bir maske çıkar -- bunun yerine "
       "ikinci bir nesne açın. Tıklamalar yapıldıkları kareye aittir: kayan "
       "bir nesneyi düzeltmek için ileri bir kareye gidip yeniden tıklayın."));

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
    EN("Anything that sits in the same place in every shot: the black edge of "
       "a fisheye, a watermark, the pole the camera is on. No model is "
       "involved -- these are shapes, and they apply to the whole capture."),
    JA("どのカットでも同じ位置にあるもの、たとえば魚眼の黒枠、透かし、カメラを"
       "付けた棒などです。モデルは使いません。図形として扱い、撮影全体に"
       "適用されます。"),
    ZH_HANS("在每一张里都在同一位置的东西：鱼眼的黑边、水印、举着相机的杆。"
            "不涉及模型——它们是图形，对整段素材都生效。"),
    ZH_HANT("在每一張裡都在同一位置的東西：魚眼的黑邊、浮水印、舉著相機的桿。"
            "不涉及模型——它們是圖形，對整段素材都生效。"),
    KO("어느 장면에서나 같은 자리에 있는 것들입니다. 어안의 검은 가장자리, "
       "워터마크, 카메라를 단 장대 같은 것이죠. 모델은 쓰지 않습니다. 도형이며 "
       "촬영 전체에 적용됩니다."),
    DE("Alles, was in jeder Aufnahme an derselben Stelle sitzt: der schwarze "
       "Rand eines Fisheye, ein Wasserzeichen, die Stange, an der die Kamera "
       "hängt. Ohne Modell -- das sind Formen, und sie gelten für die ganze "
       "Aufnahme."),
    FR("Tout ce qui occupe la même place sur chaque prise : le bord noir d'un "
       "fisheye, un filigrane, la perche qui porte la caméra. Sans modèle : ce "
       "sont des formes, et elles valent pour toute la prise."),
    ES("Todo lo que ocupa el mismo sitio en cada toma: el borde negro de un "
       "ojo de pez, una marca de agua, el palo que sostiene la cámara. Sin "
       "modelo: son formas, y valen para toda la captura."),
    PT("Tudo o que fica no mesmo lugar em cada tomada: a borda preta de um "
       "olho-de-peixe, uma marca d'água, o bastão que segura a câmera. Sem "
       "modelo: são formas, e valem para a captura inteira."),
    IT("Tutto ciò che sta nello stesso punto in ogni ripresa: il bordo nero di "
       "un fisheye, una filigrana, l'asta che regge la fotocamera. Senza "
       "modello: sono forme, e valgono per l'intera ripresa."),
    NL("Alles wat in elke opname op dezelfde plek zit: de zwarte rand van een "
       "fisheye, een watermerk, de stok waar de camera aan hangt. Zonder "
       "model -- dit zijn vormen, en ze gelden voor de hele opname."),
    RU("Всё, что стоит на одном и том же месте в каждом кадре: чёрный край "
       "фишая, водяной знак, палка, на которой камера. Модель не нужна: это "
       "фигуры, и они действуют на всю съёмку."),
    TR("Her çekimde aynı yerde duran her şey: balıkgözünün siyah kenarı, bir "
       "filigran, kameranın takılı olduğu çubuk. Model yok -- bunlar biçimdir "
       "ve tüm çekim için geçerlidir."));

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
    EN("Finds the circle the lens draws and keeps only what is inside it. It "
       "is measured again for each camera when the dataset is built, so the "
       "two lenses of a 360 camera each get their own."),
    JA("レンズが描く円を見つけ、その内側だけを残します。データセットを作るとき"
       "にカメラごとに測り直すので、360度カメラの 2 つのレンズはそれぞれ別の円に"
       "なります。"),
    ZH_HANS("找出镜头成的圆，只保留圆内。建数据集时会为每台相机重新测一次，"
            "所以 360 相机的两个镜头各有各的圆。"),
    ZH_HANT("找出鏡頭成的圓，只保留圓內。建資料集時會為每台相機重新測一次，"
            "所以 360 相機的兩個鏡頭各有各的圓。"),
    KO("렌즈가 그리는 원을 찾아 그 안쪽만 남깁니다. 데이터셋을 만들 때 카메라마다 "
       "다시 재므로 360도 카메라의 두 렌즈는 각각 자기 원을 갖습니다."),
    DE("Findet den Kreis, den das Objektiv zeichnet, und behält nur, was darin "
       "liegt. Beim Bau des Datensatzes wird er je Kamera neu bestimmt, sodass "
       "die beiden Objektive einer 360-Grad-Kamera je einen eigenen bekommen."),
    FR("Trouve le cercle que dessine l'objectif et ne garde que l'intérieur. "
       "Il est remesuré pour chaque caméra à la construction du jeu de "
       "données, si bien que les deux objectifs d'une caméra 360 ont chacun le "
       "leur."),
    ES("Encuentra el círculo que dibuja el objetivo y conserva solo lo de "
       "dentro. Se vuelve a medir para cada cámara al construir el conjunto, "
       "así que los dos objetivos de una cámara 360 tienen el suyo."),
    PT("Acha o círculo que a lente desenha e mantém só o que está dentro. Ele "
       "é medido de novo para cada câmera ao montar o conjunto, então as duas "
       "lentes de uma câmera 360 ficam cada uma com o seu."),
    IT("Trova il cerchio disegnato dall'obiettivo e tiene solo ciò che vi sta "
       "dentro. Viene rimisurato per ogni fotocamera quando si costruisce il "
       "dataset, così i due obiettivi di una 360 hanno ciascuno il proprio."),
    NL("Zoekt de cirkel die de lens tekent en houdt alleen wat erbinnen ligt. "
       "Bij het bouwen van de dataset wordt hij per camera opnieuw gemeten, "
       "zodat de twee lenzen van een 360-camera elk hun eigen cirkel krijgen."),
    RU("Находит круг, который рисует объектив, и оставляет только то, что "
       "внутри. При сборке набора он измеряется заново для каждой камеры, так "
       "что два объектива камеры 360 получают каждый свой."),
    TR("Merceğin çizdiği daireyi bulur ve yalnızca içini tutar. Veri kümesi "
       "kurulurken her kamera için yeniden ölçülür, böylece bir 360 kameranın "
       "iki merceği kendi dairesini alır."));

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
    EN("Pulls the circle in a little. The outermost ring of a lens circle is "
       "dark and smeared, and it costs nothing to lose."),
    JA("円を少しだけ内側に寄せます。レンズ円のいちばん外側は暗くにじんでおり、"
       "捨てても損はありません。"),
    ZH_HANS("把圆稍微往里收一点。镜头圆最外的一圈又暗又糊，丢掉不可惜。"),
    ZH_HANT("把圓稍微往裡收一點。鏡頭圓最外的一圈又暗又糊，丟掉不可惜。"),
    KO("원을 조금 안쪽으로 당깁니다. 렌즈 원의 가장 바깥 테는 어둡고 번져 있어 "
       "버려도 아깝지 않습니다."),
    DE("Zieht den Kreis ein Stück nach innen. Der äußerste Ring eines "
       "Objektivkreises ist dunkel und verschmiert und kostet nichts."),
    FR("Resserre un peu le cercle. L'anneau le plus extérieur d'un "
       "cercle-image est sombre et étalé : le perdre ne coûte rien."),
    ES("Mete un poco el círculo. El anillo más externo del círculo del "
       "objetivo es oscuro y borroso, y perderlo no cuesta nada."),
    PT("Puxa o círculo um pouco para dentro. O anel mais externo do círculo da "
       "lente é escuro e borrado, e perdê-lo não custa nada."),
    IT("Stringe un poco il cerchio. L'anello più esterno del cerchio "
       "dell'obiettivo è scuro e sbavato, e perderlo non costa nulla."),
    NL("Haalt de cirkel een stukje naar binnen. De buitenste ring van een "
       "lenscirkel is donker en uitgesmeerd, en kost niets om kwijt te raken."),
    RU("Немного поджимает круг. Самое внешнее кольцо круга изображения тусклое "
       "и смазанное, потерять его не жалко."),
    TR("Daireyi biraz içeri çeker. Mercek dairesinin en dış halkası sönük ve "
       "bulaşıktır, gitmesi bir şey kaybettirmez."));

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
    EN("Add a box or a circle and drag it over what should go -- a watermark, "
       "a timestamp, the operator at the bottom of the frame. Each one can "
       "instead keep what is inside it, which is how you draw a lens circle by "
       "hand."),
    JA("四角か円を足して、消したいものの上にドラッグしてください。透かし、日時"
       "表示、画面の下に写り込んだ撮影者などです。逆に内側を残す設定にもでき、"
       "レンズの円を手で描くときはこちらを使います。"),
    ZH_HANS("加一个方框或圆，拖到要去掉的东西上——水印、时间戳、画面下方的拍摄者。"
            "每个也可以反过来只保留内部，手工画镜头圆时就这么用。"),
    ZH_HANT("加一個方框或圓，拖到要去掉的東西上——浮水印、時間戳、畫面下方的拍攝者。"
            "每個也可以反過來只保留內部，手工畫鏡頭圓時就這麼用。"),
    KO("상자나 원을 더해 없앨 것 위로 끌어다 놓으세요. 워터마크, 날짜 표시, 화면 "
       "아래에 든 촬영자 같은 것들입니다. 반대로 안쪽만 남기게도 할 수 있는데, "
       "렌즈 원을 손으로 그릴 때 그렇게 씁니다."),
    DE("Fügen Sie ein Rechteck oder einen Kreis hinzu und ziehen Sie es über "
       "das, was weg soll -- ein Wasserzeichen, eine Zeitangabe, den Filmenden "
       "am unteren Bildrand. Jede Form kann stattdessen behalten, was in ihr "
       "liegt; so zeichnet man einen Objektivkreis von Hand."),
    FR("Ajoutez un rectangle ou un cercle et faites-le glisser sur ce qui doit "
       "disparaître : un filigrane, un horodatage, l'opérateur en bas de "
       "l'image. Chaque forme peut au contraire garder son intérieur, ce qui "
       "permet de tracer un cercle-image à la main."),
    ES("Añada un rectángulo o un círculo y arrástrelo sobre lo que debe "
       "irse: una marca de agua, una fecha, el operador al pie del "
       "fotograma. Cada forma puede en cambio conservar su interior, que es "
       "como se dibuja a mano un círculo de objetivo."),
    PT("Acrescente um retângulo ou um círculo e arraste-o sobre o que deve "
       "sair: uma marca d'água, uma data, o operador no pé do quadro. Cada "
       "forma pode, ao contrário, manter o seu interior, e é assim que se "
       "desenha um círculo de lente à mão."),
    IT("Aggiunga un rettangolo o un cerchio e lo trascini su ciò che deve "
       "sparire: una filigrana, una data, l'operatore in fondo al fotogramma. "
       "Ogni forma può invece tenere il proprio interno, ed è così che si "
       "disegna a mano un cerchio dell'obiettivo."),
    NL("Voeg een rechthoek of cirkel toe en sleep die over wat weg moet: een "
       "watermerk, een datumstempel, de filmer onderaan het beeld. Elke vorm "
       "kan juist ook houden wat erbinnen ligt; zo teken je een lenscirkel met "
       "de hand."),
    RU("Добавьте прямоугольник или круг и перетащите его на то, что должно "
       "уйти: водяной знак, дату, оператора внизу кадра. Любую фигуру можно "
       "наоборот заставить сохранять своё нутро -- так круг объектива рисуют "
       "вручную."),
    TR("Bir dikdörtgen ya da daire ekleyip gitmesi gerekenin üstüne sürükleyin: "
       "bir filigran, bir tarih damgası, karenin altındaki çekimci. Her biri "
       "tersine içini tutabilir de; mercek dairesi elle böyle çizilir."));

SS_MSG(stencil_add_box,
    EN("Add a box"),
    JA("四角を足す"),
    ZH_HANS("加方框"),
    ZH_HANT("加方框"),
    KO("상자 더하기"),
    DE("Rechteck"),
    FR("Rectangle"),
    ES("Rectángulo"),
    PT("Retângulo"),
    IT("Rettangolo"),
    NL("Rechthoek"),
    RU("Прямоугольник"),
    TR("Dikdörtgen ekle"));

SS_MSG(stencil_add_circle,
    EN("Add a circle"),
    JA("円を足す"),
    ZH_HANS("加圆"),
    ZH_HANT("加圓"),
    KO("원 더하기"),
    DE("Kreis"),
    FR("Cercle"),
    ES("Círculo"),
    PT("Círculo"),
    IT("Cerchio"),
    NL("Cirkel"),
    RU("Круг"),
    TR("Daire ekle"));

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
    EN("Click to swap. The list is applied from top to bottom, each shape "
       "taking its inside away or putting it back, so a box that cuts a band "
       "out and a circle over it leave only the crescent outside the circle."),
    JA("押すと入れ替わります。一覧は上から順に適用され、各図形が内側を取り除く"
       "か戻すかします。帯を切る四角の上に円を置けば、円の外の三日月だけが"
       "残って消えます。"),
    ZH_HANS("点一下切换。列表自上而下依次生效，每个图形把内部去掉或加回来，"
            "所以在切出一条带的方框上再放一个圆，最后只去掉圆外的那道月牙。"),
    ZH_HANT("按一下切換。清單自上而下依次生效，每個圖形把內部去掉或加回來，"
            "所以在切出一條帶的方框上再放一個圓，最後只去掉圓外的那道月牙。"),
    KO("눌러서 바꿉니다. 목록은 위에서 아래로 차례로 적용되며 각 도형이 안쪽을 "
       "없애거나 되돌립니다. 띠를 잘라내는 상자 위에 원을 두면 원 밖의 초승달만 "
       "빠집니다."),
    DE("Klicken schaltet um. Die Liste wirkt von oben nach unten, jede Form "
       "nimmt ihr Inneres weg oder gibt es zurück: ein Rechteck, das ein Band "
       "herausschneidet, und ein Kreis darüber lassen nur die Sichel außerhalb "
       "des Kreises verschwinden."),
    FR("Cliquez pour permuter. La liste s'applique de haut en bas, chaque "
       "forme retirant son intérieur ou le rendant : un rectangle qui découpe "
       "une bande, surmonté d'un cercle, ne fait disparaître que le croissant "
       "hors du cercle."),
    ES("Pulse para cambiar. La lista se aplica de arriba abajo, cada forma "
       "quitando su interior o devolviéndolo: un rectángulo que corta una "
       "franja con un círculo encima solo elimina la media luna de fuera del "
       "círculo."),
    PT("Clique para trocar. A lista é aplicada de cima para baixo, cada forma "
       "tirando o seu interior ou devolvendo-o: um retângulo que corta uma "
       "faixa com um círculo por cima só faz sumir o crescente fora do "
       "círculo."),
    IT("Clicchi per invertire. L'elenco si applica dall'alto in basso, ogni "
       "forma togliendo il proprio interno o restituendolo: un rettangolo che "
       "ritaglia una fascia, con un cerchio sopra, fa sparire solo la falce "
       "fuori dal cerchio."),
    NL("Klik om om te wisselen. De lijst werkt van boven naar beneden: elke "
       "vorm haalt zijn binnenkant weg of geeft die terug, dus een rechthoek "
       "die een band wegsnijdt met een cirkel erover laat alleen de sikkel "
       "buiten de cirkel verdwijnen."),
    RU("Нажмите, чтобы поменять. Список применяется сверху вниз: каждая фигура "
       "либо убирает своё нутро, либо возвращает его. Прямоугольник, "
       "вырезающий полосу, и круг поверх него убирают только серп за кругом."),
    TR("Değiştirmek için tıklayın. Liste yukarıdan aşağıya uygulanır, her "
       "biçim içini alır ya da geri verir: bir şerit kesen dikdörtgenin "
       "üstüne bir daire koyunca yalnızca dairenin dışındaki hilal gider."));

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
    EN("The black edge of a fisheye, a watermark, the pole the camera is on -- "
       "whatever sits in the same place in every shot. Set it up in \"Try the "
       "mask\"; it needs no model and no download."),
    JA("魚眼の黒枠、透かし、カメラを付けた棒など、どのカットでも同じ位置に"
       "あるものです。「マスクを試す」で設定します。モデルもダウンロードも"
       "要りません。"),
    ZH_HANS("鱼眼的黑边、水印、举着相机的杆——凡是每张里都在同一位置的东西。"
            "在“试一下蒙版”里设置；不需要模型，也不用下载。"),
    ZH_HANT("魚眼的黑邊、浮水印、舉著相機的桿——凡是每張裡都在同一位置的東西。"
            "在「試一下遮罩」裡設定；不需要模型，也不用下載。"),
    KO("어안의 검은 가장자리, 워터마크, 카메라를 단 장대처럼 어느 장면에서나 "
       "같은 자리에 있는 것들입니다. '마스크를 시험해 보기'에서 설정하며, 모델도 "
       "내려받기도 필요 없습니다."),
    DE("Der schwarze Rand eines Fisheye, ein Wasserzeichen, die Stange, an der "
       "die Kamera hängt -- was auch immer in jeder Aufnahme an derselben "
       "Stelle sitzt. Einzustellen unter „Maske ausprobieren“; ohne Modell und "
       "ohne Download."),
    FR("Le bord noir d'un fisheye, un filigrane, la perche qui porte la caméra "
       "-- tout ce qui occupe la même place sur chaque prise. Cela se règle "
       "dans « Essayer le masque » ; sans modèle ni téléchargement."),
    ES("El borde negro de un ojo de pez, una marca de agua, el palo que "
       "sostiene la cámara: lo que ocupe el mismo sitio en cada toma. Se "
       "ajusta en «Probar la máscara»; no necesita modelo ni descarga."),
    PT("A borda preta de um olho-de-peixe, uma marca d'água, o bastão que "
       "segura a câmera -- o que ficar no mesmo lugar em cada tomada. "
       "Ajusta-se em \"Testar a máscara\"; não precisa de modelo nem de "
       "download."),
    IT("Il bordo nero di un fisheye, una filigrana, l'asta che regge la "
       "fotocamera: tutto ciò che sta nello stesso punto in ogni ripresa. Si "
       "imposta in \"Prova la maschera\"; non serve alcun modello né alcun "
       "download."),
    NL("De zwarte rand van een fisheye, een watermerk, de stok waar de camera "
       "aan hangt -- alles wat in elke opname op dezelfde plek zit. In te "
       "stellen bij \"Masker uitproberen\"; zonder model en zonder download."),
    RU("Чёрный край фишая, водяной знак, палка, на которой камера, -- всё, что "
       "стоит на одном месте в каждом кадре. Настраивается в «Проверить "
       "маску»; ни модели, ни загрузки не нужно."),
    TR("Balıkgözünün siyah kenarı, bir filigran, kameranın takılı olduğu çubuk "
       "-- her çekimde aynı yerde duran ne varsa. \"Maskeyi dene\" içinden "
       "ayarlanır; model de indirme de gerekmez."));

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
    EN("A few frames from across the capture. Check a prompt on more than one "
       "before running -- and, for a video, click here to correct an object "
       "part way through: what you draw is used from this frame on."),
    JA("撮影全体から取った数枚のフレームです。実行する前に複数のフレームで"
       "プロンプトを確かめてください。動画では、途中で物体を修正したいときに"
       "ここでクリックします。描いた内容はこのフレーム以降に使われます。"),
    ZH_HANS("从整段拍摄中取出的几帧。运行前请在不止一帧上检查提示词——"
            "对视频而言，还可以在这里点选来修正中途的物体：你画的内容会从这一帧"
            "起生效。"),
    ZH_HANT("從整段拍攝中取出的幾格。執行前請在不止一格上檢查提示詞——"
            "對影片而言，還可以在這裡點選來修正中途的物體：你畫的內容會從這一格"
            "起生效。"),
    KO("촬영 전체에서 뽑은 몇 장의 프레임입니다. 실행하기 전에 두 장 이상에서 "
       "프롬프트를 확인하세요. 동영상이라면 중간에 물체를 바로잡을 때 여기서 "
       "클릭하면 됩니다. 표시한 내용은 이 프레임부터 적용됩니다."),
    DE("Ein paar Bilder aus der ganzen Aufnahme. Prüfen Sie eine Eingabe an "
       "mehr als einem, bevor Sie starten -- und bei einem Video hier klicken, "
       "um ein Objekt mittendrin zu korrigieren: was Sie einzeichnen, gilt ab "
       "diesem Bild."),
    FR("Quelques images prises tout au long de la prise. Vérifiez une invite "
       "sur plus d'une avant de lancer -- et, pour une vidéo, cliquez ici pour "
       "corriger un objet en cours de route : ce que vous tracez s'applique à "
       "partir de cette image."),
    ES("Unos cuantos fotogramas de toda la captura. Compruebe una indicación "
       "en más de uno antes de lanzar; y, en un vídeo, marque aquí para "
       "corregir un objeto a mitad de camino: lo que dibuje se aplica desde "
       "este fotograma."),
    PT("Alguns quadros de toda a captura. Confira um comando em mais de um "
       "antes de rodar -- e, num vídeo, clique aqui para corrigir um objeto no "
       "meio do caminho: o que você marcar vale a partir deste quadro."),
    IT("Alcuni fotogrammi presi da tutta la ripresa. Verifichi un testo su più "
       "d'uno prima di avviare; e, in un video, clicchi qui per correggere un "
       "oggetto a metà strada: quello che traccia vale da questo fotogramma in "
       "poi."),
    NL("Een paar beelden uit de hele opname. Controleer een prompt op meer dan "
       "één voordat je start -- en klik bij een video hier om halverwege een "
       "object te corrigeren: wat je aanwijst geldt vanaf dit beeld."),
    RU("Несколько кадров со всей съёмки. Перед запуском проверьте запрос "
       "больше чем на одном; а в видео щёлкните здесь, чтобы поправить объект "
       "по ходу: то, что вы отметите, действует начиная с этого кадра."),
    TR("Çekimin tamamından alınmış birkaç kare. Çalıştırmadan önce istemi "
       "birden çok karede sınayın -- videoda ise yolun ortasında bir nesneyi "
       "düzeltmek için buraya tıklayın: çizdikleriniz bu kareden itibaren "
       "geçerlidir."));

SS_MSG(preview_try_it,
    EN("Try it"),        JA("試す"),          ZH_HANS("试一下"),   ZH_HANT("試一下"),
    KO("해 보기"),        DE("Ausprobieren"), FR("Essayer"),      ES("Probar"),
    PT("Testar"),        IT("Prova"),        NL("Uitproberen"),  RU("Проверить"),
    TR("Dene"));

// Shown in the preview when this machine has no in-process video decoding, so
// the frame has to come from ffmpeg, and ffmpeg is not there either.
// {0} the command that was looked for.
SS_MSG(preview_needs_ffmpeg,
    EN("This video can only be read here with ffmpeg, which was not found "
       "('{0}'). Install it, or set its path under Tool locations."),
    JA("この動画をここで読むには ffmpeg が必要ですが、見つかりませんでした"
       "（{0}）。ffmpeg を入れるか、「外部ツールの場所」でパスを指定して"
       "ください。"),
    ZH_HANS("这里只能用 ffmpeg 读取该视频，但没有找到它（{0}）。"
            "请安装 ffmpeg，或在“工具位置”中指定它的路径。"),
    ZH_HANT("這裡只能用 ffmpeg 讀取該影片，但沒有找到它（{0}）。"
            "請安裝 ffmpeg，或在「工具位置」中指定它的路徑。"),
    KO("이 동영상은 여기서 ffmpeg으로만 읽을 수 있는데 ffmpeg을 찾지 "
       "못했습니다({0}). ffmpeg을 설치하거나 '도구 위치'에서 경로를 "
       "지정하세요."),
    DE("Dieses Video lässt sich hier nur mit ffmpeg lesen, und ffmpeg wurde "
       "nicht gefunden ('{0}'). Installieren Sie es, oder tragen Sie seinen "
       "Pfad unter \"Speicherorte der Werkzeuge\" ein."),
    FR("Cette vidéo ne peut être lue ici qu'avec ffmpeg, qui est introuvable "
       "(« {0} »). Installez-le, ou indiquez son chemin sous « Emplacement des "
       "outils »."),
    ES("Aquí este vídeo solo se puede leer con ffmpeg, que no se ha encontrado "
       "(«{0}»). Instálelo, o indique su ruta en «Ubicación de las "
       "herramientas»."),
    PT("Aqui este vídeo só pode ser lido com o ffmpeg, que não foi encontrado "
       "(\"{0}\"). Instale-o, ou informe o caminho dele em \"Local das "
       "ferramentas\"."),
    IT("Qui questo video si può leggere solo con ffmpeg, che non è stato "
       "trovato (\"{0}\"). Lo installi, oppure indichi il suo percorso in "
       "\"Percorsi degli strumenti\"."),
    NL("Deze video kan hier alleen met ffmpeg worden gelezen, en ffmpeg is "
       "niet gevonden ('{0}'). Installeer het, of geef het pad op onder "
       "\"Locatie van hulpprogramma's\"."),
    RU("Здесь это видео читается только через ffmpeg, а он не найден "
       "(«{0}»). Установите его или укажите путь в разделе «Расположение "
       "инструментов»."),
    TR("Bu video burada yalnızca ffmpeg ile okunabilir, ffmpeg ise bulunamadı "
       "('{0}'). Kurun ya da yolunu \"Araç konumları\" altında belirtin."));

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
    EN("What the input is, which sets the pairing strategy and how forgiving "
       "the mapper is. Set from the input type when you picked it."),
    JA("入力が何であるかです。ペアの作り方と、マッパーがどれだけ寛容かが"
       "これで決まります。入力を選んだ時点で種類から設定されます。"),
    ZH_HANS("输入是什么，它决定了配对策略以及建图器有多宽容。选择输入时会按"
            "输入类型自动设定。"),
    ZH_HANT("輸入是什麼，它決定了配對策略以及建圖器有多寬容。選擇輸入時會按"
            "輸入類型自動設定。"),
    KO("입력이 무엇인지입니다. 짝짓기 전략과 매퍼가 얼마나 관대한지가 여기서 "
       "정해집니다. 입력을 고를 때 그 종류에서 자동으로 설정됩니다."),
    DE("Was die Eingabe ist; davon hängen die Paarungsstrategie und die "
       "Nachsicht des Mappers ab. Wird beim Auswählen aus der Eingabeart "
       "gesetzt."),
    FR("Ce qu'est l'entrée, ce qui fixe la stratégie d'appariement et la "
       "tolérance du mapper. Défini d'après le type d'entrée au moment du "
       "choix."),
    ES("Qué es la entrada, lo que fija la estrategia de emparejamiento y la "
       "tolerancia del mapeador. Se establece según el tipo de entrada al "
       "elegirla."),
    PT("O que é a entrada, o que define a estratégia de pareamento e a "
       "tolerância do mapeador. Definido pelo tipo de entrada ao escolhê-la."),
    IT("Che cosa è l'ingresso: da questo dipendono la strategia di "
       "accoppiamento e quanto è tollerante il mapper. Impostato dal tipo di "
       "ingresso al momento della scelta."),
    NL("Wat de invoer is; dat bepaalt de koppelstrategie en hoe mild de mapper "
       "is. Wordt bij het kiezen uit het invoertype ingesteld."),
    RU("Чем является вход; от этого зависят стратегия составления пар и "
       "снисходительность маппера. Задаётся по типу входа при его выборе."),
    TR("Girdinin ne olduğu; eşleştirme stratejisini ve haritalayıcının ne "
       "kadar hoşgörülü olduğunu bu belirler. Girdiyi seçerken türünden "
       "ayarlanır."));

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

SS_MSG(features_help,
    EN("Which detector and descriptor. SIFT is the classic one and needs "
       "nothing downloaded. The ALIKED options are a learned frontend: they "
       "fetch a small checkpoint (3-4 MB) on first use, find fewer but "
       "better-localized keypoints, and match markedly more image pairs on "
       "hard captures. N32 samples more positions per descriptor -- slower, "
       "slightly stronger for datasets without strong rotation."),
    JA("どの検出器と記述子を使うかです。SIFT は古典的な選択で、何もダウンロード"
       "しません。ALIKED は学習型のフロントエンドで、初回に小さな"
       "チェックポイント（3〜4 MB）を取得します。キーポイントの数は少ない"
       "ものの位置が正確で、難しい撮影ではマッチする画像ペアが目に見えて"
       "増えます。N32 は記述子あたりのサンプル位置が多く、遅いぶん、回転の大きくないデータセットでは少し強力です。"),
    ZH_HANS("使用哪种检测器和描述子。SIFT 是经典选择，什么都不用下载。"
            "ALIKED 是学习型前端：首次使用会取一个小的检查点（3-4 MB），"
            "找到的关键点更少但定位更准，在困难拍摄上匹配上的图像对明显更多。"
            "N32 每个描述子采样更多位置——更慢，在没有大幅旋转的数据集上略强。"),
    ZH_HANT("使用哪種偵測器和描述子。SIFT 是經典選擇，什麼都不用下載。"
            "ALIKED 是學習型前端：首次使用會取一個小的檢查點（3-4 MB），"
            "找到的關鍵點更少但定位更準，在困難拍攝上配對上的影像對明顯更多。"
            "N32 每個描述子取樣更多位置——更慢，在沒有大幅旋轉的資料集上略強。"),
    KO("어떤 검출기와 기술자를 쓸지입니다. SIFT는 고전적인 선택이며 내려받을 것이 "
       "없습니다. ALIKED는 학습형 프런트엔드로, 처음 쓸 때 작은 체크포인트"
       "(3~4 MB)를 받아옵니다. 키포인트 수는 적지만 위치가 더 정확하고, 어려운 "
       "촬영에서 매칭되는 이미지 쌍이 눈에 띄게 늘어납니다. N32는 기술자당 더 "
       "많은 위치를 표본화합니다 — 더 느리지만, 회전이 크지 않은 데이터셋에서는 "
       "조금 더 강합니다."),
    DE("Welcher Detektor und Deskriptor. SIFT ist der klassische und braucht "
       "keinen Download. Die ALIKED-Varianten sind ein gelerntes Frontend: sie "
       "holen beim ersten Gebrauch einen kleinen Prüfpunkt (3-4 MB), finden "
       "weniger, aber genauer verortete Schlüsselpunkte und ordnen bei "
       "schwierigen Aufnahmen deutlich mehr Bildpaare zu. N32 tastet je "
       "Deskriptor mehr Stellen ab -- langsamer, bei Datensätzen ohne starke "
       "Drehung etwas stärker."),
    FR("Quel détecteur et quel descripteur. SIFT est le classique et ne "
       "demande aucun téléchargement. Les options ALIKED forment un frontal "
       "appris : elles récupèrent un petit point de sauvegarde (3-4 Mo) au "
       "premier usage, trouvent moins de points mais mieux localisés, et "
       "apparient nettement plus de paires sur les prises difficiles. N32 "
       "échantillonne plus de positions par descripteur -- plus lent, un peu "
       "plus solide sur les jeux de données sans forte rotation."),
    ES("Qué detector y descriptor. SIFT es el clásico y no necesita descargar "
       "nada. Las opciones ALIKED son un frontal aprendido: bajan un pequeño "
       "punto de control (3-4 MB) la primera vez, encuentran menos puntos "
       "pero mejor localizados, y emparejan bastantes más pares de imágenes "
       "en capturas difíciles. N32 muestrea más posiciones por descriptor: "
       "más lento, algo más robusto en conjuntos sin rotaciones fuertes."),
    PT("Qual detector e descritor. O SIFT é o clássico e não precisa baixar "
       "nada. As opções ALIKED são um front-end aprendido: buscam um pequeno "
       "ponto de verificação (3-4 MB) no primeiro uso, encontram menos pontos "
       "mas melhor localizados, e casam bem mais pares de imagens em capturas "
       "difíceis. O N32 amostra mais posições por descritor -- mais lento, um "
       "pouco mais forte em conjuntos sem rotação acentuada."),
    IT("Quale rilevatore e descrittore. SIFT è quello classico e non richiede "
       "scaricamenti. Le opzioni ALIKED sono un frontend appreso: al primo "
       "uso prelevano un piccolo punto di controllo (3-4 MB), trovano meno "
       "punti chiave ma meglio localizzati e mettono in corrispondenza molte "
       "più coppie nelle riprese difficili. N32 campiona più posizioni per "
       "descrittore: più lento, un po' più robusto sui set di dati senza "
       "forti rotazioni."),
    NL("Welke detector en descriptor. SIFT is de klassieke en hoeft niets te "
       "downloaden. De ALIKED-opties zijn een geleerde frontend: ze halen bij "
       "eerste gebruik een klein controlepunt (3-4 MB) op, vinden minder maar "
       "beter geplaatste sleutelpunten en matchen bij lastige opnamen "
       "merkbaar meer beeldparen. N32 bemonstert meer posities per descriptor "
       "-- trager, bij datasets zonder sterke rotatie iets sterker."),
    RU("Какой детектор и дескриптор. SIFT — классический, ничего скачивать не "
       "нужно. Варианты ALIKED — обученный фронтенд: при первом использовании "
       "они получают небольшую контрольную точку (3-4 МБ), находят меньше "
       "ключевых точек, но точнее локализованных, и на сложных съёмках "
       "сопоставляют заметно больше пар. N32 берёт больше позиций на "
       "дескриптор — медленнее, на наборах без сильного поворота чуть "
       "надёжнее."),
    TR("Hangi bulucu ve betimleyici. SIFT klasik olanıdır ve indirme "
       "gerektirmez. ALIKED seçenekleri öğrenilmiş bir ön uçtur: ilk "
       "kullanımda küçük bir denetim noktası (3-4 MB) indirir, daha az ama "
       "daha iyi konumlanmış anahtar nokta bulur ve zor çekimlerde belirgin "
       "biçimde daha çok görüntü çiftini eşleştirir. N32 betimleyici başına "
       "daha çok konum örnekler -- daha yavaş, güçlü dönme içermeyen veri "
       "kümelerinde biraz daha güçlü."));

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

SS_MSG(mapper_schedule_help,
    EN("How the scene is built. Flat grows one reconstruction image by image, "
       "and is the default for any capture. Bottom-up cuts the view graph "
       "into small groups, reconstructs them independently and merges "
       "upwards."),
    JA("シーンをどう組み立てるかです。フラットは1つの再構成を画像ごとに"
       "育てていく方式で、どの撮影でも既定です。ボトムアップはビューグラフを"
       "小さなグループに切り、それぞれを独立に再構成してから上へ統合します。"),
    ZH_HANS("场景如何构建。扁平方式逐张图像地扩展同一个重建，是任何拍摄的默认选择。"
            "自下而上把视图图切成小组，各自独立重建后再向上合并。"),
    ZH_HANT("場景如何建構。扁平方式逐張影像地擴展同一個重建，是任何拍攝的預設選擇。"
            "由下而上把視圖圖切成小組，各自獨立重建後再向上合併。"),
    KO("장면을 어떻게 만들지입니다. 평면 방식은 재구성 하나를 이미지마다 키워 "
       "가며, 어떤 촬영에서든 기본값입니다. 상향식은 뷰 그래프를 작은 묶음으로 "
       "자르고 각각 독립적으로 재구성한 뒤 위로 병합합니다."),
    DE("Wie die Szene aufgebaut wird. Flach lässt eine Rekonstruktion Bild für "
       "Bild wachsen und ist die Vorgabe für jede Aufnahme. Von unten zerlegt "
       "den Sichtgraphen in kleine Gruppen, rekonstruiert sie unabhängig und "
       "führt sie nach oben zusammen."),
    FR("Comment la scène est construite. Le mode plat fait croître une seule "
       "reconstruction image par image, et c'est la valeur par défaut pour "
       "toute prise. Le mode ascendant découpe le graphe de vues en petits "
       "groupes, les reconstruit indépendamment et fusionne vers le haut."),
    ES("Cómo se construye la escena. El modo plano hace crecer una sola "
       "reconstrucción imagen a imagen, y es lo predeterminado en cualquier "
       "captura. El ascendente corta el grafo de vistas en grupos pequeños, "
       "los reconstruye por separado y los fusiona hacia arriba."),
    PT("Como a cena é construída. O modo plano faz uma única reconstrução "
       "crescer imagem a imagem, e é o padrão para qualquer captura. O "
       "ascendente corta o grafo de vistas em grupos pequenos, reconstrói "
       "cada um em separado e mescla para cima."),
    IT("Come viene costruita la scena. Il modo piatto fa crescere una sola "
       "ricostruzione immagine dopo immagine ed è l'impostazione predefinita "
       "per qualsiasi ripresa. Quello dal basso taglia il grafo delle viste in "
       "piccoli gruppi, li ricostruisce in modo indipendente e li unisce verso "
       "l'alto."),
    NL("Hoe de scène wordt opgebouwd. Vlak laat één reconstructie beeld voor "
       "beeld groeien en is de standaard voor elke opname. Bottom-up knipt de "
       "beeldgraaf in kleine groepen, reconstrueert die apart en voegt ze naar "
       "boven samen."),
    RU("Как строится сцена. Плоская схема наращивает одну реконструкцию снимок "
       "за снимком и подходит любой съёмке. Схема снизу вверх режет граф видов "
       "на небольшие группы, восстанавливает их независимо и объединяет вверх."),
    TR("Sahnenin nasıl kurulacağı. Düz plan tek bir yeniden oluşturmayı "
       "görüntü görüntü büyütür ve her çekim için varsayılandır. Aşağıdan "
       "yukarı plan görünüm çizgesini küçük öbeklere böler, her birini ayrı "
       "yeniden oluşturur ve yukarı doğru birleştirir."));

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
    EN("Starting guess for the focal length, in pixels of the source image. 0 "
       "reads EXIF and falls back to a guess from the image size. Worth "
       "setting for a fisheye, where a bad initial guess can stop the "
       "reconstruction from starting at all."),
    JA("焦点距離の初期推定値を、元画像のピクセル単位で指定します。0 なら EXIF を"
       "読み、なければ画像サイズから推定します。魚眼では初期値が悪いと再構成が"
       "そもそも始まらないことがあるので、指定する価値があります。"),
    ZH_HANS("焦距的初始猜测值，以源图像的像素为单位。填 0 会读 EXIF，读不到就按"
            "图像尺寸估计。鱼眼值得设一下：初值不好可能让重建根本起不来。"),
    ZH_HANT("焦距的初始猜測值，以來源影像的像素為單位。填 0 會讀 EXIF，讀不到就按"
            "影像尺寸估計。魚眼值得設一下：初值不好可能讓重建根本起不來。"),
    KO("초점거리의 초기 추정값을 원본 이미지의 픽셀 단위로 지정합니다. 0이면 "
       "EXIF를 읽고, 없으면 이미지 크기로 추정합니다. 어안에서는 초기 추정이 "
       "나쁘면 재구성이 아예 시작되지 않을 수 있어 설정할 값어치가 있습니다."),
    DE("Anfangsschätzung der Brennweite, in Pixeln des Ausgangsbildes. 0 liest "
       "EXIF und fällt auf eine Schätzung aus der Bildgröße zurück. Bei einem "
       "Fischauge lohnt es sich, denn eine schlechte Anfangsschätzung kann die "
       "Rekonstruktion ganz verhindern."),
    FR("Estimation de départ de la focale, en pixels de l'image source. 0 lit "
       "l'EXIF et retombe sur une estimation d'après la taille d'image. Utile "
       "pour un fisheye, où une mauvaise valeur de départ peut empêcher la "
       "reconstruction de démarrer."),
    ES("Estimación inicial de la focal, en píxeles de la imagen de origen. 0 "
       "lee el EXIF y recurre a una estimación por el tamaño de imagen. Vale "
       "la pena fijarla en un ojo de pez, donde un mal valor inicial puede "
       "impedir que la reconstrucción arranque."),
    PT("Palpite inicial da distância focal, em pixels da imagem de origem. 0 "
       "lê o EXIF e recorre a uma estimativa pelo tamanho da imagem. Vale "
       "definir num olho de peixe, onde um palpite inicial ruim pode impedir a "
       "reconstrução de começar."),
    IT("Stima iniziale della focale, in pixel dell'immagine di partenza. 0 "
       "legge l'EXIF e ripiega su una stima dalla dimensione dell'immagine. "
       "Conviene impostarla per un fisheye, dove una cattiva stima iniziale "
       "può impedire del tutto l'avvio della ricostruzione."),
    NL("Beginschatting van de brandpuntsafstand, in pixels van het "
       "bronbeeld. 0 leest EXIF en valt terug op een schatting uit de "
       "beeldgrootte. De moeite waard bij een fisheye, waar een slechte "
       "beginschatting de reconstructie helemaal kan blokkeren."),
    RU("Начальная оценка фокусного расстояния в пикселях исходного "
       "изображения. 0 читает EXIF, а при его отсутствии оценивает по размеру "
       "изображения. Для фишая задать стоит: плохая начальная оценка может "
       "вовсе не дать реконструкции начаться."),
    TR("Odak uzaklığı için başlangıç tahmini, kaynak görüntünün pikselleri "
       "cinsinden. 0, EXIF'i okur ve bulamazsa görüntü boyutundan tahmin "
       "eder. Balıkgözünde ayarlamaya değer: kötü bir başlangıç tahmini "
       "yeniden oluşturmanın hiç başlamamasına yol açabilir."));

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
    EN("Where the distortion coefficients start, in the order the chosen lens "
       "model lists them (OpenCV: k1,k2,p1,p2). Empty starts at zero, which is "
       "right unless the lens is already calibrated. Combine with \"Distortion "
       "refinement: never\" to hold a known calibration exactly."),
    JA("歪み係数の開始値を、選んだレンズモデルの並び順で指定します（OpenCV なら "
       "k1,k2,p1,p2）。空欄なら 0 から始まり、レンズが校正済みでない限りそれが"
       "適切です。「歪みの最適化: しない」と組み合わせると、既知の校正値をそのまま"
       "固定できます。"),
    ZH_HANS("畸变系数的起始值，按所选镜头模型列出的顺序填写（OpenCV 为 "
            "k1,k2,p1,p2）。留空则从 0 开始；除非镜头已经标定，否则留空就是对的。"
            "与「畸变优化: 不优化」搭配，可原样固定一组已知标定值。"),
    ZH_HANT("畸變係數的起始值，按所選鏡頭模型列出的順序填寫（OpenCV 為 "
            "k1,k2,p1,p2）。留空則從 0 開始；除非鏡頭已經標定，否則留空就是對的。"
            "與「畸變最佳化: 不最佳化」搭配，可原樣固定一組已知標定值。"),
    KO("왜곡 계수의 시작값을 선택한 렌즈 모델이 나열하는 순서로 적습니다"
       "(OpenCV 는 k1,k2,p1,p2). 비우면 0 에서 시작하며, 렌즈가 이미 보정되어 "
       "있지 않다면 그것이 맞습니다. ‘왜곡 보정 최적화: 하지 않음’과 함께 쓰면 "
       "알려진 보정값을 그대로 고정할 수 있습니다."),
    DE("Wo die Verzeichnungskoeffizienten beginnen, in der Reihenfolge des "
       "gewählten Objektivmodells (OpenCV: k1,k2,p1,p2). Leer beginnt bei null, "
       "was richtig ist, solange das Objektiv nicht bereits kalibriert ist. "
       "Zusammen mit „Verzeichnung mitoptimieren: nie“ hält es eine bekannte "
       "Kalibrierung exakt fest."),
    FR("Où démarrent les coefficients de distorsion, dans l'ordre où le modèle "
       "d'objectif choisi les énumère (OpenCV : k1,k2,p1,p2). Vide, ils partent "
       "de zéro, ce qui convient sauf si l'objectif est déjà calibré. À combiner "
       "avec « Affinage de la distorsion : jamais » pour figer exactement un "
       "étalonnage connu."),
    ES("Dónde empiezan los coeficientes de distorsión, en el orden en que los "
       "enumera el modelo de óptica elegido (OpenCV: k1,k2,p1,p2). Vacío empieza "
       "en cero, que es lo correcto salvo que la óptica ya esté calibrada. "
       "Combínalo con «Refinado de la distorsión: nunca» para fijar exactamente "
       "una calibración conocida."),
    PT("Onde os coeficientes de distorção começam, na ordem em que o modelo de "
       "lente escolhido os lista (OpenCV: k1,k2,p1,p2). Vazio começa do zero, o "
       "que é o certo a menos que a lente já esteja calibrada. Combine com "
       "«Refino da distorção: nunca» para fixar exatamente uma calibração "
       "conhecida."),
    IT("Da dove partono i coefficienti di distorsione, nell'ordine in cui il "
       "modello di obiettivo scelto li elenca (OpenCV: k1,k2,p1,p2). Vuoto parte "
       "da zero, che è giusto a meno che l'obiettivo non sia già calibrato. "
       "Insieme a «Affinamento della distorsione: mai» blocca esattamente una "
       "calibrazione nota."),
    NL("Waar de vertekeningscoëfficiënten beginnen, in de volgorde die het "
       "gekozen lensmodel aanhoudt (OpenCV: k1,k2,p1,p2). Leeg begint bij nul, "
       "wat klopt tenzij de lens al gekalibreerd is. Samen met "
       "\"Vertekening bijstellen: nooit\" houdt het een bekende kalibratie "
       "precies vast."),
    RU("С чего начинаются коэффициенты дисторсии, в том порядке, в каком их "
       "перечисляет выбранная модель объектива (OpenCV: k1,k2,p1,p2). Пустое поле "
       "начинает с нуля, и это верно, если объектив ещё не откалиброван. Вместе с "
       "«Уточнение дисторсии: никогда» это точно закрепляет известную калибровку."),
    TR("Bozulma katsayılarının nereden başlayacağı, seçilen objektif modelinin "
       "sıraladığı düzende (OpenCV: k1,k2,p1,p2). Boş bırakılırsa sıfırdan "
       "başlar; objektif zaten kalibre edilmiş değilse doğrusu budur. "
       "\"Bozulmanın iyileştirilmesi: asla\" ile birlikte bilinen bir "
       "kalibrasyonu tam olarak sabitler."));

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
    EN("When the distortion coefficients are fitted. \"Final pass only\" holds "
       "them at their initial value while the model is built and recovers them "
       "in one bundle adjustment at the end, which is steadier on a lens with "
       "little distortion; \"Never\" keeps a calibration you already trust."),
    JA("歪み係数をいつ当てはめるかです。「最後の仕上げだけ」ならモデルを組む間は"
       "初期値のまま固定し、最後の 1 回のバンドル調整で求めます。歪みの小さい"
       "レンズではこちらが安定します。「しない」は、すでに信頼している校正値を"
       "そのまま保ちます。"),
    ZH_HANS("何时拟合畸变系数。选「只在最后一遍」时，建图期间保持初值不动，最后用"
            "一次平差求出；对畸变较小的镜头更稳。「不优化」则保留你已经信任的标定值。"),
    ZH_HANT("何時擬合畸變係數。選「只在最後一遍」時，建圖期間保持初值不動，最後用"
            "一次平差求出；對畸變較小的鏡頭更穩。「不最佳化」則保留你已經信任的標定值。"),
    KO("왜곡 계수를 언제 맞출지입니다. ‘마지막 단계에서만’은 모델을 만드는 동안 "
       "초기값에 고정해 두고 마지막 번들 조정 한 번으로 구합니다. 왜곡이 작은 "
       "렌즈에서 더 안정적입니다. ‘하지 않음’은 이미 신뢰하는 보정값을 그대로 "
       "지킵니다."),
    DE("Wann die Verzeichnungskoeffizienten angepasst werden. „Nur im letzten "
       "Durchgang“ hält sie beim Aufbau des Modells auf ihrem Anfangswert und "
       "bestimmt sie in einem Bündelausgleich am Schluss, was bei einem "
       "Objektiv mit wenig Verzeichnung ruhiger läuft; „Nie“ behält eine "
       "Kalibrierung, der Sie bereits trauen."),
    FR("Quand les coefficients de distorsion sont ajustés. « Seulement à la "
       "passe finale » les tient à leur valeur de départ pendant la "
       "construction du modèle et les retrouve en un ajustement à la fin, ce "
       "qui est plus stable sur un objectif peu distordu ; « Jamais » conserve "
       "un étalonnage auquel vous vous fiez déjà."),
    ES("Cuándo se ajustan los coeficientes de distorsión. «Solo en la pasada "
       "final» los mantiene en su valor inicial mientras se construye el "
       "modelo y los recupera en un ajuste al final, lo que es más estable en "
       "una óptica poco distorsionada; «Nunca» conserva una calibración en la "
       "que ya confías."),
    PT("Quando os coeficientes de distorção são ajustados. «Só na passagem "
       "final» mantém-nos no valor inicial enquanto o modelo é construído e "
       "recupera-os num ajuste no fim, o que é mais estável numa lente com "
       "pouca distorção; «Nunca» conserva uma calibração em que você já "
       "confia."),
    IT("Quando i coefficienti di distorsione vengono adattati. «Solo nella "
       "passata finale» li tiene al valore iniziale mentre il modello viene "
       "costruito e li ricava in un bundle adjustment alla fine, il che è più "
       "stabile su un obiettivo poco distorto; «Mai» conserva una calibrazione "
       "di cui ci si fida già."),
    NL("Wanneer de vertekeningscoëfficiënten worden gepast. \"Alleen in de "
       "laatste ronde\" houdt ze tijdens het opbouwen van het model op hun "
       "beginwaarde en bepaalt ze in één bundelaanpassing aan het eind, wat "
       "rustiger verloopt bij een lens met weinig vertekening; \"Nooit\" "
       "behoudt een kalibratie die u al vertrouwt."),
    RU("Когда подбираются коэффициенты дисторсии. «Только на финальном проходе» "
       "держит их на начальном значении, пока строится модель, и находит их "
       "одним уравниванием в конце — на объективе с малой дисторсией это "
       "спокойнее; «Никогда» сохраняет калибровку, которой вы уже доверяете."),
    TR("Bozulma katsayılarının ne zaman oturtulacağı. \"Yalnızca son geçişte\", "
       "model kurulurken onları başlangıç değerinde tutar ve sonda tek bir "
       "demet dengelemesiyle bulur; bozulması az bir objektifte bu daha "
       "kararlıdır. \"Asla\" ise hâlihazırda güvendiğiniz bir kalibrasyonu "
       "korur."));

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
    EN("Reconstruct with the cameras shared as usual, then run one last bundle "
       "adjustment in which every image carries its own focal length and "
       "distortion. It follows a lens that drifted -- a zoom that crept, a "
       "focus that breathed -- at the cost of a much larger solve."),
    JA("カメラの共有は通常どおりで再構成し、最後に画像ごとの焦点距離と歪みを"
       "持たせたバンドル調整を 1 回だけ行います。ズームが動いた、ピント送りで"
       "画角が変わったといったレンズの変化に追従できますが、解く問題は大きく"
       "なります。"),
    ZH_HANS("按常规共享相机完成重建，最后再做一次平差，其中每张图像各自带有焦距和"
            "畸变。这样能跟上镜头的漂移——变焦发生漂移、对焦引起的取景变化——代价是"
            "求解规模大得多。"),
    ZH_HANT("按常規共享相機完成重建，最後再做一次平差，其中每張影像各自帶有焦距和"
            "畸變。這樣能跟上鏡頭的漂移——變焦發生漂移、對焦引起的取景變化——代價是"
            "求解規模大得多。"),
    KO("카메라 공유는 평소대로 두고 재구성한 뒤, 마지막에 이미지마다 자체 초점거리와 "
       "왜곡을 갖는 번들 조정을 한 번 돌립니다. 줌이 밀리거나 초점에 따라 화각이 "
       "변하는 렌즈의 흐트러짐을 따라갈 수 있지만, 풀어야 할 문제가 훨씬 커집니다."),
    DE("Wie gewohnt mit geteilten Kameras rekonstruieren und danach einen "
       "letzten Bündelausgleich rechnen, in dem jedes Bild eigene Brennweite "
       "und Verzeichnung trägt. Das folgt einem Objektiv, das sich verstellt "
       "hat -- ein verrutschter Zoom, ein atmender Fokus -- kostet aber eine "
       "deutlich größere Lösung."),
    FR("Reconstruire avec les caméras partagées comme d'habitude, puis lancer "
       "un dernier ajustement où chaque image porte sa propre focale et sa "
       "propre distorsion. Cela suit un objectif qui a dérivé -- un zoom qui a "
       "bougé, une mise au point qui respire -- au prix d'un calcul bien plus "
       "gros."),
    ES("Reconstruir con las cámaras compartidas como de costumbre y luego "
       "hacer un último ajuste en el que cada imagen lleva su propia focal y "
       "su propia distorsión. Sigue a una óptica que se movió -- un zoom que "
       "se corrió, un enfoque que respira -- a costa de un cálculo mucho mayor."),
    PT("Reconstruir com as câmeras compartilhadas como de costume e depois "
       "rodar um último ajuste em que cada imagem carrega sua própria focal e "
       "sua própria distorção. Isso acompanha uma lente que derivou -- um zoom "
       "que se mexeu, um foco que respira -- ao custo de um cálculo bem maior."),
    IT("Ricostruire con le camere condivise come al solito e poi eseguire un "
       "ultimo bundle adjustment in cui ogni immagine porta la propria focale "
       "e la propria distorsione. Segue un obiettivo che è andato alla deriva "
       "-- uno zoom che si è spostato, un fuoco che respira -- al prezzo di un "
       "calcolo molto più grande."),
    NL("Reconstrueer met gedeelde camera's zoals gewoonlijk en draai daarna "
       "één laatste bundelaanpassing waarin elk beeld zijn eigen "
       "brandpuntsafstand en vertekening draagt. Dat volgt een lens die is "
       "weggelopen -- een verschoven zoom, een ademende scherpstelling -- ten "
       "koste van een veel grotere oplossing."),
    RU("Реконструировать с общими камерами как обычно, а затем выполнить "
       "последнее уравнивание, в котором у каждого кадра своё фокусное "
       "расстояние и своя дисторсия. Это отслеживает уплывший объектив — "
       "сдвинувшийся зум, дышащий фокус — ценой куда более крупной задачи."),
    TR("Kameralar her zamanki gibi paylaşılarak yeniden oluşturulur, ardından "
       "her görüntünün kendi odak uzaklığını ve bozulmasını taşıdığı son bir "
       "demet dengelemesi çalıştırılır. Kayan bir zum ya da nefes alan bir "
       "odak gibi savrulmuş bir objektifi izler; bedeli çok daha büyük bir "
       "çözümdür."));

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
    EN("Keypoints kept per image -- largest scales first for SIFT, highest "
       "detection scores for a learned frontend. Overrides the quality preset "
       "when non-zero. The two are not comparable: SIFT wants tens of "
       "thousands, ALIKED a few thousand."),
    JA("1枚あたりに残すキーポイントの数です。SIFT ではスケールの大きい順、"
       "学習型フロントエンドでは検出スコアの高い順に残します。0 以外なら"
       "品質プリセットより優先されます。両者の数は比べられません。SIFT は"
       "数万、ALIKED は数千を求めます。"),
    ZH_HANS("每张图像保留的关键点数量——SIFT 按尺度从大到小，学习型前端按检测"
            "分数从高到低。非零时会覆盖质量预设。两者的数值不可比：SIFT 要几万个，"
            "ALIKED 只要几千个。"),
    ZH_HANT("每張影像保留的關鍵點數量——SIFT 按尺度從大到小，學習型前端按偵測"
            "分數從高到低。非零時會覆蓋品質預設。兩者的數值不可比：SIFT 要幾萬個，"
            "ALIKED 只要幾千個。"),
    KO("이미지당 남기는 키포인트 수입니다. SIFT는 스케일이 큰 것부터, 학습형 "
       "프런트엔드는 검출 점수가 높은 것부터 남깁니다. 0이 아니면 품질 프리셋보다 "
       "우선합니다. 두 값은 서로 비교할 수 없습니다. SIFT는 수만 개, ALIKED는 "
       "수천 개를 원합니다."),
    DE("Schlüsselpunkte je Bild -- bei SIFT die größten Skalen zuerst, bei "
       "einem gelernten Frontend die höchsten Erkennungswerte. Übergeht die "
       "Qualitätsvorgabe, wenn ungleich null. Die Zahlen sind nicht "
       "vergleichbar: SIFT will Zehntausende, ALIKED ein paar Tausend."),
    FR("Points clés conservés par image -- les plus grandes échelles d'abord "
       "pour SIFT, les meilleurs scores de détection pour un frontal appris. "
       "Prend le pas sur le préréglage de qualité s'il est non nul. Les deux "
       "ne sont pas comparables : SIFT en veut des dizaines de milliers, "
       "ALIKED quelques milliers."),
    ES("Puntos clave conservados por imagen: las escalas mayores primero en "
       "SIFT, las puntuaciones de detección más altas en un frontal "
       "aprendido. Si no es cero, prevalece sobre el ajuste de calidad. Los "
       "dos no son comparables: SIFT quiere decenas de miles, ALIKED unos "
       "pocos miles."),
    PT("Pontos-chave mantidos por imagem -- as maiores escalas primeiro no "
       "SIFT, as maiores pontuações de detecção num front-end aprendido. "
       "Prevalece sobre a predefinição de qualidade quando não é zero. Os dois "
       "não são comparáveis: o SIFT quer dezenas de milhares, o ALIKED alguns "
       "milhares."),
    IT("Punti chiave tenuti per immagine: le scale maggiori prima per SIFT, i "
       "punteggi di rilevamento più alti per un frontend appreso. Se diverso "
       "da zero, prevale sulla preimpostazione di qualità. I due numeri non "
       "sono confrontabili: SIFT ne vuole decine di migliaia, ALIKED qualche "
       "migliaio."),
    NL("Sleutelpunten per beeld -- bij SIFT de grootste schalen eerst, bij een "
       "geleerde frontend de hoogste detectiescores. Gaat boven de "
       "kwaliteitsvoorinstelling als het niet nul is. De twee zijn niet "
       "vergelijkbaar: SIFT wil er tienduizenden, ALIKED een paar duizend."),
    RU("Сколько ключевых точек оставлять на снимок — у SIFT сначала самые "
       "крупные масштабы, у обученного фронтенда самые высокие оценки "
       "детекции. Ненулевое значение важнее пресета качества. Числа несравнимы: "
       "SIFT хочет десятки тысяч, ALIKED — несколько тысяч."),
    TR("Görüntü başına tutulan anahtar nokta sayısı -- SIFT'te önce en büyük "
       "ölçekler, öğrenilmiş bir ön uçta en yüksek bulma puanları. Sıfır "
       "değilse kalite hazır ayarını geçersiz kılar. İkisi kıyaslanabilir "
       "değildir: SIFT on binlerce, ALIKED birkaç bin ister."));

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
    EN("Longest edge the feature extractor runs on; bigger images are "
       "downscaled first. Keypoints are still reported in the source image's "
       "pixels."),
    JA("特徴抽出を行う長辺の長さです。これより大きい画像は先に縮小されます。"
       "キーポイントの座標は元画像のピクセルで報告されます。"),
    ZH_HANS("特征提取所用的最长边长度；更大的图像会先缩小。关键点坐标仍以源图像"
            "的像素给出。"),
    ZH_HANT("特徵擷取所用的最長邊長度；更大的影像會先縮小。關鍵點座標仍以來源影像"
            "的像素給出。"),
    KO("특징 추출을 수행하는 긴 변의 길이입니다. 그보다 큰 이미지는 먼저 "
       "축소됩니다. 키포인트 좌표는 여전히 원본 이미지의 픽셀로 보고됩니다."),
    DE("Längste Kante, auf der die Merkmalsextraktion läuft; größere Bilder "
       "werden zuvor verkleinert. Schlüsselpunkte werden weiterhin in Pixeln "
       "des Ausgangsbildes angegeben."),
    FR("Plus grand côté sur lequel l'extraction de points s'exécute ; les "
       "images plus grandes sont d'abord réduites. Les points clés restent "
       "exprimés en pixels de l'image source."),
    ES("Lado más largo sobre el que se ejecuta la extracción de "
       "características; las imágenes mayores se reducen antes. Los puntos "
       "clave se siguen dando en píxeles de la imagen de origen."),
    PT("Maior lado sobre o qual a extração de características roda; imagens "
       "maiores são reduzidas antes. Os pontos-chave continuam em pixels da "
       "imagem de origem."),
    IT("Lato più lungo su cui gira l'estrazione delle caratteristiche; le "
       "immagini più grandi vengono prima ridotte. I punti chiave restano "
       "espressi in pixel dell'immagine di partenza."),
    NL("Langste zijde waarop de kenmerkextractie draait; grotere beelden "
       "worden eerst verkleind. Sleutelpunten worden nog steeds in pixels van "
       "het bronbeeld gegeven."),
    RU("Наибольшая сторона, на которой работает выделение особых точек; "
       "изображения крупнее сначала уменьшаются. Координаты точек всё равно "
       "даются в пикселях исходного изображения."),
    TR("Öznitelik çıkarımının çalıştığı en uzun kenar; daha büyük görüntüler "
       "önce küçültülür. Anahtar noktalar yine kaynak görüntünün pikselleri "
       "cinsinden bildirilir."));

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
    EN("A mask is normally white where the image is used. Turn this on when "
       "the ones beside the photos are the other way round -- white on the "
       "people or the rig to take out. The run reads them that way and writes "
       "everything it makes the usual way, so nothing downstream needs telling."),
    JA("マスクは通常、使う部分が白です。写真の隣にあるマスクが逆で、取り除く人や"
       "機材の側が白い場合にこれを有効にしてください。実行時はその向きで読み取り、"
       "書き出すものはすべて通常の向きになるので、後段に伝える設定は要りません。"),
    ZH_HANS("蒙版通常以白色表示要用的部分。若照片旁边的蒙版正好相反——把要去掉的"
            "人或器材涂成白色——请打开此项。运行时会按这个方向读取，写出的内容一"
            "律用通常的方向，后续步骤无需再设置。"),
    ZH_HANT("遮罩通常以白色表示要用的部分。若照片旁邊的遮罩正好相反——把要去掉的"
            "人或器材塗成白色——請開啟此項。執行時會按這個方向讀取，寫出的內容一"
            "律用通常的方向，後續步驟無需再設定。"),
    KO("마스크는 보통 쓸 부분이 흰색입니다. 사진 옆의 마스크가 그 반대로 지울 "
       "사람이나 장비 쪽이 흰색이라면 이 항목을 켜십시오. 실행은 그 방향으로 읽고 "
       "만들어 내는 것은 모두 보통 방향으로 쓰므로 뒤쪽에 따로 알릴 필요가 없습니다."),
    DE("Eine Maske ist normalerweise weiß, wo das Bild benutzt wird. Einschalten, "
       "wenn die neben den Fotos andersherum sind -- weiß auf den Personen oder "
       "dem Gestell, das weg soll. Der Lauf liest sie so und schreibt alles "
       "Eigene wie üblich, sodass nichts danach davon wissen muss."),
    FR("Un masque est normalement blanc là où l'image sert. Activez ceci quand "
       "ceux à côté des photos sont à l'envers -- blancs sur les personnes ou le "
       "support à retirer. L'exécution les lit ainsi et écrit tout le reste "
       "comme d'habitude, donc rien ensuite n'a besoin d'être prévenu."),
    ES("Una máscara es normalmente blanca donde se usa la imagen. Actívalo "
       "cuando las que están junto a las fotos sean al revés: blancas sobre las "
       "personas o el soporte que se quitan. La ejecución las lee así y escribe "
       "lo suyo del modo habitual, así que nada posterior necesita saberlo."),
    PT("Uma máscara é normalmente branca onde a imagem é usada. Ative isto "
       "quando as que estão ao lado das fotos forem ao contrário: brancas sobre "
       "as pessoas ou o suporte a remover. A execução lê-as assim e escreve o "
       "que faz do modo habitual, por isso nada a seguir precisa de saber."),
    IT("Una maschera è normalmente bianca dove l'immagine viene usata. Attivalo "
       "quando quelle accanto alle foto sono al contrario: bianche sulle persone "
       "o sul supporto da togliere. L'esecuzione le legge così e scrive il "
       "proprio nel modo solito, quindi nulla a valle deve saperlo."),
    NL("Een masker is normaal wit waar het beeld wordt gebruikt. Zet dit aan "
       "wanneer die naast de foto's andersom zijn -- wit op de mensen of het "
       "statief die weg moeten. De run leest ze zo en schrijft alles wat hij "
       "zelf maakt op de gewone manier, dus verderop hoeft niets te weten."),
    RU("Маска обычно белая там, где изображение используется. Включите это, "
       "если те, что лежат рядом с фотографиями, наоборот — белые на людях или "
       "штативе, которые надо убрать. Запуск читает их так, а всё своё пишет "
       "как обычно, поэтому дальше об этом сообщать не нужно."),
    TR("Maske normalde görüntünün kullanıldığı yerde beyazdır. Fotoğrafların "
       "yanındakiler tersse -- kaldırılacak kişilerin ya da düzeneğin üstü "
       "beyazsa -- bunu açın. Çalıştırma onları öyle okur, kendi yazdıklarını "
       "her zamanki gibi yazar; sonrasında hiçbir yere söylemek gerekmez."));

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
    EN("Keep features/ and matches.bin in the output folder after a "
       "successful run. They are large, and only useful for re-running the "
       "mapper by hand with `spirula sfm`."),
    JA("実行が成功したあとも、出力フォルダに features/ と matches.bin を"
       "残します。サイズが大きく、`spirula sfm` で手動でマッパーを再実行する"
       "とき以外は使いません。"),
    ZH_HANS("运行成功后仍在输出文件夹里保留 features/ 和 matches.bin。它们体积很大，"
            "只有在用 `spirula sfm` 手动重跑建图时才有用。"),
    ZH_HANT("執行成功後仍在輸出資料夾裡保留 features/ 和 matches.bin。它們體積很大，"
            "只有在用 `spirula sfm` 手動重跑建圖時才有用。"),
    KO("실행이 성공한 뒤에도 출력 폴더에 features/와 matches.bin을 남깁니다. "
       "크기가 크고, `spirula sfm`으로 매퍼를 손수 다시 돌릴 때만 쓸모가 있습니다."),
    DE("features/ und matches.bin nach einem erfolgreichen Lauf im "
       "Ausgabeordner behalten. Sie sind groß und nur nützlich, um den Mapper "
       "von Hand mit `spirula sfm` erneut laufen zu lassen."),
    FR("Conserver features/ et matches.bin dans le dossier de sortie après une "
       "exécution réussie. Ils sont volumineux et ne servent qu'à relancer le "
       "mapper à la main avec `spirula sfm`."),
    ES("Conservar features/ y matches.bin en la carpeta de salida tras una "
       "ejecución correcta. Son grandes y solo sirven para volver a lanzar el "
       "mapeador a mano con `spirula sfm`."),
    PT("Manter features/ e matches.bin na pasta de saída após uma execução "
       "bem-sucedida. São grandes e só servem para rodar o mapeador à mão com "
       "o `spirula sfm`."),
    IT("Conservare features/ e matches.bin nella cartella di destinazione dopo "
       "un'esecuzione riuscita. Sono grandi e servono solo per rilanciare a "
       "mano il mapper con `spirula sfm`."),
    NL("features/ en matches.bin na een geslaagde run in de uitvoermap "
       "bewaren. Ze zijn groot en alleen nuttig om de mapper met de hand "
       "opnieuw te draaien met `spirula sfm`."),
    RU("Оставлять features/ и matches.bin в папке результатов после успешного "
       "запуска. Они большие и нужны, только чтобы вручную перезапустить "
       "маппер через `spirula sfm`."),
    TR("Başarılı bir çalıştırmadan sonra features/ ve matches.bin dosyalarını "
       "çıktı klasöründe tutar. Büyüktürler ve yalnızca haritalayıcıyı "
       "`spirula sfm` ile elle yeniden çalıştırmak için işe yararlar."));

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
    EN("Solve the reconstruction's largest step on the CPU rather than the "
       "GPU. Slower, and only worth it where the GPU cannot finish it: a "
       "driver that resets under a long solve, or one card doing this and "
       "something else at once. A run that hits either falls back by itself, "
       "but the fallback costs the failure first."),
    JA("復元でいちばん大きな計算を GPU ではなく CPU で解きます。遅くなるので、"
       "GPU で完了できないときだけ使ってください。長い計算の途中でドライバが"
       "リセットされる場合や、1 枚のカードでこれと別の作業を同時に走らせている"
       "場合です。どちらに当たっても実行は自動で CPU に切り替わりますが、"
       "そのぶん一度失敗する時間がかかります。"),
    ZH_HANS("把重建里最大的一步放到 CPU 上解，而不是 GPU。会更慢，只在 GPU 跑不完"
            "时才值得: 长时间计算中驱动被重置，或者同一块显卡还在做别的事。"
            "遇到这两种情况时程序会自动改用 CPU，但要先花掉一次失败的时间。"),
    ZH_HANT("把重建裡最大的一步放到 CPU 上解，而不是 GPU。會更慢，只在 GPU 跑不完"
            "時才值得: 長時間計算中驅動被重設，或者同一張顯示卡還在做別的事。"
            "遇到這兩種情況時程式會自動改用 CPU，但要先花掉一次失敗的時間。"),
    KO("복원에서 가장 큰 단계를 GPU 대신 CPU에서 계산합니다. 느리므로 GPU가 끝내지 "
       "못할 때만 쓸 만합니다. 긴 계산 도중 드라이버가 초기화되거나, 한 장의 "
       "카드가 이 작업과 다른 작업을 함께 할 때입니다. 둘 중 하나에 걸리면 "
       "실행이 알아서 CPU로 넘어가지만, 그전에 한 번 실패하는 시간이 듭니다."),
    DE("Den größten Schritt der Rekonstruktion auf der CPU lösen statt auf der "
       "GPU. Langsamer, und nur dort sinnvoll, wo die GPU ihn nicht zu Ende "
       "bringt: ein Treiber, der bei einer langen Rechnung zurückgesetzt wird, "
       "oder eine Karte, die nebenher noch etwas anderes tut. Ein Lauf weicht "
       "in beiden Fällen von selbst aus, zahlt dafür aber erst den Fehlschlag."),
    FR("Résoudre l'étape la plus lourde de la reconstruction sur le CPU plutôt "
       "que sur le GPU. Plus lent, et utile seulement là où le GPU n'y arrive "
       "pas : un pilote réinitialisé pendant un long calcul, ou une carte qui "
       "fait autre chose en même temps. Un calcul qui tombe sur l'un ou "
       "l'autre bascule tout seul, mais paie d'abord l'échec."),
    ES("Resolver el paso más grande de la reconstrucción en la CPU en lugar de "
       "la GPU. Más lento, y solo vale la pena donde la GPU no lo termina: un "
       "controlador que se reinicia durante un cálculo largo, o una tarjeta "
       "que además hace otra cosa. Una ejecución que se topa con eso cambia "
       "sola, pero antes paga el fallo."),
    PT("Resolver o passo maior da reconstrução na CPU em vez da GPU. Mais "
       "lento, e só vale onde a GPU não consegue terminar: um driver que "
       "reinicia durante um cálculo longo, ou uma placa que ainda faz outra "
       "coisa. Uma execução que esbarra nisso muda sozinha, mas paga a falha "
       "antes."),
    IT("Risolvere il passaggio più grande della ricostruzione sulla CPU invece "
       "che sulla GPU. Più lento, e utile solo dove la GPU non ci arriva: un "
       "driver che si reimposta durante un calcolo lungo, o una scheda che sta "
       "facendo anche altro. Un'esecuzione che incappa in questo ripiega da "
       "sola, ma prima paga il fallimento."),
    NL("De grootste stap van de reconstructie op de CPU oplossen in plaats van "
       "op de GPU. Trager, en alleen de moeite waar de GPU hem niet afmaakt: "
       "een stuurprogramma dat tijdens een lange berekening opnieuw start, of "
       "een kaart die er iets anders bij doet. Een run die daartegenaan loopt "
       "wijkt vanzelf uit, maar betaalt eerst de mislukking."),
    RU("Решать самый большой шаг реконструкции на CPU, а не на GPU. Медленнее "
       "и оправдано только там, где GPU его не заканчивает: драйвер "
       "сбрасывается на длинном расчёте или та же карта занята чем-то ещё. "
       "Запуск в обоих случаях переходит на CPU сам, но сначала теряет время "
       "на неудачу."),
    TR("Yeniden kurulumun en büyük adımını GPU yerine CPU'da çözer. Daha "
       "yavaştır ve yalnızca GPU'nun bitiremediği yerde değer: uzun bir hesap "
       "sırasında sıfırlanan bir sürücü ya da aynı anda başka iş de yapan bir "
       "kart. Bunlara denk gelen bir çalışma kendiliğinden CPU'ya geçer, ama "
       "önce başarısızlığın bedelini öder."));

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

SS_MSG(use_python_masking,
    EN("Mask with the external Python script"),
    JA("外部の Python スクリプトでマスクする"),
    ZH_HANS("用外部 Python 脚本做蒙版"),
    ZH_HANT("用外部 Python 指令稿做遮罩"),
    KO("외부 Python 스크립트로 마스킹"),
    DE("Mit dem externen Python-Skript maskieren"),
    FR("Masquer avec le script Python externe"),
    ES("Enmascarar con el script externo de Python"),
    PT("Mascarar com o script externo em Python"),
    IT("Mascherare con lo script Python esterno"),
    NL("Maskeren met het externe Python-script"),
    RU("Маскировать внешним скриптом Python"),
    TR("Harici Python betiğiyle maskele"));

SS_MSG(use_python_masking_help,
    EN("Use reference/scripts/mask.py through an external Python with "
       "lang-segment-anything, instead of the built-in segmentation."),
    JA("内蔵のセグメンテーションの代わりに、lang-segment-anything を入れた"
       "外部の Python で reference/scripts/mask.py を使います。"),
    ZH_HANS("不用内置分割，而是通过装有 lang-segment-anything 的外部 Python "
            "运行 reference/scripts/mask.py。"),
    ZH_HANT("不用內建分割，而是透過裝有 lang-segment-anything 的外部 Python "
            "執行 reference/scripts/mask.py。"),
    KO("내장 분할 대신, lang-segment-anything이 설치된 외부 Python으로 "
       "reference/scripts/mask.py를 실행합니다."),
    DE("reference/scripts/mask.py über ein externes Python mit "
       "lang-segment-anything benutzen statt der eingebauten Segmentierung."),
    FR("Utiliser reference/scripts/mask.py via un Python externe doté de "
       "lang-segment-anything, au lieu de la segmentation intégrée."),
    ES("Usar reference/scripts/mask.py mediante un Python externo con "
       "lang-segment-anything, en lugar de la segmentación integrada."),
    PT("Usar reference/scripts/mask.py por meio de um Python externo com "
       "lang-segment-anything, em vez da segmentação integrada."),
    IT("Usare reference/scripts/mask.py tramite un Python esterno con "
       "lang-segment-anything, invece della segmentazione integrata."),
    NL("reference/scripts/mask.py gebruiken via een externe Python met "
       "lang-segment-anything, in plaats van de ingebouwde segmentatie."),
    RU("Использовать reference/scripts/mask.py через внешний Python с "
       "lang-segment-anything вместо встроенной сегментации."),
    TR("Yerleşik bölütleme yerine, lang-segment-anything kurulu harici bir "
       "Python üzerinden reference/scripts/mask.py kullanır."));

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
    EN("Seed COLMAP with fx = fy = factor * image width (principal point "
       "centered, zero distortion) instead of its generic guess. A known focal "
       "length stabilizes mapper initialization a lot, especially for fisheye "
       "lenses. Insta360 X5: ~0.269 (set automatically for .insv input)."),
    JA("COLMAP の一般的な推測ではなく、fx = fy = 係数 × 画像幅（主点は中央、"
       "歪みなし）を初期値として渡します。焦点距離が分かっているとマッパーの"
       "初期化がかなり安定し、特に魚眼レンズで効きます。Insta360 X5 は約 0.269"
       "（.insv 入力では自動で設定されます）。"),
    ZH_HANS("不用 COLMAP 的通用猜测，而是以 fx = fy = 系数 × 图像宽度（主点居中，"
            "无畸变）作为初值。已知焦距会让建图的初始化稳定得多，尤其是鱼眼镜头。"
            "Insta360 X5 约为 0.269（.insv 输入会自动设置）。"),
    ZH_HANT("不用 COLMAP 的通用猜測，而是以 fx = fy = 係數 × 影像寬度（主點置中，"
            "無畸變）作為初值。已知焦距會讓建圖的初始化穩定得多，尤其是魚眼鏡頭。"
            "Insta360 X5 約為 0.269（.insv 輸入會自動設定）。"),
    KO("COLMAP의 일반적인 추측 대신 fx = fy = 계수 × 이미지 너비(주점은 중앙, "
       "왜곡 없음)를 초기값으로 넘깁니다. 초점 거리를 알면 매퍼 초기화가 훨씬 "
       "안정되며, 특히 어안 렌즈에서 그렇습니다. Insta360 X5는 약 0.269"
       "(.insv 입력에서는 자동으로 설정됩니다)."),
    DE("COLMAP mit fx = fy = Faktor × Bildbreite (Hauptpunkt mittig, keine "
       "Verzeichnung) starten statt mit seiner allgemeinen Schätzung. Eine "
       "bekannte Brennweite stabilisiert den Start des Mappers erheblich, "
       "besonders bei Fisheye-Objektiven. Insta360 X5: ~0.269 (bei "
       ".insv-Eingaben automatisch gesetzt)."),
    FR("Amorcer COLMAP avec fx = fy = facteur × largeur de l'image (point "
       "principal centré, distorsion nulle) au lieu de son estimation "
       "générique. Une distance focale connue stabilise beaucoup "
       "l'initialisation du mapper, surtout avec des objectifs fisheye. "
       "Insta360 X5 : ~0.269 (réglé automatiquement pour une entrée .insv)."),
    ES("Arrancar COLMAP con fx = fy = factor × ancho de la imagen (punto "
       "principal centrado, sin distorsión) en lugar de su estimación "
       "genérica. Conocer la distancia focal estabiliza mucho el arranque del "
       "mapper, sobre todo con objetivos de ojo de pez. Insta360 X5: ~0.269 "
       "(se ajusta solo con entradas .insv)."),
    PT("Iniciar o COLMAP com fx = fy = fator × largura da imagem (ponto "
       "principal centrado, sem distorção) em vez do palpite genérico dele. "
       "Uma distância focal conhecida estabiliza muito o início do mapper, "
       "principalmente com lentes olho de peixe. Insta360 X5: ~0.269 "
       "(definido automaticamente para entradas .insv)."),
    IT("Avviare COLMAP con fx = fy = fattore × larghezza dell'immagine (punto "
       "principale centrato, distorsione nulla) invece della sua stima "
       "generica. Una focale nota stabilizza molto l'avvio del mapper, "
       "soprattutto con obiettivi fisheye. Insta360 X5: ~0.269 (impostato da "
       "solo per gli ingressi .insv)."),
    NL("COLMAP starten met fx = fy = factor × beeldbreedte (hoofdpunt "
       "gecentreerd, geen vertekening) in plaats van met zijn algemene "
       "schatting. Een bekende brandpuntsafstand maakt de start van de mapper "
       "veel stabieler, zeker bij fisheye-lenzen. Insta360 X5: ~0.269 (wordt "
       "bij .insv-invoer vanzelf ingesteld)."),
    RU("Задать COLMAP начальное fx = fy = коэффициент × ширина изображения "
       "(главная точка в центре, без дисторсии) вместо его общей догадки. "
       "Известное фокусное расстояние заметно стабилизирует запуск маппера, "
       "особенно для объективов «рыбий глаз». Insta360 X5: ~0.269 (для входа "
       ".insv ставится автоматически)."),
    TR("COLMAP'ı genel tahmini yerine fx = fy = katsayı × görüntü genişliği "
       "(ana nokta ortada, bozulma yok) ile başlatır. Bilinen bir odak "
       "uzaklığı mapper'ın başlangıcını epeyce dengeler, özellikle balıkgözü "
       "objektiflerde. Insta360 X5: ~0.269 (.insv girdilerinde kendiliğinden "
       "ayarlanır)."));

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
    EN("Raw ImageReader.camera_params for the selected camera model (full "
       "calibration prior). Leave empty to use the focal-length factor above, "
       "or both empty for COLMAP's default initialization."),
    JA("選んだカメラモデルに対する ImageReader.camera_params をそのまま渡します"
       "（較正値をすべて与える指定）。空にすると上の焦点距離の係数を使い、"
       "両方とも空なら COLMAP の既定の初期化になります。"),
    ZH_HANS("按所选相机模型直接给出 ImageReader.camera_params（完整的标定先验）。"
            "留空则使用上面的焦距系数；两者都留空则用 COLMAP 的默认初始化。"),
    ZH_HANT("依所選相機模型直接給出 ImageReader.camera_params（完整的校正先驗）。"
            "留空則使用上面的焦距係數；兩者都留空則用 COLMAP 的預設初始化。"),
    KO("선택한 카메라 모델에 대한 ImageReader.camera_params를 그대로 넘깁니다"
       "(교정값 전체를 지정). 비워 두면 위의 초점 거리 계수를 쓰고, 둘 다 비우면 "
       "COLMAP의 기본 초기화를 씁니다."),
    DE("ImageReader.camera_params für das gewählte Kameramodell, unverändert "
       "(vollständige Kalibrierung als Vorgabe). Leer lassen, um den "
       "Brennweitenfaktor oben zu benutzen; sind beide leer, initialisiert "
       "COLMAP wie üblich."),
    FR("ImageReader.camera_params tel quel pour le modèle de caméra choisi "
       "(étalonnage complet donné a priori). Laisser vide pour utiliser le "
       "facteur de distance focale ci-dessus ; les deux vides, COLMAP "
       "s'initialise par défaut."),
    ES("ImageReader.camera_params tal cual para el modelo de cámara elegido "
       "(calibración completa como valor previo). Déjalo vacío para usar el "
       "factor de distancia focal de arriba; con ambos vacíos, COLMAP se "
       "inicializa por defecto."),
    PT("ImageReader.camera_params tal como está, para o modelo de câmera "
       "escolhido (calibração completa como valor prévio). Deixe vazio para "
       "usar o fator de distância focal acima; com os dois vazios, o COLMAP "
       "inicializa como de costume."),
    IT("ImageReader.camera_params così com'è, per il modello di camera scelto "
       "(calibrazione completa data a priori). Lascialo vuoto per usare il "
       "fattore di focale qui sopra; con entrambi vuoti, COLMAP si inizializza "
       "in modo predefinito."),
    NL("ImageReader.camera_params ongewijzigd, voor het gekozen cameramodel "
       "(volledige kalibratie vooraf). Laat het leeg om de "
       "brandpuntsafstandfactor hierboven te gebruiken; zijn beide leeg, dan "
       "initialiseert COLMAP zoals gewoonlijk."),
    RU("ImageReader.camera_params как есть, для выбранной модели камеры "
       "(полная калибровка как априорные данные). Оставьте пустым, чтобы "
       "использовать коэффициент фокусного расстояния выше; если пусто и то и "
       "другое, COLMAP инициализируется по умолчанию."),
    TR("Seçilen kamera modeli için ImageReader.camera_params değerini olduğu "
       "gibi verir (kalibrasyonun tamamı önsel olarak). Yukarıdaki odak "
       "uzaklığı katsayısını kullanmak için boş bırakın; ikisi de boşsa COLMAP "
       "kendi varsayılanıyla başlar."));

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
    EN("Neural feature matcher (FeatureMatching.type *_LIGHTGLUE): more matches "
       "on hard pairs than brute-force descriptor distance. Default for ALIKED "
       "features; also works with SIFT."),
    JA("ニューラルネットによる特徴マッチャーです（FeatureMatching.type の "
       "*_LIGHTGLUE）。記述子の総当たり距離より、難しい組み合わせで多くの対応が"
       "得られます。ALIKED 特徴では既定で、SIFT でも使えます。"),
    ZH_HANS("基于神经网络的特征匹配器（FeatureMatching.type 的 *_LIGHTGLUE）："
            "在困难的图像对上比暴力比较描述子距离能找到更多匹配。ALIKED 特征默认"
            "使用；SIFT 也可以用。"),
    ZH_HANT("以神經網路為基礎的特徵比對器（FeatureMatching.type 的 *_LIGHTGLUE）："
            "在困難的影像對上比暴力比較描述子距離能找到更多對應。ALIKED 特徵預設"
            "使用；SIFT 也可以用。"),
    KO("신경망 특징 매처입니다(FeatureMatching.type의 *_LIGHTGLUE). 서술자 거리를 "
       "전수 비교하는 방식보다 어려운 쌍에서 대응을 더 많이 찾습니다. ALIKED "
       "특징에서는 기본이며, SIFT에서도 동작합니다."),
    DE("Neuronaler Merkmals-Matcher (FeatureMatching.type *_LIGHTGLUE): findet "
       "bei schwierigen Paaren mehr Zuordnungen als der Brute-Force-Vergleich "
       "der Deskriptoren. Standard für ALIKED-Merkmale, funktioniert auch mit "
       "SIFT."),
    FR("Appariement neuronal (FeatureMatching.type *_LIGHTGLUE) : trouve plus "
       "de correspondances sur les paires difficiles que la comparaison "
       "exhaustive des descripteurs. Par défaut pour les points ALIKED, "
       "fonctionne aussi avec SIFT."),
    ES("Emparejador neuronal (FeatureMatching.type *_LIGHTGLUE): encuentra más "
       "correspondencias en los pares difíciles que comparar descriptores por "
       "fuerza bruta. Es el predeterminado con puntos ALIKED y también sirve "
       "con SIFT."),
    PT("Emparelhador neural (FeatureMatching.type *_LIGHTGLUE): encontra mais "
       "correspondências nos pares difíceis do que comparar descritores por "
       "força bruta. É o padrão com pontos ALIKED e também funciona com SIFT."),
    IT("Matcher neurale (FeatureMatching.type *_LIGHTGLUE): trova più "
       "corrispondenze sulle coppie difficili rispetto al confronto esaustivo "
       "dei descrittori. Predefinito per i punti ALIKED, funziona anche con "
       "SIFT."),
    NL("Neurale kenmerk-matcher (FeatureMatching.type *_LIGHTGLUE): vindt op "
       "lastige paren meer overeenkomsten dan het brute-force vergelijken van "
       "descriptoren. Standaard bij ALIKED-kenmerken, werkt ook met SIFT."),
    RU("Нейросетевое сопоставление (FeatureMatching.type *_LIGHTGLUE): на "
       "трудных парах находит больше соответствий, чем полный перебор "
       "дескрипторов. По умолчанию для точек ALIKED, работает и с SIFT."),
    TR("Sinir ağı tabanlı öznitelik eşleştirici (FeatureMatching.type "
       "*_LIGHTGLUE): zor çiftlerde betimleyicileri kaba kuvvetle "
       "karşılaştırmaktan daha çok eşleşme bulur. ALIKED özniteliklerinde "
       "varsayılandır, SIFT ile de çalışır."));

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
    EN("When distortion coefficients are optimized. \"Final pass only\" holds "
       "them fixed during mapping (Mapper.ba_refine_extra_params 0) -- more "
       "stable for low-distortion perspective lenses -- and recovers them in "
       "the final refinement pass. Auto: final-pass-only for perspective "
       "models, during mapping for fisheye."),
    JA("歪み係数をいつ最適化するかです。「最後の仕上げだけ」ならマッピング中は"
       "固定し（Mapper.ba_refine_extra_params 0）、最後の仕上げで求めます。"
       "歪みの小さい透視レンズではこちらが安定します。「自動」は透視モデルなら"
       "最後の仕上げだけ、魚眼ならマッピング中です。"),
    ZH_HANS("何时优化畸变系数。选「只在最后一遍」时，建图过程中保持固定"
            "（Mapper.ba_refine_extra_params 0），到最后一遍精修时再求解；对畸变"
            "较小的透视镜头更稳。「自动」：透视模型只在最后一遍，鱼眼在建图过程中。"),
    ZH_HANT("何時最佳化畸變係數。選「只在最後一遍」時，建圖過程中保持固定"
            "（Mapper.ba_refine_extra_params 0），到最後一遍精修時再求解；對畸變"
            "較小的透視鏡頭更穩。「自動」：透視模型只在最後一遍，魚眼在建圖過程中。"),
    KO("왜곡 계수를 언제 최적화할지입니다. ‘마지막 단계에서만’은 매핑 중에는 "
       "고정해 두고(Mapper.ba_refine_extra_params 0) 마지막 정밀화 단계에서 "
       "구합니다. 왜곡이 작은 원근 렌즈에서는 이쪽이 더 안정적입니다. ‘자동’은 "
       "원근 모델이면 마지막 단계에서만, 어안이면 매핑 중입니다."),
    DE("Wann die Verzeichnungskoeffizienten optimiert werden. „Nur im letzten "
       "Durchgang“ hält sie während des Mappings fest "
       "(Mapper.ba_refine_extra_params 0) -- bei perspektivischen Objektiven "
       "mit wenig Verzeichnung stabiler -- und bestimmt sie erst im letzten "
       "Feinschliff. Automatisch: bei perspektivischen Modellen nur im letzten "
       "Durchgang, bei Fisheye während des Mappings."),
    FR("Quand les coefficients de distorsion sont optimisés. « Seulement à la "
       "passe finale » les garde fixes pendant le mapping "
       "(Mapper.ba_refine_extra_params 0) -- plus stable pour les objectifs "
       "perspectifs peu distordus -- et les retrouve à l'affinage final. "
       "Automatique : passe finale seule pour les modèles perspectifs, pendant "
       "le mapping pour le fisheye."),
    ES("Cuándo se optimizan los coeficientes de distorsión. «Solo en la pasada "
       "final» los mantiene fijos durante el mapeo "
       "(Mapper.ba_refine_extra_params 0) -- más estable con objetivos "
       "perspectivos de poca distorsión -- y los recupera en el refinado "
       "final. Automático: solo pasada final para los modelos perspectivos, "
       "durante el mapeo para el ojo de pez."),
    PT("Quando os coeficientes de distorção são otimizados. “Só na passagem "
       "final” mantém-nos fixos durante o mapeamento "
       "(Mapper.ba_refine_extra_params 0) -- mais estável com lentes "
       "perspectivas de pouca distorção -- e recupera-os no refino final. "
       "Automático: só a passagem final para modelos perspectivos, durante o "
       "mapeamento para olho de peixe."),
    IT("Quando vengono ottimizzati i coefficienti di distorsione. «Solo nella "
       "passata finale» li tiene fissi durante il mapping "
       "(Mapper.ba_refine_extra_params 0) -- più stabile con obiettivi "
       "prospettici poco distorti -- e li ricava nell'affinamento finale. "
       "Automatico: solo passata finale per i modelli prospettici, durante il "
       "mapping per il fisheye."),
    NL("Wanneer de vertekeningscoëfficiënten worden geoptimaliseerd. „Alleen in "
       "de laatste ronde” houdt ze vast tijdens het mappen "
       "(Mapper.ba_refine_extra_params 0) -- stabieler bij perspectivische "
       "lenzen met weinig vertekening -- en bepaalt ze pas in de laatste "
       "verfijning. Automatisch: alleen de laatste ronde bij perspectivische "
       "modellen, tijdens het mappen bij fisheye."),
    RU("Когда оптимизируются коэффициенты дисторсии. «Только на финальном "
       "проходе» держит их фиксированными во время реконструкции "
       "(Mapper.ba_refine_extra_params 0) -- устойчивее для перспективных "
       "объективов с малой дисторсией -- и находит их на финальном уточнении. "
       "Автоматически: только финальный проход для перспективных моделей, во "
       "время реконструкции для «рыбьего глаза»."),
    TR("Bozulma katsayılarının ne zaman iyileştirileceği. «Yalnızca son "
       "geçişte», haritalama boyunca onları sabit tutar "
       "(Mapper.ba_refine_extra_params 0) -- bozulması az perspektif "
       "objektiflerde daha kararlıdır -- ve son iyileştirme geçişinde bulur. "
       "Otomatik: perspektif modellerde yalnızca son geçiş, balıkgözünde "
       "haritalama sırasında."));

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
    EN("Mapper.min_num_matches (default 15): image pairs with fewer inlier "
       "matches are ignored by the mapper. Raise to suppress spurious "
       "registrations, lower for sparse overlap."),
    JA("Mapper.min_num_matches（既定 15）です。インライアの対応がこれより少ない"
       "画像ペアはマッパーが無視します。誤った登録を抑えたいときは上げ、重なりが"
       "少ない撮影では下げます。"),
    ZH_HANS("即 Mapper.min_num_matches（默认 15）：内点匹配少于此数的图像对会被"
            "建图阶段忽略。想压制错误的配准就调高，重叠很少时就调低。"),
    ZH_HANT("即 Mapper.min_num_matches（預設 15）：內點對應少於此數的影像對會被"
            "建圖階段忽略。想壓制錯誤的註冊就調高，重疊很少時就調低。"),
    KO("Mapper.min_num_matches(기본 15)입니다. 인라이어 대응이 이보다 적은 이미지 "
       "쌍은 매퍼가 무시합니다. 잘못된 등록을 줄이려면 올리고, 겹침이 적으면 "
       "내립니다."),
    DE("Mapper.min_num_matches (Standard 15): Bildpaare mit weniger "
       "Inlier-Zuordnungen ignoriert der Mapper. Höher unterdrückt falsche "
       "Registrierungen, niedriger hilft bei wenig Überlappung."),
    FR("Mapper.min_num_matches (15 par défaut) : le mapper ignore les paires "
       "d'images ayant moins de correspondances inliers. Augmenter pour "
       "supprimer les enregistrements douteux, baisser si le recouvrement est "
       "faible."),
    ES("Mapper.min_num_matches (15 por defecto): el mapper ignora los pares de "
       "imágenes con menos correspondencias inlier. Súbelo para eliminar "
       "registros espurios, bájalo si hay poco solapamiento."),
    PT("Mapper.min_num_matches (padrão 15): o mapper ignora pares de imagens "
       "com menos correspondências inlier. Aumente para eliminar registros "
       "espúrios, diminua quando houver pouca sobreposição."),
    IT("Mapper.min_num_matches (predefinito 15): il mapper ignora le coppie di "
       "immagini con meno corrispondenze inlier. Alzalo per eliminare "
       "registrazioni spurie, abbassalo se la sovrapposizione è scarsa."),
    NL("Mapper.min_num_matches (standaard 15): beeldparen met minder "
       "inlier-overeenkomsten negeert de mapper. Hoger onderdrukt valse "
       "registraties, lager helpt bij weinig overlap."),
    RU("Mapper.min_num_matches (по умолчанию 15): пары изображений с меньшим "
       "числом инлаерных соответствий маппер игнорирует. Повысьте, чтобы убрать "
       "ложные регистрации, понизьте при малом перекрытии."),
    TR("Mapper.min_num_matches (varsayılan 15): inlier eşleşmesi bundan az olan "
       "görüntü çiftlerini mapper yok sayar. Yanlış kayıtları bastırmak için "
       "yükseltin, örtüşme azsa düşürün."));

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
    EN("Large scenes with repeating structure (several similar rooms, tiled "
       "facades) often weld physically different but similar-looking parts "
       "together. These make matching and registration stricter to suppress "
       "that; 0 = COLMAP default."),
    JA("似た部屋がいくつも並ぶ、同じ模様の外壁が続くといった繰り返しの多い"
       "大きなシーンでは、物理的には別の場所が見た目の似ているせいで一つに"
       "溶けてしまいがちです。ここの設定はマッチングと登録を厳しくして、それを"
       "抑えます。0 は COLMAP の既定値です。"),
    ZH_HANS("在有重复结构的大场景里（好几个相似的房间、成排的同款外墙），物理上"
            "不同但看起来相似的部分常被焊在一起。这几项会让匹配和配准更严格来"
            "压制它；0 表示 COLMAP 的默认值。"),
    ZH_HANT("在有重複結構的大場景裡（好幾個相似的房間、成排的同款外牆），實際上"
            "不同但看起來相似的部分常被焊在一起。這幾項會讓比對和註冊更嚴格來"
            "壓制它；0 表示 COLMAP 的預設值。"),
    KO("비슷한 방이 여러 개 있거나 같은 무늬의 외벽이 이어지는 등 반복이 많은 큰 "
       "장면에서는, 실제로는 다른 곳인데 비슷해 보인다는 이유로 하나로 붙어버리기 "
       "쉽습니다. 아래 설정은 매칭과 등록을 엄격하게 만들어 그것을 억제합니다. "
       "0은 COLMAP 기본값입니다."),
    DE("In großen Szenen mit wiederkehrender Struktur (mehrere ähnliche Räume, "
       "gleichförmige Fassaden) verschmelzen oft physisch verschiedene, aber "
       "ähnlich aussehende Teile. Diese Werte verschärfen Zuordnung und "
       "Registrierung, um das zu verhindern; 0 = COLMAP-Standard."),
    FR("Dans les grandes scènes à structure répétée (plusieurs pièces "
       "semblables, façades identiques), des parties physiquement différentes "
       "mais d'aspect proche finissent souvent soudées. Ces réglages durcissent "
       "l'appariement et l'enregistrement pour l'éviter ; 0 = valeur COLMAP par "
       "défaut."),
    ES("En escenas grandes con estructura repetida (varias habitaciones "
       "parecidas, fachadas iguales), partes físicamente distintas pero de "
       "aspecto similar acaban soldadas. Estos valores endurecen el "
       "emparejamiento y el registro para evitarlo; 0 = valor de COLMAP por "
       "defecto."),
    PT("Em cenas grandes com estrutura repetida (vários cômodos parecidos, "
       "fachadas iguais), partes fisicamente diferentes mas de aparência "
       "semelhante acabam soldadas. Estes valores tornam a correspondência e o "
       "registro mais rígidos para evitar isso; 0 = padrão do COLMAP."),
    IT("Nelle scene grandi con struttura ripetuta (più stanze simili, facciate "
       "uguali) parti fisicamente diverse ma di aspetto simile finiscono "
       "spesso saldate insieme. Questi valori rendono più severe corrispondenze "
       "e registrazione per evitarlo; 0 = predefinito di COLMAP."),
    NL("In grote scènes met herhalende structuur (meerdere gelijkende kamers, "
       "identieke gevels) worden fysiek verschillende maar gelijkend delen vaak "
       "aan elkaar gelast. Deze waarden maken matching en registratie strenger "
       "om dat tegen te gaan; 0 = COLMAP-standaard."),
    RU("В больших сценах с повторяющейся структурой (несколько похожих комнат, "
       "однотипные фасады) физически разные, но похожие на вид части часто "
       "склеиваются. Эти значения ужесточают сопоставление и регистрацию, чтобы "
       "этого не было; 0 = значение COLMAP по умолчанию."),
    TR("Yinelenen yapıya sahip büyük sahnelerde (birbirine benzeyen birkaç oda, "
       "aynı desende cepheler) fiziksel olarak farklı ama benzer görünen "
       "yerler sıkça birbirine kaynar. Bu değerler eşleştirmeyi ve kaydı "
       "sıkılaştırarak bunu bastırır; 0 = COLMAP varsayılanı."));
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
    EN("How aggressively wrong matches are suppressed; fills the fields below. "
       "Low: mild tightening, keeps registration rate. Medium: good first "
       "attempt for multi-room indoor captures. High: for heavy repetition "
       "(identical rooms/facades) -- expect fewer registered images if overlap "
       "is thin."),
    JA("誤った対応をどれだけ強く抑えるかで、下の各項目を埋めます。「弱」は"
       "少し厳しくするだけで、登録できる枚数はほぼ保てます。「中」は部屋が"
       "いくつもある屋内撮影の最初の一手として手堅い設定です。「強」は同じ部屋や"
       "同じ外壁が繰り返される場合向けで、重なりが薄いと登録できる画像は減ります。"),
    ZH_HANS("压制错误匹配的力度，会自动填好下面各项。「低」只是稍微收紧，配准率"
            "基本不变。「中」适合作为多房间室内拍摄的第一次尝试。「高」用于重复"
            "非常严重的情况（一模一样的房间／外墙）——如果重叠本来就少，配准上的"
            "图像会变少。"),
    ZH_HANT("壓制錯誤對應的力度，會自動填好下面各項。「低」只是稍微收緊，註冊率"
            "基本不變。「中」適合作為多房間室內拍攝的第一次嘗試。「高」用於重複"
            "非常嚴重的情況（一模一樣的房間／外牆）——如果重疊本來就少，註冊上的"
            "影像會變少。"),
    KO("잘못된 대응을 얼마나 강하게 억제할지이며, 아래 항목들을 자동으로 채웁니다. "
       "‘낮음’은 살짝 조이는 정도라 등록되는 장수는 거의 유지됩니다. ‘보통’은 방이 "
       "여럿인 실내 촬영의 첫 시도로 무난합니다. ‘높음’은 똑같은 방이나 외벽이 "
       "반복될 때 쓰며, 겹침이 얇으면 등록되는 이미지가 줄어듭니다."),
    DE("Wie stark falsche Zuordnungen unterdrückt werden; füllt die Felder "
       "unten. Niedrig: leichtes Anziehen, die Registrierungsrate bleibt. "
       "Mittel: guter erster Versuch für Innenaufnahmen mit mehreren Räumen. "
       "Hoch: für starke Wiederholung (identische Räume/Fassaden) -- bei "
       "dünner Überlappung werden weniger Bilder registriert."),
    FR("Avec quelle force les mauvaises correspondances sont supprimées ; "
       "remplit les champs ci-dessous. Faible : léger durcissement, le taux "
       "d'enregistrement tient. Moyen : bon premier essai pour un intérieur à "
       "plusieurs pièces. Élevé : pour une répétition forte (pièces ou façades "
       "identiques) -- avec peu de recouvrement, moins d'images seront "
       "enregistrées."),
    ES("Con cuánta fuerza se eliminan las correspondencias erróneas; rellena "
       "los campos de abajo. Bajo: aprieta un poco y mantiene la tasa de "
       "registro. Medio: buen primer intento en interiores de varias "
       "habitaciones. Alto: para repetición fuerte (habitaciones o fachadas "
       "idénticas) -- con poco solapamiento se registrarán menos imágenes."),
    PT("Com que força as correspondências erradas são eliminadas; preenche os "
       "campos abaixo. Baixo: aperta pouco e mantém a taxa de registro. Médio: "
       "boa primeira tentativa em interiores com vários cômodos. Alto: para "
       "repetição forte (cômodos ou fachadas idênticos) -- com pouca "
       "sobreposição, menos imagens serão registradas."),
    IT("Con quanta forza vengono soppresse le corrispondenze sbagliate; "
       "compila i campi qui sotto. Basso: stringe poco e mantiene il tasso di "
       "registrazione. Medio: buon primo tentativo per interni con più stanze. "
       "Alto: per ripetizioni forti (stanze o facciate identiche) -- con poca "
       "sovrapposizione verranno registrate meno immagini."),
    NL("Hoe hard verkeerde overeenkomsten worden onderdrukt; vult de velden "
       "hieronder in. Laag: licht aandraaien, het registratiepercentage blijft. "
       "Gemiddeld: goede eerste poging voor binnenopnamen met meerdere kamers. "
       "Hoog: bij sterke herhaling (identieke kamers of gevels) -- bij weinig "
       "overlap worden minder beelden geregistreerd."),
    RU("Насколько жёстко подавляются ошибочные соответствия; заполняет поля "
       "ниже. Низкий: лёгкое ужесточение, доля зарегистрированных снимков "
       "сохраняется. Средний: хорошая первая попытка для съёмки в помещении из "
       "нескольких комнат. Высокий: при сильной повторяемости (одинаковые "
       "комнаты или фасады) -- при тонком перекрытии снимков зарегистрируется "
       "меньше."),
    TR("Yanlış eşleşmelerin ne kadar sert bastırılacağı; aşağıdaki alanları "
       "doldurur. Düşük: hafif sıkılaştırma, kayıt oranı korunur. Orta: çok "
       "odalı iç mekân çekimleri için iyi bir ilk deneme. Yüksek: yoğun "
       "yinelenmede (birbirinin aynı oda/cephe) -- örtüşme zayıfsa daha az "
       "görüntü kaydedilir."));

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
    EN("SiftMatching.max_ratio, the Lowe ratio test: a feature match is kept "
       "only when its best match is this much better than the second best. "
       "LOWER is stricter -- try 0.6-0.7 when repetitive texture creates false "
       "matches. SIFT only."),
    JA("SiftMatching.max_ratio、いわゆる Lowe の比率テストです。最良の対応が"
       "2 番目の対応よりこの割合だけ良いときにだけ、その対応を残します。"
       "小さいほど厳しくなります。模様の繰り返しで誤対応が出るときは 0.6〜0.7 を"
       "試してください。SIFT のみ。"),
    ZH_HANS("即 SiftMatching.max_ratio，也就是 Lowe 比率检验：只有当最佳匹配比"
            "次佳匹配好这么多时才保留该匹配。数值越小越严格——纹理重复导致误匹配"
            "时可以试 0.6～0.7。仅对 SIFT 有效。"),
    ZH_HANT("即 SiftMatching.max_ratio，也就是 Lowe 比率檢驗：只有當最佳對應比"
            "次佳對應好這麼多時才保留該對應。數值越小越嚴格——紋理重複導致誤對應"
            "時可以試 0.6～0.7。僅對 SIFT 有效。"),
    KO("SiftMatching.max_ratio, 이른바 Lowe 비율 검사입니다. 최선의 대응이 차선보다 "
       "이만큼 좋을 때만 그 대응을 남깁니다. 값이 작을수록 엄격합니다. 무늬가 "
       "반복되어 잘못된 대응이 생기면 0.6~0.7을 시도해 보세요. SIFT 전용."),
    DE("SiftMatching.max_ratio, der Lowe-Verhältnistest: eine Zuordnung bleibt "
       "nur, wenn die beste Übereinstimmung um diesen Faktor besser ist als die "
       "zweitbeste. KLEINER ist strenger -- bei falschen Zuordnungen durch "
       "wiederkehrende Textur 0.6-0.7 versuchen. Nur SIFT."),
    FR("SiftMatching.max_ratio, le test du rapport de Lowe : une correspondance "
       "n'est gardée que si la meilleure dépasse la deuxième de ce facteur. "
       "PLUS BAS est plus strict -- essayer 0.6-0.7 quand une texture répétée "
       "crée de fausses correspondances. SIFT uniquement."),
    ES("SiftMatching.max_ratio, la prueba de razón de Lowe: una correspondencia "
       "se conserva solo si la mejor supera a la segunda por este factor. MÁS "
       "BAJO es más estricto -- prueba 0.6-0.7 cuando una textura repetida crea "
       "correspondencias falsas. Solo SIFT."),
    PT("SiftMatching.max_ratio, o teste de razão de Lowe: uma correspondência "
       "só fica quando a melhor supera a segunda por este fator. MAIS BAIXO é "
       "mais rígido -- tente 0.6-0.7 quando textura repetida cria "
       "correspondências falsas. Só SIFT."),
    IT("SiftMatching.max_ratio, il test del rapporto di Lowe: una corrispondenza "
       "resta solo se la migliore supera la seconda di questo fattore. PIÙ "
       "BASSO è più severo -- prova 0.6-0.7 quando una texture ripetuta crea "
       "false corrispondenze. Solo SIFT."),
    NL("SiftMatching.max_ratio, de ratiotest van Lowe: een overeenkomst blijft "
       "alleen als de beste die factor beter is dan de op één na beste. LAGER "
       "is strenger -- probeer 0.6-0.7 als herhalende textuur valse "
       "overeenkomsten oplevert. Alleen SIFT."),
    RU("SiftMatching.max_ratio, тест отношения Лоу: соответствие сохраняется, "
       "только если лучшее лучше второго во столько раз. МЕНЬШЕ -- строже: при "
       "ложных соответствиях из-за повторяющейся текстуры попробуйте 0.6-0.7. "
       "Только для SIFT."),
    TR("SiftMatching.max_ratio, yani Lowe oran testi: bir eşleşme, ancak en "
       "iyisi ikinciden bu kadar iyiyse tutulur. DÜŞÜK olan daha katıdır -- "
       "yinelenen doku yanlış eşleşme üretiyorsa 0.6-0.7 deneyin. Yalnızca "
       "SIFT."));

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
    EN("TwoViewGeometry.min_num_inliers: image pairs whose geometric "
       "verification finds fewer inliers are discarded outright. Raise to "
       "50-100 so weakly-supported (usually false) links between "
       "similar-looking areas never enter the database."),
    JA("TwoViewGeometry.min_num_inliers です。幾何検証でこれより少ないインライア"
       "しか得られなかった画像ペアは、その場で捨てられます。50〜100 まで上げると、"
       "見た目の似た場所どうしの根拠の弱い（たいてい誤った）つながりが"
       "データベースに入らなくなります。"),
    ZH_HANS("即 TwoViewGeometry.min_num_inliers：几何验证得到的内点少于此数的图像对"
            "会被直接丢弃。调到 50～100，可以让相似区域之间那些依据不足（通常是"
            "错误）的连接根本进不了数据库。"),
    ZH_HANT("即 TwoViewGeometry.min_num_inliers：幾何驗證得到的內點少於此數的影像對"
            "會被直接丟棄。調到 50～100，可以讓相似區域之間那些依據不足（通常是"
            "錯誤）的連結根本進不了資料庫。"),
    KO("TwoViewGeometry.min_num_inliers입니다. 기하 검증에서 인라이어가 이보다 적게 "
       "나온 이미지 쌍은 그 자리에서 버립니다. 50~100까지 올리면 비슷해 보이는 "
       "영역 사이의 근거가 약한(대개 잘못된) 연결이 아예 데이터베이스에 들어오지 "
       "않습니다."),
    DE("TwoViewGeometry.min_num_inliers: Bildpaare, deren geometrische Prüfung "
       "weniger Inlier findet, werden sofort verworfen. Auf 50-100 anheben, "
       "damit schwach gestützte (meist falsche) Verbindungen zwischen ähnlich "
       "aussehenden Bereichen gar nicht erst in die Datenbank kommen."),
    FR("TwoViewGeometry.min_num_inliers : les paires dont la vérification "
       "géométrique trouve moins d'inliers sont rejetées d'emblée. Monter à "
       "50-100 pour que des liens peu étayés (le plus souvent faux) entre zones "
       "d'aspect proche n'entrent jamais dans la base."),
    ES("TwoViewGeometry.min_num_inliers: los pares cuya verificación geométrica "
       "encuentra menos inliers se descartan de inmediato. Súbelo a 50-100 para "
       "que los enlaces poco respaldados (casi siempre falsos) entre zonas "
       "parecidas nunca entren en la base de datos."),
    PT("TwoViewGeometry.min_num_inliers: pares cuja verificação geométrica "
       "encontra menos inliers são descartados na hora. Aumente para 50-100 "
       "para que ligações pouco fundamentadas (quase sempre falsas) entre áreas "
       "parecidas nunca entrem no banco de dados."),
    IT("TwoViewGeometry.min_num_inliers: le coppie la cui verifica geometrica "
       "trova meno inlier vengono scartate subito. Portalo a 50-100 perché i "
       "collegamenti poco sostenuti (di solito falsi) fra zone simili non "
       "entrino nemmeno nel database."),
    NL("TwoViewGeometry.min_num_inliers: beeldparen waarvan de geometrische "
       "controle minder inliers vindt, gaan meteen weg. Zet het op 50-100 zodat "
       "zwak onderbouwde (meestal onjuiste) verbindingen tussen gelijkend "
       "gebieden nooit in de database komen."),
    RU("TwoViewGeometry.min_num_inliers: пары, у которых геометрическая проверка "
       "нашла меньше инлаеров, отбрасываются сразу. Поднимите до 50-100, чтобы "
       "слабо подкреплённые (обычно ложные) связи между похожими участками "
       "вообще не попадали в базу."),
    TR("TwoViewGeometry.min_num_inliers: geometrik doğrulaması bundan az inlier "
       "bulan görüntü çiftleri hemen atılır. 50-100'e çıkarırsanız, benzer "
       "görünen bölgeler arasındaki zayıf temelli (çoğunlukla yanlış) bağlar "
       "veritabanına hiç girmez."));

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
    EN("Mapper.abs_pose_min_num_inliers: minimum absolute-pose inliers to "
       "register an image into the model. Raise to 50-100 to stop images from "
       "registering onto the wrong (similar-looking) part of the scene."),
    JA("Mapper.abs_pose_min_num_inliers です。画像をモデルに登録するのに必要な、"
       "絶対姿勢のインライアの最小数です。50〜100 に上げると、見た目の似た別の"
       "場所に画像が登録されてしまうのを防げます。"),
    ZH_HANS("即 Mapper.abs_pose_min_num_inliers：把一张图像配准进模型所需的绝对位姿"
            "内点下限。调到 50～100 可以避免图像被配准到场景中看起来相似的错误位置。"),
    ZH_HANT("即 Mapper.abs_pose_min_num_inliers：把一張影像註冊進模型所需的絕對姿態"
            "內點下限。調到 50～100 可以避免影像被註冊到場景中看起來相似的錯誤位置。"),
    KO("Mapper.abs_pose_min_num_inliers입니다. 이미지를 모델에 등록하는 데 필요한 "
       "절대 자세 인라이어의 최소 개수입니다. 50~100으로 올리면 비슷해 보이는 "
       "엉뚱한 위치에 이미지가 등록되는 것을 막을 수 있습니다."),
    DE("Mapper.abs_pose_min_num_inliers: Mindestzahl Inlier der absoluten Pose, "
       "um ein Bild ins Modell aufzunehmen. Auf 50-100 anheben, damit Bilder "
       "nicht am falschen (ähnlich aussehenden) Ort der Szene landen."),
    FR("Mapper.abs_pose_min_num_inliers : nombre minimal d'inliers de pose "
       "absolue pour enregistrer une image dans le modèle. Monter à 50-100 pour "
       "empêcher qu'une image s'enregistre au mauvais endroit, d'aspect "
       "semblable."),
    ES("Mapper.abs_pose_min_num_inliers: mínimo de inliers de pose absoluta "
       "para registrar una imagen en el modelo. Súbelo a 50-100 para que las "
       "imágenes no se registren en la parte equivocada (de aspecto parecido) "
       "de la escena."),
    PT("Mapper.abs_pose_min_num_inliers: mínimo de inliers de pose absoluta "
       "para registrar uma imagem no modelo. Aumente para 50-100 para impedir "
       "que imagens sejam registradas na parte errada (parecida) da cena."),
    IT("Mapper.abs_pose_min_num_inliers: numero minimo di inlier della posa "
       "assoluta per registrare un'immagine nel modello. Portalo a 50-100 per "
       "evitare che le immagini finiscano sulla parte sbagliata (simile) della "
       "scena."),
    NL("Mapper.abs_pose_min_num_inliers: minimum aantal inliers van de absolute "
       "pose om een beeld in het model op te nemen. Zet het op 50-100 zodat "
       "beelden niet op het verkeerde, gelijkend deel van de scène belanden."),
    RU("Mapper.abs_pose_min_num_inliers: минимум инлаеров абсолютной позы, чтобы "
       "зарегистрировать снимок в модели. Поднимите до 50-100, чтобы снимки не "
       "регистрировались на похожем, но неверном участке сцены."),
    TR("Mapper.abs_pose_min_num_inliers: bir görüntüyü modele kaydetmek için "
       "gereken en az mutlak duruş inlier sayısı. 50-100'e çıkarmak, "
       "görüntülerin sahnenin benzer görünen yanlış bölümüne kaydedilmesini "
       "engeller."));

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
    EN("Mapper.abs_pose_max_error: reprojection error threshold (px) for "
       "absolute-pose RANSAC when registering images. Lower (6-8) = stricter; "
       "combine with the inlier thresholds above."),
    JA("Mapper.abs_pose_max_error です。画像を登録するときの絶対姿勢 RANSAC の"
       "再投影誤差のしきい値（px）です。小さいほど（6〜8）厳しくなります。"
       "上のインライアのしきい値と組み合わせて使ってください。"),
    ZH_HANS("即 Mapper.abs_pose_max_error：配准图像时绝对位姿 RANSAC 的重投影误差"
            "阈值（像素）。调小（6～8）更严格；配合上面的内点阈值一起用。"),
    ZH_HANT("即 Mapper.abs_pose_max_error：註冊影像時絕對姿態 RANSAC 的重投影誤差"
            "門檻（像素）。調小（6～8）更嚴格；搭配上面的內點門檻一起用。"),
    KO("Mapper.abs_pose_max_error입니다. 이미지를 등록할 때 절대 자세 RANSAC의 "
       "재투영 오차 임계값(픽셀)입니다. 작을수록(6~8) 엄격합니다. 위의 인라이어 "
       "임계값과 함께 쓰세요."),
    DE("Mapper.abs_pose_max_error: Schwelle des Rückprojektionsfehlers (px) für "
       "das RANSAC der absoluten Pose beim Registrieren. Niedriger (6-8) ist "
       "strenger; zusammen mit den Inlier-Schwellen oben verwenden."),
    FR("Mapper.abs_pose_max_error : seuil d'erreur de reprojection (px) du "
       "RANSAC de pose absolue lors de l'enregistrement. Plus bas (6-8) = plus "
       "strict ; à combiner avec les seuils d'inliers ci-dessus."),
    ES("Mapper.abs_pose_max_error: umbral de error de reproyección (px) del "
       "RANSAC de pose absoluta al registrar imágenes. Más bajo (6-8) es más "
       "estricto; combínalo con los umbrales de inliers de arriba."),
    PT("Mapper.abs_pose_max_error: limiar do erro de reprojeção (px) do RANSAC "
       "de pose absoluta ao registrar imagens. Mais baixo (6-8) é mais rígido; "
       "combine com os limiares de inliers acima."),
    IT("Mapper.abs_pose_max_error: soglia dell'errore di riproiezione (px) per "
       "il RANSAC della posa assoluta durante la registrazione. Più basso (6-8) "
       "è più severo; usalo insieme alle soglie di inlier qui sopra."),
    NL("Mapper.abs_pose_max_error: drempel voor de herprojectiefout (px) van de "
       "RANSAC voor de absolute pose bij het registreren. Lager (6-8) is "
       "strenger; gebruik het samen met de inlier-drempels hierboven."),
    RU("Mapper.abs_pose_max_error: порог ошибки репроекции (px) для RANSAC "
       "абсолютной позы при регистрации снимков. Меньше (6-8) -- строже; "
       "используйте вместе с порогами инлаеров выше."),
    TR("Mapper.abs_pose_max_error: görüntüleri kaydederken mutlak duruş "
       "RANSAC'ının yeniden izdüşüm hatası eşiği (px). Düşük olan (6-8) daha "
       "katıdır; yukarıdaki inlier eşikleriyle birlikte kullanın."));

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
    EN("When the mapper splits the scene into several partial models, try "
       "colmap model_merger to fuse them (kept only when the merged model "
       "registers more images). The trainer otherwise auto-picks the largest "
       "partial model."),
    JA("マッパーがシーンを複数の部分モデルに分けてしまったとき、colmap "
       "model_merger で統合を試みます（統合後のほうが登録画像が多いときだけ"
       "採用します）。そうしない場合、学習側は最大の部分モデルを自動で選びます。"),
    ZH_HANS("当建图把场景拆成好几个局部模型时，尝试用 colmap model_merger 把它们"
            "合起来（只有合并后配准的图像更多才保留）。否则训练端会自动选最大的"
            "那个局部模型。"),
    ZH_HANT("當建圖把場景拆成好幾個局部模型時，嘗試用 colmap model_merger 把它們"
            "合起來（只有合併後註冊的影像更多才保留）。否則訓練端會自動選最大的"
            "那個局部模型。"),
    KO("매퍼가 장면을 여러 부분 모델로 쪼갰을 때 colmap model_merger로 합쳐 봅니다"
       "(합친 쪽이 등록된 이미지가 더 많을 때만 씁니다). 그렇지 않으면 학습 쪽이 "
       "가장 큰 부분 모델을 자동으로 고릅니다."),
    DE("Wenn der Mapper die Szene in mehrere Teilmodelle zerlegt, versuchen, "
       "sie mit colmap model_merger zu verschmelzen (wird nur behalten, wenn "
       "das vereinte Modell mehr Bilder registriert). Sonst nimmt das Training "
       "automatisch das größte Teilmodell."),
    FR("Quand le mapper découpe la scène en plusieurs modèles partiels, essayer "
       "de les fusionner avec colmap model_merger (gardé seulement si le modèle "
       "fusionné enregistre plus d'images). Sinon l'entraînement choisit tout "
       "seul le plus grand modèle partiel."),
    ES("Cuando el mapper parte la escena en varios modelos parciales, intentar "
       "fusionarlos con colmap model_merger (solo se conserva si el modelo "
       "fusionado registra más imágenes). Si no, el entrenamiento elige por su "
       "cuenta el modelo parcial más grande."),
    PT("Quando o mapper divide a cena em vários modelos parciais, tentar "
       "fundi-los com colmap model_merger (só fica se o modelo fundido "
       "registrar mais imagens). Caso contrário, o treinamento escolhe sozinho "
       "o maior modelo parcial."),
    IT("Quando il mapper divide la scena in più modelli parziali, provare a "
       "fonderli con colmap model_merger (tenuto solo se il modello fuso "
       "registra più immagini). Altrimenti l'addestramento sceglie da sé il "
       "modello parziale più grande."),
    NL("Als de mapper de scène in meerdere deelmodellen splitst, proberen ze "
       "met colmap model_merger samen te voegen (alleen behouden als het "
       "samengevoegde model meer beelden registreert). Anders kiest de training "
       "vanzelf het grootste deelmodel."),
    RU("Если маппер разбил сцену на несколько частичных моделей, попытаться "
       "склеить их через colmap model_merger (оставляется, только если "
       "объединённая модель регистрирует больше снимков). Иначе обучение само "
       "берёт самую большую частичную модель."),
    TR("Mapper sahneyi birkaç kısmi modele böldüğünde, colmap model_merger ile "
       "birleştirmeyi dener (yalnızca birleşik model daha çok görüntü "
       "kaydediyorsa tutulur). Aksi hâlde eğitim en büyük kısmi modeli kendisi "
       "seçer."));

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

SS_MSG(python_executable,
    EN("python executable"),
    JA("python の実行ファイル"),
    ZH_HANS("python 可执行文件"),
    ZH_HANT("python 執行檔"),
    KO("python 실행 파일"),
    DE("python-Programmdatei"),
    FR("exécutable python"),
    ES("ejecutable de python"),
    PT("executável do python"),
    IT("eseguibile python"),
    NL("python-programma"),
    RU("исполняемый файл python"),
    TR("python çalıştırılabiliri"));

SS_MSG(python_executable_help,
    EN("Only used by the external masking script."),
    JA("外部のマスキングスクリプトだけが使います。"),
    ZH_HANS("只有外部的蒙版脚本会用到。"),
    ZH_HANT("只有外部的遮罩指令稿會用到。"),
    KO("외부 마스킹 스크립트만 사용합니다."),
    DE("Wird nur vom externen Maskierungsskript benutzt."),
    FR("Utilisé uniquement par le script de masquage externe."),
    ES("Solo lo usa el script externo de enmascarado."),
    PT("Usado apenas pelo script externo de mascaramento."),
    IT("Usato solo dallo script esterno di mascheratura."),
    NL("Wordt alleen gebruikt door het externe maskeerscript."),
    RU("Используется только внешним скриптом маскирования."),
    TR("Yalnızca harici maskeleme betiği kullanır."));

// ===========================================================================
// The segmentation checkpoints (src/app/gui/ModelCache.cpp)
//
// The blurbs all quote the same measurement -- one instance, 1080p frames,
// laptop GPU -- so that they can actually be compared. Keep that parallel
// structure when translating; it is the only thing that makes the choice
// makeable.
// ===========================================================================

SS_MSG(model_sam3_label,
    EN("SAM 3 (recommended)"),
    JA("SAM 3（推奨）"),  ZH_HANS("SAM 3（推荐）"), ZH_HANT("SAM 3（建議）"),
    KO("SAM 3(권장)"),   DE("SAM 3 (empfohlen)"), FR("SAM 3 (recommandé)"),
    ES("SAM 3 (recomendado)"), PT("SAM 3 (recomendado)"), IT("SAM 3 (consigliato)"),
    NL("SAM 3 (aanbevolen)"), RU("SAM 3 (рекомендуется)"), TR("SAM 3 (önerilen)"));

SS_MSG(model_sam3_blurb,
    EN("Understands text prompts -- type what to mask out. 707 MB, ~2 GB "
       "VRAM, about 1 s per frame on a laptop GPU -- 3x slower than any "
       "SAM 2.1 below."),
    JA("テキストのプロンプトを理解します。消したいものを入力してください。"
       "707 MB、VRAM 約 2 GB、ノート PC の GPU で 1 フレームおよそ 1 秒。"
       "下の SAM 2.1 のどれよりも 3 倍遅くなります。"),
    ZH_HANS("能理解文字提示——直接输入要遮掉什么。707 MB，约 2 GB 显存，"
            "笔记本 GPU 上每帧约 1 秒——比下面任何一个 SAM 2.1 都慢 3 倍。"),
    ZH_HANT("能理解文字提示——直接輸入要遮掉什麼。707 MB，約 2 GB 顯示記憶體，"
            "筆電 GPU 上每格約 1 秒——比下面任何一個 SAM 2.1 都慢 3 倍。"),
    KO("텍스트 프롬프트를 이해합니다. 가릴 것을 입력하세요. 707 MB, VRAM 약 "
       "2 GB, 노트북 GPU에서 프레임당 약 1초 — 아래 어떤 SAM 2.1보다도 3배 "
       "느립니다."),
    DE("Versteht Texteingaben -- schreiben Sie hinein, was maskiert werden "
       "soll. 707 MB, etwa 2 GB VRAM, rund 1 s je Bild auf einer Laptop-GPU "
       "-- dreimal langsamer als jedes SAM 2.1 darunter."),
    FR("Comprend les invites textuelles -- écrivez ce qu'il faut masquer. "
       "707 Mo, environ 2 Go de VRAM, à peu près 1 s par image sur un GPU "
       "d'ordinateur portable -- 3 fois plus lent que n'importe quel SAM 2.1 "
       "ci-dessous."),
    ES("Entiende indicaciones de texto: escriba qué enmascarar. 707 MB, unos "
       "2 GB de VRAM, alrededor de 1 s por fotograma en una GPU de portátil: "
       "3 veces más lento que cualquier SAM 2.1 de abajo."),
    PT("Entende comandos de texto -- escreva o que mascarar. 707 MB, cerca de "
       "2 GB de VRAM, aproximadamente 1 s por quadro numa GPU de notebook -- "
       "3 vezes mais lento que qualquer SAM 2.1 abaixo."),
    IT("Capisce il testo: scriva che cosa mascherare. 707 MB, circa 2 GB di "
       "VRAM, all'incirca 1 s per fotogramma su una GPU da portatile: 3 volte "
       "più lento di qualsiasi SAM 2.1 qui sotto."),
    NL("Begrijpt tekstprompts -- typ wat gemaskeerd moet worden. 707 MB, "
       "ongeveer 2 GB VRAM, zo'n 1 s per beeld op een laptop-GPU -- 3 keer "
       "trager dan elke SAM 2.1 hieronder."),
    RU("Понимает текстовые запросы — напишите, что замаскировать. 707 МБ, "
       "около 2 ГБ видеопамяти, примерно 1 с на кадр на ноутбучной "
       "видеокарте — втрое медленнее любой SAM 2.1 ниже."),
    TR("Metin istemlerini anlar -- neyin maskeleneceğini yazın. 707 MB, ~2 GB "
       "VRAM, dizüstü GPU'da kare başına yaklaşık 1 sn -- aşağıdaki her "
       "SAM 2.1'den 3 kat yavaş."));

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
    EN("Click or draw a box to select an object; no text prompts. The most "
       "accurate of the four and the one to pick for thin structure -- "
       "railings, wires and cables, foliage. ~470 ms per frame. Apache-2.0."),
    JA("クリックまたは矩形で対象を選びます。テキストのプロンプトはありません。"
       "4つの中でいちばん正確で、手すり、電線やケーブル、葉のような細い構造には"
       "これを選んでください。1 フレームおよそ 470 ms。Apache-2.0。"),
    ZH_HANS("用点击或拉框来选对象；不支持文字提示。四者中最准确，栏杆、电线电缆、"
            "枝叶这类细结构就选它。每帧约 470 毫秒。Apache-2.0。"),
    ZH_HANT("用點擊或拉框來選物件；不支援文字提示。四者中最準確，欄杆、電線電纜、"
            "枝葉這類細結構就選它。每格約 470 毫秒。Apache-2.0。"),
    KO("클릭하거나 상자를 그려 물체를 고릅니다. 텍스트 프롬프트는 없습니다. "
       "넷 중 가장 정확하며 난간, 전선과 케이블, 잎사귀 같은 가느다란 구조에는 "
       "이것을 고르세요. 프레임당 약 470 ms. Apache-2.0."),
    DE("Zum Auswählen anklicken oder einen Rahmen ziehen; keine Texteingaben. "
       "Das genaueste der vier und die Wahl für feine Strukturen -- Geländer, "
       "Drähte und Kabel, Laub. Etwa 470 ms je Bild. Apache-2.0."),
    FR("Cliquez ou tracez un cadre pour sélectionner un objet ; pas d'invite "
       "textuelle. Le plus précis des quatre et celui à prendre pour les "
       "structures fines -- garde-corps, fils et câbles, feuillage. Environ "
       "470 ms par image. Apache-2.0."),
    ES("Haga clic o dibuje un recuadro para elegir un objeto; sin "
       "indicaciones de texto. El más preciso de los cuatro y el indicado "
       "para estructuras finas: barandillas, cables y tendidos, follaje. Unos "
       "470 ms por fotograma. Apache-2.0."),
    PT("Clique ou desenhe uma caixa para escolher um objeto; sem comandos de "
       "texto. O mais preciso dos quatro e o indicado para estruturas finas: "
       "corrimãos, fios e cabos, folhagem. Cerca de 470 ms por quadro. "
       "Apache-2.0."),
    IT("Clicchi o tracci un rettangolo per scegliere un oggetto; niente "
       "testo. Il più preciso dei quattro e quello da prendere per le "
       "strutture sottili: ringhiere, fili e cavi, fogliame. Circa 470 ms per "
       "fotogramma. Apache-2.0."),
    NL("Klik of trek een kader om een object te kiezen; geen tekstprompts. Het "
       "nauwkeurigste van de vier en de keuze voor fijne structuur -- "
       "leuningen, draden en kabels, gebladerte. Ongeveer 470 ms per beeld. "
       "Apache-2.0."),
    RU("Щелчок или рамка выбирают объект; текстовых запросов нет. Самая точная "
       "из четырёх и та, что нужна для тонких структур — перил, проводов и "
       "кабелей, листвы. Около 470 мс на кадр. Apache-2.0."),
    TR("Nesne seçmek için tıklayın veya kutu çizin; metin istemi yok. Dördü "
       "arasında en doğru olanı ve ince yapılar için seçilecek olanı -- "
       "korkuluk, tel ve kablo, yaprak. Kare başına ~470 ms. Apache-2.0."));

SS_MSG(model_sam21_baseplus_label,
    EN("SAM 2.1 Base+"),  JA("SAM 2.1 Base+"), ZH_HANS("SAM 2.1 Base+"),
    ZH_HANT("SAM 2.1 Base+"), KO("SAM 2.1 Base+"), DE("SAM 2.1 Base+"),
    FR("SAM 2.1 Base+"),  ES("SAM 2.1 Base+"), PT("SAM 2.1 Base+"),
    IT("SAM 2.1 Base+"),  NL("SAM 2.1 Base+"), RU("SAM 2.1 Base+"),
    TR("SAM 2.1 Base+"));

SS_MSG(model_sam21_baseplus_blurb,
    EN("Clicks and boxes only. Close to Large on most subjects at two thirds "
       "the time, ~320 ms per frame. Apache-2.0."),
    JA("クリックと矩形のみです。ほとんどの被写体で Large に近い結果を3分の2の"
       "時間で出します。1 フレームおよそ 320 ms。Apache-2.0。"),
    ZH_HANS("只支持点击和拉框。在多数对象上接近 Large，耗时只有三分之二，"
            "每帧约 320 毫秒。Apache-2.0。"),
    ZH_HANT("只支援點擊和拉框。在多數物件上接近 Large，耗時只有三分之二，"
            "每格約 320 毫秒。Apache-2.0。"),
    KO("클릭과 상자만 됩니다. 대부분의 피사체에서 Large에 가까운 결과를 3분의 2 "
       "시간에 냅니다. 프레임당 약 320 ms. Apache-2.0."),
    DE("Nur Klicks und Rahmen. Bei den meisten Motiven nah an Large, in zwei "
       "Dritteln der Zeit, etwa 320 ms je Bild. Apache-2.0."),
    FR("Clics et cadres seulement. Proche de Large sur la plupart des sujets "
       "en deux tiers du temps, environ 320 ms par image. Apache-2.0."),
    ES("Solo clics y recuadros. Cerca de Large en la mayoría de sujetos en dos "
       "tercios del tiempo, unos 320 ms por fotograma. Apache-2.0."),
    PT("Só cliques e caixas. Perto do Large na maioria dos sujeitos em dois "
       "terços do tempo, cerca de 320 ms por quadro. Apache-2.0."),
    IT("Solo clic e rettangoli. Vicino a Large su quasi tutti i soggetti in "
       "due terzi del tempo, circa 320 ms per fotogramma. Apache-2.0."),
    NL("Alleen klikken en kaders. Dicht bij Large op de meeste onderwerpen in "
       "twee derde van de tijd, ongeveer 320 ms per beeld. Apache-2.0."),
    RU("Только щелчки и рамки. На большинстве объектов близко к Large за две "
       "трети времени, около 320 мс на кадр. Apache-2.0."),
    TR("Yalnızca tıklama ve kutu. Çoğu öznede Large'a yakın, sürenin üçte "
       "ikisinde, kare başına ~320 ms. Apache-2.0."));

SS_MSG(model_sam21_small_label,
    EN("SAM 2.1 Small"),  JA("SAM 2.1 Small"), ZH_HANS("SAM 2.1 Small"),
    ZH_HANT("SAM 2.1 Small"), KO("SAM 2.1 Small"), DE("SAM 2.1 Small"),
    FR("SAM 2.1 Small"),  ES("SAM 2.1 Small"), PT("SAM 2.1 Small"),
    IT("SAM 2.1 Small"),  NL("SAM 2.1 Small"), RU("SAM 2.1 Small"),
    TR("SAM 2.1 Small"));

SS_MSG(model_sam21_small_blurb,
    EN("Clicks and boxes only, ~255 ms per frame. The best speed-for-quality "
       "of the four: below Large the frame is mostly tracking, which does not "
       "care how big the backbone is. Apache-2.0."),
    JA("クリックと矩形のみ、1 フレームおよそ 255 ms。4つの中で速度対品質が"
       "いちばん良い選択です。Large 未満ではフレームの処理はほとんど追跡で、"
       "バックボーンの大きさはあまり効きません。Apache-2.0。"),
    ZH_HANS("只支持点击和拉框，每帧约 255 毫秒。四者中速度与质量的平衡最好："
            "在 Large 以下，每帧的工作主要是跟踪，而跟踪并不在乎主干有多大。"
            "Apache-2.0。"),
    ZH_HANT("只支援點擊和拉框，每格約 255 毫秒。四者中速度與品質的平衡最好："
            "在 Large 以下，每格的工作主要是追蹤，而追蹤並不在乎骨幹有多大。"
            "Apache-2.0。"),
    KO("클릭과 상자만 되며 프레임당 약 255 ms. 넷 중 속도 대비 품질이 가장 "
       "좋습니다. Large 아래에서는 프레임 처리의 대부분이 추적이고, 추적은 "
       "백본 크기에 크게 좌우되지 않습니다. Apache-2.0."),
    DE("Nur Klicks und Rahmen, etwa 255 ms je Bild. Das beste Verhältnis von "
       "Tempo zu Qualität der vier: unterhalb von Large ist ein Bild vor allem "
       "Nachverfolgung, und der ist die Größe des Rückgrats fast egal. "
       "Apache-2.0."),
    FR("Clics et cadres seulement, environ 255 ms par image. Le meilleur "
       "rapport vitesse/qualité des quatre : en dessous de Large, le travail "
       "par image est surtout du suivi, qui se moque de la taille du réseau. "
       "Apache-2.0."),
    ES("Solo clics y recuadros, unos 255 ms por fotograma. La mejor relación "
       "velocidad-calidad de los cuatro: por debajo de Large, el trabajo por "
       "fotograma es sobre todo seguimiento, al que le da igual el tamaño de "
       "la red. Apache-2.0."),
    PT("Só cliques e caixas, cerca de 255 ms por quadro. A melhor relação "
       "velocidade/qualidade dos quatro: abaixo do Large, o trabalho por "
       "quadro é sobretudo rastreamento, que não liga para o tamanho da rede. "
       "Apache-2.0."),
    IT("Solo clic e rettangoli, circa 255 ms per fotogramma. Il miglior "
       "rapporto velocità/qualità dei quattro: sotto Large il lavoro per "
       "fotogramma è soprattutto inseguimento, a cui la dimensione della rete "
       "importa poco. Apache-2.0."),
    NL("Alleen klikken en kaders, ongeveer 255 ms per beeld. De beste "
       "verhouding snelheid/kwaliteit van de vier: onder Large is het werk per "
       "beeld vooral volgen, en dat maalt niet om de grootte van het netwerk. "
       "Apache-2.0."),
    RU("Только щелчки и рамки, около 255 мс на кадр. Лучшее соотношение "
       "скорости и качества из четырёх: ниже Large работа над кадром — это в "
       "основном отслеживание, которому размер сети почти безразличен. "
       "Apache-2.0."),
    TR("Yalnızca tıklama ve kutu, kare başına ~255 ms. Dördü arasında hız/"
       "kalite dengesi en iyi olanı: Large'ın altında kare başına iş çoğunlukla "
       "izlemedir ve izleme omurganın büyüklüğüne pek aldırmaz. Apache-2.0."));

SS_MSG(model_sam21_tiny_label,
    EN("SAM 2.1 Tiny (fastest)"),
    JA("SAM 2.1 Tiny（最速）"), ZH_HANS("SAM 2.1 Tiny（最快）"),
    ZH_HANT("SAM 2.1 Tiny（最快）"), KO("SAM 2.1 Tiny(가장 빠름)"),
    DE("SAM 2.1 Tiny (am schnellsten)"), FR("SAM 2.1 Tiny (le plus rapide)"),
    ES("SAM 2.1 Tiny (el más rápido)"), PT("SAM 2.1 Tiny (o mais rápido)"),
    IT("SAM 2.1 Tiny (il più veloce)"), NL("SAM 2.1 Tiny (snelste)"),
    RU("SAM 2.1 Tiny (самая быстрая)"), TR("SAM 2.1 Tiny (en hızlı)"));

SS_MSG(model_sam21_tiny_blurb,
    EN("Clicks and boxes only, 76 MB. Only 4% quicker than Small at ~245 ms "
       "per frame, and it loses thin structure first -- take it for the "
       "download size, not the speed. Apache-2.0."),
    JA("クリックと矩形のみ、76 MB。1 フレームおよそ 245 ms で Small より 4% "
       "速いだけですし、細い構造から先に失われます。速度ではなく"
       "ダウンロードサイズのために選んでください。Apache-2.0。"),
    ZH_HANS("只支持点击和拉框，76 MB。每帧约 245 毫秒，只比 Small 快 4%，"
            "而且最先丢失细结构——选它是为了下载体积，不是速度。Apache-2.0。"),
    ZH_HANT("只支援點擊和拉框，76 MB。每格約 245 毫秒，只比 Small 快 4%，"
            "而且最先丟失細結構——選它是為了下載體積，不是速度。Apache-2.0。"),
    KO("클릭과 상자만 되며 76 MB. 프레임당 약 245 ms로 Small보다 4% 빠를 뿐이고 "
       "가느다란 구조를 가장 먼저 잃습니다. 속도가 아니라 내려받기 크기 때문에 "
       "고르세요. Apache-2.0."),
    DE("Nur Klicks und Rahmen, 76 MB. Mit etwa 245 ms je Bild nur 4 % "
       "schneller als Small, und feine Strukturen gehen zuerst verloren -- "
       "wegen der Downloadgröße nehmen, nicht wegen des Tempos. Apache-2.0."),
    FR("Clics et cadres seulement, 76 Mo. À environ 245 ms par image, à peine "
       "4 % plus rapide que Small, et c'est lui qui perd les structures fines "
       "en premier -- à prendre pour la taille du téléchargement, pas pour la "
       "vitesse. Apache-2.0."),
    ES("Solo clics y recuadros, 76 MB. Con unos 245 ms por fotograma, apenas "
       "un 4 % más rápido que Small, y es el primero en perder las "
       "estructuras finas: tómelo por el tamaño de descarga, no por la "
       "velocidad. Apache-2.0."),
    PT("Só cliques e caixas, 76 MB. Com cerca de 245 ms por quadro, apenas 4% "
       "mais rápido que o Small, e é o primeiro a perder estruturas finas -- "
       "escolha pelo tamanho do download, não pela velocidade. Apache-2.0."),
    IT("Solo clic e rettangoli, 76 MB. A circa 245 ms per fotogramma è appena "
       "il 4% più veloce di Small, ed è il primo a perdere le strutture "
       "sottili: lo prenda per la dimensione del download, non per la "
       "velocità. Apache-2.0."),
    NL("Alleen klikken en kaders, 76 MB. Met ongeveer 245 ms per beeld maar "
       "4% sneller dan Small, en het verliest fijne structuur het eerst -- "
       "neem het om de downloadgrootte, niet om de snelheid. Apache-2.0."),
    RU("Только щелчки и рамки, 76 МБ. При примерно 245 мс на кадр она быстрее "
       "Small всего на 4 % и первой теряет тонкие структуры — берите её ради "
       "размера загрузки, а не скорости. Apache-2.0."),
    TR("Yalnızca tıklama ve kutu, 76 MB. Kare başına ~245 ms ile Small'dan "
       "yalnızca %4 hızlı ve ince yapıyı ilk kaybeden o -- indirme boyutu "
       "için alın, hız için değil. Apache-2.0."));

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
    EN("SAM 2.1 is Meta's model, released under the Apache 2.0 licence. "
       "Nothing unusual to agree to; it is downloaded rather than bundled "
       "only to keep the app small."),
    JA("SAM 2.1 は Meta のモデルで、Apache 2.0 ライセンスで公開されています。"
       "特別に同意が必要なことはありません。同梱せずダウンロードにしているのは、"
       "アプリを小さく保つためだけです。"),
    ZH_HANS("SAM 2.1 是 Meta 的模型，以 Apache 2.0 许可协议发布。没有什么"
            "特别需要同意的；之所以下载而不是打包，只是为了让应用保持小巧。"),
    ZH_HANT("SAM 2.1 是 Meta 的模型，以 Apache 2.0 授權條款發布。沒有什麼"
            "特別需要同意的；之所以下載而不是打包，只是為了讓應用保持小巧。"),
    KO("SAM 2.1은 Meta의 모델이며 Apache 2.0 라이선스로 공개되어 있습니다. "
       "특별히 동의할 것은 없습니다. 함께 담지 않고 내려받게 한 것은 앱을 작게 "
       "유지하기 위해서일 뿐입니다."),
    DE("SAM 2.1 ist Metas Modell, veröffentlicht unter der Apache-2.0-Lizenz. "
       "Es ist nichts Ungewöhnliches zuzustimmen; heruntergeladen statt "
       "mitgeliefert wird es nur, damit die Anwendung klein bleibt."),
    FR("SAM 2.1 est le modèle de Meta, publié sous licence Apache 2.0. Rien "
       "d'inhabituel à accepter ; il est téléchargé plutôt que livré avec "
       "l'application uniquement pour la garder légère."),
    ES("SAM 2.1 es el modelo de Meta, publicado bajo la licencia Apache 2.0. "
       "No hay nada inusual que aceptar; se descarga en lugar de incluirse "
       "solo para que la aplicación siga siendo pequeña."),
    PT("O SAM 2.1 é o modelo da Meta, publicado sob a licença Apache 2.0. Não "
       "há nada de incomum a aceitar; ele é baixado em vez de embutido apenas "
       "para manter o aplicativo pequeno."),
    IT("SAM 2.1 è il modello di Meta, pubblicato con licenza Apache 2.0. Non "
       "c'è nulla di insolito da accettare; viene scaricato anziché incluso "
       "solo per tenere piccola l'applicazione."),
    NL("SAM 2.1 is het model van Meta, uitgebracht onder de Apache 2.0-"
       "licentie. Er valt niets ongebruikelijks te aanvaarden; het wordt "
       "gedownload in plaats van meegeleverd, alleen om de toepassing klein "
       "te houden."),
    RU("SAM 2.1 — модель Meta, выпущенная под лицензией Apache 2.0. Ничего "
       "необычного принимать не нужно; она загружается, а не поставляется в "
       "комплекте, лишь чтобы программа оставалась небольшой."),
    TR("SAM 2.1 Meta'nın modelidir ve Apache 2.0 lisansıyla yayımlanmıştır. "
       "Kabul edilecek olağandışı bir şey yok; uygulamanın küçük kalması için "
       "birlikte verilmek yerine indiriliyor."));

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

SS_MSG(license_accept_tick,
    EN("I have read and accept these terms"),
    JA("これらの条件を読み、同意します"),
    ZH_HANS("我已阅读并接受这些条款"),
    ZH_HANT("我已閱讀並接受這些條款"),
    KO("이 조건을 읽었고 이에 동의합니다"),
    DE("Ich habe diese Bedingungen gelesen und nehme sie an"),
    FR("J'ai lu et j'accepte ces conditions"),
    ES("He leído y acepto estos términos"),
    PT("Li e aceito estes termos"),
    IT("Ho letto e accetto queste condizioni"),
    NL("Ik heb deze voorwaarden gelezen en aanvaard ze"),
    RU("Я прочитал эти условия и принимаю их"),
    TR("Bu koşulları okudum ve kabul ediyorum"));

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

// The pictures are EXRs, so the colour space under Advanced was filled in from
// their header; {0} is the gamut it found.
SS_MSG(log_exr_color_space,
    EN("These are EXR images: reading them as linear {0}, from the file."),
    JA("EXR 画像です。ファイルの情報に従い、線形 {0} として読み込みます。"),
    ZH_HANS("这些是 EXR 图像：按文件所记录的线性 {0} 读取。"),
    ZH_HANT("這些是 EXR 影像：依檔案所記錄的線性 {0} 讀取。"),
    KO("EXR 이미지입니다. 파일에 기록된 대로 선형 {0}(으)로 읽습니다."),
    DE("Das sind EXR-Bilder: Sie werden laut Datei als lineares {0} gelesen."),
    FR("Ce sont des images EXR : elles sont lues comme {0} linéaire, "
       "d'après le fichier."),
    ES("Son imágenes EXR: se leen como {0} lineal, según el archivo."),
    PT("São imagens EXR: lidas como {0} linear, conforme o arquivo."),
    IT("Sono immagini EXR: vengono lette come {0} lineare, dal file."),
    NL("Dit zijn EXR-beelden: ze worden gelezen als lineair {0}, uit het bestand."),
    RU("Это снимки EXR: они читаются как линейный {0}, по данным файла."),
    TR("Bunlar EXR görüntüleri: dosyaya göre doğrusal {0} olarak okunuyor."));

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
    EN("Ignored {0}: that is a folder of masks, and the images they belong to "
       "were not picked. Add the images folder -- its masks are found on their "
       "own."),
    JA("{0} は無視しました。マスクのフォルダですが、対応する画像が選ばれて"
       "いません。画像のフォルダを追加してください。マスクは自動で見つかります。"),
    ZH_HANS("已忽略 {0}：那是一个蒙版文件夹，但它对应的图像没有被选中。"
            "请添加图像文件夹——它的蒙版会被自动找到。"),
    ZH_HANT("已忽略 {0}：那是一個遮罩資料夾，但它對應的影像沒有被選取。"
            "請新增影像資料夾——它的遮罩會被自動找到。"),
    KO("{0}은(는) 무시했습니다. 마스크 폴더인데 그것이 딸린 이미지가 선택되지 "
       "않았습니다. 이미지 폴더를 추가하세요. 마스크는 알아서 찾습니다."),
    DE("{0} wurde übergangen: das ist ein Maskenordner, und die zugehörigen "
       "Bilder wurden nicht gewählt. Fügen Sie den Bilderordner hinzu -- seine "
       "Masken werden von selbst gefunden."),
    FR("{0} a été ignoré : c'est un dossier de masques, et les images "
       "auxquelles ils appartiennent n'ont pas été choisies. Ajoutez le "
       "dossier d'images -- ses masques sont trouvés tout seuls."),
    ES("Se ignoró {0}: es una carpeta de máscaras y no se eligieron las "
       "imágenes a las que pertenecen. Añada la carpeta de imágenes: sus "
       "máscaras se encuentran solas."),
    PT("{0} foi ignorada: é uma pasta de máscaras, e as imagens a que "
       "pertencem não foram escolhidas. Adicione a pasta de imagens -- as "
       "máscaras dela são encontradas sozinhas."),
    IT("{0} è stata ignorata: è una cartella di maschere e le immagini a cui "
       "appartengono non sono state scelte. Aggiunga la cartella delle "
       "immagini: le sue maschere vengono trovate da sole."),
    NL("{0} is genegeerd: dat is een map met maskers, en de beelden waar ze "
       "bij horen zijn niet gekozen. Voeg de beeldmap toe -- de bijbehorende "
       "maskers worden vanzelf gevonden."),
    RU("{0} пропущена: это папка масок, а снимки, к которым они относятся, не "
       "выбраны. Добавьте папку со снимками — их маски находятся сами."),
    TR("{0} yok sayıldı: burası bir maske klasörü ve ait oldukları görüntüler "
       "seçilmedi. Görüntü klasörünü ekleyin -- maskeleri kendiliğinden "
       "bulunur."));

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

SS_MSG(model_found_reuse,
    EN("This folder already holds a reconstruction. It is kept as it is, and "
       "only the steps below are run over it."),
    JA("このフォルダにはすでに再構成結果があります。そのまま残し、下の工程だけを"
       "その上で実行します。"),
    ZH_HANS("这个文件夹里已经有一份重建结果。它会原样保留，只在其之上运行下面的"
            "步骤。"),
    ZH_HANT("這個資料夾裡已經有一份重建結果。它會原樣保留，只在其之上執行下面的"
            "步驟。"),
    KO("이 폴더에는 이미 재구성 결과가 있습니다. 그대로 두고, 그 위에서 아래 "
       "단계만 실행합니다."),
    DE("In diesem Ordner liegt bereits eine Rekonstruktion. Sie bleibt, wie sie "
       "ist; darüber laufen nur die Schritte unten."),
    FR("Ce dossier contient déjà une reconstruction. Elle est conservée telle "
       "quelle, et seules les étapes ci-dessous s'exécutent par-dessus."),
    ES("Esta carpeta ya contiene una reconstrucción. Se conserva tal cual y "
       "solo se ejecutan sobre ella los pasos de abajo."),
    PT("Esta pasta já contém uma reconstrução. Fica como está, e só os passos "
       "abaixo correm sobre ela."),
    IT("Questa cartella contiene già una ricostruzione. Resta com'è, e sopra di "
       "essa girano solo i passi qui sotto."),
    NL("Deze map bevat al een reconstructie. Die blijft zoals hij is; alleen de "
       "stappen hieronder draaien eroverheen."),
    RU("В этой папке уже есть реконструкция. Она остаётся как есть, поверх неё "
       "выполняются только шаги ниже."),
    TR("Bu klasörde zaten bir yeniden kurma var. Olduğu gibi kalır ve üzerinde "
       "yalnızca aşağıdaki adımlar çalışır."));

SS_MSG(model_will_be_replaced,
    EN("The reconstruction in this folder will be replaced by a new one."),
    JA("このフォルダの再構成結果は、新しいものに置き換えられます。"),
    ZH_HANS("这个文件夹里的重建结果会被新的替换掉。"),
    ZH_HANT("這個資料夾裡的重建結果會被新的取代。"),
    KO("이 폴더의 재구성 결과는 새것으로 바뀝니다."),
    DE("Die Rekonstruktion in diesem Ordner wird durch eine neue ersetzt."),
    FR("La reconstruction de ce dossier sera remplacée par une nouvelle."),
    ES("La reconstrucción de esta carpeta será sustituida por una nueva."),
    PT("A reconstrução desta pasta será substituída por uma nova."),
    IT("La ricostruzione in questa cartella sarà sostituita da una nuova."),
    NL("De reconstructie in deze map wordt door een nieuwe vervangen."),
    RU("Реконструкция в этой папке будет заменена новой."),
    TR("Bu klasördeki yeniden kurma yenisiyle değiştirilecek."));

SS_MSG(reconstruct_again,
    EN("Reconstruct again"),
    JA("再構成をやり直す"), ZH_HANS("重新重建"),  ZH_HANT("重新重建"),
    KO("다시 재구성"),     DE("Neu rekonstruieren"),
    FR("Reconstruire à nouveau"), ES("Reconstruir de nuevo"),
    PT("Reconstruir de novo"), IT("Ricostruisci di nuovo"),
    NL("Opnieuw reconstrueren"), RU("Реконструировать заново"),
    TR("Yeniden kur"));

SS_MSG(reconstruct_again_help,
    EN("Build the cameras again from the images, replacing what is here. Leave "
       "it off to add masks, depth and normals to a dataset that is already "
       "reconstructed -- including one COLMAP, Nerfstudio or Metashape made."),
    JA("画像からカメラを求め直し、いまあるものを置き換えます。オフのままにすると、"
       "すでに再構成済みのデータセット（COLMAP や Nerfstudio、Metashape が作った"
       "ものも含む）に、マスクや深度・法線を足すだけになります。"),
    ZH_HANS("从图像重新求解相机，替换现有结果。保持关闭，就只是给已经重建好的数据"
            "集补上蒙版和深度、法线——包括 COLMAP、Nerfstudio 或 Metashape 做的。"),
    ZH_HANT("從影像重新求解相機，取代現有結果。保持關閉，就只是給已經重建好的資料"
            "集補上遮罩和深度、法線——包括 COLMAP、Nerfstudio 或 Metashape 做的。"),
    KO("이미지에서 카메라를 다시 구해 지금 있는 것을 대체합니다. 꺼 두면 이미 "
       "재구성된 데이터셋(COLMAP, Nerfstudio, Metashape 이 만든 것 포함)에 마스크와 "
       "깊이·법선만 더합니다."),
    DE("Die Kameras erneut aus den Bildern bestimmen und das Vorhandene "
       "ersetzen. Aus gelassen, kommen zu einem bereits rekonstruierten "
       "Datensatz nur Masken, Tiefe und Normalen hinzu -- auch zu einem von "
       "COLMAP, Nerfstudio oder Metashape."),
    FR("Recalculer les caméras à partir des images et remplacer ce qui est là. "
       "Laissé désactivé, cela ajoute seulement masques, profondeur et normales "
       "à un jeu déjà reconstruit -- y compris par COLMAP, Nerfstudio ou "
       "Metashape."),
    ES("Volver a calcular las cámaras desde las imágenes, sustituyendo lo que "
       "hay. Si se deja apagado, solo añade máscaras, profundidad y normales a "
       "un conjunto ya reconstruido, incluido uno de COLMAP, Nerfstudio o "
       "Metashape."),
    PT("Voltar a calcular as câmaras a partir das imagens, substituindo o que "
       "está aqui. Deixado desligado, só acrescenta máscaras, profundidade e "
       "normais a um conjunto já reconstruído, incluindo um do COLMAP, "
       "Nerfstudio ou Metashape."),
    IT("Ricalcolare le fotocamere dalle immagini, sostituendo ciò che c'è. "
       "Lasciato spento, aggiunge soltanto maschere, profondità e normali a un "
       "insieme già ricostruito, anche di COLMAP, Nerfstudio o Metashape."),
    NL("De camera's opnieuw uit de beelden bepalen en vervangen wat er staat. "
       "Uit gelaten voegt dit alleen maskers, diepte en normalen toe aan een "
       "reeds gereconstrueerde dataset -- ook een van COLMAP, Nerfstudio of "
       "Metashape."),
    RU("Заново определить камеры по изображениям, заменив то, что есть. Если "
       "оставить выключенным, к уже реконструированному набору — в том числе "
       "сделанному COLMAP, Nerfstudio или Metashape — только добавятся маски, "
       "глубина и нормали."),
    TR("Kameraları görüntülerden yeniden hesaplar ve buradakini değiştirir. "
       "Kapalı bırakılırsa, zaten yeniden kurulmuş bir veri kümesine -- COLMAP, "
       "Nerfstudio ya da Metashape'in yaptığı biri dahil -- yalnızca maske, "
       "derinlik ve normaller eklenir."));

SS_MSG(update_dataset,
    EN("Update Dataset"),
    JA("データセットを更新"), ZH_HANS("更新数据集"), ZH_HANT("更新資料集"),
    KO("데이터셋 갱신"),    DE("Datensatz ergänzen"),
    FR("Compléter le jeu de données"), ES("Actualizar el conjunto de datos"),
    PT("Atualizar o conjunto de dados"), IT("Aggiorna il set di dati"),
    NL("Dataset bijwerken"), RU("Дополнить набор данных"),
    TR("Veri kümesini güncelle"));

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
    EN("After the reconstruction, run a monocular network over every image and "
       "write the maps beside them. Training reads them by name and uses them "
       "to keep flat things flat -- walls, floors, tables -- which is where "
       "splatting without them goes wrong. About a second an image."),
    JA("再構成のあと、各画像に単眼ネットワークをかけ、マップを画像の隣に書き出し"
       "ます。学習は名前でそれを見つけ、壁・床・机のような平らな面を平らに保つの"
       "に使います。これが無いスプラッティングが崩れるのはまさにそこです。1 枚あ"
       "たり約 1 秒。"),
    ZH_HANS("重建之后，对每张图跑一遍单目网络，把结果写在图像旁边。训练会按名字读"
            "取它们，用来让平的地方保持平——墙面、地板、桌面，正是没有它们时泼溅"
            "最容易出错的地方。每张约一秒。"),
    ZH_HANT("重建之後，對每張圖跑一遍單目網路，把結果寫在影像旁邊。訓練會按名字讀"
            "取它們，用來讓平的地方保持平——牆面、地板、桌面，正是沒有它們時潑濺"
            "最容易出錯的地方。每張約一秒。"),
    KO("재구성 뒤에 모든 이미지에 단안 신경망을 돌려 맵을 이미지 옆에 씁니다. 학습은 "
       "이름으로 그것을 찾아 벽·바닥·책상 같은 평평한 면을 평평하게 유지하는 데 "
       "씁니다. 그것이 없을 때 스플래팅이 무너지는 지점입니다. 장당 약 1 초."),
    DE("Nach der Rekonstruktion ein monokulares Netz über jedes Bild laufen "
       "lassen und die Karten daneben schreiben. Das Training findet sie am "
       "Namen und hält damit Flaches flach -- Wände, Böden, Tische -- genau da, "
       "wo Splatting ohne sie danebengeht. Etwa eine Sekunde je Bild."),
    FR("Après la reconstruction, faire passer un réseau monoculaire sur chaque "
       "image et écrire les cartes à côté. L'entraînement les trouve par leur "
       "nom et s'en sert pour garder plat ce qui est plat -- murs, sols, tables "
       "-- là précisément où le splatting dérape sans elles. Environ une seconde "
       "par image."),
    ES("Tras la reconstrucción, pasar una red monocular por cada imagen y "
       "escribir los mapas al lado. El entrenamiento los encuentra por su nombre "
       "y los usa para mantener plano lo que es plano -- paredes, suelos, mesas "
       "--, justo donde el splatting falla sin ellos. Alrededor de un segundo "
       "por imagen."),
    PT("Depois da reconstrução, passar uma rede monocular por cada imagem e "
       "escrever os mapas ao lado. O treino encontra-os pelo nome e usa-os para "
       "manter plano o que é plano -- paredes, chãos, mesas --, precisamente "
       "onde o splatting falha sem eles. Cerca de um segundo por imagem."),
    IT("Dopo la ricostruzione, far passare una rete monoculare su ogni immagine "
       "e scrivere le mappe accanto. L'addestramento le trova per nome e le usa "
       "per tenere piatto ciò che è piatto -- muri, pavimenti, tavoli -- proprio "
       "dove lo splatting sbaglia senza. Circa un secondo per immagine."),
    NL("Na de reconstructie een monoculair netwerk over elk beeld halen en de "
       "kaarten ernaast schrijven. De training vindt ze op naam en houdt er vlak "
       "mee wat vlak is -- muren, vloeren, tafels -- precies waar splatting "
       "zonder hen misgaat. Ongeveer een seconde per beeld."),
    RU("После реконструкции прогнать по каждому изображению монокулярную сеть и "
       "записать карты рядом. Обучение находит их по имени и держит плоское "
       "плоским — стены, полы, столы, — именно там, где сплаттинг без них "
       "ошибается. Около секунды на изображение."),
    TR("Yeniden kurmadan sonra her görüntüde tek gözlü bir ağ çalıştırıp "
       "haritaları yanlarına yazar. Eğitim onları adıyla bulur ve düz olanı düz "
       "tutmakta kullanır -- duvarlar, zeminler, masalar -- ki bunlar olmadan "
       "splatting tam orada yanılır. Görüntü başına yaklaşık bir saniye."));

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
       "the sharpest maps of the three."),
    JA("ダウンロード 1.3 GB、カード上は 630 MB、1 枚あたり約 0.7 秒。3 つの中で"
       "最も鮮明なマップになります。"),
    ZH_HANS("下载 1.3 GB，显存占 630 MB，每张约 0.7 秒，贴图是三者中最清晰的。"),
    ZH_HANT("下載 1.3 GB，顯存佔 630 MB，每張約 0.7 秒，貼圖是三者中最清晰的。"),
    KO("내려받기 1.3 GB, 카드에서 630 MB, 장당 약 0.7 초로 셋 중 가장 선명한 "
       "맵을 냅니다."),
    DE("1,3 GB Download und 630 MB auf der Karte, etwa 0,7 s je Bild, für die "
       "schärfsten Karten der drei."),
    FR("1,3 Go à télécharger et 630 Mo sur la carte, environ 0,7 s par image, "
       "pour les cartes les plus nettes des trois."),
    ES("1,3 GB de descarga y 630 MB en la tarjeta, unos 0,7 s por imagen, para "
       "los mapas más nítidos de los tres."),
    PT("1,3 GB de transferência e 630 MB na placa, cerca de 0,7 s por imagem, "
       "para os mapas mais nítidos dos três."),
    IT("1,3 GB da scaricare e 630 MB sulla scheda, circa 0,7 s per immagine, "
       "per le mappe più nitide dei tre."),
    NL("1,3 GB download en 630 MB op de kaart, ongeveer 0,7 s per beeld, voor "
       "de scherpste kaarten van de drie."),
    RU("1,3 ГБ загрузки и 630 МБ на карте, около 0,7 с на изображение, ради "
       "самых резких карт из трёх."),
    TR("1,3 GB indirme ve kartta 630 MB, görüntü başına yaklaşık 0,7 s; üçünün "
       "en keskin haritaları."));

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
    EN("The same network and the same camera handling the run uses, on one "
       "picture. It also says how long one image takes, which is the only "
       "honest way to know what the whole capture will cost."),
    JA("実行時と同じネットワーク・同じカメラ処理を、1 枚の絵に対して走らせます。"
       "1 枚あたりの所要時間も出るので、撮影全体にどれだけかかるかを正しく見積も"
       "れます。"),
    ZH_HANS("用与正式运行完全相同的网络和相机处理，跑一张图。它还会给出单张耗时，"
            "这是估算整段拍摄要花多久的唯一可靠办法。"),
    ZH_HANT("用與正式執行完全相同的網路和相機處理，跑一張圖。它還會給出單張耗時，"
            "這是估算整段拍攝要花多久的唯一可靠辦法。"),
    KO("실행 때와 똑같은 신경망과 똑같은 카메라 처리를 그림 한 장에 돌립니다. 장당 "
       "걸리는 시간도 알려 주므로 촬영 전체의 비용을 정확히 가늠할 수 있습니다."),
    DE("Dasselbe Netz und dieselbe Kamerabehandlung wie im Lauf, an einem Bild. "
       "Es nennt auch die Zeit je Bild -- die einzige ehrliche Art zu wissen, was "
       "die ganze Aufnahme kostet."),
    FR("Le même réseau et le même traitement de caméra que l'exécution, sur une "
       "image. Il donne aussi le temps par image, seule façon honnête de savoir "
       "ce que coûtera toute la prise."),
    ES("La misma red y el mismo tratamiento de cámara que la ejecución, sobre una "
       "imagen. También indica el tiempo por imagen, la única forma honesta de "
       "saber qué costará toda la toma."),
    PT("A mesma rede e o mesmo tratamento de câmara que a execução, sobre uma "
       "imagem. Também diz o tempo por imagem, a única forma honesta de saber o "
       "que custará toda a captura."),
    IT("La stessa rete e lo stesso trattamento della fotocamera dell'esecuzione, "
       "su un'immagine. Dice anche il tempo per immagine, l'unico modo onesto di "
       "sapere quanto costerà l'intera ripresa."),
    NL("Hetzelfde netwerk en dezelfde camerabehandeling als de run, op één beeld. "
       "Het noemt ook de tijd per beeld, de enige eerlijke manier om te weten wat "
       "de hele opname kost."),
    RU("Та же сеть и та же обработка камеры, что и в запуске, на одном снимке. "
       "Заодно называет время на изображение — единственный честный способ узнать, "
       "во что обойдётся вся съёмка."),
    TR("Çalıştırmadaki ağın ve kamera işleyişinin aynısı, tek bir resim üzerinde. "
       "Görüntü başına süreyi de söyler; bütün çekimin neye mal olacağını bilmenin "
       "tek dürüst yolu budur."));

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
    EN("Nothing is reconstructed yet, so this assumes the lens named on the "
       "screen ({0}) with no distortion. The run itself uses the "
       "reconstruction's own cameras."),
    JA("まだ再構成が無いので、画面で指定したレンズ（{0}）を歪み無しと仮定して"
       "います。実行時は再構成結果のカメラを使います。"),
    ZH_HANS("目前还没有重建结果，所以这里假定画面上选的镜头（{0}）且无畸变。"
            "正式运行时会使用重建结果自己的相机。"),
    ZH_HANT("目前還沒有重建結果，所以這裡假定畫面上選的鏡頭（{0}）且無畸變。"
            "正式執行時會使用重建結果自己的相機。"),
    KO("아직 재구성이 없어서 화면에서 고른 렌즈({0})를 왜곡 없이 가정합니다. 실제 "
       "실행은 재구성 결과의 카메라를 씁니다."),
    DE("Noch ist nichts rekonstruiert, daher wird das auf dem Bildschirm genannte "
       "Objektiv ({0}) ohne Verzeichnung angenommen. Der Lauf selbst nimmt die "
       "Kameras der Rekonstruktion."),
    FR("Rien n'est encore reconstruit : on suppose l'objectif indiqué à l'écran "
       "({0}) sans distorsion. L'exécution, elle, utilise les caméras de la "
       "reconstruction."),
    ES("Todavía no hay nada reconstruido, así que se supone el objetivo indicado "
       "en pantalla ({0}) sin distorsión. La ejecución usa las cámaras de la "
       "reconstrucción."),
    PT("Ainda não há nada reconstruído, por isso assume-se a objetiva indicada no "
       "ecrã ({0}) sem distorção. A execução usa as câmaras da reconstrução."),
    IT("Non c'è ancora nulla di ricostruito, quindi si assume l'obiettivo "
       "indicato a schermo ({0}) senza distorsione. L'esecuzione usa le "
       "fotocamere della ricostruzione."),
    NL("Er is nog niets gereconstrueerd, dus dit veronderstelt de op het scherm "
       "genoemde lens ({0}) zonder vervorming. De run zelf gebruikt de camera's "
       "van de reconstructie."),
    RU("Реконструкции ещё нет, поэтому берётся объектив, названный на экране "
       "({0}), без дисторсии. Сам запуск использует камеры реконструкции."),
    TR("Henüz yeniden kurulmuş bir şey yok, bu yüzden ekranda belirtilen mercek "
       "({0}) bozulmasız varsayılıyor. Çalıştırmanın kendisi yeniden kurmanın "
       "kameralarını kullanır."));

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

}  // namespace dataset
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
