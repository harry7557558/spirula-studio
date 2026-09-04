#pragma once

// What every `spirula sfm` flag does -- one help sentence per row of
// sfm/SfmConfig.h's SFM_CONFIG_FIELDS table, plus the name of each group they
// are printed under.
//
// The text lives here rather than in the field table for the same reason the
// trainer's does (i18n/catalog/TrainFields.h): `spirula sfm --help` is read by
// whoever is running a reconstruction, and a terminal has no language picker
// to fix it with afterwards.
//
// The link to the table is a TOKEN in the table's last column, so a flag added
// with no entry here is a compile error naming the flag rather than a blank
// line in the help. The FLAG NAMES themselves are never translated --
// `--min-tri-angle` is `--min-tri-angle` in every language -- and neither are
// the choice lists beside them (`low|medium|high|extreme`), which are what the
// reader types.
//
// Two flags may share a name across commands (`--max-error` means one thing to
// the mapper and a looser one to `merge`), so a few entries carry the group in
// their name to keep them apart.
//
// D-numbers (D47, D68, ...) are decision records in src/sfm/README.md. They
// stay as they are: they are a citation, not a word.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace sfmfield {

// ---- the group headings the options are printed under ----

SS_MSG(group_pipeline,
    EN("pipeline"), JA("パイプライン"), ZH_HANS("流程"), ZH_HANT("流程"),
    KO("파이프라인"), DE("Ablauf"), FR("chaîne"), ES("cadena"), PT("cadeia"),
    IT("catena"), NL("keten"), RU("конвейер"), TR("işlem hattı"));

SS_MSG(group_colour,
    EN("colour"), JA("色"), ZH_HANS("颜色"), ZH_HANT("顏色"), KO("색"),
    DE("Farbe"), FR("couleur"), ES("color"), PT("cor"), IT("colore"),
    NL("kleur"), RU("цвет"), TR("renk"));

SS_MSG(group_camera,
    EN("camera"), JA("カメラ"), ZH_HANS("相机"), ZH_HANT("相機"), KO("카메라"),
    DE("Kamera"), FR("caméra"), ES("cámara"), PT("câmera"), IT("camera"),
    NL("camera"), RU("камера"), TR("kamera"));

SS_MSG(group_features,
    EN("features"), JA("特徴点"), ZH_HANS("特征点"), ZH_HANT("特徵點"),
    KO("특징점"), DE("Merkmale"), FR("points"), ES("rasgos"), PT("traços"),
    IT("punti"), NL("kenmerken"), RU("признаки"), TR("öznitelikler"));

SS_MSG(group_matching,
    EN("matching"), JA("マッチング"), ZH_HANS("匹配"), ZH_HANT("匹配"),
    KO("매칭"), DE("Zuordnung"), FR("appariement"), ES("emparejamiento"),
    PT("emparelhamento"), IT("abbinamento"), NL("matchen"),
    RU("сопоставление"), TR("eşleştirme"));

SS_MSG(group_mapper,
    EN("mapper"), JA("マッパー"), ZH_HANS("建图"), ZH_HANT("建圖"),
    KO("매퍼"), DE("Kartierer"), FR("cartographe"), ES("cartógrafo"),
    PT("cartógrafo"), IT("cartografo"), NL("kaartmaker"),
    RU("построитель"), TR("haritalayıcı"));

SS_MSG(group_manage,
    EN("manage"), JA("モデル管理"), ZH_HANS("模型管理"), ZH_HANT("模型管理"),
    KO("모델 관리"), DE("Verwaltung"), FR("gestion"), ES("gestión"),
    PT("gestão"), IT("gestione"), NL("beheer"), RU("управление"),
    TR("yönetim"));

SS_MSG(group_merge,
    EN("merge"), JA("統合"), ZH_HANS("合并"), ZH_HANT("合併"), KO("병합"),
    DE("Zusammenführen"), FR("fusion"), ES("fusión"), PT("fusão"),
    IT("fusione"), NL("samenvoegen"), RU("слияние"), TR("birleştirme"));

SS_MSG(group_input,
    EN("input"), JA("入力"), ZH_HANS("输入"), ZH_HANT("輸入"), KO("입력"),
    DE("Eingabe"), FR("entrée"), ES("entrada"), PT("entrada"), IT("ingresso"),
    NL("invoer"), RU("ввод"), TR("girdi"));

SS_MSG(group_runtime,
    EN("runtime"), JA("実行環境"), ZH_HANS("运行"), ZH_HANT("執行"),
    KO("실행 환경"), DE("Laufzeit"), FR("exécution"), ES("ejecución"),
    PT("execução"), IT("esecuzione"), NL("uitvoering"), RU("выполнение"),
    TR("çalışma"));

// ===========================================================================
// pipeline
// ===========================================================================

SS_MSG(quality_help,
    EN("Working resolution, feature budget and pair-selection breadth"),
    JA("処理する解像度、特徴点の予算、ペア選択の広さ"),
    ZH_HANS("处理分辨率、特征点预算与像对筛选的广度"),
    ZH_HANT("處理解析度、特徵點預算與影像對篩選的廣度"),
    KO("처리 해상도, 특징점 예산, 쌍 선택의 폭"),
    DE("Arbeitsauflösung, Merkmalsbudget und Breite der Paarauswahl"),
    FR("Résolution de travail, budget de points et largeur de la sélection de "
       "paires"),
    ES("Resolución de trabajo, presupuesto de rasgos y amplitud de la selección "
       "de pares"),
    PT("Resolução de trabalho, orçamento de traços e amplitude da seleção de "
       "pares"),
    IT("Risoluzione di lavoro, budget di punti e ampiezza della selezione delle "
       "coppie"),
    NL("Werkresolutie, kenmerkbudget en breedte van de paarselectie"),
    RU("Рабочее разрешение, бюджет признаков и широта отбора пар"),
    TR("Çalışma çözünürlüğü, öznitelik bütçesi ve çift seçiminin genişliği"));

SS_MSG(data_type_help,
    EN("What the capture is: individual photos, video frames, or an unordered "
       "internet collection"),
    JA("撮影の種類: 個別の写真、動画のフレーム、または順序のないインターネット"
       "収集"),
    ZH_HANS("拍摄的性质：独立照片、视频帧，或来自互联网的无序集合"),
    ZH_HANT("拍攝的性質：獨立照片、影片影格，或來自網際網路的無序集合"),
    KO("촬영의 성격: 개별 사진, 동영상 프레임, 또는 순서 없는 인터넷 모음"),
    DE("Was die Aufnahme ist: einzelne Fotos, Videobilder oder eine ungeordnete "
       "Internetsammlung"),
    FR("Ce qu'est la prise de vue : photos individuelles, images de vidéo, ou "
       "collection internet sans ordre"),
    ES("Qué es la captura: fotos sueltas, fotogramas de vídeo, o una colección "
       "de internet sin orden"),
    PT("O que é a captura: fotos avulsas, quadros de vídeo, ou uma coleção da "
       "internet sem ordem"),
    IT("Che cos'è la ripresa: foto singole, fotogrammi di un video, o una "
       "raccolta internet senza ordine"),
    NL("Wat de opname is: losse foto's, videobeelden, of een ongeordende "
       "internetverzameling"),
    RU("Что представляет собой съёмка: отдельные фотографии, кадры видео или "
       "неупорядоченная интернет-подборка"),
    TR("Çekimin ne olduğu: tek tek fotoğraflar, video kareleri ya da sırasız "
       "bir internet derlemesi"));

SS_MSG(pairs_help,
    EN("Which image pairs are matched; auto is GPU pair selection at 100 images "
       "or more, sequential (plus loop closure) for video below that, and "
       "exhaustive otherwise"),
    JA("どの画像ペアをマッチングするか。auto は画像 100 枚以上なら GPU による"
       "ペア選択、それ未満の動画なら sequential（ループ閉じ込みつき）、それ以外は "
       "exhaustive"),
    ZH_HANS("匹配哪些图像对；auto 表示图像达到 100 张以上时用 GPU 筛选像对，"
            "低于此数的视频用 sequential（并做回环闭合），其余情况用 exhaustive"),
    ZH_HANT("匹配哪些影像對；auto 表示影像達到 100 張以上時用 GPU 篩選影像對，"
            "低於此數的影片用 sequential（並做迴環閉合），其餘情況用 exhaustive"),
    KO("어떤 이미지 쌍을 매칭할지. auto 는 이미지가 100장 이상이면 GPU 쌍 선택, "
       "그보다 적은 동영상이면 sequential(루프 클로저 포함), 그 밖에는 exhaustive"),
    DE("Welche Bildpaare zugeordnet werden; auto heißt GPU-Paarauswahl ab 100 "
       "Bildern, sequential (mit Schleifenschluss) für Video darunter, sonst "
       "exhaustive"),
    FR("Quelles paires d'images sont appariées ; auto vaut sélection de paires "
       "sur GPU à partir de 100 images, sequential (avec fermeture de boucle) "
       "pour la vidéo en dessous, et exhaustive sinon"),
    ES("Qué pares de imágenes se emparejan; auto es selección de pares en la GPU "
       "a partir de 100 imágenes, sequential (con cierre de bucle) para vídeo "
       "por debajo de eso, y exhaustive en los demás casos"),
    PT("Quais pares de imagens são emparelhados; auto é seleção de pares na GPU "
       "a partir de 100 imagens, sequential (com fechamento de laço) para vídeo "
       "abaixo disso, e exhaustive nos demais casos"),
    IT("Quali coppie di immagini vengono abbinate; auto è la selezione di coppie "
       "su GPU da 100 immagini in su, sequential (con chiusura d'anello) per il "
       "video al di sotto, ed exhaustive negli altri casi"),
    NL("Welke beeldparen gematcht worden; auto is paarselectie op de GPU vanaf "
       "100 beelden, sequential (met lussluiting) voor video daaronder, en "
       "anders exhaustive"),
    RU("Какие пары изображений сопоставляются; auto -- это отбор пар на GPU от "
       "100 изображений, sequential (с замыканием петли) для видео ниже этого "
       "числа и exhaustive в остальных случаях"),
    TR("Hangi görüntü çiftlerinin eşleştirileceği; auto, 100 görüntüden itibaren "
       "GPU ile çift seçimi, bunun altındaki videoda sequential (döngü "
       "kapatmayla), diğer durumlarda exhaustive demektir"));

SS_MSG(overlap_help,
    EN("Neighbours each image is paired with under --pairs sequential"),
    JA("--pairs sequential のとき各画像がペアを組む近傍の数"),
    ZH_HANS("在 --pairs sequential 下，每张图像与多少个相邻图像配对"),
    ZH_HANT("在 --pairs sequential 下，每張影像與多少個相鄰影像配對"),
    KO("--pairs sequential 일 때 각 이미지가 짝을 이루는 이웃의 수"),
    DE("Nachbarn, mit denen jedes Bild unter --pairs sequential gepaart wird"),
    FR("Voisins avec lesquels chaque image est appariée sous --pairs sequential"),
    ES("Vecinos con los que se empareja cada imagen bajo --pairs sequential"),
    PT("Vizinhos com que cada imagem é emparelhada sob --pairs sequential"),
    IT("Vicini con cui ogni immagine viene abbinata sotto --pairs sequential"),
    NL("Buren waarmee elk beeld gepaard wordt onder --pairs sequential"),
    RU("Сколько соседей получает каждое изображение при --pairs sequential"),
    TR("--pairs sequential altında her görüntünün eşleştiği komşu sayısı"));

SS_MSG(loop_closure_help,
    EN("Under --pairs sequential, also match the content-similar pairs GPU pair "
       "selection finds, so a capture that revisits a place links back to it"),
    JA("--pairs sequential のとき、GPU のペア選択が見つけた内容の似たペアも"
       "マッチングします。同じ場所に戻ってくる撮影がそこへつながります"),
    ZH_HANS("在 --pairs sequential 下，也匹配 GPU 筛选找出的内容相似像对，"
            "让重新走回同一处的拍摄能连回去"),
    ZH_HANT("在 --pairs sequential 下，也匹配 GPU 篩選找出的內容相似影像對，"
            "讓重新走回同一處的拍攝能連回去"),
    KO("--pairs sequential 일 때 GPU 쌍 선택이 찾아낸 내용이 비슷한 쌍도 "
       "매칭합니다. 같은 곳으로 되돌아오는 촬영이 그곳과 다시 이어집니다"),
    DE("Unter --pairs sequential auch die inhaltlich ähnlichen Paare zuordnen, "
       "die die GPU-Paarauswahl findet, damit eine Aufnahme, die an einen Ort "
       "zurückkehrt, sich dort wieder anschließt"),
    FR("Sous --pairs sequential, apparier aussi les paires au contenu proche que "
       "trouve la sélection sur GPU, pour qu'une prise de vue repassant au même "
       "endroit s'y raccroche"),
    ES("Bajo --pairs sequential, emparejar también los pares de contenido "
       "parecido que encuentra la selección en la GPU, para que una captura que "
       "vuelve a pasar por un sitio se enlace de nuevo con él"),
    PT("Sob --pairs sequential, emparelhar também os pares de conteúdo parecido "
       "que a seleção na GPU encontra, para que uma captura que volta a um lugar "
       "se ligue de novo a ele"),
    IT("Sotto --pairs sequential, abbinare anche le coppie dal contenuto simile "
       "che la selezione su GPU trova, così una ripresa che ripassa da un luogo "
       "vi si riaggancia"),
    NL("Onder --pairs sequential ook de inhoudelijk gelijkende paren matchen die "
       "de GPU-paarselectie vindt, zodat een opname die een plek opnieuw "
       "aandoet daar weer op aansluit"),
    RU("При --pairs sequential сопоставлять и близкие по содержанию пары, "
       "которые находит отбор на GPU, чтобы съёмка, вернувшаяся в то же место, "
       "связалась с ним снова"),
    TR("--pairs sequential altında, GPU çift seçiminin bulduğu içerikçe benzer "
       "çiftleri de eşleştir; böylece aynı yere dönen bir çekim oraya geri "
       "bağlanır"));

SS_MSG(max_error_help,
    EN("Inlier radius for verification and mapping, in pixels of the image SIFT "
       "ran on rather than of the source file (D47)"),
    JA("検証とマッピングでのインライア半径。元ファイルではなく SIFT が実際に"
       "処理した画像のピクセル単位（D47）"),
    ZH_HANS("验证与建图时的内点半径，单位是 SIFT 实际处理的图像的像素，"
            "而非源文件的像素（D47）"),
    ZH_HANT("驗證與建圖時的內點半徑，單位是 SIFT 實際處理的影像的像素，"
            "而非原始檔案的像素（D47）"),
    KO("검증과 매핑에서의 내부점 반지름. 원본 파일이 아니라 SIFT 가 실제로 "
       "처리한 이미지의 픽셀 단위(D47)"),
    DE("Inlier-Radius für Prüfung und Kartierung, in Pixeln des Bildes, auf dem "
       "SIFT lief, nicht der Quelldatei (D47)"),
    FR("Rayon d'inlier pour la vérification et la cartographie, en pixels de "
       "l'image sur laquelle SIFT a tourné, non du fichier source (D47)"),
    ES("Radio de inlier para la verificación y la cartografía, en píxeles de la "
       "imagen sobre la que corrió SIFT, no del archivo de origen (D47)"),
    PT("Raio de inlier para a verificação e a cartografia, em pixels da imagem "
       "em que o SIFT rodou, não do arquivo de origem (D47)"),
    IT("Raggio di inlier per verifica e cartografia, in pixel dell'immagine su "
       "cui è girato SIFT, non del file di origine (D47)"),
    NL("Inlier-straal voor verificatie en kartering, in pixels van het beeld "
       "waarop SIFT liep, niet van het bronbestand (D47)"),
    RU("Радиус инлайера для проверки и построения, в пикселях того изображения, "
       "на котором работал SIFT, а не исходного файла (D47)"),
    TR("Doğrulama ve haritalama için içleyen yarıçapı; kaynak dosyanın değil, "
       "SIFT'in üzerinde çalıştığı görüntünün pikseli cinsinden (D47)"));

SS_MSG(max_image_size_help,
    EN("Longest edge the extractor runs on; larger images are downscaled first, "
       "and keypoints are still reported in the source image's pixels. 0 picks "
       "the frontend's own default (3200 for sift, 1600 for aliked)"),
    JA("抽出器が処理する長辺の上限。これより大きい画像は先に縮小されますが、"
       "キーポイントは元画像のピクセルで報告されます。0 ならフロントエンド既定"
       "（sift は 3200、aliked は 1600）"),
    ZH_HANS("提取器处理的最长边；更大的图像会先缩小，但关键点仍按源图像的像素报告。"
            "0 表示采用前端自身的默认值（sift 为 3200，aliked 为 1600）"),
    ZH_HANT("擷取器處理的最長邊；更大的影像會先縮小，但關鍵點仍按原始影像的像素回報。"
            "0 表示採用前端自身的預設值（sift 為 3200，aliked 為 1600）"),
    KO("추출기가 처리하는 가장 긴 변. 이보다 큰 이미지는 먼저 줄이지만 키포인트는 "
       "원본 이미지의 픽셀로 보고합니다. 0 이면 프런트엔드 자체 기본값"
       "(sift 는 3200, aliked 는 1600)"),
    DE("Längste Kante, auf der der Extraktor läuft; größere Bilder werden zuerst "
       "verkleinert, Schlüsselpunkte werden dennoch in Pixeln des Quellbildes "
       "gemeldet. 0 nimmt die Vorgabe des Frontends (3200 für sift, 1600 für "
       "aliked)"),
    FR("Plus grand côté sur lequel tourne l'extracteur ; les images plus grandes "
       "sont d'abord réduites, et les points clés restent donnés en pixels de "
       "l'image source. 0 prend la valeur par défaut du frontal (3200 pour sift, "
       "1600 pour aliked)"),
    ES("Lado más largo sobre el que corre el extractor; las imágenes mayores se "
       "reducen antes, y los puntos clave se siguen dando en píxeles de la "
       "imagen de origen. 0 toma el valor por defecto del frontal (3200 para "
       "sift, 1600 para aliked)"),
    PT("Maior lado em que o extrator roda; imagens maiores são reduzidas antes, e "
       "os pontos-chave continuam sendo dados em pixels da imagem de origem. 0 "
       "toma o padrão do frontal (3200 para sift, 1600 para aliked)"),
    IT("Lato più lungo su cui gira l'estrattore; le immagini più grandi vengono "
       "prima ridotte, e i punti chiave restano espressi in pixel dell'immagine "
       "di origine. 0 prende il valore predefinito del frontend (3200 per sift, "
       "1600 per aliked)"),
    NL("Langste zijde waarop de extractor draait; grotere beelden worden eerst "
       "verkleind, en sleutelpunten blijven in pixels van het bronbeeld. 0 neemt "
       "de eigen standaard van de frontend (3200 voor sift, 1600 voor aliked)"),
    RU("Наибольшая сторона, на которой работает извлекатель; изображения крупнее "
       "сперва уменьшаются, а ключевые точки всё равно даются в пикселях "
       "исходного изображения. 0 берёт значение по умолчанию самого фронтенда "
       "(3200 для sift, 1600 для aliked)"),
    TR("Çıkarıcının üzerinde çalıştığı en uzun kenar; daha büyük görüntüler önce "
       "küçültülür, anahtar noktalar yine de kaynak görüntünün pikseliyle "
       "bildirilir. 0, ön ucun kendi varsayılanını seçer (sift için 3200, aliked "
       "için 1600)"));

SS_MSG(masks_help,
    EN("Directory of masks; keypoints on zero (black) pixels are dropped. auto "
       "defaults it to `masks` beside the image directory"),
    JA("マスクのディレクトリ。値が 0（黒）の画素にあるキーポイントは捨てられます。"
       "auto では画像ディレクトリの隣の `masks` が既定になります"),
    ZH_HANS("掩码目录；落在取值为 0（黑色）像素上的关键点会被丢弃。auto 默认取"
            "图像目录旁边的 `masks`"),
    ZH_HANT("遮罩目錄；落在取值為 0（黑色）像素上的關鍵點會被丟棄。auto 預設取"
            "影像目錄旁邊的 `masks`"),
    KO("마스크 디렉터리. 값이 0(검정)인 화소 위의 키포인트는 버립니다. auto 는 "
       "이미지 디렉터리 옆의 `masks` 를 기본값으로 씁니다"),
    DE("Verzeichnis der Masken; Schlüsselpunkte auf Pixeln mit Wert null "
       "(schwarz) entfallen. auto nimmt dafür `masks` neben dem Bildverzeichnis"),
    FR("Dossier des masques ; les points clés sur des pixels à zéro (noirs) sont "
       "écartés. auto prend par défaut `masks` à côté du dossier d'images"),
    ES("Carpeta de máscaras; los puntos clave sobre píxeles a cero (negros) se "
       "descartan. auto toma por defecto `masks` junto a la carpeta de imágenes"),
    PT("Pasta de máscaras; os pontos-chave sobre pixels em zero (pretos) são "
       "descartados. auto toma por padrão `masks` ao lado da pasta de imagens"),
    IT("Cartella delle maschere; i punti chiave su pixel a zero (neri) vengono "
       "scartati. auto prende come predefinito `masks` accanto alla cartella "
       "delle immagini"),
    NL("Map met maskers; sleutelpunten op pixels met waarde nul (zwart) vervallen. "
       "auto neemt standaard `masks` naast de beeldmap"),
    RU("Каталог масок; ключевые точки на нулевых (чёрных) пикселях отбрасываются. "
       "auto по умолчанию берёт `masks` рядом с каталогом изображений"),
    TR("Maske dizini; değeri sıfır (siyah) olan piksellerdeki anahtar noktalar "
       "atılır. auto, görüntü dizininin yanındaki `masks` dizinini varsayar"));

SS_MSG(flip_mask_help,
    EN("Swap keep and ignore in every mask, for the exporters that paint the "
       "region to REMOVE rather than the region to keep"),
    JA("すべてのマスクで残す領域と無視する領域を入れ替えます。残す領域ではなく取"
       "り除く領域を塗る書き出しツール向けです"),
    ZH_HANS("交换每个掩码里保留与忽略的区域，适用于画出要去掉的区域而不是要保留"
            "区域的导出工具"),
    ZH_HANT("交換每個遮罩裡保留與忽略的區域，適用於畫出要去掉的區域而不是要保留"
            "區域的匯出工具"),
    KO("모든 마스크에서 남길 영역과 무시할 영역을 맞바꿉니다. 남길 영역이 아니라 "
       "지울 영역을 칠하는 내보내기 도구를 위한 것입니다"),
    DE("Vertauscht in jeder Maske Behalten und Ignorieren, für Exporter, die den "
       "zu ENTFERNENDEN Bereich zeichnen statt den zu behaltenden"),
    FR("Échange garder et ignorer dans chaque masque, pour les exporteurs qui "
       "peignent la zone à RETIRER plutôt que celle à garder"),
    ES("Intercambia conservar e ignorar en cada máscara, para los exportadores "
       "que pintan la zona a QUITAR en vez de la que se conserva"),
    PT("Troca manter e ignorar em cada máscara, para os exportadores que pintam a "
       "área a REMOVER em vez da área a manter"),
    IT("Scambia conserva e ignora in ogni maschera, per gli esportatori che "
       "dipingono l'area da TOGLIERE invece di quella da conservare"),
    NL("Verwisselt behouden en negeren in elk masker, voor exporteurs die het te "
       "VERWIJDEREN gebied inkleuren in plaats van het te behouden gebied"),
    RU("Меняет местами «сохранить» и «пропустить» в каждой маске — для "
       "экспортёров, которые закрашивают УДАЛЯЕМУЮ область, а не сохраняемую"),
    TR("Her maskede korunanla yok sayılanı yer değiştirir; korunacak alan yerine "
       "KALDIRILACAK alanı boyayan dışa aktarma araçları için"));

SS_MSG(mask_dir_help,
    EN("Alias of --masks"),
    JA("--masks の別名"),
    ZH_HANS("--masks 的别名"),
    ZH_HANT("--masks 的別名"),
    KO("--masks 의 다른 이름"),
    DE("Anderer Name für --masks"),
    FR("Autre nom de --masks"),
    ES("Otro nombre de --masks"),
    PT("Outro nome de --masks"),
    IT("Altro nome di --masks"),
    NL("Andere naam voor --masks"),
    RU("Другое имя для --masks"),
    TR("--masks için başka bir ad"));

// ===========================================================================
// colour
// ===========================================================================

SS_MSG(image_gamut_help,
    EN("Colour primaries the input images were captured in. Pixels are "
       "converted to sRGB before feature extraction, which is what the "
       "detectors and the AI models expect"),
    JA("入力画像が撮影された色域。特徴抽出の前に sRGB へ変換されます。検出器と "
       "AI モデルはそれを前提としています"),
    ZH_HANS("输入图像拍摄时使用的色域。像素在特征提取前转换为 sRGB，检测器和 AI "
            "模型都以此为前提"),
    ZH_HANT("輸入影像拍攝時使用的色域。像素在特徵擷取前轉換為 sRGB，偵測器與 AI "
            "模型都以此為前提"),
    KO("입력 이미지가 촬영된 색역. 픽셀은 특징 추출 전에 sRGB 로 변환되며, "
       "검출기와 AI 모델이 이를 전제로 합니다"),
    DE("Farbprimärvalenzen der Eingabebilder. Pixel werden vor der "
       "Merkmalsextraktion nach sRGB gewandelt, worauf die Detektoren und die "
       "KI-Modelle ausgelegt sind"),
    FR("Primaires de couleur des images d'entrée. Les pixels passent en sRGB "
       "avant l'extraction, ce que les détecteurs et les modèles d'IA "
       "attendent"),
    ES("Primarios de color de las imágenes de entrada. Los píxeles pasan a sRGB "
       "antes de la extracción, que es lo que esperan los detectores y los "
       "modelos de IA"),
    PT("Primárias de cor das imagens de entrada. Os pixels passam a sRGB antes "
       "da extração, que é o que os detectores e os modelos de IA esperam"),
    IT("Primarie di colore delle immagini di ingresso. I pixel passano a sRGB "
       "prima dell'estrazione, che è ciò che i rilevatori e i modelli di IA si "
       "aspettano"),
    NL("Kleurprimairen van de invoerbeelden. Pixels gaan naar sRGB voor de "
       "extractie, waarop de detectoren en de AI-modellen zijn gebouwd"),
    RU("Основные цвета исходных изображений. Пиксели переводятся в sRGB перед "
       "выделением признаков — именно этого ждут детекторы и модели ИИ"),
    TR("Girdi görüntülerinin çekildiği renk birincilleri. Pikseller öznitelik "
       "çıkarımından önce sRGB'ye çevrilir; algılayıcılar ve yapay zekâ "
       "modelleri bunu bekler"));

SS_MSG(image_linear_help,
    EN("Treat the input images as linear light rather than display-encoded"),
    JA("入力画像を表示用エンコードではなくリニア光として扱う"),
    ZH_HANS("将输入图像视为线性光而非显示编码"),
    ZH_HANT("將輸入影像視為線性光而非顯示編碼"),
    KO("입력 이미지를 디스플레이 인코딩이 아니라 선형 광으로 취급"),
    DE("Eingabebilder als lineares Licht statt als anzeigecodiert behandeln"),
    FR("Traiter les images d'entrée comme de la lumière linéaire plutôt "
       "qu'encodées pour l'affichage"),
    ES("Tratar las imágenes de entrada como luz lineal en vez de codificadas "
       "para pantalla"),
    PT("Tratar as imagens de entrada como luz linear em vez de codificadas "
       "para exibição"),
    IT("Trattare le immagini di ingresso come luce lineare anziché codificate "
       "per lo schermo"),
    NL("Invoerbeelden als lineair licht behandelen in plaats van "
       "weergavegecodeerd"),
    RU("Считать исходные изображения линейным светом, а не экранно "
       "закодированными"),
    TR("Girdi görüntülerini ekran kodlu değil, doğrusal ışık olarak ele al"));

SS_MSG(point_color_help,
    EN("Colour space the sparse point cloud is written in. srgb matches the "
       "trainer's convert-initial-point-cloud-color; image writes the points "
       "in the same space as the photographs"),
    JA("疎な点群を書き出す色空間。srgb は学習側の "
       "convert-initial-point-cloud-color に対応し、image は写真と同じ空間で"
       "書き出します"),
    ZH_HANS("稀疏点云写出的色彩空间。srgb 与训练端的 "
            "convert-initial-point-cloud-color 对应，image 则与照片同一空间"),
    ZH_HANT("稀疏點雲寫出的色彩空間。srgb 與訓練端的 "
            "convert-initial-point-cloud-color 對應，image 則與照片同一空間"),
    KO("희소 점 구름을 기록할 색 공간. srgb 는 학습 쪽의 "
       "convert-initial-point-cloud-color 에 대응하고, image 는 사진과 같은 "
       "공간으로 씁니다"),
    DE("Farbraum, in dem die dünne Punktwolke geschrieben wird. srgb passt zu "
       "convert-initial-point-cloud-color des Trainings; image schreibt die "
       "Punkte im Raum der Fotos"),
    FR("Espace colorimétrique du nuage de points épars. srgb correspond au "
       "convert-initial-point-cloud-color de l'entraînement ; image écrit les "
       "points dans l'espace des photographies"),
    ES("Espacio de color del nube de puntos dispersa. srgb concuerda con el "
       "convert-initial-point-cloud-color del entrenamiento; image escribe los "
       "puntos en el espacio de las fotografías"),
    PT("Espaço de cor da nuvem de pontos esparsa. srgb combina com o "
       "convert-initial-point-cloud-color do treino; image escreve os pontos "
       "no espaço das fotografias"),
    IT("Spazio colore in cui è scritta la nuvola di punti sparsa. srgb combacia "
       "con convert-initial-point-cloud-color dell'addestramento; image scrive "
       "i punti nello spazio delle fotografie"),
    NL("Kleurruimte waarin de dunne puntenwolk wordt geschreven. srgb sluit aan "
       "op convert-initial-point-cloud-color van de training; image schrijft de "
       "punten in de ruimte van de foto's"),
    RU("Цветовое пространство разреженного облака точек. srgb соответствует "
       "convert-initial-point-cloud-color обучения; image пишет точки в "
       "пространстве фотографий"),
    TR("Seyrek nokta bulutunun yazıldığı renk uzayı. srgb, eğitimdeki "
       "convert-initial-point-cloud-color ile eşleşir; image noktaları "
       "fotoğraflarla aynı uzayda yazar"));

// ===========================================================================
// camera
// ===========================================================================

SS_MSG(camera_mode_help,
    EN("How images are grouped into cameras; every mode splits on image "
       "resolution first"),
    JA("画像をどうカメラごとにまとめるか。どのモードでもまず画像の解像度で"
       "分けます"),
    ZH_HANS("如何把图像归入相机；任何模式都先按图像分辨率划分"),
    ZH_HANT("如何把影像歸入相機；任何模式都先按影像解析度劃分"),
    KO("이미지를 카메라로 묶는 방식. 어느 모드든 이미지 해상도로 먼저 나눕니다"),
    DE("Wie Bilder zu Kameras gruppiert werden; jeder Modus trennt zuerst nach "
       "Bildauflösung"),
    FR("Comment les images sont regroupées en caméras ; tout mode sépare "
       "d'abord par résolution d'image"),
    ES("Cómo se agrupan las imágenes en cámaras; todo modo separa primero por "
       "resolución de imagen"),
    PT("Como as imagens são agrupadas em câmeras; todo modo separa primeiro por "
       "resolução de imagem"),
    IT("Come le immagini vengono raggruppate in camere; ogni modalità divide "
       "prima per risoluzione dell'immagine"),
    NL("Hoe beelden tot camera's gegroepeerd worden; elke modus splitst eerst op "
       "beeldresolutie"),
    RU("Как изображения объединяются в камеры; любой режим сперва делит по "
       "разрешению изображения"),
    TR("Görüntülerin kameralara nasıl gruplandığı; her kip önce görüntü "
       "çözünürlüğüne göre ayırır"));

SS_MSG(camera_model_help,
    EN("Distortion model fitted to each camera group; also takes PREFIX=MODEL to "
       "set one group"),
    JA("各カメラグループに当てはめる歪みモデル。PREFIX=MODEL の形で 1 つの"
       "グループだけ指定することもできます"),
    ZH_HANS("为每个相机组拟合的畸变模型；也可写成 PREFIX=MODEL 只设定某一组"),
    ZH_HANT("為每個相機群組擬合的畸變模型；也可寫成 PREFIX=MODEL 只設定某一組"),
    KO("각 카메라 그룹에 맞추는 왜곡 모델. PREFIX=MODEL 형태로 한 그룹만 지정할 "
       "수도 있습니다"),
    DE("Verzeichnungsmodell, das an jede Kameragruppe angepasst wird; nimmt auch "
       "PREFIX=MODEL, um eine einzelne Gruppe zu setzen"),
    FR("Modèle de distorsion ajusté à chaque groupe de caméras ; accepte aussi "
       "PREFIX=MODEL pour n'en régler qu'un"),
    ES("Modelo de distorsión ajustado a cada grupo de cámaras; también acepta "
       "PREFIX=MODEL para fijar uno solo"),
    PT("Modelo de distorção ajustado a cada grupo de câmeras; também aceita "
       "PREFIX=MODEL para definir apenas um"),
    IT("Modello di distorsione adattato a ogni gruppo di camere; accetta anche "
       "PREFIX=MODEL per impostarne uno solo"),
    NL("Vertekeningsmodel dat op elke cameragroep gepast wordt; neemt ook "
       "PREFIX=MODEL om er één te zetten"),
    RU("Модель дисторсии, подгоняемая к каждой группе камер; принимает и "
       "PREFIX=MODEL, чтобы задать одну группу"),
    TR("Her kamera grubuna uydurulan bozulma modeli; tek bir grubu ayarlamak "
       "için PREFIX=MODEL biçimini de alır"));

SS_MSG(focal_help,
    EN("Starting focal length in pixels, 0 to guess from EXIF or image size; "
       "also takes PREFIX=F"),
    JA("初期焦点距離（ピクセル）。0 なら EXIF か画像サイズから推測します。"
       "PREFIX=F の形も使えます"),
    ZH_HANS("初始焦距，单位像素；0 表示从 EXIF 或图像尺寸推测。也可写成 PREFIX=F"),
    ZH_HANT("初始焦距，單位像素；0 表示從 EXIF 或影像尺寸推測。也可寫成 PREFIX=F"),
    KO("초기 초점 거리(픽셀). 0 이면 EXIF 나 이미지 크기에서 추측합니다. "
       "PREFIX=F 형태도 됩니다"),
    DE("Anfängliche Brennweite in Pixeln, 0 heißt aus EXIF oder Bildgröße raten; "
       "nimmt auch PREFIX=F"),
    FR("Focale de départ en pixels, 0 pour la deviner d'après l'EXIF ou la "
       "taille de l'image ; accepte aussi PREFIX=F"),
    ES("Focal inicial en píxeles, 0 para adivinarla del EXIF o del tamaño de la "
       "imagen; también acepta PREFIX=F"),
    PT("Focal inicial em pixels, 0 para adivinhá-la do EXIF ou do tamanho da "
       "imagem; também aceita PREFIX=F"),
    IT("Focale iniziale in pixel, 0 per indovinarla dall'EXIF o dalla dimensione "
       "dell'immagine; accetta anche PREFIX=F"),
    NL("Beginbrandpuntsafstand in pixels, 0 om hem uit de EXIF of de "
       "beeldgrootte te raden; neemt ook PREFIX=F"),
    RU("Начальное фокусное расстояние в пикселях, 0 -- угадать по EXIF или "
       "размеру изображения; принимает и PREFIX=F"),
    TR("Piksel cinsinden başlangıç odak uzaklığı, 0 ise EXIF'ten ya da görüntü "
       "boyutundan tahmin edilir; PREFIX=F biçimini de alır"));

SS_MSG(distortion_help,
    EN("Distortion coefficients to start from, \"k1,k2,...\" in the model's own "
       "order; also takes PREFIX=k1,k2,..."),
    JA("歪み係数の初期値。モデル自身の並び順で \"k1,k2,...\" と書きます。"
       "PREFIX=k1,k2,... の形も使えます"),
    ZH_HANS("畸变系数的初值，按该相机模型自身的顺序写成 \"k1,k2,...\"。也可写成 "
            "PREFIX=k1,k2,..."),
    ZH_HANT("畸變係數的初值，按該相機模型自身的順序寫成 \"k1,k2,...\"。也可寫成 "
            "PREFIX=k1,k2,..."),
    KO("왜곡 계수의 시작값. 해당 모델 자신의 순서로 \"k1,k2,...\" 로 씁니다. "
       "PREFIX=k1,k2,... 형태도 됩니다"),
    DE("Startwerte der Verzeichnungskoeffizienten, \"k1,k2,...\" in der Reihenfolge "
       "des Modells; nimmt auch PREFIX=k1,k2,..."),
    FR("Coefficients de distorsion de départ, « k1,k2,... » dans l'ordre propre au "
       "modèle ; accepte aussi PREFIX=k1,k2,..."),
    ES("Coeficientes de distorsión de partida, \"k1,k2,...\" en el orden propio del "
       "modelo; también acepta PREFIX=k1,k2,..."),
    PT("Coeficientes de distorção iniciais, \"k1,k2,...\" na ordem do próprio "
       "modelo; também aceita PREFIX=k1,k2,..."),
    IT("Coefficienti di distorsione iniziali, \"k1,k2,...\" nell'ordine proprio del "
       "modello; accetta anche PREFIX=k1,k2,..."),
    NL("Beginwaarden van de vertekeningscoëfficiënten, \"k1,k2,...\" in de volgorde "
       "van het model; neemt ook PREFIX=k1,k2,..."),
    RU("Начальные коэффициенты дисторсии, \"k1,k2,...\" в порядке самой модели; "
       "принимает и PREFIX=k1,k2,..."),
    TR("Başlangıç bozulma katsayıları, modelin kendi sırasına göre \"k1,k2,...\"; "
       "PREFIX=k1,k2,... biçimini de alır"));

SS_MSG(exif_focal_help,
    EN("Use the focal length EXIF recorded when no --focal covers the group"),
    JA("--focal がそのグループを覆っていないとき、EXIF に記録された焦点距離を"
       "使います"),
    ZH_HANS("当没有 --focal 覆盖该组时，使用 EXIF 中记录的焦距"),
    ZH_HANT("當沒有 --focal 涵蓋該組時，使用 EXIF 中記錄的焦距"),
    KO("--focal 이 그 그룹을 덮지 않을 때 EXIF 에 기록된 초점 거리를 씁니다"),
    DE("Die in EXIF vermerkte Brennweite verwenden, wenn kein --focal die Gruppe "
       "abdeckt"),
    FR("Utiliser la focale inscrite dans l'EXIF quand aucun --focal ne couvre le "
       "groupe"),
    ES("Usar la focal anotada en el EXIF cuando ningún --focal cubre el grupo"),
    PT("Usar a focal anotada no EXIF quando nenhum --focal cobre o grupo"),
    IT("Usare la focale annotata nell'EXIF quando nessun --focal copre il gruppo"),
    NL("De in EXIF genoteerde brandpuntsafstand gebruiken als geen --focal de "
       "groep dekt"),
    RU("Использовать фокусное расстояние из EXIF, когда группу не покрывает "
       "никакой --focal"),
    TR("Grubu hiçbir --focal kapsamadığında EXIF'te kayıtlı odak uzaklığını "
       "kullan"));

SS_MSG(exif_groups_help,
    EN("Split camera groups by what EXIF says the body and the focal setting "
       "were (D48)"),
    JA("EXIF が伝えるボディと焦点距離の設定でカメラグループを分けます（D48）"),
    ZH_HANS("按 EXIF 记录的机身与焦距设置划分相机组（D48）"),
    ZH_HANT("按 EXIF 記錄的機身與焦距設定劃分相機群組（D48）"),
    KO("EXIF 가 말하는 본체와 초점 거리 설정으로 카메라 그룹을 나눕니다(D48)"),
    DE("Kameragruppen danach trennen, was EXIF über Gehäuse und "
       "Brennweiteneinstellung sagt (D48)"),
    FR("Séparer les groupes de caméras selon ce que l'EXIF dit du boîtier et du "
       "réglage de focale (D48)"),
    ES("Separar los grupos de cámaras según lo que el EXIF dice del cuerpo y del "
       "ajuste de focal (D48)"),
    PT("Separar os grupos de câmeras conforme o que o EXIF diz do corpo e do "
       "ajuste de focal (D48)"),
    IT("Separare i gruppi di camere in base a ciò che l'EXIF dice del corpo e "
       "dell'impostazione di focale (D48)"),
    NL("Cameragroepen splitsen naar wat de EXIF zegt over de body en de "
       "brandpuntsinstelling (D48)"),
    RU("Делить группы камер по тому, что EXIF сообщает о корпусе и установленном "
       "фокусном расстоянии (D48)"),
    TR("Kamera gruplarını, EXIF'in gövde ve odak ayarı hakkında söylediğine göre "
       "ayır (D48)"));

SS_MSG(exif_focal_tol_help,
    EN("Relative tolerance clustering EXIF focals into one group; must exceed "
       "EXIF's 1 mm quantization and stay under a real zoom step"),
    JA("EXIF の焦点距離を 1 つのグループにまとめる相対許容差。EXIF の 1 mm 刻みより"
       "大きく、実際のズーム段より小さくしてください"),
    ZH_HANS("把 EXIF 焦距聚成一组时的相对容差；须大于 EXIF 的 1 mm 量化步长，"
            "又小于真实的变焦档位"),
    ZH_HANT("把 EXIF 焦距聚成一組時的相對容差；須大於 EXIF 的 1 mm 量化步長，"
            "又小於真實的變焦檔位"),
    KO("EXIF 초점 거리를 한 그룹으로 묶을 때의 상대 허용 오차. EXIF 의 1 mm 양자화 "
       "폭보다 크고 실제 줌 단계보다 작아야 합니다"),
    DE("Relative Toleranz, mit der EXIF-Brennweiten zu einer Gruppe "
       "zusammengefasst werden; muss über EXIFs 1-mm-Quantisierung liegen und "
       "unter einer echten Zoomstufe bleiben"),
    FR("Tolérance relative regroupant les focales EXIF en un seul groupe ; elle "
       "doit dépasser la quantification EXIF de 1 mm et rester sous un vrai pas "
       "de zoom"),
    ES("Tolerancia relativa que agrupa las focales EXIF en un solo grupo; debe "
       "superar la cuantización de 1 mm del EXIF y quedar por debajo de un paso "
       "real de zoom"),
    PT("Tolerância relativa que agrupa as focais EXIF num só grupo; deve superar "
       "a quantização de 1 mm do EXIF e ficar abaixo de um passo real de zoom"),
    IT("Tolleranza relativa che raggruppa le focali EXIF in un solo gruppo; deve "
       "superare la quantizzazione di 1 mm dell'EXIF e restare sotto un vero "
       "scatto di zoom"),
    NL("Relatieve tolerantie die EXIF-brandpuntsafstanden tot één groep bundelt; "
       "moet boven EXIF's kwantisering van 1 mm liggen en onder een echte "
       "zoomstap blijven"),
    RU("Относительный допуск, объединяющий фокусные расстояния из EXIF в одну "
       "группу; должен превышать шаг квантования EXIF в 1 мм и оставаться меньше "
       "настоящего шага зума"),
    TR("EXIF odak uzaklıklarını tek bir grupta toplayan göreli tolerans; EXIF'in "
       "1 mm'lik nicemlemesini aşmalı ve gerçek bir yakınlaştırma adımının "
       "altında kalmalı"));

// ===========================================================================
// features
// ===========================================================================

SS_MSG(features_help,
    EN("Which detector and descriptor; the aliked and loma ones are learned and "
       "fetch a checkpoint on first use"),
    JA("どの検出器と記述子を使うか。aliked と loma のものは学習済みで、初回に"
       "チェックポイントを取得します"),
    ZH_HANS("使用哪种检测子与描述子；aliked 与 loma 系列是学习得到的，首次使用时会下载检查点"),
    ZH_HANT("使用哪種偵測子與描述子；aliked 與 loma 系列是學習得到的，首次使用時會下載檢查點"),
    KO("어떤 검출기와 기술자를 쓸지. aliked 와 loma 계열은 학습된 것이라 처음 쓸 때 "
       "체크포인트를 내려받습니다"),
    DE("Welcher Detektor und Deskriptor; die aliked- und loma-Varianten sind gelernt und "
       "holen beim ersten Gebrauch einen Prüfpunkt"),
    FR("Quel détecteur et quel descripteur ; les variantes aliked et loma sont apprises "
       "et téléchargent un point de contrôle au premier usage"),
    ES("Qué detector y qué descriptor; las variantes aliked y loma son aprendidas y "
       "descargan un punto de control la primera vez"),
    PT("Qual detector e qual descritor; as variantes aliked e loma são aprendidas e "
       "baixam um ponto de verificação no primeiro uso"),
    IT("Quale rivelatore e quale descrittore; le varianti aliked e loma sono apprese e "
       "scaricano un checkpoint al primo uso"),
    NL("Welke detector en descriptor; de aliked- en loma-varianten zijn geleerd en halen "
       "bij het eerste gebruik een controlepunt op"),
    RU("Какой детектор и дескриптор; варианты aliked и loma обучены и при первом "
       "использовании скачивают контрольную точку"),
    TR("Hangi bulucu ve betimleyici; aliked ve loma olanlar öğrenilmiştir ve ilk "
       "kullanımda bir denetim noktası indirir"));

SS_MSG(max_features_help,
    EN("Keypoints kept per image with --features sift, the largest scales first"),
    JA("--features sift のとき 1 画像あたり残すキーポイント数。大きいスケールから"),
    ZH_HANS("使用 --features sift 时每张图像保留的关键点数，从最大尺度开始"),
    ZH_HANT("使用 --features sift 時每張影像保留的關鍵點數，從最大尺度開始"),
    KO("--features sift 일 때 이미지당 남기는 키포인트 수. 큰 스케일부터"),
    DE("Schlüsselpunkte je Bild mit --features sift, die größten Skalen zuerst"),
    FR("Points clés gardés par image avec --features sift, les plus grandes "
       "échelles d'abord"),
    ES("Puntos clave que se conservan por imagen con --features sift, las "
       "escalas mayores primero"),
    PT("Pontos-chave mantidos por imagem com --features sift, as maiores escalas "
       "primeiro"),
    IT("Punti chiave tenuti per immagine con --features sift, prima le scale più "
       "grandi"),
    NL("Sleutelpunten per beeld met --features sift, de grootste schalen eerst"),
    RU("Сколько ключевых точек оставлять на изображение при --features sift, "
       "начиная с крупнейших масштабов"),
    TR("--features sift ile görüntü başına tutulan anahtar nokta sayısı, en "
       "büyük ölçekten başlayarak"));

SS_MSG(aliked_max_features_help,
    EN("Keypoints kept per image with a learned frontend, the highest scores "
       "first"),
    JA("学習済みフロントエンドで 1 画像あたり残すキーポイント数。スコアの高い順"),
    ZH_HANS("使用学习型前端时每张图像保留的关键点数，从得分最高的开始"),
    ZH_HANT("使用學習型前端時每張影像保留的關鍵點數，從得分最高的開始"),
    KO("학습된 프런트엔드에서 이미지당 남기는 키포인트 수. 점수가 높은 것부터"),
    DE("Schlüsselpunkte je Bild mit einem gelernten Frontend, die höchsten "
       "Bewertungen zuerst"),
    FR("Points clés gardés par image avec un frontal appris, les meilleurs "
       "scores d'abord"),
    ES("Puntos clave que se conservan por imagen con un frontal aprendido, las "
       "puntuaciones más altas primero"),
    PT("Pontos-chave mantidos por imagem com um frontal aprendido, as maiores "
       "pontuações primeiro"),
    IT("Punti chiave tenuti per immagine con un frontend appreso, prima i "
       "punteggi più alti"),
    NL("Sleutelpunten per beeld met een geleerde frontend, de hoogste scores "
       "eerst"),
    RU("Сколько ключевых точек оставлять на изображение при обученном фронтенде, "
       "начиная с наибольших оценок"),
    TR("Öğrenilmiş bir ön uçla görüntü başına tutulan anahtar nokta sayısı, en "
       "yüksek puandan başlayarak"));

SS_MSG(aliked_min_score_help,
    EN("Detection score a learned keypoint must reach"),
    JA("学習済みキーポイントが達すべき検出スコア"),
    ZH_HANS("学习型关键点必须达到的检测得分"),
    ZH_HANT("學習型關鍵點必須達到的偵測得分"),
    KO("학습된 키포인트가 넘어야 하는 검출 점수"),
    DE("Erkennungsbewertung, die ein gelernter Schlüsselpunkt erreichen muss"),
    FR("Score de détection qu'un point clé appris doit atteindre"),
    ES("Puntuación de detección que un punto clave aprendido debe alcanzar"),
    PT("Pontuação de detecção que um ponto-chave aprendido deve alcançar"),
    IT("Punteggio di rilevamento che un punto chiave appreso deve raggiungere"),
    NL("Detectiescore die een geleerd sleutelpunt moet halen"),
    RU("Оценка обнаружения, которой должна достичь обученная ключевая точка"),
    TR("Öğrenilmiş bir anahtar noktanın ulaşması gereken bulma puanı"));

SS_MSG(aliked_model_help,
    EN("Path to an ALIKED .onnx checkpoint, overriding the one --features names"),
    JA("ALIKED の .onnx チェックポイントへのパス。--features が指す既定を"
       "上書きします"),
    ZH_HANS("ALIKED 的 .onnx 检查点路径，覆盖 --features 指定的那个"),
    ZH_HANT("ALIKED 的 .onnx 檢查點路徑，覆蓋 --features 指定的那個"),
    KO("ALIKED .onnx 체크포인트 경로. --features 가 가리키는 것을 대신합니다"),
    DE("Pfad zu einem ALIKED-.onnx-Prüfpunkt, der den von --features benannten "
       "ersetzt"),
    FR("Chemin d'un point de contrôle ALIKED .onnx, qui remplace celui que "
       "désigne --features"),
    ES("Ruta de un punto de control ALIKED .onnx, que sustituye al que nombra "
       "--features"),
    PT("Caminho de um ponto de verificação ALIKED .onnx, que substitui o que "
       "--features nomeia"),
    IT("Percorso di un checkpoint ALIKED .onnx, che sostituisce quello indicato "
       "da --features"),
    NL("Pad naar een ALIKED-.onnx-controlepunt, dat het door --features "
       "genoemde vervangt"),
    RU("Путь к контрольной точке ALIKED .onnx, заменяющей ту, что называет "
       "--features"),
    TR("Bir ALIKED .onnx denetim noktasının yolu; --features'ın adlandırdığının "
       "yerine geçer"));

SS_MSG(octaves_help,
    EN("Scale-space octaves"),
    JA("スケール空間のオクターブ数"),
    ZH_HANS("尺度空间的组数"),
    ZH_HANT("尺度空間的組數"),
    KO("스케일 공간의 옥타브 수"),
    DE("Oktaven des Skalenraums"),
    FR("Octaves de l'espace d'échelles"),
    ES("Octavas del espacio de escalas"),
    PT("Oitavas do espaço de escalas"),
    IT("Ottave dello spazio delle scale"),
    NL("Octaven van de schaalruimte"),
    RU("Число октав масштабного пространства"),
    TR("Ölçek uzayı oktavları"));

SS_MSG(peak_threshold_help,
    EN("DoG response a keypoint must reach"),
    JA("キーポイントが達すべき DoG 応答"),
    ZH_HANS("关键点必须达到的 DoG 响应"),
    ZH_HANT("關鍵點必須達到的 DoG 回應"),
    KO("키포인트가 넘어야 하는 DoG 응답"),
    DE("DoG-Antwort, die ein Schlüsselpunkt erreichen muss"),
    FR("Réponse DoG qu'un point clé doit atteindre"),
    ES("Respuesta DoG que un punto clave debe alcanzar"),
    PT("Resposta DoG que um ponto-chave deve alcançar"),
    IT("Risposta DoG che un punto chiave deve raggiungere"),
    NL("DoG-respons die een sleutelpunt moet halen"),
    RU("Отклик DoG, которого должна достичь ключевая точка"),
    TR("Bir anahtar noktanın ulaşması gereken DoG yanıtı"));

SS_MSG(edge_threshold_help,
    EN("Principal-curvature ratio above which a keypoint is an edge, not a "
       "corner"),
    JA("これを超えるとキーポイントが角ではなく辺と見なされる主曲率比"),
    ZH_HANS("主曲率比超过此值时，关键点被视为边缘而非角点"),
    ZH_HANT("主曲率比超過此值時，關鍵點被視為邊緣而非角點"),
    KO("이 값을 넘으면 키포인트를 모서리가 아니라 경계로 보는 주곡률 비"),
    DE("Hauptkrümmungsverhältnis, oberhalb dessen ein Schlüsselpunkt eine Kante "
       "ist und keine Ecke"),
    FR("Rapport des courbures principales au-delà duquel un point clé est une "
       "arête et non un coin"),
    ES("Razón de curvaturas principales por encima de la cual un punto clave es "
       "un borde y no una esquina"),
    PT("Razão das curvaturas principais acima da qual um ponto-chave é uma borda "
       "e não um canto"),
    IT("Rapporto delle curvature principali oltre il quale un punto chiave è un "
       "bordo e non uno spigolo"),
    NL("Verhouding van hoofdkrommingen waarboven een sleutelpunt een rand is en "
       "geen hoek"),
    RU("Отношение главных кривизн, выше которого ключевая точка считается краем, "
       "а не углом"),
    TR("Bir anahtar noktanın köşe değil kenar sayıldığı ana eğrilik oranı"));

SS_MSG(max_orientations_help,
    EN("Descriptors emitted per keypoint when its gradient is ambiguous"),
    JA("勾配の向きが定まらないキーポイント 1 つあたりに出す記述子の数"),
    ZH_HANS("当关键点的梯度方向不明确时，为其输出的描述子个数"),
    ZH_HANT("當關鍵點的梯度方向不明確時，為其輸出的描述子個數"),
    KO("기울기 방향이 모호한 키포인트 하나에 대해 내보내는 기술자 수"),
    DE("Deskriptoren je Schlüsselpunkt, wenn dessen Gradient mehrdeutig ist"),
    FR("Descripteurs émis par point clé quand son gradient est ambigu"),
    ES("Descriptores emitidos por punto clave cuando su gradiente es ambiguo"),
    PT("Descritores emitidos por ponto-chave quando o seu gradiente é ambíguo"),
    IT("Descrittori emessi per punto chiave quando il suo gradiente è ambiguo"),
    NL("Descriptoren per sleutelpunt wanneer de gradiënt dubbelzinnig is"),
    RU("Сколько дескрипторов выдавать на ключевую точку, когда её градиент "
       "неоднозначен"),
    TR("Gradyanı belirsiz olan bir anahtar nokta için üretilen betimleyici "
       "sayısı"));

SS_MSG(profile_help,
    EN("Print per-stage GPU timings for the extractor"),
    JA("抽出器の段階ごとの GPU 時間を表示します"),
    ZH_HANS("打印提取器各阶段的 GPU 计时"),
    ZH_HANT("列印擷取器各階段的 GPU 計時"),
    KO("추출기의 단계별 GPU 시간을 출력합니다"),
    DE("GPU-Zeiten des Extraktors je Stufe ausgeben"),
    FR("Afficher les temps GPU de l'extracteur, étape par étape"),
    ES("Mostrar los tiempos de GPU del extractor, etapa por etapa"),
    PT("Mostrar os tempos de GPU do extrator, etapa por etapa"),
    IT("Stampare i tempi GPU dell'estrattore, fase per fase"),
    NL("De GPU-tijden van de extractor per fase tonen"),
    RU("Печатать время GPU по стадиям извлекателя"),
    TR("Çıkarıcının aşama aşama GPU sürelerini yazdır"));

SS_MSG(spv_path_help,
    EN("Load the SIFT kernels from this SPIR-V file instead of the embedded blob"),
    JA("SIFT のカーネルを埋め込みではなくこの SPIR-V ファイルから読み込みます"),
    ZH_HANS("从这个 SPIR-V 文件加载 SIFT 内核，而不用内置的那份"),
    ZH_HANT("從這個 SPIR-V 檔案載入 SIFT 核心，而不用內建的那份"),
    KO("SIFT 커널을 내장된 것 대신 이 SPIR-V 파일에서 불러옵니다"),
    DE("Die SIFT-Kernel aus dieser SPIR-V-Datei laden statt aus dem "
       "eingebetteten Blob"),
    FR("Charger les noyaux SIFT depuis ce fichier SPIR-V plutôt que depuis le "
       "blob embarqué"),
    ES("Cargar los núcleos SIFT desde este archivo SPIR-V en vez del blob "
       "incorporado"),
    PT("Carregar os núcleos SIFT deste arquivo SPIR-V em vez do blob embutido"),
    IT("Caricare i kernel SIFT da questo file SPIR-V invece che dal blob "
       "incorporato"),
    NL("De SIFT-kernels uit dit SPIR-V-bestand laden in plaats van uit de "
       "ingebouwde blob"),
    RU("Загружать ядра SIFT из этого файла SPIR-V, а не из встроенного блоба"),
    TR("SIFT çekirdeklerini gömülü blob yerine bu SPIR-V dosyasından yükle"));

// ===========================================================================
// matching
// ===========================================================================

SS_MSG(matcher_help,
    EN("How descriptors are matched. lightglue and loma-* are learned matchers, "
       "each for the --features of the same name; both are an order of "
       "magnitude slower per pair, so they only make sense behind pair selection"),
    JA("記述子のマッチング方法。lightglue と loma-* は同名の --features 向けの"
       "学習済みマッチャーで、1 ペアあたり 1 桁遅いため、ペア選択の後ろでのみ"
       "意味があります"),
    ZH_HANS("如何匹配描述子。lightglue 与 loma-* 是学习型匹配器，各自面向同名的 "
            "--features；它们每对慢一个数量级，因此只有放在像对筛选之后才划算"),
    ZH_HANT("如何匹配描述子。lightglue 與 loma-* 是學習型匹配器，各自面向同名的 "
            "--features；它們每對慢一個數量級，因此只有放在影像對篩選之後才划算"),
    KO("기술자를 매칭하는 방식. lightglue 와 loma-* 는 각각 같은 이름의 --features "
       "용 학습된 매처로, 쌍당 한 자릿수만큼 느리므로 쌍 선택 뒤에서만 쓸 "
       "만합니다"),
    DE("Wie Deskriptoren zugeordnet werden. lightglue und loma-* sind gelernte "
       "Matcher, jeder für die gleichnamigen --features; beide sind je Paar eine "
       "Größenordnung langsamer und lohnen sich nur hinter der Paarauswahl"),
    FR("Comment les descripteurs sont appariés. lightglue et loma-* sont des "
       "apparieurs appris, chacun pour les --features du même nom ; tous deux "
       "sont bien plus lents par paire, et n'ont de sens qu'après la sélection "
       "de paires"),
    ES("Cómo se emparejan los descriptores. lightglue y loma-* son emparejadores "
       "aprendidos, cada uno para las --features del mismo nombre; ambos son un "
       "orden de magnitud más lentos por par, así que solo tienen sentido tras "
       "la selección de pares"),
    PT("Como os descritores são emparelhados. lightglue e loma-* são emparelhadores "
       "aprendidos, cada um para as --features de mesmo nome; ambos são uma ordem "
       "de grandeza mais lentos por par, então só fazem sentido depois da seleção "
       "de pares"),
    IT("Come vengono abbinati i descrittori. lightglue e loma-* sono abbinatori "
       "appresi, ciascuno per le --features dello stesso nome; entrambi sono un "
       "ordine di grandezza più lenti per coppia, quindi hanno senso solo dopo "
       "la selezione delle coppie"),
    NL("Hoe descriptoren gematcht worden. lightglue en loma-* zijn geleerde "
       "matchers, elk voor de gelijknamige --features; beide zijn per paar een "
       "orde van grootte trager en lonen dus alleen achter de paarselectie"),
    RU("Как сопоставляются дескрипторы. lightglue и loma-* -- обученные "
       "сопоставители, каждый для одноимённых --features; оба на порядок "
       "медленнее на пару, поэтому имеют смысл только после отбора пар"),
    TR("Betimleyicilerin nasıl eşleştirileceği. lightglue ve loma-*, aynı adlı "
       "--features için öğrenilmiş eşleştiricilerdir; ikisi de çift başına bir "
       "büyüklük derecesi daha yavaştır, bu yüzden ancak çift seçiminin ardında "
       "anlamlıdır"));

SS_MSG(loma_model_help,
    EN("Path to a LoMa .onnx checkpoint, overriding the one --features or "
       "--matcher names"),
    JA("LoMa の .onnx チェックポイントへのパス。--features や --matcher が"
       "指す既定を上書きします"),
    ZH_HANS("LoMa 的 .onnx 检查点路径，覆盖 --features 或 --matcher 指定的那个"),
    ZH_HANT("LoMa 的 .onnx 檢查點路徑，覆蓋 --features 或 --matcher 指定的那個"),
    KO("LoMa .onnx 체크포인트 경로. --features 나 --matcher 가 가리키는 것을 "
       "대신합니다"),
    DE("Pfad zu einem LoMa-.onnx-Prüfpunkt, der den von --features oder "
       "--matcher benannten ersetzt"),
    FR("Chemin d'un point de contrôle LoMa .onnx, qui remplace celui que "
       "désignent --features ou --matcher"),
    ES("Ruta de un punto de control LoMa .onnx, que sustituye al que nombran "
       "--features o --matcher"),
    PT("Caminho de um ponto de verificação LoMa .onnx, que substitui o que "
       "--features ou --matcher nomeia"),
    IT("Percorso di un checkpoint LoMa .onnx, che sostituisce quello indicato "
       "da --features o --matcher"),
    NL("Pad naar een LoMa-.onnx-controlepunt, dat het door --features of "
       "--matcher genoemde vervangt"),
    RU("Путь к контрольной точке LoMa .onnx, заменяющей ту, что называют "
       "--features или --matcher"),
    TR("Bir LoMa .onnx denetim noktasının yolu; --features veya --matcher'ın "
       "adlandırdığının yerine geçer"));

SS_MSG(lightglue_min_score_help,
    EN("Assignment confidence a LightGlue match must reach"),
    JA("LightGlue の対応が達すべき割り当て信頼度"),
    ZH_HANS("LightGlue 匹配必须达到的指派置信度"),
    ZH_HANT("LightGlue 匹配必須達到的指派信賴度"),
    KO("LightGlue 대응이 넘어야 하는 할당 신뢰도"),
    DE("Zuordnungsvertrauen, das eine LightGlue-Zuordnung erreichen muss"),
    FR("Confiance d'affectation qu'un appariement LightGlue doit atteindre"),
    ES("Confianza de asignación que un emparejamiento LightGlue debe alcanzar"),
    PT("Confiança de atribuição que uma correspondência LightGlue deve alcançar"),
    IT("Fiducia di assegnazione che una corrispondenza LightGlue deve "
       "raggiungere"),
    NL("Toewijzingsvertrouwen dat een LightGlue-match moet halen"),
    RU("Уверенность назначения, которой должно достичь соответствие LightGlue"),
    TR("Bir LightGlue eşleşmesinin ulaşması gereken atama güveni"));

SS_MSG(loma_min_match_score_help,
    EN("Assignment confidence a LoMa match must reach"),
    JA("LoMa のマッチが達すべき割り当て信頼度"),
    ZH_HANS("LoMa 匹配必须达到的指派置信度"),
    ZH_HANT("LoMa 比對必須達到的指派信賴度"),
    KO("LoMa 매치가 넘어야 하는 할당 신뢰도"),
    DE("Zuordnungssicherheit, die eine LoMa-Zuordnung erreichen muss"),
    FR("Confiance d'appariement qu'une correspondance LoMa doit atteindre"),
    ES("Confianza de asignación que una correspondencia LoMa debe alcanzar"),
    PT("Confiança de atribuição que uma correspondência LoMa deve alcançar"),
    IT("Fiducia di assegnazione che un abbinamento LoMa deve raggiungere"),
    NL("Toewijzingszekerheid die een LoMa-match moet halen"),
    RU("Уверенность назначения, которой должно достичь соответствие LoMa"),
    TR("Bir LoMa eşleşmesinin ulaşması gereken atama güveni"));

SS_MSG(lightglue_model_help,
    EN("Path to a LightGlue .onnx checkpoint, overriding the fetched one"),
    JA("LightGlue の .onnx チェックポイントへのパス。取得済みのものを上書きします"),
    ZH_HANS("LightGlue 的 .onnx 检查点路径，覆盖已下载的那个"),
    ZH_HANT("LightGlue 的 .onnx 檢查點路徑，覆蓋已下載的那個"),
    KO("LightGlue .onnx 체크포인트 경로. 내려받은 것을 대신합니다"),
    DE("Pfad zu einem LightGlue-.onnx-Prüfpunkt, der den geholten ersetzt"),
    FR("Chemin d'un point de contrôle LightGlue .onnx, qui remplace celui "
       "téléchargé"),
    ES("Ruta de un punto de control LightGlue .onnx, que sustituye al descargado"),
    PT("Caminho de um ponto de verificação LightGlue .onnx, que substitui o "
       "baixado"),
    IT("Percorso di un checkpoint LightGlue .onnx, che sostituisce quello "
       "scaricato"),
    NL("Pad naar een LightGlue-.onnx-controlepunt, dat het opgehaalde vervangt"),
    RU("Путь к контрольной точке LightGlue .onnx, заменяющей скачанную"),
    TR("Bir LightGlue .onnx denetim noktasının yolu; indirilenin yerine geçer"));

SS_MSG(ratio_help,
    EN("Lowe ratio: a match is kept when the best distance is below this times "
       "the second best"),
    JA("Lowe の比率。最良距離が 2 番目の距離のこの倍数を下回るとき対応を残します"),
    ZH_HANS("Lowe 比值：当最优距离小于此值乘以次优距离时保留该匹配"),
    ZH_HANT("Lowe 比值：當最優距離小於此值乘以次優距離時保留該匹配"),
    KO("Lowe 비율: 최적 거리가 차순위 거리의 이 배수보다 작을 때 대응을 남깁니다"),
    DE("Lowe-Verhältnis: eine Zuordnung bleibt, wenn der beste Abstand unter "
       "diesem Vielfachen des zweitbesten liegt"),
    FR("Rapport de Lowe : un appariement est gardé quand la meilleure distance "
       "est sous ce multiple de la deuxième"),
    ES("Razón de Lowe: un emparejamiento se conserva cuando la mejor distancia "
       "queda por debajo de este múltiplo de la segunda"),
    PT("Razão de Lowe: uma correspondência é mantida quando a melhor distância "
       "fica abaixo deste múltiplo da segunda"),
    IT("Rapporto di Lowe: un abbinamento si tiene quando la distanza migliore "
       "sta sotto questo multiplo della seconda"),
    NL("Lowe-verhouding: een match blijft wanneer de beste afstand onder dit "
       "veelvoud van de op één na beste ligt"),
    RU("Отношение Лоу: соответствие остаётся, когда лучшее расстояние меньше "
       "этого множителя от второго лучшего"),
    TR("Lowe oranı: en iyi uzaklık, ikinci en iyinin bu katının altındaysa "
       "eşleşme tutulur"));

SS_MSG(min_similarity_help,
    EN("Cosine similarity a match must reach, for float descriptors; 0 disables "
       "it. Learned descriptors are filtered on this rather than on the ratio"),
    JA("浮動小数の記述子で対応が達すべきコサイン類似度。0 で無効。学習済み記述子は"
       "比率ではなくこちらでふるいます"),
    ZH_HANS("对浮点描述子而言，匹配必须达到的余弦相似度；0 表示关闭。"
            "学习型描述子按这项而非比值筛选"),
    ZH_HANT("對浮點描述子而言，匹配必須達到的餘弦相似度；0 表示關閉。"
            "學習型描述子按這項而非比值篩選"),
    KO("실수 기술자에서 대응이 넘어야 하는 코사인 유사도. 0 이면 끕니다. 학습된 "
       "기술자는 비율이 아니라 이것으로 거릅니다"),
    DE("Kosinusähnlichkeit, die eine Zuordnung bei Fließkomma-Deskriptoren "
       "erreichen muss; 0 schaltet sie ab. Gelernte Deskriptoren werden hierauf "
       "gefiltert statt auf das Verhältnis"),
    FR("Similarité cosinus qu'un appariement doit atteindre, pour des "
       "descripteurs flottants ; 0 la désactive. Les descripteurs appris sont "
       "filtrés là-dessus plutôt que sur le rapport"),
    ES("Similitud del coseno que un emparejamiento debe alcanzar, para "
       "descriptores en coma flotante; 0 la desactiva. Los descriptores "
       "aprendidos se filtran por esto y no por la razón"),
    PT("Similaridade de cosseno que uma correspondência deve alcançar, para "
       "descritores em ponto flutuante; 0 a desativa. Os descritores aprendidos "
       "são filtrados por isto e não pela razão"),
    IT("Similarità coseno che un abbinamento deve raggiungere, per descrittori "
       "in virgola mobile; 0 la disattiva. I descrittori appresi si filtrano su "
       "questa e non sul rapporto"),
    NL("Cosinusgelijkenis die een match moet halen, voor drijvende-komma-"
       "descriptoren; 0 zet hem uit. Geleerde descriptoren worden hierop "
       "gefilterd in plaats van op de verhouding"),
    RU("Косинусная близость, которой должно достичь соответствие для "
       "вещественных дескрипторов; 0 отключает её. Обученные дескрипторы "
       "фильтруются по ней, а не по отношению"),
    TR("Kayan noktalı betimleyicilerde bir eşleşmenin ulaşması gereken kosinüs "
       "benzerliği; 0 kapatır. Öğrenilmiş betimleyiciler oran yerine buna göre "
       "elenir"));

SS_MSG(cross_check_help,
    EN("Keep only mutual nearest neighbours"),
    JA("互いに最近傍である対応だけを残します"),
    ZH_HANS("只保留互为最近邻的匹配"),
    ZH_HANT("只保留互為最近鄰的匹配"),
    KO("서로가 최근접인 대응만 남깁니다"),
    DE("Nur wechselseitig nächste Nachbarn behalten"),
    FR("Ne garder que les plus proches voisins mutuels"),
    ES("Conservar solo los vecinos más cercanos mutuos"),
    PT("Manter apenas os vizinhos mais próximos mútuos"),
    IT("Tenere solo i vicini più prossimi reciproci"),
    NL("Alleen wederzijds dichtstbijzijnde buren behouden"),
    RU("Оставлять только взаимно ближайших соседей"),
    TR("Yalnızca karşılıklı en yakın komşuları tut"));

SS_MSG(max_matches_help,
    EN("Cap on matches per pair, 0 for no cap"),
    JA("1 ペアあたりの対応数の上限。0 で無制限"),
    ZH_HANS("每对图像的匹配数上限，0 表示不限"),
    ZH_HANT("每對影像的匹配數上限，0 表示不限"),
    KO("쌍당 대응 수 상한. 0 이면 제한 없음"),
    DE("Obergrenze der Zuordnungen je Paar, 0 für keine Grenze"),
    FR("Plafond d'appariements par paire, 0 pour aucun"),
    ES("Tope de emparejamientos por par, 0 para ninguno"),
    PT("Limite de correspondências por par, 0 para nenhum"),
    IT("Tetto di abbinamenti per coppia, 0 per nessuno"),
    NL("Plafond op matches per paar, 0 voor geen plafond"),
    RU("Предел числа соответствий на пару, 0 -- без предела"),
    TR("Çift başına eşleşme üst sınırı, sınırsız için 0"));

SS_MSG(verify_help,
    EN("Geometrically verify each pair (F/H RANSAC); off keeps the raw putative "
       "matches"),
    JA("各ペアを幾何的に検証します（F/H の RANSAC）。無効なら候補対応をそのまま"
       "残します"),
    ZH_HANS("对每对图像做几何验证（F/H 的 RANSAC）；关闭则保留原始的候选匹配"),
    ZH_HANT("對每對影像做幾何驗證（F/H 的 RANSAC）；關閉則保留原始的候選匹配"),
    KO("각 쌍을 기하적으로 검증합니다(F/H RANSAC). 끄면 후보 대응을 그대로 둡니다"),
    DE("Jedes Paar geometrisch prüfen (F/H-RANSAC); aus behält die rohen "
       "mutmaßlichen Zuordnungen"),
    FR("Vérifier chaque paire géométriquement (RANSAC F/H) ; désactivé, les "
       "appariements présumés bruts sont conservés"),
    ES("Verificar geométricamente cada par (RANSAC de F/H); desactivado conserva "
       "los emparejamientos supuestos en bruto"),
    PT("Verificar geometricamente cada par (RANSAC de F/H); desligado mantém as "
       "correspondências supostas em bruto"),
    IT("Verificare geometricamente ogni coppia (RANSAC F/H); disattivato tiene "
       "gli abbinamenti presunti grezzi"),
    NL("Elk paar meetkundig verifiëren (F/H-RANSAC); uit houdt de ruwe vermoede "
       "matches"),
    RU("Геометрически проверять каждую пару (RANSAC для F/H); выключено -- "
       "остаются исходные предполагаемые соответствия"),
    TR("Her çifti geometrik olarak doğrula (F/H RANSAC); kapalıyken ham aday "
       "eşleşmeler korunur"));

SS_MSG(min_inliers_help,
    EN("Inliers a verified pair must keep to be recorded"),
    JA("検証済みペアが記録されるために残すべきインライア数"),
    ZH_HANS("经过验证的像对需保留多少内点才会被记录"),
    ZH_HANT("經過驗證的影像對需保留多少內點才會被記錄"),
    KO("검증된 쌍이 기록되려면 남겨야 하는 내부점 수"),
    DE("Inlier, die ein geprüftes Paar behalten muss, um vermerkt zu werden"),
    FR("Inliers qu'une paire vérifiée doit garder pour être enregistrée"),
    ES("Inliers que un par verificado debe conservar para quedar registrado"),
    PT("Inliers que um par verificado deve manter para ser registrado"),
    IT("Inlier che una coppia verificata deve tenere per essere registrata"),
    NL("Inliers die een geverifieerd paar moet houden om vastgelegd te worden"),
    RU("Сколько инлайеров должна сохранить проверенная пара, чтобы быть "
       "записанной"),
    TR("Doğrulanmış bir çiftin kaydedilmesi için tutması gereken içleyen sayısı"));

SS_MSG(prefilter_features_help,
    EN("Query descriptors per image in the pair-selection pass"),
    JA("ペア選択のパスで 1 画像あたり照会する記述子の数"),
    ZH_HANS("像对筛选阶段每张图像用于查询的描述子数"),
    ZH_HANT("影像對篩選階段每張影像用於查詢的描述子數"),
    KO("쌍 선택 단계에서 이미지당 질의에 쓰는 기술자 수"),
    DE("Abfrage-Deskriptoren je Bild im Paarauswahl-Durchgang"),
    FR("Descripteurs de requête par image dans la passe de sélection de paires"),
    ES("Descriptores de consulta por imagen en la pasada de selección de pares"),
    PT("Descritores de consulta por imagem na passagem de seleção de pares"),
    IT("Descrittori di interrogazione per immagine nel passaggio di selezione "
       "delle coppie"),
    NL("Zoekdescriptoren per beeld in de paarselectiegang"),
    RU("Сколько дескрипторов-запросов на изображение в проходе отбора пар"),
    TR("Çift seçimi geçişinde görüntü başına sorgu betimleyicisi sayısı"));

SS_MSG(prefilter_train_help,
    EN("Cap on the train side of that pass, 0 to score against every descriptor"),
    JA("そのパスの参照側の上限。0 ならすべての記述子に対して採点します"),
    ZH_HANS("该阶段被检索一侧的上限，0 表示对全部描述子打分"),
    ZH_HANT("該階段被檢索一側的上限，0 表示對全部描述子打分"),
    KO("그 단계에서 대조되는 쪽의 상한. 0 이면 모든 기술자에 대해 점수를 냅니다"),
    DE("Obergrenze der Referenzseite dieses Durchgangs, 0 bewertet gegen jeden "
       "Deskriptor"),
    FR("Plafond du côté référence de cette passe, 0 pour noter contre tous les "
       "descripteurs"),
    ES("Tope del lado de referencia de esa pasada, 0 para puntuar contra todos "
       "los descriptores"),
    PT("Limite do lado de referência dessa passagem, 0 para pontuar contra todos "
       "os descritores"),
    IT("Tetto del lato di riferimento di quel passaggio, 0 per valutare contro "
       "ogni descrittore"),
    NL("Plafond op de referentiekant van die gang, 0 om tegen elke descriptor te "
       "scoren"),
    RU("Предел для эталонной стороны этого прохода, 0 -- оценивать по всем "
       "дескрипторам"),
    TR("O geçişin referans tarafının üst sınırı, her betimleyiciye karşı "
       "puanlamak için 0"));

SS_MSG(prefilter_neighbors_help,
    EN("Best-scoring partners each image keeps for full matching"),
    JA("各画像が本マッチングのために残す、上位スコアの相手の数"),
    ZH_HANS("每张图像为完整匹配保留的高分伙伴数"),
    ZH_HANT("每張影像為完整匹配保留的高分夥伴數"),
    KO("각 이미지가 본 매칭을 위해 남기는, 점수가 높은 상대의 수"),
    DE("Bestbewertete Partner, die jedes Bild für die volle Zuordnung behält"),
    FR("Partenaires les mieux notés que chaque image garde pour l'appariement "
       "complet"),
    ES("Compañeras mejor puntuadas que cada imagen conserva para el "
       "emparejamiento completo"),
    PT("Parceiras mais bem pontuadas que cada imagem mantém para o "
       "emparelhamento completo"),
    IT("Partner col punteggio più alto che ogni immagine tiene per l'abbinamento "
       "completo"),
    NL("Best scorende partners die elk beeld houdt voor het volledige matchen"),
    RU("Сколько лучших по оценке партнёров каждое изображение оставляет для "
       "полного сопоставления"),
    TR("Her görüntünün tam eşleştirme için tuttuğu en yüksek puanlı eş sayısı"));

SS_MSG(prefilter_min_score_help,
    EN("Mini-matches below which a pair never qualifies"),
    JA("これを下回るとペアが決して通らない小マッチ数"),
    ZH_HANS("低于此数的小匹配量将使该像对无法入选"),
    ZH_HANT("低於此數的小匹配量將使該影像對無法入選"),
    KO("이 값에 못 미치면 쌍이 결코 통과하지 못하는 소규모 대응 수"),
    DE("Mini-Zuordnungen, unter denen ein Paar nie in Frage kommt"),
    FR("Mini-appariements en dessous desquels une paire n'est jamais retenue"),
    ES("Miniemparejamientos por debajo de los cuales un par nunca entra"),
    PT("Minicorrespondências abaixo das quais um par nunca entra"),
    IT("Mini-abbinamenti sotto i quali una coppia non passa mai"),
    NL("Mini-matches waaronder een paar nooit in aanmerking komt"),
    RU("Число мини-соответствий, ниже которого пара никогда не проходит"),
    TR("Altında bir çiftin asla geçmediği mini eşleşme sayısı"));

SS_MSG(prefilter_ratio_help,
    EN("Lowe ratio of the scoring pass; looser than the matcher's, as it only "
       "ranks"),
    JA("採点パスの Lowe 比率。順位づけしかしないため、マッチャーのものより緩め"),
    ZH_HANS("打分阶段的 Lowe 比值；因为只用来排序，可以比匹配器的更宽松"),
    ZH_HANT("打分階段的 Lowe 比值；因為只用來排序，可以比匹配器的更寬鬆"),
    KO("점수 단계의 Lowe 비율. 순위만 매기므로 매처의 것보다 느슨합니다"),
    DE("Lowe-Verhältnis des Bewertungsdurchgangs; lockerer als das des Matchers, "
       "da es nur eine Rangfolge bildet"),
    FR("Rapport de Lowe de la passe de notation ; plus lâche que celui de "
       "l'apparieur, puisqu'il ne fait que classer"),
    ES("Razón de Lowe de la pasada de puntuación; más laxa que la del "
       "emparejador, pues solo ordena"),
    PT("Razão de Lowe da passagem de pontuação; mais frouxa que a do "
       "emparelhador, pois só ordena"),
    IT("Rapporto di Lowe del passaggio di punteggio; più largo di quello "
       "dell'abbinatore, dato che si limita a ordinare"),
    NL("Lowe-verhouding van de scoringsgang; losser dan die van de matcher, "
       "omdat hij alleen rangschikt"),
    RU("Отношение Лоу в оценочном проходе; свободнее, чем у сопоставителя, ведь "
       "оно только ранжирует"),
    TR("Puanlama geçişinin Lowe oranı; yalnızca sıraladığı için eşleştiricininki "
       "kadar sıkı değildir"));

// ===========================================================================
// mapper
// ===========================================================================

SS_MSG(compact_unused_features_help,
    EN("Keep in memory only feature rows referenced by stored matches; files on "
       "disk stay unchanged"),
    JA("保存済みマッチが参照する特徴点行だけをメモリに保持します。ディスク上の"
       "ファイルは変更しません"),
    ZH_HANS("内存中只保留已存储匹配所引用的特征行；磁盘上的文件保持不变"),
    ZH_HANT("記憶體中只保留已儲存匹配所引用的特徵列；磁碟上的檔案保持不變"),
    KO("저장된 매치가 참조하는 특징점 행만 메모리에 유지합니다. 디스크의 파일은 "
       "변경하지 않습니다"),
    DE("Im Speicher nur Merkmalszeilen behalten, auf die gespeicherte Matches "
       "verweisen; Dateien auf dem Datenträger bleiben unverändert"),
    FR("Ne garder en mémoire que les lignes de points référencées par les "
       "correspondances stockées ; les fichiers restent inchangés sur le disque"),
    ES("Mantener en memoria solo las filas de puntos referenciadas por las "
       "correspondencias almacenadas; los archivos del disco no cambian"),
    PT("Manter na memória apenas as linhas de pontos referenciadas pelas "
       "correspondências armazenadas; os arquivos no disco não mudam"),
    IT("Mantenere in memoria solo le righe dei punti richiamate dalle "
       "corrispondenze memorizzate; i file su disco restano invariati"),
    NL("Alleen kenmerkrijen waar opgeslagen overeenkomsten naar verwijzen in het "
       "geheugen houden; bestanden op schijf blijven ongewijzigd"),
    RU("Оставлять в памяти только строки признаков, на которые ссылаются сохранённые "
       "соответствия; файлы на диске не изменяются"),
    TR("Bellekte yalnızca saklanan eşleşmelerin başvurduğu öznitelik satırlarını "
       "tut; diskteki dosyalar değişmeden kalır"));

SS_MSG(focal_trials_help,
    EN("Trial reconstructions used to pick a focal the motion cannot determine "
       "(D48), 0 to skip"),
    JA("動きだけでは決まらない焦点距離を選ぶための試行再構成の回数（D48）。0 で"
       "省略"),
    ZH_HANS("为选出运动本身无法确定的焦距而做的试探性重建次数（D48），0 表示跳过"),
    ZH_HANT("為選出運動本身無法確定的焦距而做的試探性重建次數（D48），0 表示略過"),
    KO("움직임만으로는 정해지지 않는 초점 거리를 고르기 위한 시험 재구성 횟수"
       "(D48). 0 이면 건너뜁니다"),
    DE("Probe-Rekonstruktionen, um eine Brennweite zu wählen, die die Bewegung "
       "nicht festlegt (D48); 0 überspringt sie"),
    FR("Reconstructions d'essai servant à choisir une focale que le mouvement ne "
       "détermine pas (D48), 0 pour sauter"),
    ES("Reconstrucciones de prueba para elegir una focal que el movimiento no "
       "determina (D48), 0 para omitirlas"),
    PT("Reconstruções de teste para escolher uma focal que o movimento não "
       "determina (D48), 0 para pular"),
    IT("Ricostruzioni di prova per scegliere una focale che il moto non "
       "determina (D48), 0 per saltarle"),
    NL("Proefreconstructies om een brandpuntsafstand te kiezen die de beweging "
       "niet vastlegt (D48), 0 om over te slaan"),
    RU("Пробные реконструкции для выбора фокусного расстояния, которое движение "
       "не определяет (D48); 0 -- пропустить"),
    TR("Hareketin belirleyemediği bir odak uzaklığını seçmek için yapılan deneme "
       "yeniden oluşturmaları (D48), atlamak için 0"));

SS_MSG(refine_principal_point_help,
    EN("Let BA move cx,cy throughout; off as in COLMAP, where it is nearly a "
       "camera rotation (D50)"),
    JA("バンドル調整に cx,cy を動かさせます。COLMAP と同じく既定は無効"
       "（カメラ回転とほとんど区別できないため、D50）"),
    ZH_HANS("允许平差全程移动 cx,cy；默认关闭，与 COLMAP 一致，因为它几乎等同于"
            "相机旋转（D50）"),
    ZH_HANT("允許平差全程移動 cx,cy；預設關閉，與 COLMAP 一致，因為它幾乎等同於"
            "相機旋轉（D50）"),
    KO("번들 조정이 cx,cy 를 계속 움직이도록 허용합니다. COLMAP 처럼 기본은 꺼짐 "
       "-- 카메라 회전과 거의 구별되지 않기 때문입니다(D50)"),
    DE("Dem Bündelausgleich erlauben, cx,cy durchgehend zu bewegen; aus wie in "
       "COLMAP, wo das fast eine Kameradrehung ist (D50)"),
    FR("Laisser l'ajustement déplacer cx,cy tout du long ; désactivé comme dans "
       "COLMAP, où cela équivaut presque à une rotation de caméra (D50)"),
    ES("Dejar que el ajuste mueva cx,cy en todo momento; desactivado como en "
       "COLMAP, donde equivale casi a una rotación de cámara (D50)"),
    PT("Deixar o ajuste mover cx,cy o tempo todo; desligado como no COLMAP, onde "
       "isso equivale quase a uma rotação de câmera (D50)"),
    IT("Lasciare che il bundle adjustment sposti cx,cy per tutto il tempo; "
       "disattivo come in COLMAP, dove equivale quasi a una rotazione di camera "
       "(D50)"),
    NL("De bundelaanpassing cx,cy voortdurend laten verschuiven; uit zoals in "
       "COLMAP, waar dat bijna een camerarotatie is (D50)"),
    RU("Разрешить уравниванию двигать cx,cy на всём протяжении; выключено, как в "
       "COLMAP, где это почти поворот камеры (D50)"),
    TR("Demet dengelemesinin cx,cy'yi baştan sona oynatmasına izin ver; COLMAP'te "
       "olduğu gibi kapalı, çünkü bu neredeyse bir kamera dönüşüdür (D50)"));

SS_MSG(final_principal_point_help,
    EN("One principal-point-free global BA on the finished model, for a single "
       "camera group (D51)"),
    JA("カメラグループが 1 つのとき、完成モデルに対して主点を自由にした全体"
       "バンドル調整を 1 回行います（D51）"),
    ZH_HANS("当只有一个相机组时，对完成的模型再做一次放开主点的全局平差（D51）"),
    ZH_HANT("當只有一個相機群組時，對完成的模型再做一次放開主點的全域平差（D51）"),
    KO("카메라 그룹이 하나일 때, 완성된 모델에 주점을 풀어 준 전역 번들 조정을 "
       "한 번 돌립니다(D51)"),
    DE("Ein globaler Bündelausgleich ohne Festhalten des Hauptpunkts auf dem "
       "fertigen Modell, bei einer einzigen Kameragruppe (D51)"),
    FR("Un ajustement global libérant le point principal sur le modèle terminé, "
       "pour un seul groupe de caméras (D51)"),
    ES("Un ajuste global que libera el punto principal sobre el modelo "
       "terminado, para un único grupo de cámaras (D51)"),
    PT("Um ajuste global que libera o ponto principal no modelo terminado, para "
       "um único grupo de câmeras (D51)"),
    IT("Un bundle adjustment globale che libera il punto principale sul modello "
       "finito, per un solo gruppo di camere (D51)"),
    NL("Eén globale bundelaanpassing met vrij hoofdpunt op het voltooide model, "
       "bij één cameragroep (D51)"),
    RU("Одно глобальное уравнивание с освобождённой главной точкой на готовой "
       "модели, для единственной группы камер (D51)"),
    TR("Tek bir kamera grubu için, bitmiş modelde ana noktayı serbest bırakan bir "
       "genel demet dengelemesi (D51)"));

SS_MSG(pp_min_images_help,
    EN("Images a group needs before that final pass runs on it"),
    JA("その最終パスを走らせるためにグループが必要とする画像数"),
    ZH_HANS("某个相机组需要多少张图像，才会对它执行那次最终平差"),
    ZH_HANT("某個相機群組需要多少張影像，才會對它執行那次最終平差"),
    KO("그 마지막 단계를 돌리려면 그룹에 필요한 이미지 수"),
    DE("Bilder, die eine Gruppe braucht, bevor dieser Schlussdurchgang auf ihr "
       "läuft"),
    FR("Images qu'un groupe doit avoir pour que cette passe finale s'y applique"),
    ES("Imágenes que un grupo necesita para que esa pasada final se ejecute "
       "sobre él"),
    PT("Imagens de que um grupo precisa para que essa passagem final rode nele"),
    IT("Immagini che un gruppo deve avere perché quel passaggio finale vi giri"),
    NL("Beelden die een groep nodig heeft voordat die slotgang erop draait"),
    RU("Сколько изображений нужно группе, чтобы этот финальный проход по ней "
       "выполнился"),
    TR("Bu son geçişin bir grupta çalışması için gereken görüntü sayısı"));

SS_MSG(refine_extra_params_help,
    EN("Let BA move the distortion coefficients throughout; off holds them and "
       "leaves them to the final pass (D72)"),
    JA("バンドル調整に歪み係数を常時動かさせます。無効にすると固定し、最後の"
       "仕上げに任せます（D72）"),
    ZH_HANS("允许平差全程调整畸变系数；关闭则保持固定，交给最后一遍处理（D72）"),
    ZH_HANT("允許平差全程調整畸變係數；關閉則保持固定，交給最後一遍處理（D72）"),
    KO("번들 조정이 왜곡 계수를 계속 움직이도록 허용합니다. 끄면 고정해 두고 "
       "마지막 단계에 맡깁니다(D72)"),
    DE("Dem Bündelausgleich erlauben, die Verzeichnungskoeffizienten durchgehend "
       "zu bewegen; aus hält sie fest und überlässt sie dem letzten Durchgang (D72)"),
    FR("Laisser l'ajustement déplacer les coefficients de distorsion tout du "
       "long ; désactivé, ils restent fixes jusqu'à la passe finale (D72)"),
    ES("Dejar que el ajuste mueva los coeficientes de distorsión en todo momento; "
       "desactivado los mantiene fijos hasta la pasada final (D72)"),
    PT("Deixar o ajuste mover os coeficientes de distorção o tempo todo; desligado "
       "os mantém fixos até a passagem final (D72)"),
    IT("Lasciare che il bundle adjustment sposti i coefficienti di distorsione per "
       "tutto il tempo; disattivo li tiene fermi fino alla passata finale (D72)"),
    NL("De bundelaanpassing de vertekeningscoëfficiënten voortdurend laten "
       "bijstellen; uit houdt ze vast tot de laatste ronde (D72)"),
    RU("Разрешить уравниванию двигать коэффициенты дисторсии на всём протяжении; "
       "выключено -- держит их до финального прохода (D72)"),
    TR("Demet dengelemesinin bozulma katsayılarını baştan sona oynatmasına izin "
       "ver; kapalıyken sabit tutulur ve son geçişe bırakılır (D72)"));

SS_MSG(final_extra_params_help,
    EN("Release the distortion coefficients for the global BA on the finished "
       "model; off holds the principal point with them"),
    JA("完成モデルへの全体バンドル調整で歪み係数を自由にします。無効にすると"
       "主点も一緒に固定されます"),
    ZH_HANS("在对完成模型的全局平差中放开畸变系数；关闭时主点也随之固定"),
    ZH_HANT("在對完成模型的全域平差中放開畸變係數；關閉時主點也隨之固定"),
    KO("완성된 모델의 전역 번들 조정에서 왜곡 계수를 풀어 줍니다. 끄면 주점도 함께 "
       "고정됩니다"),
    DE("Die Verzeichnungskoeffizienten im globalen Bündelausgleich auf dem "
       "fertigen Modell freigeben; aus hält den Hauptpunkt mit ihnen fest"),
    FR("Libérer les coefficients de distorsion pour l'ajustement global sur le "
       "modèle terminé ; désactivé, le point principal reste fixe avec eux"),
    ES("Liberar los coeficientes de distorsión en el ajuste global sobre el "
       "modelo terminado; desactivado mantiene fijo también el punto principal"),
    PT("Liberar os coeficientes de distorção no ajuste global sobre o modelo "
       "terminado; desligado mantém o ponto principal fixo junto com eles"),
    IT("Liberare i coefficienti di distorsione nel bundle adjustment globale sul "
       "modello finito; disattivo tiene fermo con loro anche il punto principale"),
    NL("De vertekeningscoëfficiënten vrijgeven in de globale bundelaanpassing op "
       "het voltooide model; uit houdt het hoofdpunt er mee vast"),
    RU("Освободить коэффициенты дисторсии в глобальном уравнивании готовой "
       "модели; выключено -- вместе с ними держит и главную точку"),
    TR("Bitmiş modeldeki genel demet dengelemesinde bozulma katsayılarını serbest "
       "bırak; kapalıyken ana nokta da onlarla birlikte sabit tutulur"));

SS_MSG(final_per_image_intrinsics_help,
    EN("One more global BA at the very end with every image on its own "
       "intrinsics, and nothing clamped back to the group (D73)"),
    JA("最後にもう一度、画像ごとの内部パラメータで全体バンドル調整を行います。"
       "グループの値への引き戻しもしません（D73）"),
    ZH_HANS("最后再做一次全局平差，每张图像各用自己的内参，且不再把参数拉回相机组"
            "（D73）"),
    ZH_HANT("最後再做一次全域平差，每張影像各用自己的內參，且不再把參數拉回相機群組"
            "（D73）"),
    KO("맨 마지막에 이미지마다 자체 내부 파라미터로 전역 번들 조정을 한 번 더 "
       "돌리며, 그룹 값으로 되돌리지 않습니다(D73)"),
    DE("Ganz am Ende ein weiterer globaler Bündelausgleich mit eigener innerer "
       "Orientierung je Bild, ohne Zurückholen auf die Gruppe (D73)"),
    FR("Un ajustement global de plus tout à la fin, chaque image avec ses propres "
       "paramètres internes et sans rappel vers le groupe (D73)"),
    ES("Un ajuste global más al final, cada imagen con sus propios parámetros "
       "internos y sin devolverlos al grupo (D73)"),
    PT("Mais um ajuste global no fim, cada imagem com seus próprios parâmetros "
       "internos e sem puxá-los de volta para o grupo (D73)"),
    IT("Un ulteriore bundle adjustment globale alla fine, ogni immagine con i "
       "propri parametri interni e senza richiamarli al gruppo (D73)"),
    NL("Nog één globale bundelaanpassing helemaal aan het eind, elk beeld met "
       "eigen interne parameters en zonder terughalen naar de groep (D73)"),
    RU("Ещё одно глобальное уравнивание в самом конце: у каждого кадра свои "
       "внутренние параметры, и возврата к группе нет (D73)"),
    TR("En sonda bir genel demet dengelemesi daha: her görüntü kendi iç "
       "parametreleriyle ve gruba geri çekilmeden (D73)"));

SS_MSG(orient_help,
    EN("Write the model upright, centred and unit-scaled from the camera poses, "
       "instead of in the seed pair's arbitrary gauge"),
    JA("シードペアの任意のゲージではなく、カメラ姿勢から向きを起こし、中心を"
       "そろえ、単位スケールにしてモデルを書き出します"),
    ZH_HANS("按相机位姿把模型摆正、居中并归一化尺度后写出，而不是沿用种子对的"
            "任意基准"),
    ZH_HANT("按相機位姿把模型擺正、置中並歸一化尺度後寫出，而不是沿用種子對的"
            "任意基準"),
    KO("씨앗 쌍의 임의 기준 대신, 카메라 자세로부터 모델을 바로 세우고 중심을 "
       "맞추고 단위 크기로 만들어 씁니다"),
    DE("Das Modell aus den Kameraposen aufgerichtet, zentriert und einheitlich "
       "skaliert schreiben statt in der willkürlichen Eichung des Startpaars"),
    FR("Écrire le modèle redressé, centré et mis à l'échelle unité d'après les "
       "poses de caméra, plutôt que dans la jauge arbitraire de la paire "
       "d'amorce"),
    ES("Escribir el modelo enderezado, centrado y a escala unidad a partir de "
       "las poses de cámara, en vez de en el marco arbitrario del par semilla"),
    PT("Escrever o modelo endireitado, centrado e em escala unitária a partir "
       "das poses de câmera, em vez de no referencial arbitrário do par semente"),
    IT("Scrivere il modello raddrizzato, centrato e in scala unitaria a partire "
       "dalle pose delle camere, invece che nel riferimento arbitrario della "
       "coppia iniziale"),
    NL("Het model rechtop, gecentreerd en op eenheidsschaal wegschrijven vanuit "
       "de cameraposes, in plaats van in de willekeurige ijking van het "
       "startpaar"),
    RU("Записывать модель выпрямленной, отцентрованной и в единичном масштабе "
       "по позам камер, а не в произвольной калибровке начальной пары"),
    TR("Modeli, başlangıç çiftinin gelişigüzel ölçeği yerine, kamera "
       "duruşlarından doğrultulmuş, ortalanmış ve birim ölçekli olarak yaz"));

SS_MSG(min_tri_angle_help,
    EN("Triangulation angle a 3D point must subtend to be kept"),
    JA("3D 点が残るために張るべき三角測量角"),
    ZH_HANS("三维点需张开多大的三角化角度才会被保留"),
    ZH_HANT("三維點需張開多大的三角化角度才會被保留"),
    KO("3D 점이 남으려면 이뤄야 하는 삼각측량 각"),
    DE("Triangulationswinkel, den ein 3D-Punkt aufspannen muss, um zu bleiben"),
    FR("Angle de triangulation qu'un point 3D doit sous-tendre pour être gardé"),
    ES("Ángulo de triangulación que un punto 3D debe subtender para conservarse"),
    PT("Ângulo de triangulação que um ponto 3D deve subtender para ser mantido"),
    IT("Angolo di triangolazione che un punto 3D deve sottendere per essere "
       "tenuto"),
    NL("Triangulatiehoek die een 3D-punt moet opspannen om te blijven"),
    RU("Угол триангуляции, который должна образовывать 3D-точка, чтобы остаться"),
    TR("Bir 3B noktanın tutulması için oluşturması gereken üçgenleme açısı"));

SS_MSG(init_min_tri_angle_help,
    EN("Median triangulation angle the seed pair must reach; relaxed stepwise if "
       "nothing passes"),
    JA("シードペアが達すべき三角測量角の中央値。通るものがなければ段階的に"
       "緩めます"),
    ZH_HANS("种子对必须达到的三角化角度中位数；若无一通过则逐步放宽"),
    ZH_HANT("種子對必須達到的三角化角度中位數；若無一通過則逐步放寬"),
    KO("씨앗 쌍이 넘어야 하는 삼각측량 각의 중앙값. 통과하는 것이 없으면 단계적으로 "
       "완화합니다"),
    DE("Median-Triangulationswinkel, den das Startpaar erreichen muss; wird "
       "schrittweise gelockert, wenn nichts besteht"),
    FR("Angle de triangulation médian que la paire d'amorce doit atteindre ; "
       "relâché par paliers si rien ne passe"),
    ES("Ángulo de triangulación mediano que el par semilla debe alcanzar; se "
       "relaja por pasos si no pasa nada"),
    PT("Ângulo de triangulação mediano que o par semente deve alcançar; é "
       "afrouxado por etapas se nada passar"),
    IT("Angolo di triangolazione mediano che la coppia iniziale deve "
       "raggiungere; viene allentato a passi se non passa nulla"),
    NL("Mediane triangulatiehoek die het startpaar moet halen; stapsgewijs "
       "versoepeld als niets slaagt"),
    RU("Медианный угол триангуляции, которого должна достичь начальная пара; "
       "ослабляется по шагам, если ничего не проходит"),
    TR("Başlangıç çiftinin ulaşması gereken ortanca üçgenleme açısı; hiçbir şey "
       "geçmezse adım adım gevşetilir"));

SS_MSG(min_pnp_inliers_help,
    EN("2D-3D inliers an image needs to register"),
    JA("画像が登録されるために必要な 2D-3D インライア数"),
    ZH_HANS("图像完成注册所需的 2D-3D 内点数"),
    ZH_HANT("影像完成註冊所需的 2D-3D 內點數"),
    KO("이미지가 등록되는 데 필요한 2D-3D 내부점 수"),
    DE("2D-3D-Inlier, die ein Bild zum Registrieren braucht"),
    FR("Inliers 2D-3D nécessaires pour enregistrer une image"),
    ES("Inliers 2D-3D que una imagen necesita para registrarse"),
    PT("Inliers 2D-3D de que uma imagem precisa para se registrar"),
    IT("Inlier 2D-3D che un'immagine richiede per registrarsi"),
    NL("2D-3D-inliers die een beeld nodig heeft om te registreren"),
    RU("Сколько 2D-3D инлайеров нужно изображению для регистрации"),
    TR("Bir görüntünün kaydolması için gereken 2B-3B içleyen sayısı"));

SS_MSG(min_pnp_ratio_help,
    EN("Inlier fraction an image needs to register, which rejects accidental "
       "agreement"),
    JA("画像が登録されるために必要なインライア比。偶然の一致を退けます"),
    ZH_HANS("图像完成注册所需的内点比例，用以排除偶然的一致"),
    ZH_HANT("影像完成註冊所需的內點比例，用以排除偶然的一致"),
    KO("이미지가 등록되는 데 필요한 내부점 비율. 우연의 일치를 걸러 냅니다"),
    DE("Inlier-Anteil, den ein Bild zum Registrieren braucht; er weist zufällige "
       "Übereinstimmung ab"),
    FR("Fraction d'inliers nécessaire pour enregistrer une image, qui écarte les "
       "accords fortuits"),
    ES("Fracción de inliers que una imagen necesita para registrarse, que "
       "rechaza las coincidencias fortuitas"),
    PT("Fração de inliers de que uma imagem precisa para se registrar, que "
       "rejeita concordâncias fortuitas"),
    IT("Frazione di inlier che un'immagine richiede per registrarsi, che respinge "
       "gli accordi casuali"),
    NL("Inlier-aandeel dat een beeld nodig heeft om te registreren, dat "
       "toevallige overeenstemming afwijst"),
    RU("Доля инлайеров, нужная изображению для регистрации; она отсекает "
       "случайные совпадения"),
    TR("Bir görüntünün kaydolması için gereken içleyen oranı; rastlantısal "
       "uyuşmayı eler"));

SS_MSG(min_image_points_help,
    EN("Observations below which a registered image is dropped again"),
    JA("これを下回ると登録済み画像が再び外される観測数"),
    ZH_HANS("观测数低于此值时，已注册的图像会被重新剔除"),
    ZH_HANT("觀測數低於此值時，已註冊的影像會被重新剔除"),
    KO("관측 수가 이 값에 못 미치면 등록된 이미지를 다시 버립니다"),
    DE("Beobachtungen, unter denen ein registriertes Bild wieder entfällt"),
    FR("Observations en dessous desquelles une image enregistrée est retirée"),
    ES("Observaciones por debajo de las cuales una imagen registrada se retira"),
    PT("Observações abaixo das quais uma imagem registrada é retirada"),
    IT("Osservazioni sotto le quali un'immagine registrata viene tolta di nuovo"),
    NL("Waarnemingen waaronder een geregistreerd beeld weer vervalt"),
    RU("Число наблюдений, ниже которого зарегистрированное изображение снова "
       "отбрасывается"),
    TR("Altında kayıtlı bir görüntünün yeniden atıldığı gözlem sayısı"));

SS_MSG(ba_loss_help,
    EN("Robust loss used by mapping-time bundle adjustment"),
    JA("マッピング中のバンドル調整が使うロバスト損失"),
    ZH_HANS("建图阶段的平差所用的稳健损失函数"),
    ZH_HANT("建圖階段的平差所用的穩健損失函數"),
    KO("매핑 중 번들 조정이 쓰는 강건 손실"),
    DE("Robuster Verlust, den der Bündelausgleich beim Kartieren verwendet"),
    FR("Perte robuste employée par l'ajustement de faisceaux pendant la "
       "cartographie"),
    ES("Pérdida robusta que emplea el ajuste de haces durante la cartografía"),
    PT("Perda robusta usada pelo ajuste de feixes durante a cartografia"),
    IT("Perdita robusta usata dal bundle adjustment durante la cartografia"),
    NL("Robuust verlies dat de bundelaanpassing tijdens het karteren gebruikt"),
    RU("Устойчивая функция потерь, применяемая уравниванием во время построения"),
    TR("Haritalama sırasındaki demet dengelemesinin kullandığı gürbüz yitim"));

SS_MSG(ba_loss_param_help,
    EN("Huber delta / Cauchy c, in extraction pixels"),
    JA("Huber の delta / Cauchy の c。抽出時のピクセル単位"),
    ZH_HANS("Huber 的 delta 或 Cauchy 的 c，单位是提取时的像素"),
    ZH_HANT("Huber 的 delta 或 Cauchy 的 c，單位是擷取時的像素"),
    KO("Huber 의 delta 또는 Cauchy 의 c. 추출 시 픽셀 단위"),
    DE("Huber-Delta / Cauchy-c, in Extraktionspixeln"),
    FR("Delta de Huber / c de Cauchy, en pixels d'extraction"),
    ES("Delta de Huber / c de Cauchy, en píxeles de extracción"),
    PT("Delta de Huber / c de Cauchy, em pixels de extração"),
    IT("Delta di Huber / c di Cauchy, in pixel di estrazione"),
    NL("Huber-delta / Cauchy-c, in extractiepixels"),
    RU("Huber delta / Cauchy c, в пикселях извлечения"),
    TR("Huber delta / Cauchy c, çıkarım pikseli cinsinden"));

SS_MSG(seed_homography_help,
    EN("Re-test a seed candidate for a planar or panoramic configuration on its "
       "own inliers, rather than trusting verification's verdict"),
    JA("検証の判断をそのまま信じるのではなく、シード候補が平面的あるいは"
       "パノラマ的な配置かどうかを自身のインライアで検定し直します"),
    ZH_HANS("不直接采信验证的结论，而是用种子候选自身的内点重新检验它是否为平面或"
            "全景配置"),
    ZH_HANT("不直接採信驗證的結論，而是用種子候選自身的內點重新檢驗它是否為平面或"
            "全景配置"),
    KO("검증의 판단을 그대로 믿지 않고, 씨앗 후보가 평면이나 파노라마 배치인지 "
       "자체 내부점으로 다시 검정합니다"),
    DE("Einen Startkandidaten auf seinen eigenen Inliern erneut auf eine ebene "
       "oder panoramische Konfiguration prüfen, statt dem Urteil der Prüfung zu "
       "trauen"),
    FR("Retester un candidat d'amorce, sur ses propres inliers, pour une "
       "configuration plane ou panoramique, au lieu de se fier au verdict de la "
       "vérification"),
    ES("Volver a examinar un candidato semilla, sobre sus propios inliers, en "
       "busca de una configuración plana o panorámica, en vez de fiarse del "
       "veredicto de la verificación"),
    PT("Reexaminar um candidato semente, sobre os seus próprios inliers, à "
       "procura de uma configuração plana ou panorâmica, em vez de confiar no "
       "veredito da verificação"),
    IT("Riesaminare un candidato iniziale, sui suoi stessi inlier, per una "
       "configurazione piana o panoramica, invece di fidarsi del verdetto della "
       "verifica"),
    NL("Een startkandidaat op zijn eigen inliers opnieuw toetsen op een vlakke "
       "of panoramische opstelling, in plaats van op het oordeel van de "
       "verificatie te vertrouwen"),
    RU("Заново проверить кандидата в начальную пару на плоскую или панорамную "
       "конфигурацию по его собственным инлайерам, вместо того чтобы доверять "
       "вердикту проверки"),
    TR("Bir başlangıç adayını, doğrulamanın kararına güvenmek yerine kendi "
       "içleyenleri üzerinde düzlemsel ya da panoramik yerleşim için yeniden "
       "sına"));

SS_MSG(ba_real_help,
    EN("Scalar bundle adjustment computes in; df emulates double with a pair of "
       "floats, which wins where fp64 throughput is a fraction of fp32"),
    JA("バンドル調整が計算に使うスカラー型。df は float 2 つで double を模擬し、"
       "fp64 のスループットが fp32 のごく一部しかない環境で有利です"),
    ZH_HANS("平差所用的标量类型；df 用两个 float 模拟 double，在 fp64 吞吐远低于 "
            "fp32 的硬件上更划算"),
    ZH_HANT("平差所用的純量型別；df 用兩個 float 模擬 double，在 fp64 吞吐遠低於 "
            "fp32 的硬體上更划算"),
    KO("번들 조정이 계산에 쓰는 스칼라 형. df 는 float 두 개로 double 을 흉내 내며, "
       "fp64 처리량이 fp32 의 일부에 불과한 곳에서 유리합니다"),
    DE("Skalar, in dem der Bündelausgleich rechnet; df ahmt double mit einem "
       "Paar Floats nach und gewinnt dort, wo der fp64-Durchsatz nur ein "
       "Bruchteil von fp32 ist"),
    FR("Scalaire dans lequel calcule l'ajustement ; df émule le double avec deux "
       "flottants, ce qui l'emporte là où le débit fp64 n'est qu'une fraction du "
       "fp32"),
    ES("Escalar con el que calcula el ajuste; df emula el doble con un par de "
       "flotantes, lo que gana donde el rendimiento fp64 es una fracción del "
       "fp32"),
    PT("Escalar em que o ajuste calcula; df emula o double com um par de floats, "
       "o que ganha onde a vazão em fp64 é uma fração da de fp32"),
    IT("Scalare in cui calcola il bundle adjustment; df emula il double con una "
       "coppia di float, e vince dove il throughput fp64 è una frazione di fp32"),
    NL("Scalair waarin de bundelaanpassing rekent; df bootst double na met twee "
       "floats, wat wint waar de fp64-doorvoer een fractie van fp32 is"),
    RU("Скаляр, в котором считает уравнивание; df эмулирует double парой float и "
       "выигрывает там, где пропускная способность fp64 -- малая доля от fp32"),
    TR("Demet dengelemesinin hesapladığı skaler; df, double'ı iki float ile "
       "taklit eder ve fp64 veriminin fp32'nin bir kesri olduğu yerde kazanır"));

SS_MSG(ba_real_coarse_help,
    EN("Scalar for solves another solve will redo -- growth refinements and "
       "merge-tree levels; set equal to --ba-real to compute everything the same "
       "way"),
    JA("あとで解き直される解法に使うスカラー型（成長時の精密化と統合ツリーの各"
       "段）。--ba-real と同じにすればすべて同じ精度で計算します"),
    ZH_HANS("用于会被后续求解重做的那些求解的标量类型 —— 增长阶段的细化与合并树各层；"
            "设成与 --ba-real 相同即可全程用同一种精度"),
    ZH_HANT("用於會被後續求解重做的那些求解的純量型別 —— 增長階段的細化與合併樹各層；"
            "設成與 --ba-real 相同即可全程用同一種精度"),
    KO("나중에 다시 풀릴 계산에 쓰는 스칼라 형 -- 성장 단계의 정련과 병합 트리의 "
       "각 층. --ba-real 과 같게 두면 전부 같은 방식으로 계산합니다"),
    DE("Skalar für Lösungen, die eine spätere ohnehin wiederholt -- "
       "Wachstumsverfeinerungen und Ebenen des Merge-Baums; gleich --ba-real "
       "setzen, um alles gleich zu rechnen"),
    FR("Scalaire pour les résolutions qu'une autre refera -- affinements de "
       "croissance et niveaux de l'arbre de fusion ; mettez-le à --ba-real pour "
       "tout calculer de la même façon"),
    ES("Escalar para las resoluciones que otra rehará: refinamientos de "
       "crecimiento y niveles del árbol de fusión; póngalo igual a --ba-real "
       "para calcularlo todo igual"),
    PT("Escalar para as resoluções que outra irá refazer -- refinamentos de "
       "crescimento e níveis da árvore de fusão; ponha igual a --ba-real para "
       "calcular tudo do mesmo jeito"),
    IT("Scalare per le risoluzioni che un'altra rifarà -- affinamenti di "
       "crescita e livelli dell'albero di fusione; lo imposti uguale a --ba-real "
       "per calcolare tutto allo stesso modo"),
    NL("Scalair voor oplossingen die een latere toch overdoet -- "
       "groeiverfijningen en niveaus van de samenvoegboom; zet gelijk aan "
       "--ba-real om alles hetzelfde te rekenen"),
    RU("Скаляр для решений, которые всё равно будут переделаны -- уточнения при "
       "росте и уровни дерева слияния; приравняйте к --ba-real, чтобы считать "
       "всё одинаково"),
    TR("Bir başka çözümün yineleyeceği çözümler için skaler -- büyüme "
       "iyileştirmeleri ve birleştirme ağacı düzeyleri; her şeyi aynı biçimde "
       "hesaplamak için --ba-real ile eşitleyin"));

SS_MSG(ba_solver_help,
    EN("Linear solver for the reduced camera system; auto switches to CG above "
       "the size where the dense factorization stops paying"),
    JA("縮約カメラ系の線形ソルバー。auto は密な因数分解が割に合わなくなる規模を"
       "超えると CG に切り替えます"),
    ZH_HANS("求解简化相机系统的线性求解器；auto 在稠密分解不再划算的规模之上改用 CG"),
    ZH_HANT("求解簡化相機系統的線性求解器；auto 在稠密分解不再划算的規模之上改用 CG"),
    KO("축약 카메라 계의 선형 솔버. auto 는 조밀 분해가 수지에 맞지 않게 되는 "
       "크기를 넘으면 CG 로 바꿉니다"),
    DE("Linearer Löser für das reduzierte Kamerasystem; auto wechselt oberhalb "
       "der Größe zu CG, ab der sich die dichte Faktorisierung nicht mehr lohnt"),
    FR("Solveur linéaire du système caméra réduit ; auto passe à CG au-delà de "
       "la taille où la factorisation dense cesse d'être rentable"),
    ES("Solucionador lineal del sistema de cámaras reducido; auto pasa a CG por "
       "encima del tamaño en que la factorización densa deja de compensar"),
    PT("Solucionador linear do sistema de câmeras reduzido; auto passa a CG "
       "acima do tamanho em que a fatoração densa deixa de compensar"),
    IT("Risolutore lineare del sistema camera ridotto; auto passa a CG oltre la "
       "dimensione in cui la fattorizzazione densa smette di convenire"),
    NL("Lineaire oplosser voor het gereduceerde camerastelsel; auto stapt over "
       "op CG boven de omvang waar dichte factorisatie niet meer loont"),
    RU("Линейный решатель для приведённой системы камер; auto переходит на CG "
       "выше того размера, при котором плотная факторизация перестаёт окупаться"),
    TR("İndirgenmiş kamera sistemi için doğrusal çözücü; auto, yoğun çarpanlara "
       "ayırmanın karşılığını vermediği boyutun üstünde CG'ye geçer"));

SS_MSG(retri_scale_help,
    EN("Retriangulation tolerance as a fraction of --max-error, 0 to skip the "
       "pass"),
    JA("再三角測量の許容差。--max-error に対する割合で指定します。0 でこのパスを"
       "省略"),
    ZH_HANS("重三角化的容差，以 --max-error 的比例给出；0 表示跳过该阶段"),
    ZH_HANT("重三角化的容差，以 --max-error 的比例給出；0 表示略過該階段"),
    KO("재삼각측량 허용 오차. --max-error 에 대한 비율로 줍니다. 0 이면 이 단계를 "
       "건너뜁니다"),
    DE("Toleranz der Retriangulation als Anteil von --max-error, 0 überspringt "
       "den Durchgang"),
    FR("Tolérance de retriangulation en fraction de --max-error, 0 pour sauter "
       "la passe"),
    ES("Tolerancia de retriangulación como fracción de --max-error, 0 para "
       "saltarse la pasada"),
    PT("Tolerância de retriangulação como fração de --max-error, 0 para pular a "
       "passagem"),
    IT("Tolleranza della ritriangolazione come frazione di --max-error, 0 per "
       "saltare il passaggio"),
    NL("Hertriangulatietolerantie als deel van --max-error, 0 om de gang over te "
       "slaan"),
    RU("Допуск повторной триангуляции как доля от --max-error; 0 -- пропустить "
       "проход"),
    TR("Yeniden üçgenleme toleransı, --max-error'ın oranı olarak; geçişi atlamak "
       "için 0"));

SS_MSG(merge_tracks_help,
    EN("Fuse two 3D points a correspondence says are the same feature"),
    JA("対応が同じ特徴だと示す 2 つの 3D 点を融合します"),
    ZH_HANS("把对应关系判定为同一特征的两个三维点合并"),
    ZH_HANT("把對應關係判定為同一特徵的兩個三維點合併"),
    KO("대응이 같은 특징이라고 말하는 두 3D 점을 합칩니다"),
    DE("Zwei 3D-Punkte verschmelzen, die eine Korrespondenz für dasselbe Merkmal "
       "erklärt"),
    FR("Fusionner deux points 3D qu'une correspondance déclare être le même point "
       "d'intérêt"),
    ES("Fundir dos puntos 3D que una correspondencia declara el mismo rasgo"),
    PT("Fundir dois pontos 3D que uma correspondência declara ser o mesmo traço"),
    IT("Fondere due punti 3D che una corrispondenza dichiara essere lo stesso "
       "punto"),
    NL("Twee 3D-punten samensmelten die een correspondentie hetzelfde kenmerk "
       "noemt"),
    RU("Сливать две 3D-точки, которые соответствие объявляет одним и тем же "
       "признаком"),
    TR("Bir karşılığın aynı öznitelik dediği iki 3B noktayı kaynaştır"));

SS_MSG(rank_by_visibility_help,
    EN("Rank the next image by how its visible structure spreads over the frame, "
       "not by count"),
    JA("次に登録する画像を、見えている構造が画面にどう広がっているかで順位づけ"
       "します（数ではなく）"),
    ZH_HANS("按可见结构在画面中的铺开程度而非数量来给下一张待注册图像排序"),
    ZH_HANT("按可見結構在畫面中的鋪開程度而非數量來給下一張待註冊影像排序"),
    KO("다음 이미지를 개수가 아니라 보이는 구조가 화면에 얼마나 퍼져 있는지로 "
       "순위 매깁니다"),
    DE("Das nächste Bild danach reihen, wie sich seine sichtbare Struktur über "
       "das Bild verteilt, nicht nach deren Anzahl"),
    FR("Classer l'image suivante selon l'étalement de sa structure visible dans "
       "le cadre, et non selon le nombre"),
    ES("Ordenar la imagen siguiente por cómo se reparte su estructura visible "
       "por el encuadre, no por la cantidad"),
    PT("Ordenar a próxima imagem pelo modo como a sua estrutura visível se "
       "espalha pelo enquadramento, não pela quantidade"),
    IT("Ordinare l'immagine successiva per come la sua struttura visibile si "
       "distribuisce nell'inquadratura, non per quantità"),
    NL("Het volgende beeld rangschikken naar hoe zijn zichtbare structuur zich "
       "over het kader spreidt, niet naar aantal"),
    RU("Ранжировать следующее изображение по тому, как видимая структура "
       "распределена по кадру, а не по её количеству"),
    TR("Sonraki görüntüyü, görünen yapının kare üzerine nasıl yayıldığına göre "
       "sırala, sayısına göre değil"));

SS_MSG(seed_blocking_help,
    EN("A seed retry starts somewhere no earlier attempt reached, instead of "
       "rebuilding it"),
    JA("シードの再試行を、以前の試行が届かなかった場所から始めます"
       "（同じものを作り直しません）"),
    ZH_HANS("重新选取种子时，从此前任何一次尝试都未触及的位置开始，而不是重建同一处"),
    ZH_HANT("重新選取種子時，從此前任何一次嘗試都未觸及的位置開始，而不是重建同一處"),
    KO("씨앗을 다시 시도할 때, 앞선 시도가 닿지 못한 곳에서 시작합니다"),
    DE("Ein neuer Startversuch beginnt dort, wo kein früherer hinkam, statt ihn "
       "nachzubauen"),
    FR("Une nouvelle tentative d'amorce démarre là où aucune précédente n'est "
       "allée, au lieu de la refaire"),
    ES("Un nuevo intento de semilla arranca donde ninguno anterior llegó, en vez "
       "de rehacerlo"),
    PT("Uma nova tentativa de semente começa onde nenhuma anterior chegou, em vez "
       "de refazê-la"),
    IT("Un nuovo tentativo di innesco parte da dove nessuno precedente è "
       "arrivato, invece di rifarlo"),
    NL("Een nieuwe startpoging begint waar geen eerdere kwam, in plaats van hem "
       "over te doen"),
    RU("Повторная попытка начальной пары стартует там, куда не добралась ни одна "
       "прежняя, а не повторяет её"),
    TR("Yeni bir başlangıç denemesi, öncekilerin ulaşmadığı bir yerden başlar, "
       "aynısını yeniden kurmaz"));

SS_MSG(mapper_help,
    EN("One incremental reconstruction of the whole capture, or small atoms of "
       "the view graph reconstructed separately and merged upwards"),
    JA("撮影全体を 1 つの逐次再構成で作るか、視点グラフの小さな原子を別々に"
       "再構成して上へ統合するか"),
    ZH_HANS("对整段拍摄做一次增量重建，或把视图图切成小原子分别重建再逐层向上合并"),
    ZH_HANT("對整段拍攝做一次增量重建，或把視圖圖切成小原子分別重建再逐層向上合併"),
    KO("촬영 전체를 한 번의 증분 재구성으로 만들지, 뷰 그래프의 작은 원자들을 따로 "
       "재구성해 위로 병합할지"),
    DE("Eine inkrementelle Rekonstruktion der ganzen Aufnahme, oder kleine Atome "
       "des Sichtgraphen einzeln rekonstruiert und nach oben zusammengeführt"),
    FR("Une reconstruction incrémentale de toute la prise de vue, ou de petits "
       "atomes du graphe de vues reconstruits séparément puis fusionnés vers le "
       "haut"),
    ES("Una reconstrucción incremental de toda la captura, o átomos pequeños del "
       "grafo de vistas reconstruidos por separado y fusionados hacia arriba"),
    PT("Uma reconstrução incremental de toda a captura, ou pequenos átomos do "
       "grafo de vistas reconstruídos à parte e fundidos para cima"),
    IT("Una ricostruzione incrementale dell'intera ripresa, o piccoli atomi del "
       "grafo delle viste ricostruiti a parte e fusi verso l'alto"),
    NL("Eén incrementele reconstructie van de hele opname, of kleine atomen van "
       "de zichtgraaf apart gereconstrueerd en naar boven samengevoegd"),
    RU("Одна инкрементальная реконструкция всей съёмки либо мелкие атомы графа "
       "видов, восстановленные по отдельности и слитые вверх"),
    TR("Tüm çekimin tek bir artımlı yeniden oluşturması ya da görüş çizgesinin "
       "küçük atomlarının ayrı ayrı oluşturulup yukarı doğru birleştirilmesi"));

SS_MSG(bup_atom_size_help,
    EN("Images a view-graph atom is split until it is under"),
    JA("視点グラフの原子を、これを下回るまで分割する画像数"),
    ZH_HANS("视图图的原子会一直拆分，直到图像数低于此值"),
    ZH_HANT("視圖圖的原子會一直拆分，直到影像數低於此值"),
    KO("뷰 그래프 원자를 이 값 아래로 내려갈 때까지 쪼갭니다"),
    DE("Bilder, unter die ein Atom des Sichtgraphen zerteilt wird"),
    FR("Images en dessous desquelles un atome du graphe de vues est découpé"),
    ES("Imágenes por debajo de las cuales se divide un átomo del grafo de vistas"),
    PT("Imagens abaixo das quais um átomo do grafo de vistas é dividido"),
    IT("Immagini sotto le quali un atomo del grafo delle viste viene diviso"),
    NL("Beelden waaronder een atoom van de zichtgraaf wordt opgesplitst"),
    RU("До скольких изображений дробится атом графа видов"),
    TR("Bir görüş çizgesi atomunun altına inene dek bölündüğü görüntü sayısı"));

SS_MSG(bup_overlap_help,
    EN("Images each atom borrows from its sibling, which is what a merge aligns "
       "on"),
    JA("各原子が隣の原子から借りる画像数。統合はこれを手がかりに位置合わせします"),
    ZH_HANS("每个原子向相邻原子借用的图像数，合并正是依据这些图像对齐"),
    ZH_HANT("每個原子向相鄰原子借用的影像數，合併正是依據這些影像對齊"),
    KO("각 원자가 이웃 원자에서 빌려오는 이미지 수. 병합은 이것을 기준으로 "
       "맞춥니다"),
    DE("Bilder, die jedes Atom von seinem Geschwister leiht -- daran richtet eine "
       "Zusammenführung aus"),
    FR("Images que chaque atome emprunte à son voisin, et sur lesquelles une "
       "fusion s'aligne"),
    ES("Imágenes que cada átomo toma prestadas de su hermano, y sobre las que se "
       "alinea una fusión"),
    PT("Imagens que cada átomo toma emprestadas do seu irmão, e sobre as quais "
       "uma fusão se alinha"),
    IT("Immagini che ogni atomo prende in prestito dal fratello, e su cui una "
       "fusione si allinea"),
    NL("Beelden die elk atoom van zijn buur leent, en waarop een samenvoeging "
       "uitlijnt"),
    RU("Сколько изображений каждый атом занимает у соседа -- по ним и "
       "выравнивается слияние"),
    TR("Her atomun kardeşinden ödünç aldığı görüntü sayısı; birleştirme bunlara "
       "göre hizalanır"));

SS_MSG(bup_rounds_help,
    EN("Levels of the merge tree"),
    JA("統合ツリーの段数"),
    ZH_HANS("合并树的层数"),
    ZH_HANT("合併樹的層數"),
    KO("병합 트리의 층수"),
    DE("Ebenen des Merge-Baums"),
    FR("Niveaux de l'arbre de fusion"),
    ES("Niveles del árbol de fusión"),
    PT("Níveis da árvore de fusão"),
    IT("Livelli dell'albero di fusione"),
    NL("Niveaus van de samenvoegboom"),
    RU("Число уровней дерева слияния"),
    TR("Birleştirme ağacının düzeyleri"));

SS_MSG(bup_atom_threads_help,
    EN("Atoms reconstructed at once, each on its own Vulkan context; 0 for the "
       "default"),
    JA("同時に再構成する原子の数。それぞれ独自の Vulkan コンテキストで動きます。"
       "0 で既定"),
    ZH_HANS("同时重建的原子数，各自使用独立的 Vulkan 上下文；0 表示采用默认值"),
    ZH_HANT("同時重建的原子數，各自使用獨立的 Vulkan 內容；0 表示採用預設值"),
    KO("한 번에 재구성하는 원자 수. 각기 자기 Vulkan 컨텍스트에서 돕니다. 0 이면 "
       "기본값"),
    DE("Atome, die gleichzeitig rekonstruiert werden, jedes in eigenem "
       "Vulkan-Kontext; 0 nimmt die Vorgabe"),
    FR("Atomes reconstruits à la fois, chacun dans son propre contexte Vulkan ; "
       "0 pour la valeur par défaut"),
    ES("Átomos reconstruidos a la vez, cada uno en su propio contexto de Vulkan; "
       "0 para el valor por defecto"),
    PT("Átomos reconstruídos ao mesmo tempo, cada um no seu contexto Vulkan; 0 "
       "para o valor padrão"),
    IT("Atomi ricostruiti insieme, ciascuno nel proprio contesto Vulkan; 0 per "
       "il valore predefinito"),
    NL("Atomen die tegelijk gereconstrueerd worden, elk in een eigen "
       "Vulkan-context; 0 voor de standaard"),
    RU("Сколько атомов восстанавливается одновременно, каждый в своём контексте "
       "Vulkan; 0 -- значение по умолчанию"),
    TR("Aynı anda oluşturulan atom sayısı, her biri kendi Vulkan bağlamında; "
       "varsayılan için 0"));

SS_MSG(bup_atom_ba_growth_help,
    EN("Model growth that triggers a bundle adjustment inside an atom"),
    JA("原子の内部でバンドル調整を起動するモデルの成長量"),
    ZH_HANS("原子内部触发一次平差所需的模型增长量"),
    ZH_HANT("原子內部觸發一次平差所需的模型增長量"),
    KO("원자 안에서 번들 조정을 부르는 모델 성장량"),
    DE("Modellwachstum, das innerhalb eines Atoms einen Bündelausgleich auslöst"),
    FR("Croissance du modèle qui déclenche un ajustement au sein d'un atome"),
    ES("Crecimiento del modelo que dispara un ajuste dentro de un átomo"),
    PT("Crescimento do modelo que dispara um ajuste dentro de um átomo"),
    IT("Crescita del modello che innesca un bundle adjustment dentro un atomo"),
    NL("Modelgroei die binnen een atoom een bundelaanpassing uitlokt"),
    RU("Прирост модели, запускающий уравнивание внутри атома"),
    TR("Bir atom içinde demet dengelemesini tetikleyen model büyümesi"));

SS_MSG(bup_joint_every_help,
    EN("Merge-tree levels between joint solves over every model"),
    JA("すべてのモデルをまとめて解く合同解法どうしの、統合ツリー上の間隔"),
    ZH_HANS("在合并树上，两次覆盖全部模型的联合求解之间相隔几层"),
    ZH_HANT("在合併樹上，兩次涵蓋全部模型的聯合求解之間相隔幾層"),
    KO("모든 모델을 함께 푸는 합동 해법 사이의 병합 트리 층 간격"),
    DE("Ebenen des Merge-Baums zwischen gemeinsamen Lösungen über alle Modelle"),
    FR("Niveaux de l'arbre de fusion entre deux résolutions conjointes portant "
       "sur tous les modèles"),
    ES("Niveles del árbol de fusión entre resoluciones conjuntas sobre todos los "
       "modelos"),
    PT("Níveis da árvore de fusão entre resoluções conjuntas sobre todos os "
       "modelos"),
    IT("Livelli dell'albero di fusione tra risoluzioni congiunte su tutti i "
       "modelli"),
    NL("Niveaus van de samenvoegboom tussen gezamenlijke oplossingen over alle "
       "modellen"),
    RU("Сколько уровней дерева слияния между совместными решениями по всем "
       "моделям"),
    TR("Tüm modeller üzerinde ortak çözümler arasındaki birleştirme ağacı "
       "düzeyi sayısı"));

SS_MSG(bup_atom_tight_final_help,
    EN("Converge each atom fully before the merge tree re-solves it anyway"),
    JA("統合ツリーがどのみち解き直す前に、各原子を完全に収束させます"),
    ZH_HANS("在合并树反正会重解之前，先让每个原子彻底收敛"),
    ZH_HANT("在合併樹反正會重解之前，先讓每個原子徹底收斂"),
    KO("병합 트리가 어차피 다시 풀기 전에, 각 원자를 끝까지 수렴시킵니다"),
    DE("Jedes Atom voll auskonvergieren, bevor der Merge-Baum es ohnehin neu löst"),
    FR("Faire converger chaque atome à fond avant que l'arbre de fusion ne le "
       "résolve de nouveau de toute façon"),
    ES("Hacer converger del todo cada átomo antes de que el árbol de fusión lo "
       "vuelva a resolver de todos modos"),
    PT("Fazer cada átomo convergir totalmente antes de a árvore de fusão o "
       "resolver de novo de qualquer modo"),
    IT("Far convergere del tutto ogni atomo prima che l'albero di fusione lo "
       "risolva di nuovo comunque"),
    NL("Elk atoom volledig laten convergeren voordat de samenvoegboom het toch "
       "opnieuw oplost"),
    RU("Полностью сходить каждый атом до того, как дерево слияния всё равно "
       "решит его заново"),
    TR("Birleştirme ağacı nasılsa yeniden çözecekken, her atomu önce tam "
       "yakınsat"));

SS_MSG(bup_coarse_ba_help,
    EN("Run the merge tree's intermediate joint solves to the loose growth-phase "
       "tolerance"),
    JA("統合ツリーの中間的な合同解法を、成長段階の緩い許容差まで走らせます"),
    ZH_HANS("合并树的中间联合求解只解到增长阶段的宽松容差为止"),
    ZH_HANT("合併樹的中間聯合求解只解到增長階段的寬鬆容差為止"),
    KO("병합 트리의 중간 합동 해법을 성장 단계의 느슨한 허용 오차까지만 돌립니다"),
    DE("Die Zwischenlösungen des Merge-Baums nur bis zur lockeren Toleranz der "
       "Wachstumsphase rechnen"),
    FR("Mener les résolutions conjointes intermédiaires de l'arbre de fusion "
       "jusqu'à la tolérance lâche de la phase de croissance"),
    ES("Llevar las resoluciones conjuntas intermedias del árbol de fusión solo "
       "hasta la tolerancia laxa de la fase de crecimiento"),
    PT("Levar as resoluções conjuntas intermédias da árvore de fusão apenas até "
       "à tolerância frouxa da fase de crescimento"),
    IT("Portare le risoluzioni congiunte intermedie dell'albero di fusione solo "
       "alla tolleranza larga della fase di crescita"),
    NL("De tussentijdse gezamenlijke oplossingen van de samenvoegboom slechts "
       "tot de losse tolerantie van de groeifase doorrekenen"),
    RU("Доводить промежуточные совместные решения дерева слияния лишь до "
       "свободного допуска фазы роста"),
    TR("Birleştirme ağacının ara ortak çözümlerini yalnızca büyüme evresinin "
       "gevşek toleransına kadar çalıştır"));

SS_MSG(bup_atom_init_trials_help,
    EN("Seed attempts for an atom's primary model"),
    JA("原子の主モデルに対するシードの試行回数"),
    ZH_HANS("为原子的主模型选取种子的尝试次数"),
    ZH_HANT("為原子的主模型選取種子的嘗試次數"),
    KO("원자의 주 모델을 위한 씨앗 시도 횟수"),
    DE("Startversuche für das Hauptmodell eines Atoms"),
    FR("Tentatives d'amorce pour le modèle principal d'un atome"),
    ES("Intentos de semilla para el modelo principal de un átomo"),
    PT("Tentativas de semente para o modelo principal de um átomo"),
    IT("Tentativi di innesco per il modello principale di un atomo"),
    NL("Startpogingen voor het hoofdmodel van een atoom"),
    RU("Число попыток выбрать начальную пару для главной модели атома"),
    TR("Bir atomun birincil modeli için başlangıç denemeleri"));

SS_MSG(bup_atom_min_fraction_help,
    EN("Fraction of an atom its primary model must cover to be kept"),
    JA("主モデルが残るために覆うべき原子の割合"),
    ZH_HANS("主模型需覆盖原子的多大比例才会被保留"),
    ZH_HANT("主模型需涵蓋原子的多大比例才會被保留"),
    KO("주 모델이 남으려면 원자의 얼마를 덮어야 하는지"),
    DE("Anteil eines Atoms, den sein Hauptmodell abdecken muss, um zu bleiben"),
    FR("Fraction d'un atome que son modèle principal doit couvrir pour être gardé"),
    ES("Fracción de un átomo que su modelo principal debe cubrir para "
       "conservarse"),
    PT("Fração de um átomo que o seu modelo principal deve cobrir para ser "
       "mantido"),
    IT("Frazione di un atomo che il suo modello principale deve coprire per "
       "essere tenuto"),
    NL("Deel van een atoom dat het hoofdmodel moet dekken om te blijven"),
    RU("Какую долю атома должна покрыть его главная модель, чтобы остаться"),
    TR("Bir atomun birincil modelinin tutulmak için kapsaması gereken oranı"));

SS_MSG(bup_joint_intrinsics_help,
    EN("Bundle-adjust every model in one problem with the intrinsics shared per "
       "camera"),
    JA("すべてのモデルを 1 つの問題としてバンドル調整し、内部パラメータは"
       "カメラごとに共有します"),
    ZH_HANS("把所有模型放进一个问题里平差，内参按相机共享"),
    ZH_HANT("把所有模型放進一個問題裡平差，內參按相機共享"),
    KO("모든 모델을 한 문제로 묶어 번들 조정하며, 내부 파라미터는 카메라별로 "
       "공유합니다"),
    DE("Jedes Modell in einem Problem ausgleichen, mit je Kamera geteilten "
       "Intrinsics"),
    FR("Ajuster tous les modèles dans un même problème, les paramètres internes "
       "partagés par caméra"),
    ES("Ajustar todos los modelos en un mismo problema, con los parámetros "
       "internos compartidos por cámara"),
    PT("Ajustar todos os modelos num mesmo problema, com os parâmetros internos "
       "partilhados por câmera"),
    IT("Aggiustare tutti i modelli in un unico problema, con i parametri interni "
       "condivisi per camera"),
    NL("Alle modellen in één probleem aanpassen, met de intrinsieken per camera "
       "gedeeld"),
    RU("Уравнивать все модели в одной задаче с общими для каждой камеры "
       "внутренними параметрами"),
    TR("Tüm modelleri tek bir problemde dengele, iç parametreler kamera başına "
       "paylaşılsın"));

SS_MSG(bup_grow_every_help,
    EN("Levels between growth passes over the models that did not merge; 0 "
       "disables"),
    JA("統合されなかったモデルに対する成長パスどうしの段の間隔。0 で無効"),
    ZH_HANS("对未能合并的模型执行增长阶段之间相隔几层；0 表示关闭"),
    ZH_HANT("對未能合併的模型執行增長階段之間相隔幾層；0 表示關閉"),
    KO("병합되지 않은 모델에 대한 성장 단계 사이의 층 간격. 0 이면 끕니다"),
    DE("Ebenen zwischen Wachstumsdurchgängen über die Modelle, die nicht "
       "zusammengeführt wurden; 0 schaltet ab"),
    FR("Niveaux entre deux passes de croissance sur les modèles qui n'ont pas "
       "fusionné ; 0 désactive"),
    ES("Niveles entre pasadas de crecimiento sobre los modelos que no se "
       "fusionaron; 0 lo desactiva"),
    PT("Níveis entre passagens de crescimento sobre os modelos que não se "
       "fundiram; 0 desativa"),
    IT("Livelli tra i passaggi di crescita sui modelli che non si sono fusi; 0 "
       "disattiva"),
    NL("Niveaus tussen groeigangen over de modellen die niet samengingen; 0 zet "
       "het uit"),
    RU("Сколько уровней между проходами роста по моделям, которые не слились; "
       "0 отключает"),
    TR("Birleşmeyen modeller üzerindeki büyüme geçişleri arasındaki düzey "
       "sayısı; 0 kapatır"));

SS_MSG(bup_grow_budget_help,
    EN("Images one growth pass may add to a model, as a fraction of what it "
       "holds"),
    JA("1 回の成長パスがモデルに加えてよい画像数。そのモデルが持つ数に対する割合"),
    ZH_HANS("一次增长阶段最多能给模型添加多少图像，按模型现有数量的比例给出"),
    ZH_HANT("一次增長階段最多能給模型添加多少影像，按模型現有數量的比例給出"),
    KO("성장 단계 한 번이 모델에 더할 수 있는 이미지 수. 모델이 가진 수에 대한 "
       "비율"),
    DE("Bilder, die ein Wachstumsdurchgang einem Modell hinzufügen darf, als "
       "Anteil dessen, was es hält"),
    FR("Images qu'une passe de croissance peut ajouter à un modèle, en fraction "
       "de ce qu'il contient"),
    ES("Imágenes que una pasada de crecimiento puede añadir a un modelo, como "
       "fracción de las que ya tiene"),
    PT("Imagens que uma passagem de crescimento pode acrescentar a um modelo, "
       "como fração das que já tem"),
    IT("Immagini che un passaggio di crescita può aggiungere a un modello, come "
       "frazione di quelle che già contiene"),
    NL("Beelden die één groeigang aan een model mag toevoegen, als deel van wat "
       "het al bevat"),
    RU("Сколько изображений один проход роста может добавить модели -- как доля "
       "от того, что в ней уже есть"),
    TR("Bir büyüme geçişinin bir modele ekleyebileceği görüntü sayısı, modelin "
       "elindekinin oranı olarak"));

SS_MSG(ba_growth_help,
    EN("Model growth that triggers the next global bundle adjustment"),
    JA("次の全体バンドル調整を起動するモデルの成長量"),
    ZH_HANS("触发下一次全局平差所需的模型增长量"),
    ZH_HANT("觸發下一次全域平差所需的模型增長量"),
    KO("다음 전역 번들 조정을 부르는 모델 성장량"),
    DE("Modellwachstum, das den nächsten globalen Bündelausgleich auslöst"),
    FR("Croissance du modèle qui déclenche le prochain ajustement global"),
    ES("Crecimiento del modelo que dispara el siguiente ajuste global"),
    PT("Crescimento do modelo que dispara o próximo ajuste global"),
    IT("Crescita del modello che innesca il prossimo bundle adjustment globale"),
    NL("Modelgroei die de volgende globale bundelaanpassing uitlokt"),
    RU("Прирост модели, запускающий следующее глобальное уравнивание"),
    TR("Sonraki genel demet dengelemesini tetikleyen model büyümesi"));

SS_MSG(ba_growth_rtol_help,
    EN("Relative cost improvement a growth-phase BA stops below, 0 for the "
       "solver's"),
    JA("成長段階のバンドル調整が停止する相対コスト改善量。0 でソルバー既定"),
    ZH_HANS("增长阶段的平差在相对代价改善低于此值时停止；0 表示用求解器自身的值"),
    ZH_HANT("增長階段的平差在相對代價改善低於此值時停止；0 表示用求解器自身的值"),
    KO("성장 단계 번들 조정이 멈추는 상대 비용 개선량. 0 이면 솔버 기본값"),
    DE("Relative Kostenverbesserung, unter der ein BA der Wachstumsphase "
       "aufhört; 0 nimmt die des Lösers"),
    FR("Amélioration relative du coût sous laquelle un ajustement de la phase de "
       "croissance s'arrête, 0 pour celle du solveur"),
    ES("Mejora relativa del coste por debajo de la cual se detiene un ajuste de "
       "la fase de crecimiento; 0 usa la del solucionador"),
    PT("Melhoria relativa do custo abaixo da qual um ajuste da fase de "
       "crescimento para; 0 usa a do solucionador"),
    IT("Miglioramento relativo del costo sotto il quale un BA della fase di "
       "crescita si ferma, 0 per quello del risolutore"),
    NL("Relatieve kostenverbetering waaronder een BA van de groeifase stopt; 0 "
       "neemt die van de oplosser"),
    RU("Относительное улучшение стоимости, ниже которого уравнивание фазы роста "
       "останавливается; 0 -- значение решателя"),
    TR("Büyüme evresi dengelemesinin altında durduğu göreli maliyet iyileşmesi, "
       "çözücününki için 0"));

SS_MSG(ba_growth_patience_help,
    EN("Accepted steps below that tolerance before it stops"),
    JA("停止するまでに、その許容差を下回ったまま受け入れるステップ数"),
    ZH_HANS("在停止前，允许有多少个改善低于该容差的步被接受"),
    ZH_HANT("在停止前，允許有多少個改善低於該容差的步被接受"),
    KO("멈추기 전까지 그 허용 오차 아래로 받아들이는 단계 수"),
    DE("Angenommene Schritte unter dieser Toleranz, bevor er aufhört"),
    FR("Pas acceptés sous cette tolérance avant l'arrêt"),
    ES("Pasos aceptados por debajo de esa tolerancia antes de parar"),
    PT("Passos aceitos abaixo dessa tolerância antes de parar"),
    IT("Passi accettati sotto quella tolleranza prima di fermarsi"),
    NL("Aanvaarde stappen onder die tolerantie voordat hij stopt"),
    RU("Сколько принятых шагов ниже этого допуска до остановки"),
    TR("Durmadan önce o toleransın altında kabul edilen adım sayısı"));

SS_MSG(max_models_help,
    EN("Reconstructions a fragmented capture may produce; 1 for a single model"),
    JA("断片化した撮影が生み出してよい再構成の数。1 なら単一モデル"),
    ZH_HANS("零散的拍摄最多可产出多少个重建；1 表示只要一个模型"),
    ZH_HANT("零散的拍攝最多可產出多少個重建；1 表示只要一個模型"),
    KO("조각난 촬영이 만들어도 되는 재구성 수. 1 이면 단일 모델"),
    DE("Rekonstruktionen, die eine zerfallene Aufnahme hervorbringen darf; 1 für "
       "ein einziges Modell"),
    FR("Reconstructions qu'une prise de vue fragmentée peut produire ; 1 pour un "
       "seul modèle"),
    ES("Reconstrucciones que una captura fragmentada puede producir; 1 para un "
       "único modelo"),
    PT("Reconstruções que uma captura fragmentada pode produzir; 1 para um único "
       "modelo"),
    IT("Ricostruzioni che una ripresa frammentata può produrre; 1 per un solo "
       "modello"),
    NL("Reconstructies die een versnipperde opname mag opleveren; 1 voor één "
       "model"),
    RU("Сколько реконструкций может дать раздробленная съёмка; 1 -- одна модель"),
    TR("Parçalanmış bir çekimin üretebileceği yeniden oluşturma sayısı; tek "
       "model için 1"));

SS_MSG(model_overlap_help,
    EN("Images a further model may take from one already kept -- what a merge "
       "later aligns on"),
    JA("後続のモデルが既存のモデルから取ってよい画像数。あとの統合はこれを"
       "手がかりに位置合わせします"),
    ZH_HANS("后续模型可以从已保留的模型那里取用多少张图像 —— 之后的合并正是据此对齐"),
    ZH_HANT("後續模型可以從已保留的模型那裡取用多少張影像 —— 之後的合併正是據此對齊"),
    KO("나중 모델이 이미 남긴 모델에서 가져가도 되는 이미지 수 -- 뒤의 병합이 "
       "이것을 기준으로 맞춥니다"),
    DE("Bilder, die ein weiteres Modell einem bereits behaltenen entnehmen darf "
       "-- daran richtet eine spätere Zusammenführung aus"),
    FR("Images qu'un modèle suivant peut prendre à un modèle déjà gardé -- ce "
       "sur quoi une fusion s'alignera"),
    ES("Imágenes que un modelo posterior puede tomar de uno ya conservado: "
       "aquello sobre lo que una fusión se alineará"),
    PT("Imagens que um modelo posterior pode tomar de um já mantido -- aquilo "
       "sobre o que uma fusão se alinhará"),
    IT("Immagini che un modello successivo può prendere da uno già tenuto -- "
       "ciò su cui una fusione si allineerà"),
    NL("Beelden die een volgend model mag overnemen van een al behouden model -- "
       "waarop een latere samenvoeging uitlijnt"),
    RU("Сколько изображений очередная модель может взять у уже сохранённой -- "
       "по ним потом и выравнивается слияние"),
    TR("Sonraki bir modelin, tutulmuş bir modelden alabileceği görüntü sayısı -- "
       "ileride bir birleştirme bunlara göre hizalanır"));

SS_MSG(model_overlap_ratio_help,
    EN("Further images it earns per image it finds that nothing holds, on top of "
       "--model-overlap; 0 for COLMAP's flat cap (D66)"),
    JA("どのモデルも持っていない画像を 1 枚見つけるごとに追加で得られる画像数。"
       "--model-overlap に上乗せされます。0 なら COLMAP と同じ一定の上限（D66）"),
    ZH_HANS("每找到一张无人占用的图像，就在 --model-overlap 之外额外获得多少张；"
            "0 表示采用 COLMAP 的固定上限（D66）"),
    ZH_HANT("每找到一張無人占用的影像，就在 --model-overlap 之外額外獲得多少張；"
            "0 表示採用 COLMAP 的固定上限（D66）"),
    KO("아무도 갖고 있지 않은 이미지를 하나 찾을 때마다 --model-overlap 에 더해 "
       "얻는 이미지 수. 0 이면 COLMAP 과 같은 고정 상한(D66)"),
    DE("Weitere Bilder, die es je gefundenem Bild verdient, das niemand hält, "
       "zusätzlich zu --model-overlap; 0 nimmt COLMAPs feste Obergrenze (D66)"),
    FR("Images supplémentaires gagnées par image trouvée que personne ne "
       "détient, en plus de --model-overlap ; 0 pour le plafond fixe de COLMAP "
       "(D66)"),
    ES("Imágenes adicionales que gana por cada imagen que encuentra y que nadie "
       "tiene, además de --model-overlap; 0 para el tope fijo de COLMAP (D66)"),
    PT("Imagens adicionais que ganha por cada imagem que encontra e que ninguém "
       "tem, além de --model-overlap; 0 para o limite fixo do COLMAP (D66)"),
    IT("Immagini in più che guadagna per ogni immagine trovata che nessuno "
       "possiede, oltre a --model-overlap; 0 per il tetto fisso di COLMAP (D66)"),
    NL("Extra beelden die het verdient per gevonden beeld dat niemand houdt, "
       "boven op --model-overlap; 0 voor COLMAP's vaste plafond (D66)"),
    RU("Сколько ещё изображений оно получает за каждое найденное, которым никто "
       "не владеет, сверх --model-overlap; 0 -- фиксированный предел COLMAP (D66)"),
    TR("Kimsenin elinde olmayan her bulunan görüntü başına, --model-overlap "
       "üstüne kazandığı ek görüntü sayısı; COLMAP'in sabit sınırı için 0 (D66)"));

SS_MSG(min_model_size_help,
    EN("Images a model must reach to be kept at all"),
    JA("モデルが少しでも残るために達すべき画像数"),
    ZH_HANS("模型至少要有多少张图像才会被保留"),
    ZH_HANT("模型至少要有多少張影像才會被保留"),
    KO("모델이 조금이라도 남으려면 도달해야 하는 이미지 수"),
    DE("Bilder, die ein Modell erreichen muss, um überhaupt zu bleiben"),
    FR("Images qu'un modèle doit atteindre pour être seulement conservé"),
    ES("Imágenes que un modelo debe alcanzar para conservarse siquiera"),
    PT("Imagens que um modelo deve alcançar para sequer ser mantido"),
    IT("Immagini che un modello deve raggiungere anche solo per essere tenuto"),
    NL("Beelden die een model moet halen om überhaupt te blijven"),
    RU("Сколько изображений должна набрать модель, чтобы её вообще сохранили"),
    TR("Bir modelin hiç olmazsa tutulması için ulaşması gereken görüntü sayısı"));

SS_MSG(pnp_ratio_visible_help,
    EN("Measure the PnP inlier ratio over the correspondences the pose could "
       "see, not every one offered (D69)"),
    JA("PnP のインライア比を、提示された全対応ではなく、その姿勢から見えるはずの"
       "対応に対して測ります（D69）"),
    ZH_HANS("按该位姿本可看到的对应来计算 PnP 内点比例，而不是按提供的全部对应（D69）"),
    ZH_HANT("按該位姿本可看到的對應來計算 PnP 內點比例，而不是按提供的全部對應（D69）"),
    KO("PnP 내부점 비율을 제시된 모든 대응이 아니라 그 자세에서 볼 수 있었을 "
       "대응에 대해 잽니다(D69)"),
    DE("Das PnP-Inlier-Verhältnis über die Korrespondenzen messen, die die Pose "
       "sehen konnte, nicht über alle angebotenen (D69)"),
    FR("Mesurer le taux d'inliers PnP sur les correspondances que la pose "
       "pouvait voir, non sur toutes celles proposées (D69)"),
    ES("Medir la proporción de inliers de PnP sobre las correspondencias que la "
       "pose podía ver, no sobre todas las ofrecidas (D69)"),
    PT("Medir a proporção de inliers do PnP sobre as correspondências que a pose "
       "podia ver, não sobre todas as oferecidas (D69)"),
    IT("Misurare il rapporto di inlier PnP sulle corrispondenze che la posa "
       "poteva vedere, non su tutte quelle offerte (D69)"),
    NL("De PnP-inlierverhouding meten over de correspondenties die de pose kon "
       "zien, niet over alle aangeboden (D69)"),
    RU("Считать долю инлайеров PnP по тем соответствиям, которые поза могла "
       "видеть, а не по всем предложенным (D69)"),
    TR("PnP içleyen oranını, sunulan tüm karşılıklar üzerinden değil, duruşun "
       "görebileceği karşılıklar üzerinden ölç (D69)"));

SS_MSG(strong_pnp_inliers_help,
    EN("Agreeing correspondences past which a registration is admitted whatever "
       "its inlier ratio, since the ratio reads a pool the scene decides (D69); "
       "0 off"),
    JA("これを超える対応が一致していれば、インライア比によらず登録を認めます。"
       "比の分母は場面が決めるものだからです（D69）。0 で無効"),
    ZH_HANS("一致的对应超过此数时，无论内点比例如何都接受该次注册，因为比例的分母"
            "由场景决定（D69）；0 表示关闭"),
    ZH_HANT("一致的對應超過此數時，無論內點比例如何都接受該次註冊，因為比例的分母"
            "由場景決定（D69）；0 表示關閉"),
    KO("일치하는 대응이 이 수를 넘으면 내부점 비율과 무관하게 등록을 받아들입니다. "
       "비율의 모집단은 장면이 정하기 때문입니다(D69). 0 이면 끕니다"),
    DE("Übereinstimmende Korrespondenzen, ab denen eine Registrierung ungeachtet "
       "ihres Inlier-Verhältnisses zugelassen wird, da dieses Verhältnis eine "
       "Menge misst, die die Szene bestimmt (D69); 0 aus"),
    FR("Correspondances concordantes au-delà desquelles un enregistrement est "
       "admis quel que soit son taux d'inliers, ce taux portant sur un ensemble "
       "que la scène décide (D69) ; 0 désactive"),
    ES("Correspondencias concordantes por encima de las cuales se admite un "
       "registro sea cual sea su proporción de inliers, pues esa proporción mide "
       "un conjunto que decide la escena (D69); 0 lo desactiva"),
    PT("Correspondências concordantes acima das quais um registro é admitido seja "
       "qual for a sua proporção de inliers, pois essa proporção mede um "
       "conjunto que a cena decide (D69); 0 desativa"),
    IT("Corrispondenze concordi oltre le quali una registrazione è ammessa "
       "qualunque sia il suo rapporto di inlier, dato che quel rapporto misura "
       "un insieme deciso dalla scena (D69); 0 disattiva"),
    NL("Overeenstemmende correspondenties waarboven een registratie wordt "
       "toegelaten ongeacht haar inlierverhouding, omdat die verhouding een "
       "verzameling meet die het tafereel bepaalt (D69); 0 uit"),
    RU("Сколько согласующихся соответствий достаточно, чтобы регистрация была "
       "принята независимо от доли инлайеров: эта доля меряет множество, которое "
       "определяет сама сцена (D69); 0 отключает"),
    TR("Uyuşan karşılıkların, içleyen oranı ne olursa olsun bir kaydın kabul "
       "edildiği sayısı; çünkü bu oran, sahnenin belirlediği bir havuzu ölçer "
       "(D69); 0 kapatır"));

SS_MSG(strong_pnp_max_rival_help,
    EN("... but only if no second pose explains this share of the winner's count "
       "among the correspondences it rejected -- two plausible places is not "
       "evidence"),
    JA("... ただし、勝者が退けた対応の中で、勝者の数のこの割合を説明する第 2 の"
       "姿勢が存在しない場合に限ります。もっともらしい場所が 2 つあるのは証拠に"
       "なりません"),
    ZH_HANS("……但仅当在被胜者排除的对应中，没有第二个位姿能解释胜者数量的这一比例时"
            "才成立 —— 有两个看似合理的位置并不构成证据"),
    ZH_HANT("……但僅當在被勝者排除的對應中，沒有第二個位姿能解釋勝者數量的這一比例時"
            "才成立 —— 有兩個看似合理的位置並不構成證據"),
    KO("... 다만 승자가 물리친 대응 가운데, 승자 수의 이 비율을 설명하는 두 번째 "
       "자세가 없을 때만입니다. 그럴듯한 자리가 둘이라는 것은 증거가 아닙니다"),
    DE("... aber nur, wenn keine zweite Pose diesen Anteil der Zahl des Siegers "
       "unter den von ihm verworfenen Korrespondenzen erklärt -- zwei plausible "
       "Orte sind kein Beleg"),
    FR("... mais seulement si aucune seconde pose n'explique cette part du "
       "décompte du vainqueur parmi les correspondances qu'il a rejetées -- deux "
       "endroits plausibles ne font pas une preuve"),
    ES("... pero solo si ninguna segunda pose explica esta parte del recuento del "
       "ganador entre las correspondencias que este rechazó: dos sitios "
       "plausibles no son prueba"),
    PT("... mas só se nenhuma segunda pose explicar esta parcela da contagem do "
       "vencedor entre as correspondências que ele rejeitou -- dois lugares "
       "plausíveis não são prova"),
    IT("... ma solo se nessuna seconda posa spiega questa quota del conteggio del "
       "vincitore fra le corrispondenze che ha respinto -- due luoghi plausibili "
       "non sono una prova"),
    NL("... maar alleen als geen tweede pose dit aandeel van de telling van de "
       "winnaar verklaart onder de correspondenties die hij afwees -- twee "
       "plausibele plekken zijn geen bewijs"),
    RU("... но только если никакая вторая поза не объясняет эту долю счёта "
       "победителя среди отвергнутых им соответствий -- два правдоподобных места "
       "не доказательство"),
    TR("... ama yalnızca, kazananın reddettiği karşılıklar arasında hiçbir ikinci "
       "duruş kazananın sayısının bu payını açıklamıyorsa -- iki akla yatkın yer "
       "kanıt değildir"));

SS_MSG(audit_evidence_help,
    EN("Correspondences an image needs before the audit will judge its pose"),
    JA("監査が姿勢を判定するために画像が必要とする対応数"),
    ZH_HANS("审计要判定某张图像的位姿，该图像需要多少对应"),
    ZH_HANT("稽核要判定某張影像的位姿，該影像需要多少對應"),
    KO("감사가 자세를 판정하려면 이미지에 필요한 대응 수"),
    DE("Korrespondenzen, die ein Bild braucht, bevor die Prüfung seine Pose "
       "beurteilt"),
    FR("Correspondances qu'une image doit avoir avant que l'audit ne juge sa pose"),
    ES("Correspondencias que una imagen necesita antes de que la auditoría "
       "juzgue su pose"),
    PT("Correspondências de que uma imagem precisa antes de a auditoria julgar a "
       "sua pose"),
    IT("Corrispondenze che un'immagine deve avere prima che l'audit giudichi la "
       "sua posa"),
    NL("Correspondenties die een beeld nodig heeft voordat de audit zijn pose "
       "beoordeelt"),
    RU("Сколько соответствий нужно изображению, прежде чем аудит вынесет "
       "суждение о его позе"),
    TR("Denetimin duruşunu yargılaması için bir görüntünün gereksindiği karşılık "
       "sayısı"));

// ===========================================================================
// manage
// ===========================================================================

SS_MSG(rounds_help,
    EN("Merge levels, run until one changes nothing"),
    JA("統合の段数。何も変わらない段に至るまで繰り返します"),
    ZH_HANS("合并的层数，一直做到某一层不再改变任何东西为止"),
    ZH_HANT("合併的層數，一直做到某一層不再改變任何東西為止"),
    KO("병합 층수. 한 층이 아무것도 바꾸지 못할 때까지 돕니다"),
    DE("Zusammenführungs-Ebenen, bis eine nichts mehr ändert"),
    FR("Niveaux de fusion, jusqu'à ce que l'un ne change plus rien"),
    ES("Niveles de fusión, hasta que uno no cambie nada"),
    PT("Níveis de fusão, até que um não mude mais nada"),
    IT("Livelli di fusione, finché uno non cambia più nulla"),
    NL("Samenvoegniveaus, tot er één niets meer verandert"),
    RU("Уровни слияния -- пока очередной ничего не изменит"),
    TR("Birleştirme düzeyleri; biri hiçbir şeyi değiştirmeyene dek sürer"));

SS_MSG(max_bridges_help,
    EN("Merges a stalled level may attempt on shared structure instead of shared "
       "images (D70); off, because the seam test then judges the evidence that "
       "made them"),
    JA("行き詰まった段が、共有画像ではなく共有構造を手がかりに試みてよい統合の数"
       "（D70）。既定は無効です。その場合はシーム検定が根拠そのものを判定します"),
    ZH_HANS("停滞的一层可以依据共享结构（而非共享图像）尝试多少次合并（D70）；"
            "默认关闭，因为此时接缝检验判定的正是促成合并的那些证据"),
    ZH_HANT("停滯的一層可以依據共享結構（而非共享影像）嘗試多少次合併（D70）；"
            "預設關閉，因為此時接縫檢驗判定的正是促成合併的那些證據"),
    KO("정체된 층이 공유 이미지가 아니라 공유 구조를 근거로 시도해도 되는 병합 수"
       "(D70). 기본은 꺼짐 -- 그럴 때 솔기 검정이 바로 그 근거를 판정하기 때문입니다"),
    DE("Zusammenführungen, die eine stehengebliebene Ebene über gemeinsame "
       "Struktur statt gemeinsamer Bilder versuchen darf (D70); aus, weil der "
       "Nahttest dann eben jenen Beleg beurteilt"),
    FR("Fusions qu'un niveau bloqué peut tenter sur la structure partagée plutôt "
       "que sur les images partagées (D70) ; désactivé, car le test de couture "
       "juge alors la preuve même qui les a produites"),
    ES("Fusiones que un nivel estancado puede intentar sobre la estructura "
       "compartida en vez de sobre las imágenes compartidas (D70); desactivado, "
       "porque entonces la prueba de costura juzga la evidencia misma que las "
       "produjo"),
    PT("Fusões que um nível travado pode tentar sobre a estrutura partilhada em "
       "vez das imagens partilhadas (D70); desligado, porque então o teste de "
       "costura julga a própria evidência que as produziu"),
    IT("Fusioni che un livello in stallo può tentare sulla struttura condivisa "
       "invece che sulle immagini condivise (D70); disattivo, perché il test di "
       "cucitura giudica allora proprio la prova che le ha prodotte"),
    NL("Samenvoegingen die een vastgelopen niveau mag proberen op gedeelde "
       "structuur in plaats van gedeelde beelden (D70); uit, omdat de naadtoets "
       "dan juist het bewijs beoordeelt dat ze voortbracht"),
    RU("Сколько слияний застопорившийся уровень может попробовать по общей "
       "структуре, а не по общим изображениям (D70); выключено, потому что "
       "проверка шва судит тогда именно то свидетельство, которое их и породило"),
    TR("Tıkanmış bir düzeyin, ortak görüntüler yerine ortak yapı üzerinden "
       "deneyebileceği birleştirme sayısı (D70); kapalı, çünkü dikiş sınaması o "
       "durumda tam da onları doğuran kanıtı yargılar"));

SS_MSG(merge_help,
    EN("Merge models that share images, on the Sim(3) those shared poses give "
       "(D43)"),
    JA("画像を共有するモデルを、その共有姿勢が与える Sim(3) で統合します（D43）"),
    ZH_HANS("把共享图像的模型按这些共享位姿给出的 Sim(3) 合并（D43）"),
    ZH_HANT("把共享影像的模型按這些共享位姿給出的 Sim(3) 合併（D43）"),
    KO("이미지를 공유하는 모델을, 그 공유 자세가 주는 Sim(3) 으로 병합합니다(D43)"),
    DE("Modelle, die Bilder teilen, über die Sim(3) zusammenführen, die diese "
       "geteilten Posen ergeben (D43)"),
    FR("Fusionner les modèles qui partagent des images, sur la Sim(3) que "
       "donnent ces poses partagées (D43)"),
    ES("Fusionar los modelos que comparten imágenes, sobre la Sim(3) que dan "
       "esas poses compartidas (D43)"),
    PT("Fundir os modelos que partilham imagens, sobre a Sim(3) que essas poses "
       "partilhadas dão (D43)"),
    IT("Fondere i modelli che condividono immagini, sulla Sim(3) che quelle pose "
       "condivise danno (D43)"),
    NL("Modellen die beelden delen samenvoegen, op de Sim(3) die die gedeelde "
       "poses geven (D43)"),
    RU("Сливать модели, у которых есть общие изображения, по Sim(3), которую "
       "дают эти общие позы (D43)"),
    TR("Görüntü paylaşan modelleri, o paylaşılan duruşların verdiği Sim(3) "
       "üzerinden birleştir (D43)"));

SS_MSG(grow_help,
    EN("Register still-unregistered images into the models that exist"),
    JA("まだ登録されていない画像を、既存のモデルに登録します"),
    ZH_HANS("把仍未注册的图像注册进已有的模型"),
    ZH_HANT("把仍未註冊的影像註冊進已有的模型"),
    KO("아직 등록되지 않은 이미지를 이미 있는 모델에 등록합니다"),
    DE("Noch nicht registrierte Bilder in die vorhandenen Modelle aufnehmen"),
    FR("Enregistrer les images encore non enregistrées dans les modèles existants"),
    ES("Registrar en los modelos existentes las imágenes aún sin registrar"),
    PT("Registrar nos modelos existentes as imagens ainda não registradas"),
    IT("Registrare nei modelli esistenti le immagini ancora non registrate"),
    NL("Nog niet geregistreerde beelden in de bestaande modellen registreren"),
    RU("Регистрировать ещё не зарегистрированные изображения в существующие "
       "модели"),
    TR("Henüz kaydedilmemiş görüntüleri var olan modellere kaydet"));

SS_MSG(reseed_help,
    EN("Look for further models among the images nothing covers"),
    JA("どのモデルも覆っていない画像の中から、さらにモデルを探します"),
    ZH_HANS("在无人覆盖的图像中继续寻找新的模型"),
    ZH_HANT("在無人涵蓋的影像中繼續尋找新的模型"),
    KO("아무 모델도 덮지 않은 이미지들 가운데 또 다른 모델을 찾습니다"),
    DE("Unter den Bildern, die nichts abdeckt, nach weiteren Modellen suchen"),
    FR("Chercher d'autres modèles parmi les images que rien ne couvre"),
    ES("Buscar más modelos entre las imágenes que nada cubre"),
    PT("Procurar mais modelos entre as imagens que nada cobre"),
    IT("Cercare altri modelli tra le immagini che nulla copre"),
    NL("Zoeken naar meer modellen onder de beelden die niets dekt"),
    RU("Искать новые модели среди изображений, которых ничто не покрывает"),
    TR("Hiçbir şeyin kapsamadığı görüntüler arasında başka modeller ara"));

SS_MSG(audit_help,
    EN("Check each pose against the correspondence graph and re-register what a "
       "model cannot support (D44)"),
    JA("各姿勢を対応グラフに照らして確かめ、モデルが支持できないものを登録し直します"
       "（D44）"),
    ZH_HANS("对照对应关系图检查每个位姿，并把模型无法支撑的重新注册（D44）"),
    ZH_HANT("對照對應關係圖檢查每個位姿，並把模型無法支撐的重新註冊（D44）"),
    KO("각 자세를 대응 그래프에 비추어 확인하고, 모델이 뒷받침하지 못하는 것을 "
       "다시 등록합니다(D44)"),
    DE("Jede Pose gegen den Korrespondenzgraphen prüfen und neu registrieren, "
       "was ein Modell nicht tragen kann (D44)"),
    FR("Vérifier chaque pose contre le graphe de correspondances et réenregistrer "
       "ce qu'un modèle ne peut soutenir (D44)"),
    ES("Comprobar cada pose contra el grafo de correspondencias y volver a "
       "registrar lo que un modelo no puede sostener (D44)"),
    PT("Conferir cada pose contra o grafo de correspondências e registrar de novo "
       "o que um modelo não consegue sustentar (D44)"),
    IT("Controllare ogni posa rispetto al grafo delle corrispondenze e "
       "registrare di nuovo ciò che un modello non regge (D44)"),
    NL("Elke pose toetsen aan de correspondentiegraaf en opnieuw registreren wat "
       "een model niet kan dragen (D44)"),
    RU("Сверять каждую позу с графом соответствий и перерегистрировать то, что "
       "модель не может подтвердить (D44)"),
    TR("Her duruşu karşılık çizgesine göre denetle ve bir modelin "
       "destekleyemediğini yeniden kaydet (D44)"));

SS_MSG(split_help,
    EN("Break a model its own verified pairs contradict"),
    JA("自身の検証済みペアと矛盾するモデルを分割します"),
    ZH_HANS("拆开与自身已验证像对相矛盾的模型"),
    ZH_HANT("拆開與自身已驗證影像對相矛盾的模型"),
    KO("자기 검증된 쌍과 모순되는 모델을 쪼갭니다"),
    DE("Ein Modell zerlegen, dem seine eigenen geprüften Paare widersprechen"),
    FR("Casser un modèle que ses propres paires vérifiées contredisent"),
    ES("Partir un modelo al que sus propios pares verificados contradicen"),
    PT("Quebrar um modelo que os seus próprios pares verificados contradizem"),
    IT("Spezzare un modello che le sue stesse coppie verificate contraddicono"),
    NL("Een model breken dat zijn eigen geverifieerde paren tegenspreken"),
    RU("Разбивать модель, которой противоречат её же проверенные пары"),
    TR("Kendi doğrulanmış çiftlerinin çeliştiği bir modeli parçala"));

SS_MSG(fold_split_help,
    EN("Cut a model that has written two places on top of each other, when the "
       "cut severs almost no co-visibility (D46)"),
    JA("2 つの場所を重ねて書いてしまったモデルを、切断がほとんど共視性を断たない"
       "ときに切り分けます（D46）"),
    ZH_HANS("当切开几乎不切断共视关系时，把两处叠写在一起的模型切开（D46）"),
    ZH_HANT("當切開幾乎不切斷共視關係時，把兩處疊寫在一起的模型切開（D46）"),
    KO("두 곳을 겹쳐 써 버린 모델을, 자르더라도 공동 가시성이 거의 끊기지 않을 때 "
       "잘라 냅니다(D46)"),
    DE("Ein Modell zerschneiden, das zwei Orte übereinandergeschrieben hat, wenn "
       "der Schnitt fast keine Ko-Sichtbarkeit durchtrennt (D46)"),
    FR("Couper un modèle qui a écrit deux endroits l'un sur l'autre, quand la "
       "coupe ne rompt presque aucune covisibilité (D46)"),
    ES("Cortar un modelo que ha escrito dos sitios uno encima del otro, cuando "
       "el corte no rompe casi ninguna covisibilidad (D46)"),
    PT("Cortar um modelo que escreveu dois lugares um sobre o outro, quando o "
       "corte não rompe quase nenhuma covisibilidade (D46)"),
    IT("Tagliare un modello che ha scritto due luoghi uno sull'altro, quando il "
       "taglio non recide quasi nessuna co-visibilità (D46)"),
    NL("Een model doorsnijden dat twee plekken over elkaar heen heeft "
       "geschreven, wanneer de snede bijna geen co-zichtbaarheid doorbreekt (D46)"),
    RU("Разрезать модель, записавшую два места одно поверх другого, когда разрез "
       "почти не рвёт совидимость (D46)"),
    TR("İki yeri üst üste yazmış bir modeli, kesim neredeyse hiç ortak "
       "görülebilirliği koparmıyorsa kes (D46)"));

SS_MSG(fold_max_cut_help,
    EN("Co-visibility that cut may sever, as a fraction of the model's total"),
    JA("その切断が断ってよい共視性の量。モデル全体に対する割合"),
    ZH_HANS("该切割可切断的共视关系量，按模型总量的比例给出"),
    ZH_HANT("該切割可切斷的共視關係量，按模型總量的比例給出"),
    KO("그 절단이 끊어도 되는 공동 가시성의 양. 모델 전체에 대한 비율"),
    DE("Ko-Sichtbarkeit, die dieser Schnitt durchtrennen darf, als Anteil am "
       "Gesamten des Modells"),
    FR("Covisibilité que cette coupe peut rompre, en fraction du total du modèle"),
    ES("Covisibilidad que ese corte puede romper, como fracción del total del "
       "modelo"),
    PT("Covisibilidade que esse corte pode romper, como fração do total do "
       "modelo"),
    IT("Co-visibilità che quel taglio può recidere, come frazione del totale del "
       "modello"),
    NL("Co-zichtbaarheid die die snede mag doorbreken, als deel van het totaal "
       "van het model"),
    RU("Сколько совидимости этот разрез вправе порвать -- как доля от общей по "
       "модели"),
    TR("O kesimin koparabileceği ortak görülebilirlik, modelin toplamının oranı "
       "olarak"));

SS_MSG(fold_min_overlap_help,
    EN("Images of a cut-off piece that must stand where an image outside it "
       "stands, or it is put back rather than called a duplicate (D67); 0 "
       "disables"),
    JA("切り離した断片の画像のうち、その外側の画像と同じ場所に立っていなければ"
       "ならない枚数。満たさなければ重複とはせず元に戻します（D67）。0 で無効"),
    ZH_HANS("被切下的那块中，必须与块外某张图像站在同一处的图像数；不满足就放回去，"
            "而不是判为重复（D67）；0 表示关闭"),
    ZH_HANT("被切下的那塊中，必須與塊外某張影像站在同一處的影像數；不滿足就放回去，"
            "而不是判為重複（D67）；0 表示關閉"),
    KO("잘라 낸 조각의 이미지 가운데, 조각 밖의 이미지와 같은 자리에 서 있어야 하는 "
       "수. 못 채우면 중복이라 부르지 않고 되돌립니다(D67). 0 이면 끕니다"),
    DE("Bilder eines abgetrennten Stücks, die dort stehen müssen, wo ein Bild "
       "außerhalb steht; sonst wird es zurückgelegt statt als Doppelung "
       "bezeichnet (D67); 0 schaltet ab"),
    FR("Images d'un morceau détaché qui doivent se tenir là où se tient une "
       "image extérieure, sans quoi il est remis en place plutôt qu'appelé "
       "doublon (D67) ; 0 désactive"),
    ES("Imágenes de un trozo separado que deben situarse donde se sitúa una "
       "imagen de fuera; si no, se vuelve a poner en su sitio en lugar de "
       "llamarlo duplicado (D67); 0 lo desactiva"),
    PT("Imagens de um pedaço separado que devem ficar onde fica uma imagem de "
       "fora; caso contrário ele é reposto em vez de ser chamado de duplicata "
       "(D67); 0 desativa"),
    IT("Immagini di un pezzo staccato che devono stare dove sta un'immagine "
       "esterna, altrimenti viene rimesso a posto invece che chiamato duplicato "
       "(D67); 0 disattiva"),
    NL("Beelden van een afgesneden stuk die moeten staan waar een beeld erbuiten "
       "staat, anders wordt het teruggelegd in plaats van dubbel genoemd (D67); "
       "0 zet het uit"),
    RU("Сколько изображений отрезанного куска должны стоять там же, где стоит "
       "изображение вне его, иначе кусок возвращают на место, а не объявляют "
       "дубликатом (D67); 0 отключает"),
    TR("Kesilip ayrılan bir parçanın, dışındaki bir görüntünün durduğu yerde "
       "durması gereken görüntü sayısı; olmazsa yinelenmiş denmek yerine geri "
       "konur (D67); 0 kapatır"));

SS_MSG(joint_ba_help,
    EN("Refine every component in one problem with intrinsics shared per camera "
       "group (D45)"),
    JA("すべての成分を 1 つの問題として精密化し、内部パラメータはカメラグループ"
       "ごとに共有します（D45）"),
    ZH_HANS("把所有连通块放进一个问题里精化，内参按相机组共享（D45）"),
    ZH_HANT("把所有連通塊放進一個問題裡精化，內參按相機群組共享（D45）"),
    KO("모든 성분을 한 문제로 묶어 정련하며, 내부 파라미터는 카메라 그룹별로 "
       "공유합니다(D45)"),
    DE("Jede Komponente in einem Problem verfeinern, mit je Kameragruppe "
       "geteilten Intrinsics (D45)"),
    FR("Affiner toutes les composantes dans un même problème, paramètres "
       "internes partagés par groupe de caméras (D45)"),
    ES("Refinar todas las componentes en un mismo problema, con los parámetros "
       "internos compartidos por grupo de cámaras (D45)"),
    PT("Refinar todos os componentes num mesmo problema, com os parâmetros "
       "internos partilhados por grupo de câmeras (D45)"),
    IT("Affinare tutte le componenti in un unico problema, con i parametri "
       "interni condivisi per gruppo di camere (D45)"),
    NL("Alle componenten in één probleem verfijnen, met de intrinsieken per "
       "cameragroep gedeeld (D45)"),
    RU("Уточнять все компоненты в одной задаче с общими для каждой группы камер "
       "внутренними параметрами (D45)"),
    TR("Tüm bileşenleri tek bir problemde iyileştir, iç parametreler kamera "
       "grubu başına paylaşılsın (D45)"));

SS_MSG(seam_min_agreement_help,
    EN("Verified pairs crossing a merge seam that must still hold in the merged "
       "model; 0 disables"),
    JA("統合のシームをまたぐ検証済みペアのうち、統合後のモデルでも成立していなければ"
       "ならない割合。0 で無効"),
    ZH_HANS("跨越合并接缝的已验证像对中，在合并后的模型里仍须成立的比例；0 表示关闭"),
    ZH_HANT("跨越合併接縫的已驗證影像對中，在合併後的模型裡仍須成立的比例；0 表示關閉"),
    KO("병합 솔기를 가로지르는 검증된 쌍 가운데, 병합된 모델에서도 성립해야 하는 "
       "비율. 0 이면 끕니다"),
    DE("Geprüfte Paare über eine Zusammenführungsnaht, die im vereinten Modell "
       "weiter gelten müssen; 0 schaltet ab"),
    FR("Paires vérifiées traversant une couture de fusion qui doivent encore "
       "tenir dans le modèle fusionné ; 0 désactive"),
    ES("Pares verificados que cruzan una costura de fusión y deben seguir "
       "cumpliéndose en el modelo fusionado; 0 lo desactiva"),
    PT("Pares verificados que cruzam uma costura de fusão e devem continuar "
       "válidos no modelo fundido; 0 desativa"),
    IT("Coppie verificate che attraversano una cucitura di fusione e devono "
       "reggere ancora nel modello fuso; 0 disattiva"),
    NL("Geverifieerde paren die een samenvoegnaad kruisen en in het samengevoegde "
       "model moeten blijven kloppen; 0 zet het uit"),
    RU("Какая часть проверенных пар, пересекающих шов слияния, должна выполняться "
       "и в слитой модели; 0 отключает"),
    TR("Bir birleştirme dikişini geçen ve birleşmiş modelde de geçerli kalması "
       "gereken doğrulanmış çiftlerin oranı; 0 kapatır"));

SS_MSG(seam_relative_bar_help,
    EN("Judge a seam against what the model's own non-crossing pairs explain, "
       "times this; it only ever loosens --seam-min-pair-fraction (D68). 0 uses "
       "that flat"),
    JA("シームを、そのモデル自身の交差しないペアが説明できる量にこの倍率を掛けた"
       "値で判定します。--seam-min-pair-fraction を緩める方向にしか働きません"
       "（D68）。0 ならその値をそのまま使います"),
    ZH_HANS("以该模型自身不跨接缝的像对所能解释的量乘以此系数来判定接缝；"
            "它只会放宽 --seam-min-pair-fraction（D68）。0 表示直接用那个固定值"),
    ZH_HANT("以該模型自身不跨接縫的影像對所能解釋的量乘以此係數來判定接縫；"
            "它只會放寬 --seam-min-pair-fraction（D68）。0 表示直接用那個固定值"),
    KO("솔기를, 그 모델 자신의 교차하지 않는 쌍이 설명하는 양에 이 배수를 곱해 "
       "판정합니다. --seam-min-pair-fraction 을 느슨하게만 만듭니다(D68). 0 이면 "
       "그 값을 그대로 씁니다"),
    DE("Eine Naht daran messen, was die nicht kreuzenden Paare des Modells "
       "erklären, mal diesem Faktor; es lockert --seam-min-pair-fraction nur "
       "(D68). 0 nimmt jenen Wert unverändert"),
    FR("Juger une couture d'après ce qu'expliquent les paires non traversantes du "
       "modèle, multiplié par ceci ; cela ne fait que relâcher "
       "--seam-min-pair-fraction (D68). 0 prend cette valeur telle quelle"),
    ES("Juzgar una costura frente a lo que explican los pares del propio modelo "
       "que no la cruzan, multiplicado por esto; solo llega a relajar "
       "--seam-min-pair-fraction (D68). 0 usa ese valor tal cual"),
    PT("Julgar uma costura pelo que os pares do próprio modelo que não a cruzam "
       "explicam, multiplicado por isto; só chega a afrouxar "
       "--seam-min-pair-fraction (D68). 0 usa esse valor tal como está"),
    IT("Giudicare una cucitura rispetto a ciò che spiegano le coppie del modello "
       "che non la attraversano, per questo fattore; può solo allentare "
       "--seam-min-pair-fraction (D68). 0 usa quel valore così com'è"),
    NL("Een naad beoordelen aan wat de niet-kruisende paren van het model zelf "
       "verklaren, maal dit; het versoepelt --seam-min-pair-fraction alleen "
       "(D68). 0 neemt die waarde onveranderd"),
    RU("Судить шов по тому, что объясняют непересекающие его пары самой модели, "
       "умноженному на это; оно способно лишь ослабить "
       "--seam-min-pair-fraction (D68). 0 берёт то значение как есть"),
    TR("Bir dikişi, modelin kendi kesişmeyen çiftlerinin açıkladığıyla bu "
       "çarpanın çarpımına göre yargıla; yalnızca --seam-min-pair-fraction'ı "
       "gevşetir (D68). 0, o değeri olduğu gibi kullanır"));

SS_MSG(seam_min_pairs_help,
    EN("Cross-seam pairs below which the seam test has nothing to judge on"),
    JA("これを下回るとシーム検定が判定材料を持てない、シームをまたぐペアの数"),
    ZH_HANS("跨接缝的像对少于此数时，接缝检验便无从判定"),
    ZH_HANT("跨接縫的影像對少於此數時，接縫檢驗便無從判定"),
    KO("솔기를 가로지르는 쌍이 이보다 적으면 솔기 검정이 판단할 근거가 없습니다"),
    DE("Nahtquerende Paare, unter denen der Nahttest nichts zu beurteilen hat"),
    FR("Paires traversant la couture en dessous desquelles le test n'a rien à "
       "juger"),
    ES("Pares que cruzan la costura por debajo de los cuales la prueba no tiene "
       "sobre qué juzgar"),
    PT("Pares que cruzam a costura abaixo dos quais o teste não tem sobre o que "
       "julgar"),
    IT("Coppie che attraversano la cucitura sotto le quali il test non ha su cosa "
       "giudicare"),
    NL("Naadkruisende paren waaronder de naadtoets niets heeft om over te "
       "oordelen"),
    RU("Число пересекающих шов пар, ниже которого проверке шва не по чему судить"),
    TR("Altında dikiş sınamasının yargılayacak bir şeyi kalmadığı, dikişi geçen "
       "çift sayısı"));

SS_MSG(seam_rescue_help,
    EN("Median cross-seam pair a refused merge must still explain to earn one "
       "refinement and a second verdict; 0 refuses outright"),
    JA("拒否された統合が、1 回の精密化と再判定を得るために説明していなければ"
       "ならないシーム越しペアの中央値。0 なら即座に拒否します"),
    ZH_HANS("被拒绝的合并要争取一次精化和第二次判定，其跨接缝像对的中位数须达到的"
            "解释程度；0 表示直接拒绝"),
    ZH_HANT("被拒絕的合併要爭取一次精化和第二次判定，其跨接縫影像對的中位數須達到的"
            "解釋程度；0 表示直接拒絕"),
    KO("거절된 병합이 한 번의 정련과 재판정을 얻으려면 설명해야 하는 솔기 횡단 쌍의 "
       "중앙값. 0 이면 곧바로 거절합니다"),
    DE("Median-Nahtpaar, das eine abgelehnte Zusammenführung noch erklären muss, "
       "um eine Verfeinerung und ein zweites Urteil zu verdienen; 0 lehnt sofort "
       "ab"),
    FR("Paire médiane traversant la couture qu'une fusion refusée doit encore "
       "expliquer pour mériter un affinement et un second verdict ; 0 refuse "
       "d'emblée"),
    ES("Par mediano que cruza la costura y que una fusión rechazada aún debe "
       "explicar para ganarse un refinamiento y un segundo veredicto; 0 rechaza "
       "sin más"),
    PT("Par mediano que cruza a costura e que uma fusão recusada ainda deve "
       "explicar para ganhar um refinamento e um segundo veredito; 0 recusa de "
       "imediato"),
    IT("Coppia mediana che attraversa la cucitura che una fusione rifiutata deve "
       "ancora spiegare per meritare un affinamento e un secondo verdetto; 0 "
       "rifiuta subito"),
    NL("Mediaan naadkruisend paar dat een geweigerde samenvoeging nog moet "
       "verklaren om één verfijning en een tweede oordeel te verdienen; 0 "
       "weigert meteen"),
    RU("Медианная пересекающая шов пара, которую отклонённое слияние всё же "
       "должно объяснить, чтобы заслужить одно уточнение и повторный вердикт; "
       "0 отказывает сразу"),
    TR("Reddedilen bir birleştirmenin bir iyileştirme ve ikinci bir karar hak "
       "etmesi için hâlâ açıklaması gereken ortanca dikiş-aşırı çift; 0 doğrudan "
       "reddeder"));

SS_MSG(seam_max_rescues_help,
    EN("Refinements the seam rescue may spend in one merge pass"),
    JA("1 回の統合パスでシーム救済が費やしてよい精密化の回数"),
    ZH_HANS("一次合并阶段中，接缝挽救最多可动用多少次精化"),
    ZH_HANT("一次合併階段中，接縫挽救最多可動用多少次精化"),
    KO("병합 한 차례에서 솔기 구제가 쓸 수 있는 정련 횟수"),
    DE("Verfeinerungen, die die Nahtrettung in einem Zusammenführungsdurchgang "
       "aufwenden darf"),
    FR("Affinements que le sauvetage de couture peut dépenser en une passe de "
       "fusion"),
    ES("Refinamientos que el rescate de costura puede gastar en una pasada de "
       "fusión"),
    PT("Refinamentos que o resgate de costura pode gastar numa passagem de fusão"),
    IT("Affinamenti che il salvataggio della cucitura può spendere in un "
       "passaggio di fusione"),
    NL("Verfijningen die de naadredding in één samenvoeggang mag besteden"),
    RU("Сколько уточнений спасение шва может потратить за один проход слияния"),
    TR("Dikiş kurtarmanın tek bir birleştirme geçişinde harcayabileceği "
       "iyileştirme sayısı"));

// ===========================================================================
// merge
// ===========================================================================

SS_MSG(merge_align_max_error_help,
    EN("Alignment inlier threshold in pixels; looser than the mapper's, as the "
       "two models were optimized independently"),
    JA("位置合わせのインライアしきい値（ピクセル）。2 つのモデルは別々に最適化"
       "されているため、マッパーのものより緩めです"),
    ZH_HANS("对齐时的内点阈值，单位像素；因为两个模型是各自优化的，故比建图阶段更宽松"),
    ZH_HANT("對齊時的內點閾值，單位像素；因為兩個模型是各自最佳化的，故比建圖階段更寬鬆"),
    KO("정렬 내부점 임계값(픽셀). 두 모델이 따로 최적화되었으므로 매퍼의 것보다 "
       "느슨합니다"),
    DE("Inlier-Schwelle der Ausrichtung in Pixeln; lockerer als die des "
       "Kartierers, da die beiden Modelle unabhängig optimiert wurden"),
    FR("Seuil d'inlier de l'alignement en pixels ; plus lâche que celui du "
       "cartographe, les deux modèles ayant été optimisés séparément"),
    ES("Umbral de inlier del alineamiento en píxeles; más laxo que el del "
       "cartógrafo, pues los dos modelos se optimizaron por separado"),
    PT("Limiar de inlier do alinhamento em pixels; mais frouxo que o do "
       "cartógrafo, pois os dois modelos foram otimizados em separado"),
    IT("Soglia di inlier dell'allineamento in pixel; più larga di quella del "
       "cartografo, dato che i due modelli sono stati ottimizzati a parte"),
    NL("Inlier-drempel van de uitlijning in pixels; losser dan die van de "
       "kaartmaker, omdat de twee modellen apart geoptimaliseerd zijn"),
    RU("Порог инлайера при выравнивании, в пикселях; свободнее, чем у "
       "построителя, ведь обе модели оптимизировались независимо"),
    TR("Hizalama içleyen eşiği, piksel cinsinden; iki model ayrı ayrı optimize "
       "edildiği için haritalayıcınınkinden daha gevşektir"));

SS_MSG(merge_max_error_help,
    EN("Alignment inlier threshold in pixels for the merge step"),
    JA("統合段階での位置合わせのインライアしきい値（ピクセル）"),
    ZH_HANS("合并步骤中对齐的内点阈值，单位像素"),
    ZH_HANT("合併步驟中對齊的內點閾值，單位像素"),
    KO("병합 단계에서 정렬 내부점 임계값(픽셀)"),
    DE("Inlier-Schwelle der Ausrichtung in Pixeln für den Zusammenführungsschritt"),
    FR("Seuil d'inlier de l'alignement en pixels pour l'étape de fusion"),
    ES("Umbral de inlier del alineamiento en píxeles para el paso de fusión"),
    PT("Limiar de inlier do alinhamento em pixels para a etapa de fusão"),
    IT("Soglia di inlier dell'allineamento in pixel per il passo di fusione"),
    NL("Inlier-drempel van de uitlijning in pixels voor de samenvoegstap"),
    RU("Порог инлайера при выравнивании, в пикселях, для шага слияния"),
    TR("Birleştirme adımı için hizalama içleyen eşiği, piksel cinsinden"));

SS_MSG(min_common_help,
    EN("Shared images an alignment needs; two determine a Sim(3), three give it a "
       "vote"),
    JA("位置合わせに必要な共有画像数。2 枚で Sim(3) が定まり、3 枚あれば多数決が"
       "効きます"),
    ZH_HANS("对齐所需的共享图像数；两张可定出 Sim(3)，三张才有表决余地"),
    ZH_HANT("對齊所需的共享影像數；兩張可定出 Sim(3)，三張才有表決餘地"),
    KO("정렬에 필요한 공유 이미지 수. 둘이면 Sim(3) 이 정해지고, 셋이면 다수결이 "
       "생깁니다"),
    DE("Gemeinsame Bilder, die eine Ausrichtung braucht; zwei legen eine Sim(3) "
       "fest, drei geben ihr eine Stimme"),
    FR("Images partagées nécessaires à un alignement ; deux déterminent une "
       "Sim(3), trois lui donnent une voix"),
    ES("Imágenes compartidas que un alineamiento necesita; dos determinan una "
       "Sim(3), tres le dan un voto"),
    PT("Imagens partilhadas de que um alinhamento precisa; duas determinam uma "
       "Sim(3), três dão-lhe um voto"),
    IT("Immagini condivise che un allineamento richiede; due determinano una "
       "Sim(3), tre le danno un voto"),
    NL("Gedeelde beelden die een uitlijning nodig heeft; twee bepalen een Sim(3), "
       "drie geven haar een stem"),
    RU("Сколько общих изображений нужно выравниванию; два задают Sim(3), три "
       "дают голосование"),
    TR("Bir hizalamanın gereksindiği ortak görüntü sayısı; iki tanesi bir Sim(3) "
       "belirler, üç tanesi ona bir oy verir"));

SS_MSG(merge_min_common_help,
    EN("Shared images an alignment needs in the merge step"),
    JA("統合段階で位置合わせに必要な共有画像数"),
    ZH_HANS("合并步骤中对齐所需的共享图像数"),
    ZH_HANT("合併步驟中對齊所需的共享影像數"),
    KO("병합 단계에서 정렬에 필요한 공유 이미지 수"),
    DE("Gemeinsame Bilder, die eine Ausrichtung im Zusammenführungsschritt "
       "braucht"),
    FR("Images partagées nécessaires à un alignement lors de l'étape de fusion"),
    ES("Imágenes compartidas que un alineamiento necesita en el paso de fusión"),
    PT("Imagens partilhadas de que um alinhamento precisa na etapa de fusão"),
    IT("Immagini condivise che un allineamento richiede nel passo di fusione"),
    NL("Gedeelde beelden die een uitlijning in de samenvoegstap nodig heeft"),
    RU("Сколько общих изображений нужно выравниванию на шаге слияния"),
    TR("Birleştirme adımında bir hizalamanın gereksindiği ortak görüntü sayısı"));

SS_MSG(merge_arbitrate_help,
    EN("Shared images whose poses agree, past which a disagreement about the "
       "shape of what two models share is arbitrated by the cross-seam test "
       "instead of refusing the merge; 0 always refuses"),
    JA("姿勢が一致する共有画像の数。これを超えると、2 つのモデルが共有する形状に"
       "ついての不一致は統合を拒否する代わりにシーム越し検定で裁定されます。"
       "0 なら常に拒否します"),
    ZH_HANS("位姿一致的共享图像数；超过此数时，两个模型对共享部分形状的分歧改由"
            "跨接缝检验裁决，而不是直接拒绝合并；0 表示一律拒绝"),
    ZH_HANT("位姿一致的共享影像數；超過此數時，兩個模型對共享部分形狀的分歧改由"
            "跨接縫檢驗裁決，而不是直接拒絕合併；0 表示一律拒絕"),
    KO("자세가 일치하는 공유 이미지 수. 이를 넘으면 두 모델이 공유하는 형상에 대한 "
       "불일치를 병합 거절 대신 솔기 횡단 검정이 가립니다. 0 이면 항상 거절합니다"),
    DE("Gemeinsame Bilder mit übereinstimmenden Posen, ab denen ein Streit über "
       "die Form des Gemeinsamen vom Nahttest entschieden wird, statt die "
       "Zusammenführung abzulehnen; 0 lehnt immer ab"),
    FR("Images partagées dont les poses concordent, au-delà desquelles un "
       "désaccord sur la forme de ce que deux modèles partagent est arbitré par "
       "le test de couture au lieu de refuser la fusion ; 0 refuse toujours"),
    ES("Imágenes compartidas cuyas poses concuerdan, por encima de las cuales un "
       "desacuerdo sobre la forma de lo compartido lo arbitra la prueba de "
       "costura en vez de rechazar la fusión; 0 rechaza siempre"),
    PT("Imagens partilhadas cujas poses concordam, acima das quais um desacordo "
       "sobre a forma do que dois modelos partilham é arbitrado pelo teste de "
       "costura em vez de recusar a fusão; 0 recusa sempre"),
    IT("Immagini condivise le cui pose concordano, oltre le quali un disaccordo "
       "sulla forma di ciò che due modelli condividono è arbitrato dal test di "
       "cucitura invece di rifiutare la fusione; 0 rifiuta sempre"),
    NL("Gedeelde beelden waarvan de poses overeenkomen, waarboven onenigheid over "
       "de vorm van het gedeelde door de naadtoets beslecht wordt in plaats van "
       "de samenvoeging te weigeren; 0 weigert altijd"),
    RU("Сколько общих изображений с согласованными позами достаточно, чтобы спор "
       "о форме общего решался проверкой шва, а не отказом от слияния; 0 всегда "
       "отказывает"),
    TR("Duruşları uyuşan ortak görüntü sayısı; bunun üstünde, iki modelin "
       "paylaştığı şeyin biçimine dair anlaşmazlık birleştirmeyi reddetmek "
       "yerine dikiş-aşırı sınamayla karara bağlanır; 0 her zaman reddeder"));

SS_MSG(min_inlier_ratio_help,
    EN("Fraction of the shared images an alignment must explain"),
    JA("位置合わせが説明できなければならない共有画像の割合"),
    ZH_HANS("对齐必须解释掉的共享图像比例"),
    ZH_HANT("對齊必須解釋掉的共享影像比例"),
    KO("정렬이 설명해야 하는 공유 이미지의 비율"),
    DE("Anteil der gemeinsamen Bilder, den eine Ausrichtung erklären muss"),
    FR("Fraction des images partagées qu'un alignement doit expliquer"),
    ES("Fracción de las imágenes compartidas que un alineamiento debe explicar"),
    PT("Fração das imagens partilhadas que um alinhamento deve explicar"),
    IT("Frazione delle immagini condivise che un allineamento deve spiegare"),
    NL("Deel van de gedeelde beelden dat een uitlijning moet verklaren"),
    RU("Какую долю общих изображений должно объяснить выравнивание"),
    TR("Bir hizalamanın açıklaması gereken ortak görüntü oranı"));

SS_MSG(filter_error_help,
    EN("Post-merge observation filtering, in pixels"),
    JA("統合後の観測フィルタリング（ピクセル）"),
    ZH_HANS("合并后的观测过滤阈值，单位像素"),
    ZH_HANT("合併後的觀測過濾閾值，單位像素"),
    KO("병합 후 관측 걸러내기(픽셀)"),
    DE("Beobachtungsfilterung nach dem Zusammenführen, in Pixeln"),
    FR("Filtrage des observations après fusion, en pixels"),
    ES("Filtrado de observaciones tras la fusión, en píxeles"),
    PT("Filtragem de observações após a fusão, em pixels"),
    IT("Filtraggio delle osservazioni dopo la fusione, in pixel"),
    NL("Waarnemingsfiltering na het samenvoegen, in pixels"),
    RU("Отсев наблюдений после слияния, в пикселях"),
    TR("Birleştirme sonrası gözlem eleme, piksel cinsinden"));

SS_MSG(merge_min_tri_angle_help,
    EN("Post-merge triangulation-angle filtering"),
    JA("統合後の三角測量角によるフィルタリング"),
    ZH_HANS("合并后按三角化角度过滤"),
    ZH_HANT("合併後按三角化角度過濾"),
    KO("병합 후 삼각측량 각으로 걸러내기"),
    DE("Filterung nach Triangulationswinkel nach dem Zusammenführen"),
    FR("Filtrage par angle de triangulation après fusion"),
    ES("Filtrado por ángulo de triangulación tras la fusión"),
    PT("Filtragem por ângulo de triangulação após a fusão"),
    IT("Filtraggio per angolo di triangolazione dopo la fusione"),
    NL("Filtering op triangulatiehoek na het samenvoegen"),
    RU("Отсев по углу триангуляции после слияния"),
    TR("Birleştirme sonrası üçgenleme açısına göre eleme"));

SS_MSG(ba_help,
    EN("Bundle-adjust across the seams a merge created; the seam is the one part "
       "no BA has seen"),
    JA("統合が作ったシームをまたいでバンドル調整します。シームこそ、どの調整も"
       "見ていない唯一の場所です"),
    ZH_HANS("跨越合并所产生的接缝做一次平差；接缝正是此前任何平差都没有见过的那部分"),
    ZH_HANT("跨越合併所產生的接縫做一次平差；接縫正是此前任何平差都沒有見過的那部分"),
    KO("병합이 만든 솔기를 가로질러 번들 조정을 합니다. 솔기야말로 어떤 조정도 보지 "
       "못한 유일한 부분입니다"),
    DE("Über die Nähte ausgleichen, die eine Zusammenführung geschaffen hat; die "
       "Naht ist der eine Teil, den kein Ausgleich gesehen hat"),
    FR("Ajuster de part et d'autre des coutures créées par une fusion ; la "
       "couture est la seule partie qu'aucun ajustement n'a vue"),
    ES("Ajustar a través de las costuras que creó una fusión; la costura es la "
       "única parte que ningún ajuste ha visto"),
    PT("Ajustar através das costuras que uma fusão criou; a costura é a única "
       "parte que nenhum ajuste viu"),
    IT("Aggiustare attraverso le cuciture create da una fusione; la cucitura è "
       "l'unica parte che nessun aggiustamento ha visto"),
    NL("Aanpassen over de naden die een samenvoeging maakte; de naad is het ene "
       "deel dat geen enkele aanpassing heeft gezien"),
    RU("Уравнивать через швы, созданные слиянием; шов -- единственная часть, "
       "которую не видело ни одно уравнивание"),
    TR("Bir birleştirmenin oluşturduğu dikişler boyunca dengele; dikiş, hiçbir "
       "dengelemenin görmediği tek yerdir"));

SS_MSG(in_place_help,
    EN("Write the merged models back over the input directory"),
    JA("統合したモデルを入力ディレクトリに上書きします"),
    ZH_HANS("把合并后的模型写回输入目录"),
    ZH_HANT("把合併後的模型寫回輸入目錄"),
    KO("병합한 모델을 입력 디렉터리에 덮어씁니다"),
    DE("Die zusammengeführten Modelle über das Eingabeverzeichnis schreiben"),
    FR("Réécrire les modèles fusionnés par-dessus le dossier d'entrée"),
    ES("Escribir los modelos fusionados sobre el directorio de entrada"),
    PT("Escrever os modelos fundidos por cima do diretório de entrada"),
    IT("Riscrivere i modelli fusi sopra la cartella di ingresso"),
    NL("De samengevoegde modellen over de invoermap heen schrijven"),
    RU("Записать слитые модели поверх входного каталога"),
    TR("Birleştirilmiş modelleri girdi dizininin üzerine yaz"));

// ===========================================================================
// input
// ===========================================================================

SS_MSG(images_help,
    EN("Image directory, used to put real filenames back into the model"),
    JA("画像ディレクトリ。実際のファイル名をモデルに書き戻すために使います"),
    ZH_HANS("图像目录，用于把真实文件名写回模型"),
    ZH_HANT("影像目錄，用於把真實檔名寫回模型"),
    KO("이미지 디렉터리. 실제 파일 이름을 모델에 되돌려 넣는 데 씁니다"),
    DE("Bildverzeichnis, um die echten Dateinamen wieder ins Modell zu setzen"),
    FR("Dossier d'images, servant à remettre les vrais noms de fichiers dans le "
       "modèle"),
    ES("Carpeta de imágenes, que sirve para devolver los nombres de archivo "
       "reales al modelo"),
    PT("Pasta de imagens, usada para devolver os nomes de arquivo reais ao "
       "modelo"),
    IT("Cartella delle immagini, usata per rimettere i nomi di file reali nel "
       "modello"),
    NL("Beeldmap, om de echte bestandsnamen terug in het model te zetten"),
    RU("Каталог изображений -- чтобы вернуть в модель настоящие имена файлов"),
    TR("Görüntü dizini; gerçek dosya adlarını modele geri koymak için kullanılır"));

SS_MSG(feature_dir_help,
    EN("Feature directory, if not given positionally"),
    JA("特徴点のディレクトリ。位置引数で与えない場合に使います"),
    ZH_HANS("特征点目录，用于未按位置给出时"),
    ZH_HANT("特徵點目錄，用於未按位置給出時"),
    KO("특징점 디렉터리. 위치 인자로 주지 않을 때 씁니다"),
    DE("Merkmalsverzeichnis, falls es nicht als Positionsargument kommt"),
    FR("Dossier des points, s'il n'est pas donné en position"),
    ES("Carpeta de rasgos, si no se da posicionalmente"),
    PT("Pasta de traços, se não for dada posicionalmente"),
    IT("Cartella dei punti, se non è data posizionalmente"),
    NL("Kenmerkmap, als die niet positioneel gegeven is"),
    RU("Каталог признаков, если он не задан позиционно"),
    TR("Öznitelik dizini, konumsal olarak verilmediyse"));

SS_MSG(resume_help,
    EN("Adopt the models in this directory instead of mapping from scratch (D44)"),
    JA("最初からマッピングせず、このディレクトリのモデルを引き継ぎます（D44）"),
    ZH_HANS("采用该目录中的模型，而不是从头开始建图（D44）"),
    ZH_HANT("採用該目錄中的模型，而不是從頭開始建圖（D44）"),
    KO("처음부터 매핑하지 않고 이 디렉터리의 모델을 이어받습니다(D44)"),
    DE("Die Modelle in diesem Verzeichnis übernehmen, statt neu zu kartieren "
       "(D44)"),
    FR("Reprendre les modèles de ce dossier au lieu de cartographier depuis zéro "
       "(D44)"),
    ES("Adoptar los modelos de este directorio en vez de cartografiar desde cero "
       "(D44)"),
    PT("Adotar os modelos deste diretório em vez de cartografar do zero (D44)"),
    IT("Adottare i modelli in questa cartella invece di cartografare da zero "
       "(D44)"),
    NL("De modellen in deze map overnemen in plaats van vanaf nul te karteren "
       "(D44)"),
    RU("Взять модели из этого каталога вместо построения с нуля (D44)"),
    TR("Sıfırdan haritalamak yerine bu dizindeki modelleri devral (D44)"));

SS_MSG(check_help,
    EN("With --resume: report how far each model agrees with the two-view "
       "geometries it was built from, then exit without writing anything"),
    JA("--resume と併用: 各モデルが、その元になった 2 視点幾何とどこまで整合するかを"
       "報告し、何も書かずに終了します"),
    ZH_HANS("与 --resume 一同使用：报告每个模型与其所依据的双视图几何相符到什么程度，"
            "然后不写出任何东西即退出"),
    ZH_HANT("與 --resume 一同使用：回報每個模型與其所依據的雙視圖幾何相符到什麼程度，"
            "然後不寫出任何東西即結束"),
    KO("--resume 과 함께: 각 모델이 그 바탕이 된 두 시점 기하와 어디까지 맞는지 "
       "보고하고, 아무것도 쓰지 않고 끝냅니다"),
    DE("Mit --resume: melden, wie weit jedes Modell mit den Zweiansichts-"
       "Geometrien übereinstimmt, aus denen es gebaut wurde, und dann beenden, "
       "ohne etwas zu schreiben"),
    FR("Avec --resume : indiquer dans quelle mesure chaque modèle s'accorde aux "
       "géométries à deux vues dont il est issu, puis quitter sans rien écrire"),
    ES("Con --resume: informar hasta qué punto cada modelo concuerda con las "
       "geometrías de dos vistas de las que salió, y salir sin escribir nada"),
    PT("Com --resume: relatar até que ponto cada modelo concorda com as "
       "geometrias de duas vistas de que saiu, e sair sem escrever nada"),
    IT("Con --resume: riferire quanto ogni modello concorda con le geometrie a "
       "due viste da cui è nato, poi uscire senza scrivere nulla"),
    NL("Met --resume: melden hoever elk model overeenkomt met de "
       "tweebeeldgeometrieën waaruit het is gebouwd, en dan stoppen zonder iets "
       "te schrijven"),
    RU("Вместе с --resume: сообщить, насколько каждая модель согласуется с "
       "двухвидовыми геометриями, из которых она построена, и выйти, ничего не "
       "записав"),
    TR("--resume ile birlikte: her modelin, kurulduğu iki görüşlü geometrilerle "
       "ne kadar uyuştuğunu bildir ve hiçbir şey yazmadan çık"));

// ===========================================================================
// runtime
// ===========================================================================

SS_MSG(threads_help,
    EN("Host worker threads: two-view verification, and the mapper's per-point "
       "and per-image passes; 0 is every core, 1 is serial, and results do not "
       "depend on the count"),
    JA("ホスト側のワーカースレッド数。2 視点の検証と、マッパーの点ごと・画像ごとの"
       "パスに使います。0 で全コア、1 で逐次。結果はこの数に依存しません"),
    ZH_HANS("主机端工作线程数：用于双视图验证以及建图器逐点、逐图像的处理；"
            "0 表示全部核心，1 表示串行，结果与线程数无关"),
    ZH_HANT("主機端工作執行緒數：用於雙視圖驗證以及建圖器逐點、逐影像的處理；"
            "0 表示全部核心，1 表示串行，結果與執行緒數無關"),
    KO("호스트 작업 스레드 수: 두 시점 검증과 매퍼의 점별·이미지별 처리에 씁니다. "
       "0 이면 모든 코어, 1 이면 직렬이며, 결과는 이 수에 좌우되지 않습니다"),
    DE("Arbeitsthreads auf dem Host: Zweiansichtsprüfung sowie die punkt- und "
       "bildweisen Durchgänge des Kartierers; 0 heißt jeder Kern, 1 seriell, und "
       "die Ergebnisse hängen nicht von der Zahl ab"),
    FR("Fils de travail côté hôte : vérification à deux vues, et passes du "
       "cartographe par point et par image ; 0 vaut tous les cœurs, 1 est "
       "séquentiel, et les résultats ne dépendent pas du nombre"),
    ES("Hilos de trabajo en el anfitrión: verificación de dos vistas y pasadas "
       "del cartógrafo por punto y por imagen; 0 son todos los núcleos, 1 es "
       "en serie, y los resultados no dependen del número"),
    PT("Threads de trabalho no hospedeiro: verificação de duas vistas e "
       "passagens do cartógrafo por ponto e por imagem; 0 são todos os núcleos, "
       "1 é em série, e os resultados não dependem do número"),
    IT("Thread di lavoro sull'host: verifica a due viste e passaggi del "
       "cartografo per punto e per immagine; 0 significa ogni core, 1 seriale, e "
       "i risultati non dipendono dal numero"),
    NL("Werkthreads op de host: tweebeeldverificatie en de per-punt- en "
       "per-beeldgangen van de kaartmaker; 0 is elke kern, 1 is serieel, en de "
       "uitkomsten hangen niet van het aantal af"),
    RU("Рабочие потоки на хосте: двухвидовая проверка и проходы построителя по "
       "точкам и по изображениям; 0 -- все ядра, 1 -- последовательно, а "
       "результаты от их числа не зависят"),
    TR("Ana makinedeki işçi iş parçacıkları: iki görüşlü doğrulama ile "
       "haritalayıcının nokta ve görüntü başına geçişleri; 0 tüm çekirdekler, 1 "
       "sıralı demektir ve sonuçlar sayıya bağlı değildir"));

SS_MSG(decode_threads_help,
    EN("Image decode threads; 0 is every core, 1 decodes inline"),
    JA("画像デコードのスレッド数。0 で全コア、1 ならその場でデコードします"),
    ZH_HANS("图像解码线程数；0 表示全部核心，1 表示就地解码"),
    ZH_HANT("影像解碼執行緒數；0 表示全部核心，1 表示就地解碼"),
    KO("이미지 디코딩 스레드 수. 0 이면 모든 코어, 1 이면 제자리에서 디코딩합니다"),
    DE("Threads für die Bilddekodierung; 0 heißt jeder Kern, 1 dekodiert inline"),
    FR("Fils de décodage d'images ; 0 vaut tous les cœurs, 1 décode sur place"),
    ES("Hilos de descodificación de imágenes; 0 son todos los núcleos, 1 "
       "descodifica en el sitio"),
    PT("Threads de decodificação de imagens; 0 são todos os núcleos, 1 "
       "decodifica no lugar"),
    IT("Thread di decodifica delle immagini; 0 significa ogni core, 1 decodifica "
       "sul posto"),
    NL("Threads voor beelddecodering; 0 is elke kern, 1 decodeert ter plekke"),
    RU("Потоки декодирования изображений; 0 -- все ядра, 1 -- декодировать на "
       "месте"),
    TR("Görüntü çözme iş parçacıkları; 0 tüm çekirdekler, 1 yerinde çözer"));

SS_MSG(decode_budget_help,
    EN("Memory the decode pool may hold in flight, in MB"),
    JA("デコードプールが同時に保持してよいメモリ量（MB）"),
    ZH_HANS("解码池同时可占用的内存，单位 MB"),
    ZH_HANT("解碼池同時可占用的記憶體，單位 MB"),
    KO("디코딩 풀이 동시에 붙들 수 있는 메모리(MB)"),
    DE("Speicher, den der Dekodier-Pool gleichzeitig halten darf, in MB"),
    FR("Mémoire que la réserve de décodage peut détenir en vol, en Mo"),
    ES("Memoria que la reserva de descodificación puede retener a la vez, en MB"),
    PT("Memória que a reserva de decodificação pode reter ao mesmo tempo, em MB"),
    IT("Memoria che il pool di decodifica può tenere in volo, in MB"),
    NL("Geheugen dat de decodeerpool tegelijk mag vasthouden, in MB"),
    RU("Сколько памяти пул декодирования может держать одновременно, в МБ"),
    TR("Çözme havuzunun aynı anda tutabileceği bellek, MB cinsinden"));

SS_MSG(device_help,
    EN("Vulkan device index; -1 picks the first suitable one"),
    JA("Vulkan デバイスの番号。-1 なら最初に適したものを選びます"),
    ZH_HANS("Vulkan 设备序号；-1 表示选取第一个合适的设备"),
    ZH_HANT("Vulkan 裝置序號；-1 表示選取第一個合適的裝置"),
    KO("Vulkan 장치 번호. -1 이면 처음으로 적합한 것을 고릅니다"),
    DE("Index des Vulkan-Geräts; -1 wählt das erste geeignete"),
    FR("Indice du périphérique Vulkan ; -1 prend le premier qui convient"),
    ES("Índice del dispositivo Vulkan; -1 toma el primero que sirva"),
    PT("Índice do dispositivo Vulkan; -1 toma o primeiro que sirva"),
    IT("Indice del dispositivo Vulkan; -1 prende il primo adatto"),
    NL("Index van het Vulkan-apparaat; -1 kiest het eerste geschikte"),
    RU("Номер устройства Vulkan; -1 выбирает первое подходящее"),
    TR("Vulkan aygıt sırası; -1 uygun olan ilkini seçer"));

SS_MSG(quiet_help,
    EN("Print only the result lines, not per-stage progress"),
    JA("段階ごとの進捗は出さず、結果の行だけを表示します"),
    ZH_HANS("只打印结果行，不打印各阶段的进度"),
    ZH_HANT("只列印結果行，不列印各階段的進度"),
    KO("단계별 진행 상황은 빼고 결과 줄만 출력합니다"),
    DE("Nur die Ergebniszeilen ausgeben, nicht den Fortschritt je Stufe"),
    FR("N'afficher que les lignes de résultat, pas la progression par étape"),
    ES("Mostrar solo las líneas de resultado, no el progreso por etapa"),
    PT("Mostrar apenas as linhas de resultado, não o progresso por etapa"),
    IT("Stampare solo le righe di risultato, non l'avanzamento per fase"),
    NL("Alleen de resultaatregels tonen, niet de voortgang per fase"),
    RU("Печатать только строки результата, без прогресса по стадиям"),
    TR("Yalnızca sonuç satırlarını yazdır, aşama aşama ilerlemeyi değil"));

}  // namespace sfmfield
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
