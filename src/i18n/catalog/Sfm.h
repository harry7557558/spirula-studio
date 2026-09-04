#pragma once

// What a reconstruction says while it runs.
//
// `spirula sfm` is a subcommand of this program, not a foreign tool, so its
// output is ours to write and ours to translate -- see src/sfm/core/Log.h for
// the mechanism (a localized, equal-width [tag] in front of every line) and
// for what deliberately stays English.
//
// Conventions, on top of the two in src/i18n/README.md:
//   * Numbers, paths, file names, camera-model names and flag spellings are
//     ARGUMENTS, never part of the translated text. A flag is an identifier;
//     `--max-image-size` is the same in every language.
//   * Count labels, not counted nouns ("Images: 5", not "5 images"), so no
//     language needs a plural rule for a progress line.
//   * The tags are what the log's left column is made of, so keep every one of
//     them SHORT -- the column is as wide as the widest tag in the language.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace sfm {

// ===========================================================================
// Stage tags -- the [bracketed] column. Short, and no longer than they need
// to be: two Han characters is four columns, and that sets the column width
// for the whole log in that language.
// ===========================================================================

SS_MSG(tag_run,
    EN("run"),      JA("実行"),      ZH_HANS("运行"),   ZH_HANT("執行"),
    KO("실행"),      DE("Lauf"),     FR("exéc"),       ES("ejec"),
    PT("exec"),     IT("esec"),     NL("run"),        RU("запуск"),
    TR("çalışma"));

SS_MSG(tag_extract,
    EN("extract"),  JA("抽出"),      ZH_HANS("提取"),   ZH_HANT("擷取"),
    KO("추출"),      DE("Merkmale"), FR("extrait"),    ES("extraer"),
    PT("extrair"),  IT("estrai"),   NL("kenmerk"),    RU("признак"),
    TR("çıkarım"));

SS_MSG(tag_match,
    EN("match"),    JA("照合"),      ZH_HANS("匹配"),   ZH_HANT("匹配"),
    KO("정합"),      DE("Paare"),    FR("paires"),     ES("pares"),
    PT("pares"),    IT("coppie"),   NL("paren"),      RU("пары"),
    TR("eşleme"));

SS_MSG(tag_map,
    EN("map"),      JA("復元"),      ZH_HANS("重建"),   ZH_HANT("重建"),
    KO("복원"),      DE("Modell"),   FR("modèle"),     ES("modelo"),
    PT("modelo"),   IT("modello"),  NL("model"),      RU("модель"),
    TR("model"));

SS_MSG(tag_merge,
    EN("merge"),    JA("統合"),      ZH_HANS("合并"),   ZH_HANT("合併"),
    KO("병합"),      DE("Fusion"),   FR("fusion"),     ES("fusión"),
    PT("fusão"),    IT("fusione"),  NL("fusie"),      RU("слияние"),
    TR("birleşim"));

SS_MSG(tag_orient,
    EN("orient"),   JA("座標"),      ZH_HANS("坐标"),   ZH_HANT("座標"),
    KO("좌표"),      DE("Achsen"),   FR("axes"),       ES("ejes"),
    PT("eixos"),    IT("assi"),     NL("assen"),      RU("оси"),
    TR("eksen"));

SS_MSG(tag_device,
    EN("device"),   JA("機器"),      ZH_HANS("设备"),   ZH_HANT("裝置"),
    KO("장치"),      DE("Gerät"),    FR("appareil"),   ES("equipo"),
    PT("aparelho"), IT("unità"),    NL("apparaat"),   RU("GPU"),
    TR("aygıt"));

// The word in front of a warning or an error, inside the tagged line.
SS_MSG(word_warning,
    EN("WARNING:"),      JA("警告:"),        ZH_HANS("警告:"),    ZH_HANT("警告:"),
    KO("경고:"),          DE("WARNUNG:"),    FR("AVERTISSEMENT :"), ES("AVISO:"),
    PT("AVISO:"),        IT("AVVISO:"),     NL("WAARSCHUWING:"), RU("ПРЕДУПРЕЖДЕНИЕ:"),
    TR("UYARI:"));

SS_MSG(word_error,
    EN("ERROR:"),        JA("エラー:"),      ZH_HANS("错误:"),    ZH_HANT("錯誤:"),
    KO("오류:"),          DE("FEHLER:"),     FR("ERREUR :"),     ES("ERROR:"),
    PT("ERRO:"),         IT("ERRORE:"),     NL("FOUT:"),        RU("ОШИБКА:"),
    TR("HATA:"));


// ===========================================================================
// The run: what it was asked to do
// ===========================================================================

SS_MSG(run_header,
    EN("{0} -> {1}"),
    JA("{0} -> {1}"),
    ZH_HANS("{0} -> {1}"),
    ZH_HANT("{0} -> {1}"),
    KO("{0} -> {1}"),
    DE("{0} -> {1}"),
    FR("{0} -> {1}"),
    ES("{0} -> {1}"),
    PT("{0} -> {1}"),
    IT("{0} -> {1}"),
    NL("{0} -> {1}"),
    RU("{0} -> {1}"),
    TR("{0} -> {1}"));

SS_MSG(run_quality,
    EN("Quality: {0}   Image size limit: {1} px   Feature limit: {2}"),
    JA("品質: {0}   画像サイズ上限: {1} px   特徴点の上限: {2}"),
    ZH_HANS("质量: {0}   图像尺寸上限: {1} px   特征点上限: {2}"),
    ZH_HANT("品質: {0}   影像尺寸上限: {1} px   特徵點上限: {2}"),
    KO("품질: {0}   이미지 크기 상한: {1} px   특징점 상한: {2}"),
    DE("Qualität: {0}   Bildgrößengrenze: {1} px   Merkmalsgrenze: {2}"),
    FR("Qualité : {0}   Taille d'image max : {1} px   Points max : {2}"),
    ES("Calidad: {0}   Tamaño máximo de imagen: {1} px   Máximo de puntos: {2}"),
    PT("Qualidade: {0}   Tamanho máximo de imagem: {1} px   Máximo de pontos: {2}"),
    IT("Qualità: {0}   Dimensione massima immagine: {1} px   Massimo di punti: {2}"),
    NL("Kwaliteit: {0}   Maximale beeldgrootte: {1} px   Maximum kenmerken: {2}"),
    RU("Качество: {0}   Предел размера изображения: {1} px   Предел числа точек: {2}"),
    TR("Kalite: {0}   Görüntü boyutu sınırı: {1} px   Öznitelik sınırı: {2}"));

SS_MSG(run_data_type,
    EN("Capture type: {0}"),
    JA("撮影の種類: {0}"),
    ZH_HANS("拍摄类型: {0}"),
    ZH_HANT("拍攝類型: {0}"),
    KO("촬영 종류: {0}"),
    DE("Aufnahmeart: {0}"),
    FR("Type de prise de vue : {0}"),
    ES("Tipo de captura: {0}"),
    PT("Tipo de captura: {0}"),
    IT("Tipo di ripresa: {0}"),
    NL("Soort opname: {0}"),
    RU("Тип съёмки: {0}"),
    TR("Çekim türü: {0}"));

SS_MSG(run_cameras,
    EN("Lens: {0}   Camera grouping: {1}"),
    JA("レンズ: {0}   カメラのまとめ方: {1}"),
    ZH_HANS("镜头: {0}   相机分组方式: {1}"),
    ZH_HANT("鏡頭: {0}   相機分組方式: {1}"),
    KO("렌즈: {0}   카메라 묶는 방식: {1}"),
    DE("Objektiv: {0}   Kameragruppierung: {1}"),
    FR("Objectif : {0}   Regroupement des caméras : {1}"),
    ES("Objetivo: {0}   Agrupación de cámaras: {1}"),
    PT("Lente: {0}   Agrupamento de câmeras: {1}"),
    IT("Obiettivo: {0}   Raggruppamento delle fotocamere: {1}"),
    NL("Lens: {0}   Cameragroepering: {1}"),
    RU("Объектив: {0}   Группировка камер: {1}"),
    TR("Objektif: {0}   Kamera gruplaması: {1}"));

// The images are EXRs and the transfer was left to them; {0} is the gamut in
// force. Everything here converts to sRGB before it looks at a pixel.
SS_MSG(run_exr_color,
    EN("EXR input read as linear {0}"),
    JA("EXR 入力を線形 {0} として読み込みます"),
    ZH_HANS("EXR 输入按线性 {0} 读取"),
    ZH_HANT("EXR 輸入依線性 {0} 讀取"),
    KO("EXR 입력을 선형 {0}(으)로 읽습니다"),
    DE("EXR-Eingabe wird als lineares {0} gelesen"),
    FR("Entrée EXR lue comme {0} linéaire"),
    ES("Entrada EXR leída como {0} lineal"),
    PT("Entrada EXR lida como {0} linear"),
    IT("Ingresso EXR letto come {0} lineare"),
    NL("EXR-invoer gelezen als lineair {0}"),
    RU("Вход EXR читается как линейный {0}"),
    TR("EXR girdisi doğrusal {0} olarak okunuyor"));

SS_MSG(run_exr_gamut_from_file,
    EN("EXR colour space: {0}, from the file"),
    JA("EXR の色空間: {0}（ファイルの情報）"),
    ZH_HANS("EXR 色彩空间: {0}（取自文件）"),
    ZH_HANT("EXR 色彩空間: {0}（取自檔案）"),
    KO("EXR 색 공간: {0}(파일에서 읽음)"),
    DE("EXR-Farbraum: {0}, aus der Datei"),
    FR("Espace colorimétrique EXR : {0}, d'après le fichier"),
    ES("Espacio de color EXR: {0}, según el archivo"),
    PT("Espaço de cor EXR: {0}, conforme o arquivo"),
    IT("Spazio colore EXR: {0}, dal file"),
    NL("EXR-kleurruimte: {0}, uit het bestand"),
    RU("Цветовое пространство EXR: {0}, из файла"),
    TR("EXR renk uzayı: {0}, dosyadan"));

SS_MSG(run_exr_gamut_unknown,
    EN("The EXR's color primaries match no known color space; reading it as Rec.709"),
    JA("EXR の原色はどの既知の色空間とも一致しません。Rec.709 として読み込みます"),
    ZH_HANS("EXR 的色彩基色不属于任何已知色彩空间，按 Rec.709 读取"),
    ZH_HANT("EXR 的色彩基色不屬於任何已知色彩空間，依 Rec.709 讀取"),
    KO("EXR의 원색이 알려진 색 공간과 일치하지 않습니다. Rec.709로 읽습니다"),
    DE("Die Primärfarben der EXR passen zu keinem bekannten Farbraum; "
       "sie wird als Rec.709 gelesen"),
    FR("Les primaires de l'EXR ne correspondent à aucun espace connu ; "
       "lecture en Rec.709"),
    ES("Los primarios del EXR no coinciden con ningún espacio conocido; "
       "se lee como Rec.709"),
    PT("Os primários do EXR não correspondem a nenhum espaço conhecido; "
       "lido como Rec.709"),
    IT("I primari dell'EXR non corrispondono ad alcuno spazio noto; "
       "viene letto come Rec.709"),
    NL("De primaire kleuren van de EXR passen bij geen bekende kleurruimte; "
       "hij wordt als Rec.709 gelezen"),
    RU("Основные цвета EXR не совпадают ни с одним известным пространством; "
       "файл читается как Rec.709"),
    TR("EXR'nin ana renkleri bilinen hiçbir renk uzayıyla eşleşmiyor; "
       "Rec.709 olarak okunuyor"));

SS_MSG(run_masks,
    EN("Masks: {0}"),
    JA("マスク: {0}"),
    ZH_HANS("蒙版: {0}"),
    ZH_HANT("遮罩: {0}"),
    KO("마스크: {0}"),
    DE("Masken: {0}"),
    FR("Masques : {0}"),
    ES("Máscaras: {0}"),
    PT("Máscaras: {0}"),
    IT("Maschere: {0}"),
    NL("Maskers: {0}"),
    RU("Маски: {0}"),
    TR("Maskeler: {0}"));

SS_MSG(run_preset_moved,
    EN("The presets set {0} to {1} (was {2})"),
    JA("プリセットにより {0} は {1} になりました（元は {2}）"),
    ZH_HANS("预设把 {0} 设为 {1}（原为 {2}）"),
    ZH_HANT("預設把 {0} 設為 {1}（原為 {2}）"),
    KO("프리셋이 {0} 을(를) {1} 로 설정했습니다(원래 {2})"),
    DE("Die Voreinstellungen setzen {0} auf {1} (vorher {2})"),
    FR("Les préréglages fixent {0} à {1} (auparavant {2})"),
    ES("Los ajustes preestablecidos fijan {0} en {1} (antes {2})"),
    PT("As predefinições definem {0} como {1} (antes {2})"),
    IT("I preimpostati portano {0} a {1} (prima {2})"),
    NL("De voorinstellingen zetten {0} op {1} (was {2})"),
    RU("Предустановки задают {0} = {1} (было {2})"),
    TR("Ön ayarlar {0} değerini {1} yaptı (önceki {2})"));

SS_MSG(run_too_few_images,
    EN("Only {0} image(s) could be read, and at least 2 are needed."),
    JA("読み込めた画像は {0} 枚だけで、少なくとも2枚必要です。"),
    ZH_HANS("只读到 {0} 张图像，至少需要 2 张。"),
    ZH_HANT("只讀到 {0} 張影像，至少需要 2 張。"),
    KO("읽어들인 이미지가 {0} 장뿐이며, 최소 2장이 필요합니다."),
    DE("Es konnten nur {0} Bild(er) gelesen werden; mindestens 2 sind nötig."),
    FR("Seulement {0} image(s) ont pu être lues, or il en faut au moins 2."),
    ES("Solo se pudieron leer {0} imagen(es), y hacen falta al menos 2."),
    PT("Só foi possível ler {0} imagem(ns), e são necessárias pelo menos 2."),
    IT("È stato possibile leggere solo {0} immagine/i, e ne servono almeno 2."),
    NL("Er konden maar {0} afbeelding(en) worden gelezen; er zijn er minstens 2 nodig."),
    RU("Удалось прочитать только изображений: {0}, а нужно не менее 2."),
    TR("Yalnızca {0} görüntü okunabildi; en az 2 gerekiyor."));

SS_MSG(run_not_a_directory,
    EN("{0} is not a directory."),
    JA("{0} はディレクトリではありません。"),
    ZH_HANS("{0} 不是目录。"),
    ZH_HANT("{0} 不是目錄。"),
    KO("{0} 은(는) 디렉터리가 아닙니다."),
    DE("{0} ist kein Verzeichnis."),
    FR("{0} n'est pas un répertoire."),
    ES("{0} no es un directorio."),
    PT("{0} não é um diretório."),
    IT("{0} non è una directory."),
    NL("{0} is geen map."),
    RU("{0} — не каталог."),
    TR("{0} bir dizin değil."));

SS_MSG(run_nested_images,
    EN("{0} contains an images/ folder and nothing else to reconstruct, so {1} is the image directory."),
    JA("{0} には images/ フォルダしか復元対象がないため、画像ディレクトリは {1} を使います。"),
    ZH_HANS("{0} 里除了 images/ 文件夹没有别的可重建内容，因此图像目录用 {1}。"),
    ZH_HANT("{0} 裡除了 images/ 資料夾沒有別的可重建內容，因此影像目錄用 {1}。"),
    KO("{0} 안에는 images/ 폴더 말고 복원할 것이 없어 이미지 디렉터리로 {1} 을(를) 씁니다."),
    DE("{0} enthält außer einem images/-Ordner nichts zu Rekonstruierendes, daher ist {1} das Bildverzeichnis."),
    FR("{0} ne contient rien à reconstruire hormis un dossier images/ ; le répertoire d'images est donc {1}."),
    ES("{0} no contiene nada que reconstruir salvo una carpeta images/, así que el directorio de imágenes es {1}."),
    PT("{0} não contém nada a reconstruir além de uma pasta images/, então o diretório de imagens é {1}."),
    IT("{0} non contiene nulla da ricostruire tranne una cartella images/, quindi la directory delle immagini è {1}."),
    NL("{0} bevat behalve een map images/ niets om te reconstrueren, dus {1} is de afbeeldingenmap."),
    RU("В {0} нет ничего для восстановления, кроме папки images/, поэтому каталог изображений — {1}."),
    TR("{0} içinde images/ klasöründen başka yeniden oluşturulacak bir şey yok, bu yüzden görüntü dizini {1}."));

SS_MSG(device_using,
    EN("Using {0}"),
    JA("{0} を使用します"),
    ZH_HANS("使用 {0}"),
    ZH_HANT("使用 {0}"),
    KO("{0} 을(를) 사용합니다"),
    DE("Es wird {0} verwendet"),
    FR("Utilisation de {0}"),
    ES("Se usa {0}"),
    PT("Usando {0}"),
    IT("Si usa {0}"),
    NL("{0} wordt gebruikt"),
    RU("Используется {0}"),
    TR("{0} kullanılıyor"));


// ===========================================================================
// Extraction
// ===========================================================================

SS_MSG(extract_plan,
    EN("Images: {0}   Decoding threads: {1}   Window: {2}   Peak memory: about {3} MB"),
    JA("画像: {0}   デコードのスレッド: {1}   ウィンドウ: {2}   メモリのピーク: 約 {3} MB"),
    ZH_HANS("图像: {0}   解码线程: {1}   窗口: {2}   内存峰值: 约 {3} MB"),
    ZH_HANT("影像: {0}   解碼執行緒: {1}   視窗: {2}   記憶體尖峰: 約 {3} MB"),
    KO("이미지: {0}   디코딩 스레드: {1}   윈도: {2}   최대 메모리: 약 {3} MB"),
    DE("Bilder: {0}   Dekodier-Threads: {1}   Fenster: {2}   Speicherspitze: etwa {3} MB"),
    FR("Images : {0}   Fils de décodage : {1}   Fenêtre : {2}   Pic mémoire : environ {3} Mo"),
    ES("Imágenes: {0}   Hilos de decodificación: {1}   Ventana: {2}   Pico de memoria: unos {3} MB"),
    PT("Imagens: {0}   Threads de decodificação: {1}   Janela: {2}   Pico de memória: cerca de {3} MB"),
    IT("Immagini: {0}   Thread di decodifica: {1}   Finestra: {2}   Picco di memoria: circa {3} MB"),
    NL("Afbeeldingen: {0}   Decodeerthreads: {1}   Venster: {2}   Geheugenpiek: ongeveer {3} MB"),
    RU("Изображений: {0}   Потоков декодирования: {1}   Окно: {2}   Пик памяти: около {3} МБ"),
    TR("Görüntü: {0}   Çözme iş parçacığı: {1}   Pencere: {2}   Bellek tepesi: yaklaşık {3} MB"));

SS_MSG(extract_frontend,
    EN("Detector: {0}"),
    JA("検出器: {0}"),
    ZH_HANS("检测器: {0}"),
    ZH_HANT("偵測器: {0}"),
    KO("검출기: {0}"),
    DE("Detektor: {0}"),
    FR("Détecteur : {0}"),
    ES("Detector: {0}"),
    PT("Detector: {0}"),
    IT("Rilevatore: {0}"),
    NL("Detector: {0}"),
    RU("Детектор: {0}"),
    TR("Sezici: {0}"));

SS_MSG(extract_progress,
    EN("{0}/{1}   {2}   Features: {3}"),
    JA("{0}/{1}   {2}   特徴点: {3}"),
    ZH_HANS("{0}/{1}   {2}   特征点: {3}"),
    ZH_HANT("{0}/{1}   {2}   特徵點: {3}"),
    KO("{0}/{1}   {2}   특징점: {3}"),
    DE("{0}/{1}   {2}   Merkmale: {3}"),
    FR("{0}/{1}   {2}   Points : {3}"),
    ES("{0}/{1}   {2}   Puntos: {3}"),
    PT("{0}/{1}   {2}   Pontos: {3}"),
    IT("{0}/{1}   {2}   Punti: {3}"),
    NL("{0}/{1}   {2}   Kenmerken: {3}"),
    RU("{0}/{1}   {2}   Точек: {3}"),
    TR("{0}/{1}   {2}   Öznitelik: {3}"));

SS_MSG(extract_progress_masked,
    EN("{0}/{1}   {2}   Features: {3}   Masked out: {4}"),
    JA("{0}/{1}   {2}   特徴点: {3}   マスクで除外: {4}"),
    ZH_HANS("{0}/{1}   {2}   特征点: {3}   被蒙版排除: {4}"),
    ZH_HANT("{0}/{1}   {2}   特徵點: {3}   被遮罩排除: {4}"),
    KO("{0}/{1}   {2}   특징점: {3}   마스크로 제외: {4}"),
    DE("{0}/{1}   {2}   Merkmale: {3}   Von Masken entfernt: {4}"),
    FR("{0}/{1}   {2}   Points : {3}   Écartés par les masques : {4}"),
    ES("{0}/{1}   {2}   Puntos: {3}   Descartados por máscara: {4}"),
    PT("{0}/{1}   {2}   Pontos: {3}   Descartados pela máscara: {4}"),
    IT("{0}/{1}   {2}   Punti: {3}   Scartati dalle maschere: {4}"),
    NL("{0}/{1}   {2}   Kenmerken: {3}   Door maskers weggelaten: {4}"),
    RU("{0}/{1}   {2}   Точек: {3}   Отсечено масками: {4}"),
    TR("{0}/{1}   {2}   Öznitelik: {3}   Maskeyle elenen: {4}"));

SS_MSG(extract_done,
    EN("Done. Images: {0}   Features: {1}"),
    JA("完了。画像: {0}   特徴点: {1}"),
    ZH_HANS("完成。图像: {0}   特征点: {1}"),
    ZH_HANT("完成。影像: {0}   特徵點: {1}"),
    KO("완료. 이미지: {0}   특징점: {1}"),
    DE("Fertig. Bilder: {0}   Merkmale: {1}"),
    FR("Terminé. Images : {0}   Points : {1}"),
    ES("Listo. Imágenes: {0}   Puntos: {1}"),
    PT("Concluído. Imagens: {0}   Pontos: {1}"),
    IT("Fatto. Immagini: {0}   Punti: {1}"),
    NL("Klaar. Afbeeldingen: {0}   Kenmerken: {1}"),
    RU("Готово. Изображений: {0}   Точек: {1}"),
    TR("Bitti. Görüntü: {0}   Öznitelik: {1}"));

SS_MSG(extract_masks_matched,
    EN("Masks matched: {0}/{1} images, from {2}"),
    JA("マスクが一致した画像: {0}/{1}（{2} から）"),
    ZH_HANS("匹配到蒙版的图像: {0}/{1}（来自 {2}）"),
    ZH_HANT("對應到遮罩的影像: {0}/{1}（來自 {2}）"),
    KO("마스크가 맞은 이미지: {0}/{1}（{2} 에서）"),
    DE("Zugeordnete Masken: {0}/{1} Bilder, aus {2}"),
    FR("Masques appariés : {0}/{1} images, depuis {2}"),
    ES("Máscaras emparejadas: {0}/{1} imágenes, desde {2}"),
    PT("Máscaras correspondidas: {0}/{1} imagens, de {2}"),
    IT("Maschere abbinate: {0}/{1} immagini, da {2}"),
    NL("Gekoppelde maskers: {0}/{1} afbeeldingen, uit {2}"),
    RU("Сопоставлено масок: {0}/{1} изображений, из {2}"),
    TR("Eşleşen maske: {0}/{1} görüntü, {2} içinden"));

SS_MSG(extract_no_images,
    EN("No images in {0}."),
    JA("{0} に画像がありません。"),
    ZH_HANS("{0} 里没有图像。"),
    ZH_HANT("{0} 裡沒有影像。"),
    KO("{0} 안에 이미지가 없습니다."),
    DE("Keine Bilder in {0}."),
    FR("Aucune image dans {0}."),
    ES("No hay imágenes en {0}."),
    PT("Não há imagens em {0}."),
    IT("Nessuna immagine in {0}."),
    NL("Geen afbeeldingen in {0}."),
    RU("В {0} нет изображений."),
    TR("{0} içinde görüntü yok."));

SS_MSG(extract_no_decodable,
    EN("Nothing in {0} could be decoded as an image."),
    JA("{0} の中に画像として読めるものがありませんでした。"),
    ZH_HANS("{0} 里没有能作为图像解码的文件。"),
    ZH_HANT("{0} 裡沒有能當作影像解碼的檔案。"),
    KO("{0} 안에서 이미지로 디코딩할 수 있는 파일이 없습니다."),
    DE("In {0} ließ sich nichts als Bild dekodieren."),
    FR("Rien dans {0} n'a pu être décodé comme une image."),
    ES("No se pudo decodificar nada de {0} como imagen."),
    PT("Nada em {0} pôde ser decodificado como imagem."),
    IT("Niente in {0} è stato decodificabile come immagine."),
    NL("Niets in {0} kon als afbeelding worden gedecodeerd."),
    RU("Ничто в {0} не удалось декодировать как изображение."),
    TR("{0} içindekilerin hiçbiri görüntü olarak çözülemedi."));

SS_MSG(extract_skipping_file,
    EN("Skipping {0}: it is not a decodable image."),
    JA("{0} を飛ばします: 画像として読み取れません。"),
    ZH_HANS("跳过 {0}: 无法作为图像解码。"),
    ZH_HANT("略過 {0}: 無法當作影像解碼。"),
    KO("{0} 을(를) 건너뜁니다: 디코딩할 수 있는 이미지가 아닙니다."),
    DE("{0} wird übersprungen: kein dekodierbares Bild."),
    FR("{0} est ignoré : ce n'est pas une image décodable."),
    ES("Se omite {0}: no es una imagen decodificable."),
    PT("Ignorando {0}: não é uma imagem decodificável."),
    IT("Si salta {0}: non è un'immagine decodificabile."),
    NL("{0} wordt overgeslagen: geen decodeerbare afbeelding."),
    RU("Пропуск {0}: это не декодируемое изображение."),
    TR("{0} atlanıyor: çözülebilir bir görüntü değil."));

SS_MSG(extract_failed_file,
    EN("{0} failed: {1}"),
    JA("{0} は失敗しました: {1}"),
    ZH_HANS("{0} 失败: {1}"),
    ZH_HANT("{0} 失敗: {1}"),
    KO("{0} 실패: {1}"),
    DE("{0} fehlgeschlagen: {1}"),
    FR("Échec de {0} : {1}"),
    ES("Falló {0}: {1}"),
    PT("{0} falhou: {1}"),
    IT("{0} non è riuscito: {1}"),
    NL("{0} is mislukt: {1}"),
    RU("{0} не удалось: {1}"),
    TR("{0} başarısız: {1}"));

SS_MSG(extract_mask_dir_missing,
    EN("The mask directory {0} does not exist; extracting without masks."),
    JA("マスクのディレクトリ {0} がありません。マスクなしで抽出します。"),
    ZH_HANS("蒙版目录 {0} 不存在，将不使用蒙版进行提取。"),
    ZH_HANT("遮罩目錄 {0} 不存在，將不使用遮罩進行擷取。"),
    KO("마스크 디렉터리 {0} 이(가) 없습니다. 마스크 없이 추출합니다."),
    DE("Das Maskenverzeichnis {0} existiert nicht; es wird ohne Masken extrahiert."),
    FR("Le répertoire de masques {0} n'existe pas ; extraction sans masques."),
    ES("El directorio de máscaras {0} no existe; se extrae sin máscaras."),
    PT("O diretório de máscaras {0} não existe; extraindo sem máscaras."),
    IT("La directory delle maschere {0} non esiste; si estrae senza maschere."),
    NL("De maskermap {0} bestaat niet; er wordt zonder maskers geëxtraheerd."),
    RU("Каталог масок {0} не существует; извлечение без масок."),
    TR("Maske dizini {0} yok; maskesiz çıkarım yapılıyor."));

SS_MSG(extract_no_mask_matches,
    EN("No mask in {0} matches any image (tried \"{1}.png\" and \"<name>.png\"). "
       "Fix --masks or drop it: continuing unmasked would quietly produce a "
       "different reconstruction."),
    JA("{0} のどのマスクも画像に一致しません（\"{1}.png\" と \"<名前>.png\" を試しました）。"
       "--masks を直すか外してください。マスクなしで続けると、黙って別の復元結果になります。"),
    ZH_HANS("{0} 里没有任何蒙版能对应到图像（试过 \"{1}.png\" 和 \"<名称>.png\"）。"
            "请修正 --masks 或去掉它: 不用蒙版继续会悄悄得到另一个重建结果。"),
    ZH_HANT("{0} 裡沒有任何遮罩能對應到影像（試過 \"{1}.png\" 和 \"<名稱>.png\"）。"
            "請修正 --masks 或拿掉它: 不用遮罩繼續會悄悄得到另一個重建結果。"),
    KO("{0} 안의 어떤 마스크도 이미지와 맞지 않습니다(\"{1}.png\" 와 \"<이름>.png\" 를 시도). "
       "--masks 를 고치거나 빼세요. 마스크 없이 계속하면 조용히 다른 복원 결과가 나옵니다."),
    DE("Keine Maske in {0} passt zu einem Bild (versucht: \"{1}.png\" und \"<Name>.png\"). "
       "Korrigieren Sie --masks oder lassen Sie es weg: unmaskiert weiterzumachen ergäbe "
       "stillschweigend eine andere Rekonstruktion."),
    FR("Aucun masque de {0} ne correspond à une image (essais : \"{1}.png\" et \"<nom>.png\"). "
       "Corrigez --masks ou retirez-le : continuer sans masques produirait discrètement "
       "une autre reconstruction."),
    ES("Ninguna máscara de {0} corresponde a una imagen (se probó \"{1}.png\" y \"<nombre>.png\"). "
       "Corrija --masks o quítelo: seguir sin máscaras produciría en silencio otra reconstrucción."),
    PT("Nenhuma máscara em {0} corresponde a alguma imagem (tentou-se \"{1}.png\" e \"<nome>.png\"). "
       "Corrija --masks ou remova-o: continuar sem máscaras produziria silenciosamente "
       "outra reconstrução."),
    IT("Nessuna maschera in {0} corrisponde a un'immagine (provati \"{1}.png\" e \"<nome>.png\"). "
       "Corregga --masks o lo tolga: proseguire senza maschere darebbe in silenzio "
       "un'altra ricostruzione."),
    NL("Geen enkel masker in {0} hoort bij een afbeelding (geprobeerd: \"{1}.png\" en \"<naam>.png\"). "
       "Corrigeer --masks of laat het weg: zonder maskers doorgaan geeft stilletjes "
       "een andere reconstructie."),
    RU("Ни одна маска в {0} не соответствует изображению (пробовались \"{1}.png\" и \"<имя>.png\"). "
       "Исправьте --masks или уберите его: продолжение без масок молча даст другую реконструкцию."),
    TR("{0} içindeki hiçbir maske bir görüntüyle eşleşmiyor (\"{1}.png\" ve \"<ad>.png\" denendi). "
       "--masks seçeneğini düzeltin ya da kaldırın: maskesiz devam etmek sessizce "
       "başka bir yeniden oluşturma üretir."));

SS_MSG(extract_some_unmasked,
    EN("Images with no mask: {0} (for example {1}); their features are kept in full."),
    JA("マスクのない画像: {0} 枚（例: {1}）。それらの特徴点はすべて残します。"),
    ZH_HANS("没有蒙版的图像: {0} 张（例如 {1}），它们的特征点全部保留。"),
    ZH_HANT("沒有遮罩的影像: {0} 張（例如 {1}），它們的特徵點全部保留。"),
    KO("마스크가 없는 이미지: {0} 장(예: {1}). 그 특징점은 모두 남깁니다."),
    DE("Bilder ohne Maske: {0} (zum Beispiel {1}); ihre Merkmale bleiben vollständig erhalten."),
    FR("Images sans masque : {0} (par exemple {1}) ; leurs points sont conservés en entier."),
    ES("Imágenes sin máscara: {0} (por ejemplo {1}); sus puntos se conservan enteros."),
    PT("Imagens sem máscara: {0} (por exemplo {1}); seus pontos são mantidos por inteiro."),
    IT("Immagini senza maschera: {0} (per esempio {1}); i loro punti restano interi."),
    NL("Afbeeldingen zonder masker: {0} (bijvoorbeeld {1}); hun kenmerken blijven volledig."),
    RU("Изображений без маски: {0} (например, {1}); их точки сохранены целиком."),
    TR("Maskesi olmayan görüntü: {0} (örneğin {1}); öznitelikleri tümüyle korunur."));

SS_MSG(extract_mask_undecodable,
    EN("The mask {0} could not be decoded, so {1} is kept unmasked."),
    JA("マスク {0} を読み取れなかったため、{1} はマスクなしで扱います。"),
    ZH_HANS("无法解码蒙版 {0}，因此 {1} 不使用蒙版。"),
    ZH_HANT("無法解碼遮罩 {0}，因此 {1} 不使用遮罩。"),
    KO("마스크 {0} 을(를) 디코딩하지 못해 {1} 은(는) 마스크 없이 처리합니다."),
    DE("Die Maske {0} ließ sich nicht dekodieren, daher bleibt {1} unmaskiert."),
    FR("Le masque {0} n'a pas pu être décodé, {1} reste donc non masquée."),
    ES("No se pudo decodificar la máscara {0}, así que {1} queda sin máscara."),
    PT("A máscara {0} não pôde ser decodificada, então {1} fica sem máscara."),
    IT("La maschera {0} non è stata decodificabile, quindi {1} resta senza maschera."),
    NL("Het masker {0} kon niet worden gedecodeerd, dus {1} blijft ongemaskeerd."),
    RU("Маску {0} не удалось декодировать, поэтому {1} остаётся без маски."),
    TR("{0} maskesi çözülemedi, bu yüzden {1} maskesiz bırakıldı."));

SS_MSG(extract_mask_aspect,
    EN("The mask {0} is {1}x{2} but the image is {3}x{4}; a different aspect ratio "
       "means it will be stretched over the wrong content. Further warnings for "
       "this size pair are suppressed."),
    JA("マスク {0} は {1}x{2} ですが画像は {3}x{4} です。縦横比が違うので、"
       "誤った位置に引き伸ばされます。このサイズの組み合わせの警告は以後省略します。"),
    ZH_HANS("蒙版 {0} 是 {1}x{2}，而图像是 {3}x{4}; 长宽比不同意味着它会被拉伸到错误的内容上。"
            "此尺寸组合的后续警告将不再显示。"),
    ZH_HANT("遮罩 {0} 是 {1}x{2}，而影像是 {3}x{4}; 長寬比不同表示它會被拉伸到錯誤的內容上。"
            "此尺寸組合的後續警告將不再顯示。"),
    KO("마스크 {0} 은(는) {1}x{2} 인데 이미지는 {3}x{4} 입니다. 종횡비가 달라 엉뚱한 내용 위로 "
       "늘어납니다. 이 크기 조합에 대한 이후 경고는 생략합니다."),
    DE("Die Maske {0} ist {1}x{2}, das Bild aber {3}x{4}; bei abweichendem Seitenverhältnis "
       "wird sie über den falschen Inhalt gezogen. Weitere Warnungen zu diesem Größenpaar "
       "werden unterdrückt."),
    FR("Le masque {0} fait {1}x{2} alors que l'image fait {3}x{4} ; un rapport d'aspect "
       "différent signifie qu'il sera étiré sur le mauvais contenu. Les avertissements "
       "suivants pour ce couple de tailles sont supprimés."),
    ES("La máscara {0} es de {1}x{2} pero la imagen es de {3}x{4}; una relación de aspecto "
       "distinta significa que se estirará sobre el contenido equivocado. Se omiten los "
       "avisos siguientes para este par de tamaños."),
    PT("A máscara {0} é {1}x{2} mas a imagem é {3}x{4}; uma proporção diferente significa "
       "que ela será esticada sobre o conteúdo errado. Os avisos seguintes para este par "
       "de tamanhos são suprimidos."),
    IT("La maschera {0} è {1}x{2} ma l'immagine è {3}x{4}; un rapporto d'aspetto diverso "
       "significa che verrà stirata sul contenuto sbagliato. Gli avvisi successivi per "
       "questa coppia di dimensioni sono soppressi."),
    NL("Het masker {0} is {1}x{2} maar de afbeelding is {3}x{4}; een andere "
       "beeldverhouding betekent dat het over de verkeerde inhoud wordt uitgerekt. "
       "Verdere waarschuwingen voor dit maatpaar blijven achterwege."),
    RU("Маска {0} имеет размер {1}x{2}, а изображение {3}x{4}; иное соотношение сторон "
       "означает, что она растянется по неверному содержимому. Дальнейшие предупреждения "
       "для этой пары размеров подавлены."),
    TR("{0} maskesi {1}x{2}, ama görüntü {3}x{4}; farklı en-boy oranı, maskenin yanlış "
       "içeriğin üzerine gerileceği anlamına gelir. Bu boyut çifti için sonraki uyarılar "
       "gösterilmez."));

SS_MSG(extract_mask_empty,
    EN("The mask {0} left no keypoints at all in {1}. Masks keep the white pixels "
       "and ignore the black ones, so an inverted mask masks out the whole image."),
    JA("マスク {0} により {1} の特徴点がすべて消えました。マスクは白い画素を残し黒い画素を無視するので、"
       "白黒が逆のマスクは画像全体を消してしまいます。"),
    ZH_HANS("蒙版 {0} 让 {1} 里一个特征点也没剩下。蒙版保留白色像素、忽略黑色像素，"
            "所以反相的蒙版会把整张图都排除掉。"),
    ZH_HANT("遮罩 {0} 讓 {1} 裡一個特徵點也沒剩下。遮罩保留白色像素、忽略黑色像素，"
            "所以反相的遮罩會把整張圖都排除掉。"),
    KO("마스크 {0} 때문에 {1} 의 특징점이 하나도 남지 않았습니다. 마스크는 흰 픽셀을 남기고 "
       "검은 픽셀을 무시하므로, 반전된 마스크는 이미지 전체를 가려버립니다."),
    DE("Die Maske {0} hat in {1} keinen einzigen Merkmalspunkt übrig gelassen. Masken "
       "behalten die weißen Pixel und ignorieren die schwarzen, eine invertierte Maske "
       "blendet also das ganze Bild aus."),
    FR("Le masque {0} n'a laissé aucun point dans {1}. Les masques conservent les pixels "
       "blancs et ignorent les noirs : un masque inversé écarte donc toute l'image."),
    ES("La máscara {0} no dejó ni un punto en {1}. Las máscaras conservan los píxeles "
       "blancos e ignoran los negros, así que una máscara invertida descarta la imagen entera."),
    PT("A máscara {0} não deixou nenhum ponto em {1}. As máscaras mantêm os pixels brancos "
       "e ignoram os pretos, então uma máscara invertida descarta a imagem inteira."),
    IT("La maschera {0} non ha lasciato alcun punto in {1}. Le maschere tengono i pixel "
       "bianchi e ignorano i neri, quindi una maschera invertita esclude l'intera immagine."),
    NL("Het masker {0} liet in {1} geen enkel kenmerkpunt over. Maskers behouden de witte "
       "pixels en negeren de zwarte, dus een omgekeerd masker maskeert het hele beeld weg."),
    RU("Маска {0} не оставила в {1} ни одной точки. Маски сохраняют белые пиксели и "
       "игнорируют чёрные, поэтому инвертированная маска убирает всё изображение."),
    TR("{0} maskesi {1} içinde tek bir anahtar nokta bırakmadı. Maskeler beyaz pikselleri "
       "tutar, siyahları yok sayar; ters çevrilmiş bir maske tüm görüntüyü eler."));

SS_MSG(extract_masks_look_inverted,
    EN("Masks dropped {0}% of all keypoints. Unless this capture is a single object "
       "on a masked-out background, the masks are probably inverted -- this pipeline "
       "keeps what is WHITE, as COLMAP does. Re-run with --no-masks to check."),
    JA("マスクにより全特徴点の {0}% が失われました。背景をマスクした単一被写体の撮影でないなら、"
       "マスクの白黒が逆の可能性が高いです。このパイプラインはCOLMAPと同じく白い部分を残します。"
       "--no-masks を付けて再実行すると確認できます。"),
    ZH_HANS("蒙版排除了全部特征点的 {0}%。除非这是把背景遮住的单一物体拍摄，否则蒙版很可能是反的——"
            "本流程和 COLMAP 一样保留白色部分。加 --no-masks 重新运行即可确认。"),
    ZH_HANT("遮罩排除了全部特徵點的 {0}%。除非這是把背景遮住的單一物體拍攝，否則遮罩很可能是反的——"
            "本流程和 COLMAP 一樣保留白色部分。加 --no-masks 重新執行即可確認。"),
    KO("마스크가 전체 특징점의 {0}% 를 없앴습니다. 배경을 가린 단일 피사체 촬영이 아니라면 "
       "마스크가 반전되어 있을 가능성이 큽니다. 이 파이프라인은 COLMAP 처럼 흰 부분을 남깁니다. "
       "--no-masks 로 다시 실행해 확인해 보세요."),
    DE("Masken haben {0}% aller Merkmalspunkte entfernt. Sofern dies keine Aufnahme eines "
       "einzelnen Objekts vor maskiertem Hintergrund ist, sind die Masken vermutlich "
       "invertiert -- diese Pipeline behält wie COLMAP das WEISSE. Zur Probe mit --no-masks "
       "erneut ausführen."),
    FR("Les masques ont écarté {0}% de tous les points. À moins qu'il ne s'agisse d'un objet "
       "unique sur fond masqué, les masques sont probablement inversés -- cette chaîne "
       "conserve le BLANC, comme COLMAP. Relancez avec --no-masks pour vérifier."),
    ES("Las máscaras descartaron el {0}% de todos los puntos. Salvo que sea la captura de un "
       "solo objeto con el fondo enmascarado, es probable que las máscaras estén invertidas: "
       "esta cadena conserva lo BLANCO, como COLMAP. Vuelva a ejecutar con --no-masks para comprobarlo."),
    PT("As máscaras descartaram {0}% de todos os pontos. A menos que esta seja a captura de um "
       "único objeto com o fundo mascarado, as máscaras provavelmente estão invertidas -- este "
       "fluxo mantém o BRANCO, como o COLMAP. Execute de novo com --no-masks para conferir."),
    IT("Le maschere hanno scartato il {0}% di tutti i punti. A meno che non sia la ripresa di "
       "un solo oggetto su sfondo mascherato, le maschere sono probabilmente invertite: questa "
       "catena tiene il BIANCO, come COLMAP. Rilanci con --no-masks per verificare."),
    NL("Maskers lieten {0}% van alle kenmerkpunten vallen. Tenzij dit één object met een "
       "gemaskeerde achtergrond is, staan de maskers waarschijnlijk omgekeerd -- deze pijplijn "
       "houdt het WITTE, net als COLMAP. Voer opnieuw uit met --no-masks om dat te toetsen."),
    RU("Маски отсекли {0}% всех точек. Если это не съёмка одного объекта на замаскированном "
       "фоне, маски, скорее всего, инвертированы: этот конвейер сохраняет БЕЛОЕ, как COLMAP. "
       "Перезапустите с --no-masks, чтобы проверить."),
    TR("Maskeler tüm anahtar noktaların %{0} kadarını eledi. Bu, arka planı maskelenmiş tek bir "
       "nesnenin çekimi değilse maskeler büyük olasılıkla ters -- bu işlem hattı, COLMAP gibi, "
       "BEYAZ olanı tutar. Denetlemek için --no-masks ile yeniden çalıştırın."));

// ===========================================================================
// Matching and the camera grouping it settles
// ===========================================================================

SS_MSG(match_plan,
    EN("Images: {0}   Pairs: {1}   Pairing: {2}"),
    JA("画像: {0}   ペア: {1}   ペアの選び方: {2}"),
    ZH_HANS("图像: {0}   图像对: {1}   配对方式: {2}"),
    ZH_HANT("影像: {0}   影像對: {1}   配對方式: {2}"),
    KO("이미지: {0}   쌍: {1}   짝짓는 방식: {2}"),
    DE("Bilder: {0}   Paare: {1}   Paarbildung: {2}"),
    FR("Images : {0}   Paires : {1}   Appariement : {2}"),
    ES("Imágenes: {0}   Pares: {1}   Emparejamiento: {2}"),
    PT("Imagens: {0}   Pares: {1}   Pareamento: {2}"),
    IT("Immagini: {0}   Coppie: {1}   Accoppiamento: {2}"),
    NL("Afbeeldingen: {0}   Paren: {1}   Paarvorming: {2}"),
    RU("Изображений: {0}   Пар: {1}   Способ подбора пар: {2}"),
    TR("Görüntü: {0}   Çift: {1}   Çift seçimi: {2}"));

SS_MSG(match_camera_mode,
    EN("Camera grouping: {0}   Cameras: {1}   Images: {2}"),
    JA("カメラのまとめ方: {0}   カメラ: {1}   画像: {2}"),
    ZH_HANS("相机分组方式: {0}   相机数: {1}   图像: {2}"),
    ZH_HANT("相機分組方式: {0}   相機數: {1}   影像: {2}"),
    KO("카메라 묶는 방식: {0}   카메라: {1}   이미지: {2}"),
    DE("Kameragruppierung: {0}   Kameras: {1}   Bilder: {2}"),
    FR("Regroupement des caméras : {0}   Caméras : {1}   Images : {2}"),
    ES("Agrupación de cámaras: {0}   Cámaras: {1}   Imágenes: {2}"),
    PT("Agrupamento de câmeras: {0}   Câmeras: {1}   Imagens: {2}"),
    IT("Raggruppamento delle fotocamere: {0}   Fotocamere: {1}   Immagini: {2}"),
    NL("Cameragroepering: {0}   Camera's: {1}   Afbeeldingen: {2}"),
    RU("Группировка камер: {0}   Камер: {1}   Изображений: {2}"),
    TR("Kamera gruplaması: {0}   Kamera: {1}   Görüntü: {2}"));

SS_MSG(match_camera_line,
    EN("Camera {0}: images {1}, {2}x{3}, {4}, focal {5} px ({6})"),
    JA("カメラ {0}: 画像 {1}、{2}x{3}、{4}、焦点距離 {5} px（{6}）"),
    ZH_HANS("相机 {0}: 图像 {1}，{2}x{3}，{4}，焦距 {5} px（{6}）"),
    ZH_HANT("相機 {0}: 影像 {1}，{2}x{3}，{4}，焦距 {5} px（{6}）"),
    KO("카메라 {0}: 이미지 {1}, {2}x{3}, {4}, 초점거리 {5} px({6})"),
    DE("Kamera {0}: Bilder {1}, {2}x{3}, {4}, Brennweite {5} px ({6})"),
    FR("Caméra {0} : images {1}, {2}x{3}, {4}, focale {5} px ({6})"),
    ES("Cámara {0}: imágenes {1}, {2}x{3}, {4}, focal {5} px ({6})"),
    PT("Câmera {0}: imagens {1}, {2}x{3}, {4}, focal {5} px ({6})"),
    IT("Fotocamera {0}: immagini {1}, {2}x{3}, {4}, focale {5} px ({6})"),
    NL("Camera {0}: afbeeldingen {1}, {2}x{3}, {4}, brandpunt {5} px ({6})"),
    RU("Камера {0}: изображений {1}, {2}x{3}, {4}, фокус {5} px ({6})"),
    TR("Kamera {0}: görüntü {1}, {2}x{3}, {4}, odak {5} px ({6})"));

SS_MSG(focal_guessed,
    EN("guessed"),   JA("推定"),      ZH_HANS("推测"),   ZH_HANT("推測"),
    KO("추정"),       DE("geschätzt"), FR("estimée"),   ES("estimada"),
    PT("estimada"),  IT("stimata"),  NL("geschat"),    RU("оценка"),
    TR("tahmin"));

SS_MSG(focal_given,
    EN("given"),     JA("指定"),      ZH_HANS("指定"),   ZH_HANT("指定"),
    KO("지정"),       DE("angegeben"), FR("fournie"),   ES("indicada"),
    PT("informada"), IT("indicata"), NL("opgegeven"),  RU("задан"),
    TR("verilen"));

SS_MSG(focal_prior,
    EN("from EXIF"), JA("EXIFより"),  ZH_HANS("来自 EXIF"), ZH_HANT("來自 EXIF"),
    KO("EXIF에서"),  DE("aus EXIF"), FR("depuis EXIF"), ES("desde EXIF"),
    PT("do EXIF"),   IT("da EXIF"),  NL("uit EXIF"),   RU("из EXIF"),
    TR("EXIF'ten"));

SS_MSG(match_camera_mode_switched,
    EN("Distinct frame sizes: {0} over {1} images -- this is a photo collection "
       "rather than one camera's capture. --camera-mode folder overrides."),
    JA("フレームサイズの種類: {0}（画像 {1} 枚）。1台のカメラの撮影ではなく写真のコレクションです。"
       "--camera-mode folder で上書きできます。"),
    ZH_HANS("不同的画幅尺寸: {0} 种（共 {1} 张图像）——这是一批收集来的照片，而不是同一台相机的拍摄。"
            "可用 --camera-mode folder 覆盖。"),
    ZH_HANT("不同的畫幅尺寸: {0} 種（共 {1} 張影像）——這是一批收集來的相片，而不是同一台相機的拍攝。"
            "可用 --camera-mode folder 覆蓋。"),
    KO("서로 다른 프레임 크기: {0} 종(이미지 {1} 장) -- 한 대의 카메라 촬영이 아니라 사진 모음입니다. "
       "--camera-mode folder 로 덮어쓸 수 있습니다."),
    DE("Verschiedene Bildgrößen: {0} bei {1} Bildern -- das ist eine Fotosammlung und nicht "
       "die Aufnahme einer Kamera. --camera-mode folder setzt sich darüber hinweg."),
    FR("Tailles d'image distinctes : {0} sur {1} images -- il s'agit d'une collection de photos, "
       "pas de la prise de vue d'une seule caméra. --camera-mode folder passe outre."),
    ES("Tamaños de fotograma distintos: {0} en {1} imágenes: esto es una colección de fotos, "
       "no la captura de una sola cámara. --camera-mode folder lo anula."),
    PT("Tamanhos de quadro distintos: {0} em {1} imagens -- isto é uma coleção de fotos, "
       "não a captura de uma única câmera. --camera-mode folder sobrepõe isso."),
    IT("Dimensioni di fotogramma distinte: {0} su {1} immagini: è una raccolta di foto, "
       "non la ripresa di una sola fotocamera. --camera-mode folder ha la precedenza."),
    NL("Verschillende beeldformaten: {0} bij {1} afbeeldingen -- dit is een fotoverzameling "
       "en niet de opname van één camera. --camera-mode folder gaat hieroverheen."),
    RU("Различных размеров кадра: {0} на {1} изображений — это подборка фотографий, а не съёмка "
       "одной камерой. --camera-mode folder переопределяет это."),
    TR("Farklı kare boyutu: {0} adet, {1} görüntüde -- bu tek bir kameranın çekimi değil, "
       "bir fotoğraf derlemesi. --camera-mode folder bunu geçersiz kılar."));

SS_MSG(match_camera_size_split,
    EN("Camera groups split by frame size: {0} -- images of different sizes "
       "cannot share one camera."),
    JA("フレームサイズで分割したカメラのまとまり: {0}。サイズの違う画像は1台のカメラを"
       "共有できません。"),
    ZH_HANS("按画幅尺寸拆分的相机分组: {0}——尺寸不同的图像无法共用一台相机。"),
    ZH_HANT("按畫幅尺寸拆分的相機分組: {0}——尺寸不同的影像無法共用一台相機。"),
    KO("프레임 크기로 나뉜 카메라 묶음: {0} -- 크기가 다른 이미지는 카메라 하나를 "
       "함께 쓸 수 없습니다."),
    DE("Nach Bildgröße aufgeteilte Kameragruppen: {0} -- Bilder verschiedener "
       "Größe können sich keine Kamera teilen."),
    FR("Groupes de caméras séparés par taille d'image : {0} -- des images de "
       "tailles différentes ne peuvent pas partager une caméra."),
    ES("Grupos de cámaras separados por tamaño de fotograma: {0}: imágenes de "
       "distinto tamaño no pueden compartir una cámara."),
    PT("Grupos de câmeras separados por tamanho de quadro: {0} -- imagens de "
       "tamanhos diferentes não podem compartilhar uma câmera."),
    IT("Gruppi di fotocamere separati per dimensione del fotogramma: {0}: "
       "immagini di dimensioni diverse non possono condividere una fotocamera."),
    NL("Cameragroepen gesplitst op beeldformaat: {0} -- afbeeldingen van "
       "verschillend formaat kunnen geen camera delen."),
    RU("Групп камер, разделённых по размеру кадра: {0} — изображения разных "
       "размеров не могут использовать одну камеру."),
    TR("Kare boyutuna göre ayrılan kamera grubu sayısı: {0} -- farklı boyuttaki "
       "görüntüler tek bir kamerayı paylaşamaz."));

SS_MSG(match_exif_focals,
    EN("Images carrying an EXIF focal length: {0}/{1}"),
    JA("EXIFに焦点距離がある画像: {0}/{1}"),
    ZH_HANS("带有 EXIF 焦距的图像: {0}/{1}"),
    ZH_HANT("帶有 EXIF 焦距的影像: {0}/{1}"),
    KO("EXIF 초점거리가 있는 이미지: {0}/{1}"),
    DE("Bilder mit EXIF-Brennweite: {0}/{1}"),
    FR("Images portant une focale EXIF : {0}/{1}"),
    ES("Imágenes con focal EXIF: {0}/{1}"),
    PT("Imagens com focal EXIF: {0}/{1}"),
    IT("Immagini con focale EXIF: {0}/{1}"),
    NL("Afbeeldingen met EXIF-brandpuntsafstand: {0}/{1}"),
    RU("Изображений с фокусным расстоянием в EXIF: {0}/{1}"),
    TR("EXIF odak uzaklığı taşıyan görüntü: {0}/{1}"));

SS_MSG(match_exif_focals_ignored,
    EN("Images carrying an EXIF focal length: {0}/{1} (ignored, --no-exif-focal)"),
    JA("EXIFに焦点距離がある画像: {0}/{1}（--no-exif-focal のため無視）"),
    ZH_HANS("带有 EXIF 焦距的图像: {0}/{1}（因 --no-exif-focal 而忽略）"),
    ZH_HANT("帶有 EXIF 焦距的影像: {0}/{1}（因 --no-exif-focal 而忽略）"),
    KO("EXIF 초점거리가 있는 이미지: {0}/{1}(--no-exif-focal 이므로 무시)"),
    DE("Bilder mit EXIF-Brennweite: {0}/{1} (ignoriert, --no-exif-focal)"),
    FR("Images portant une focale EXIF : {0}/{1} (ignorée, --no-exif-focal)"),
    ES("Imágenes con focal EXIF: {0}/{1} (ignorada, --no-exif-focal)"),
    PT("Imagens com focal EXIF: {0}/{1} (ignorada, --no-exif-focal)"),
    IT("Immagini con focale EXIF: {0}/{1} (ignorata, --no-exif-focal)"),
    NL("Afbeeldingen met EXIF-brandpuntsafstand: {0}/{1} (genegeerd, --no-exif-focal)"),
    RU("Изображений с фокусным расстоянием в EXIF: {0}/{1} (игнорируется, --no-exif-focal)"),
    TR("EXIF odak uzaklığı taşıyan görüntü: {0}/{1} (--no-exif-focal ile yok sayıldı)"));

SS_MSG(match_more_cameras,
    EN("... and {0} more camera(s)"),
    JA("…ほかに {0} 台のカメラ"),
    ZH_HANS("…还有 {0} 台相机"),
    ZH_HANT("…還有 {0} 台相機"),
    KO("…그 외 카메라 {0} 대"),
    DE("… und {0} weitere Kamera(s)"),
    FR("… et {0} caméra(s) de plus"),
    ES("… y {0} cámara(s) más"),
    PT("… e mais {0} câmera(s)"),
    IT("… e altre {0} fotocamera/e"),
    NL("… en nog {0} camera('s)"),
    RU("… и ещё камер: {0}"),
    TR("… ve {0} kamera daha"));

SS_MSG(match_verifying,
    EN("Verifying on {0} thread(s)"),
    JA("{0} スレッドで検証しています"),
    ZH_HANS("正在用 {0} 个线程做几何验证"),
    ZH_HANT("正在用 {0} 個執行緒做幾何驗證"),
    KO("{0} 개 스레드로 검증하는 중"),
    DE("Prüfung mit {0} Thread(s)"),
    FR("Vérification sur {0} fil(s)"),
    ES("Verificando en {0} hilo(s)"),
    PT("Verificando em {0} thread(s)"),
    IT("Verifica su {0} thread"),
    NL("Verifiëren met {0} thread(s)"),
    RU("Проверка в потоках: {0}"),
    TR("{0} iş parçacığında doğrulanıyor"));

SS_MSG(match_progress,
    EN("{0}/{1} pairs matched"),
    JA("{0}/{1} ペアを照合しました"),
    ZH_HANS("已匹配 {0}/{1} 对"),
    ZH_HANT("已匹配 {0}/{1} 對"),
    KO("{0}/{1} 쌍 정합 완료"),
    DE("{0}/{1} Paare abgeglichen"),
    FR("{0}/{1} paires appariées"),
    ES("{0}/{1} pares emparejados"),
    PT("{0}/{1} pares pareados"),
    IT("{0}/{1} coppie abbinate"),
    NL("{0}/{1} paren gekoppeld"),
    RU("Сопоставлено пар: {0}/{1}"),
    TR("{0}/{1} çift eşleştirildi"));

SS_MSG(match_need_two,
    EN("At least 2 feature files are needed in {0}."),
    JA("{0} には特徴点ファイルが少なくとも2つ必要です。"),
    ZH_HANS("{0} 里至少需要 2 个特征文件。"),
    ZH_HANT("{0} 裡至少需要 2 個特徵檔案。"),
    KO("{0} 안에 특징 파일이 최소 2개 필요합니다."),
    DE("In {0} werden mindestens 2 Merkmalsdateien benötigt."),
    FR("Il faut au moins 2 fichiers de points dans {0}."),
    ES("Hacen falta al menos 2 archivos de puntos en {0}."),
    PT("São necessários pelo menos 2 arquivos de pontos em {0}."),
    IT("Servono almeno 2 file di punti in {0}."),
    NL("Er zijn minstens 2 kenmerkbestanden nodig in {0}."),
    RU("В {0} нужно не менее 2 файлов признаков."),
    TR("{0} içinde en az 2 öznitelik dosyası gerekli."));

SS_MSG(match_switch_to_selection,
    EN("Images: {0}, at or above the exhaustive cutoff -- switching from {1} pairing "
       "to content-based pair selection. --pairs {1} forces the old behaviour."),
    JA("画像 {0} 枚は総当たりの上限以上です。{1} のペア選択から内容に基づくペア選択に切り替えます。"
       "--pairs {1} を指定すると従来どおりになります。"),
    ZH_HANS("图像 {0} 张，达到或超过穷举上限——从 {1} 配对切换到基于内容的配对选择。"
            "指定 --pairs {1} 可保持原行为。"),
    ZH_HANT("影像 {0} 張，達到或超過窮舉上限——從 {1} 配對切換到基於內容的配對選擇。"
            "指定 --pairs {1} 可保持原行為。"),
    KO("이미지 {0} 장으로 전수 비교 한계 이상입니다. {1} 짝짓기에서 내용 기반 선택으로 바꿉니다. "
       "--pairs {1} 을(를) 주면 이전 동작을 유지합니다."),
    DE("Bilder: {0}, an oder über der Grenze für vollständige Paarbildung -- es wird von {1} "
       "auf inhaltsbasierte Paarauswahl umgestellt. --pairs {1} erzwingt das alte Verhalten."),
    FR("Images : {0}, au niveau ou au-delà du seuil exhaustif -- passage de l'appariement {1} "
       "à une sélection de paires fondée sur le contenu. --pairs {1} force l'ancien comportement."),
    ES("Imágenes: {0}, en el umbral exhaustivo o por encima: se pasa del emparejamiento {1} "
       "a una selección de pares basada en el contenido. --pairs {1} fuerza el comportamiento anterior."),
    PT("Imagens: {0}, no limite exaustivo ou acima dele -- passando do pareamento {1} para uma "
       "seleção de pares baseada no conteúdo. --pairs {1} força o comportamento antigo."),
    IT("Immagini: {0}, alla soglia esaustiva o oltre: si passa dall'accoppiamento {1} a una "
       "selezione di coppie basata sul contenuto. --pairs {1} impone il comportamento precedente."),
    NL("Afbeeldingen: {0}, op of boven de uitputtende drempel -- er wordt van {1}-paarvorming "
       "overgestapt op inhoudsgebaseerde paarselectie. --pairs {1} dwingt het oude gedrag af."),
    RU("Изображений: {0} — на пороге полного перебора или выше: переход от режима {1} к отбору "
       "пар по содержимому. --pairs {1} возвращает прежнее поведение."),
    TR("Görüntü: {0}, tam tarama eşiğinde veya üzerinde -- {1} çift seçiminden içerik temelli "
       "çift seçimine geçiliyor. --pairs {1} eski davranışı zorlar."));

SS_MSG(match_exhaustive_quadratic,
    EN("Images: {0} with exhaustive pairing means {1} pairs, which grows with the square "
       "of the image count. Drop --pairs exhaustive to get pair selection."),
    JA("画像 {0} 枚を総当たりでペアにすると {1} ペアになり、枚数の2乗で増えます。"
       "--pairs exhaustive を外すとペア選択になります。"),
    ZH_HANS("{0} 张图像做穷举配对会产生 {1} 对，数量随图像数的平方增长。去掉 --pairs exhaustive "
            "即可改用配对选择。"),
    ZH_HANT("{0} 張影像做窮舉配對會產生 {1} 對，數量隨影像數的平方成長。拿掉 --pairs exhaustive "
            "即可改用配對選擇。"),
    KO("이미지 {0} 장을 전수 비교하면 {1} 쌍이 되며, 장수의 제곱으로 늘어납니다. "
       "--pairs exhaustive 를 빼면 짝 선택을 씁니다."),
    DE("Bilder: {0} bei vollständiger Paarbildung ergibt {1} Paare und wächst quadratisch mit "
       "der Bildzahl. Lassen Sie --pairs exhaustive weg, um die Paarauswahl zu bekommen."),
    FR("Images : {0} en appariement exhaustif donne {1} paires, ce qui croît comme le carré du "
       "nombre d'images. Retirez --pairs exhaustive pour obtenir la sélection de paires."),
    ES("Imágenes: {0} con emparejamiento exhaustivo da {1} pares y crece con el cuadrado del "
       "número de imágenes. Quite --pairs exhaustive para usar la selección de pares."),
    PT("Imagens: {0} com pareamento exaustivo dá {1} pares e cresce com o quadrado do número de "
       "imagens. Remova --pairs exhaustive para usar a seleção de pares."),
    IT("Immagini: {0} con accoppiamento esaustivo dà {1} coppie e cresce col quadrato del numero "
       "di immagini. Tolga --pairs exhaustive per avere la selezione di coppie."),
    NL("Afbeeldingen: {0} met uitputtende paarvorming geeft {1} paren en groeit met het kwadraat "
       "van het aantal afbeeldingen. Laat --pairs exhaustive weg voor paarselectie."),
    RU("Изображений: {0} при полном переборе даёт {1} пар и растёт как квадрат числа изображений. "
       "Уберите --pairs exhaustive, чтобы включить отбор пар."),
    TR("{0} görüntüyle tam tarama {1} çift demektir ve görüntü sayısının karesiyle büyür. "
       "Çift seçimini kullanmak için --pairs exhaustive seçeneğini kaldırın."));

SS_MSG(focal_search,
    EN("Focal search over {0} pairs: {1} px (half-diagonal field of view {2} degrees)"),
    JA("{0} ペアで焦点距離を探索: {1} px（対角半分の画角 {2} 度）"),
    ZH_HANS("在 {0} 对图像上搜索焦距: {1} px（半对角视场 {2} 度）"),
    ZH_HANT("在 {0} 對影像上搜尋焦距: {1} px（半對角視場 {2} 度）"),
    KO("{0} 쌍에서 초점거리 탐색: {1} px(대각선 절반 화각 {2} 도)"),
    DE("Brennweitensuche über {0} Paare: {1} px (halbdiagonales Sichtfeld {2} Grad)"),
    FR("Recherche de focale sur {0} paires : {1} px (champ demi-diagonal {2} degrés)"),
    ES("Búsqueda de focal sobre {0} pares: {1} px (campo de visión semidiagonal {2} grados)"),
    PT("Busca de focal em {0} pares: {1} px (campo de visão semidiagonal {2} graus)"),
    IT("Ricerca della focale su {0} coppie: {1} px (campo visivo semidiagonale {2} gradi)"),
    NL("Brandpuntzoektocht over {0} paren: {1} px (halfdiagonaal gezichtsveld {2} graden)"),
    RU("Поиск фокуса по {0} парам: {1} px (полудиагональное поле зрения {2} градусов)"),
    TR("{0} çift üzerinde odak araması: {1} px (yarı köşegen görüş alanı {2} derece)"));


// ===========================================================================
// Mapping
// ===========================================================================

SS_MSG(map_feature_compaction,
    EN("Unused-feature compaction: features {0} -> {1}; removed {2} ({3}%); images {4}; "
       "zero-active images {5}; stored pairs {6}; correspondences {7}"),
    JA("未使用特徴点の整理: 特徴点 {0} -> {1}; 削除 {2} ({3}%); 画像 {4}; "
       "使用特徴点がない画像 {5}; 保存されたペア {6}; 対応点 {7}"),
    ZH_HANS("整理未用特征点: 特征点 {0} -> {1}; 移除 {2} ({3}%); 图像 {4}; "
            "使用特征点为零的图像 {5}; 已存储图像对 {6}; 对应关系 {7}"),
    ZH_HANT("整理未用特徵點: 特徵點 {0} -> {1}; 移除 {2} ({3}%); 影像 {4}; "
            "使用特徵點為零的影像 {5}; 已儲存影像對 {6}; 對應關係 {7}"),
    KO("미사용 특징점 정리: 특징점 {0} -> {1}; 제거 {2} ({3}%); 이미지 {4}; "
       "사용 특징점이 없는 이미지 {5}; 저장된 쌍 {6}; 대응점 {7}"),
    DE("Komprimierung ungenutzter Merkmale: Merkmale {0} -> {1}; entfernt {2} ({3}%); "
       "Bilder {4}; Bilder ohne aktive Merkmale {5}; gespeicherte Paare {6}; "
       "Korrespondenzen {7}"),
    FR("Compactage des points inutilisés : points {0} -> {1} ; retirés {2} ({3} %) ; "
       "images {4} ; images sans point actif {5} ; paires stockées {6} ; "
       "correspondances {7}"),
    ES("Compactación de puntos no usados: puntos {0} -> {1}; eliminados {2} ({3}%); "
       "imágenes {4}; imágenes sin puntos activos {5}; pares almacenados {6}; "
       "correspondencias {7}"),
    PT("Compactação de pontos não usados: pontos {0} -> {1}; removidos {2} ({3}%); "
       "imagens {4}; imagens sem pontos ativos {5}; pares armazenados {6}; "
       "correspondências {7}"),
    IT("Compattazione dei punti inutilizzati: punti {0} -> {1}; rimossi {2} ({3}%); "
       "immagini {4}; immagini senza punti attivi {5}; coppie memorizzate {6}; "
       "corrispondenze {7}"),
    NL("Compactie van ongebruikte kenmerken: kenmerken {0} -> {1}; verwijderd {2} ({3}%); "
       "afbeeldingen {4}; afbeeldingen zonder actieve kenmerken {5}; opgeslagen paren {6}; "
       "overeenkomsten {7}"),
    RU("Сжатие неиспользуемых признаков: признаки {0} -> {1}; удалено {2} ({3}%); "
       "изображения {4}; изображения без активных признаков {5}; сохранённые пары {6}; "
       "соответствия {7}"),
    TR("Kullanılmayan öznitelik sıkıştırması: öznitelikler {0} -> {1}; kaldırılan {2} "
       "({3}%); görüntüler {4}; etkin özniteliği olmayan görüntüler {5}; saklanan çiftler "
       "{6}; eşleşmeler {7}"));

SS_MSG(map_seed_relax,
    EN("No seed pair passed the current thresholds; relaxing to inliers {0}, angle {1} degrees"),
    JA("現在のしきい値では初期ペアが見つかりません。インライア {0}、角度 {1} 度まで緩めます"),
    ZH_HANS("当前阈值下没有可用的初始配对; 放宽到内点 {0}、角度 {1} 度"),
    ZH_HANT("目前門檻下沒有可用的初始配對; 放寬到內點 {0}、角度 {1} 度"),
    KO("현재 임계값으로는 초기 쌍이 없습니다. 인라이어 {0}, 각도 {1} 도로 완화합니다"),
    DE("Kein Startpaar bei den aktuellen Schwellen; gelockert auf Inlier {0}, Winkel {1} Grad"),
    FR("Aucune paire d'amorçage ne passe les seuils actuels ; assouplissement à inliers {0}, angle {1} degrés"),
    ES("Ninguna pareja inicial supera los umbrales actuales; se relajan a inliers {0}, ángulo {1} grados"),
    PT("Nenhum par inicial passou nos limiares atuais; afrouxando para inliers {0}, ângulo {1} graus"),
    IT("Nessuna coppia iniziale supera le soglie attuali; si allentano a inlier {0}, angolo {1} gradi"),
    NL("Geen startpaar haalde de huidige drempels; versoepeld naar inliers {0}, hoek {1} graden"),
    RU("Ни одна стартовая пара не прошла текущие пороги; ослабляем до инлаеров {0}, угол {1} градусов"),
    TR("Geçerli eşiklerde başlangıç çifti yok; içeriler {0}, açı {1} dereceye gevşetiliyor"));

SS_MSG(map_seed_relax_forward,
    EN("No seed pair passed the current thresholds; relaxing to inliers {0}, angle {1} degrees, "
       "forward motion allowed"),
    JA("現在のしきい値では初期ペアが見つかりません。インライア {0}、角度 {1} 度まで緩め、"
       "前進方向の動きも許可します"),
    ZH_HANS("当前阈值下没有可用的初始配对; 放宽到内点 {0}、角度 {1} 度，并允许前向运动"),
    ZH_HANT("目前門檻下沒有可用的初始配對; 放寬到內點 {0}、角度 {1} 度，並允許前向運動"),
    KO("현재 임계값으로는 초기 쌍이 없습니다. 인라이어 {0}, 각도 {1} 도로 완화하고 전진 운동도 허용합니다"),
    DE("Kein Startpaar bei den aktuellen Schwellen; gelockert auf Inlier {0}, Winkel {1} Grad, "
       "Vorwärtsbewegung erlaubt"),
    FR("Aucune paire d'amorçage ne passe les seuils actuels ; assouplissement à inliers {0}, "
       "angle {1} degrés, mouvement vers l'avant autorisé"),
    ES("Ninguna pareja inicial supera los umbrales actuales; se relajan a inliers {0}, ángulo {1} "
       "grados, con movimiento hacia delante permitido"),
    PT("Nenhum par inicial passou nos limiares atuais; afrouxando para inliers {0}, ângulo {1} "
       "graus, com movimento para a frente permitido"),
    IT("Nessuna coppia iniziale supera le soglie attuali; si allentano a inlier {0}, angolo {1} "
       "gradi, con moto in avanti ammesso"),
    NL("Geen startpaar haalde de huidige drempels; versoepeld naar inliers {0}, hoek {1} graden, "
       "voorwaartse beweging toegestaan"),
    RU("Ни одна стартовая пара не прошла текущие пороги; ослабляем до инлаеров {0}, угол {1} "
       "градусов, движение вперёд разрешено"),
    TR("Geçerli eşiklerde başlangıç çifti yok; içeriler {0}, açı {1} dereceye gevşetiliyor, "
       "ileri hareket serbest"));

SS_MSG(map_init_pair,
    EN("Seed pair ({0},{1}): points {2}, median angle {3} degrees, baseline {4}"),
    JA("初期ペア ({0},{1}): 点 {2}、角度の中央値 {3} 度、基線 {4}"),
    ZH_HANS("初始配对 ({0},{1}): 点数 {2}，角度中位数 {3} 度，基线 {4}"),
    ZH_HANT("初始配對 ({0},{1}): 點數 {2}，角度中位數 {3} 度，基線 {4}"),
    KO("초기 쌍 ({0},{1}): 점 {2}, 각도 중앙값 {3} 도, 기선 {4}"),
    DE("Startpaar ({0},{1}): Punkte {2}, Medianwinkel {3} Grad, Basislinie {4}"),
    FR("Paire d'amorçage ({0},{1}) : points {2}, angle médian {3} degrés, base {4}"),
    ES("Pareja inicial ({0},{1}): puntos {2}, ángulo mediano {3} grados, línea base {4}"),
    PT("Par inicial ({0},{1}): pontos {2}, ângulo mediano {3} graus, linha de base {4}"),
    IT("Coppia iniziale ({0},{1}): punti {2}, angolo mediano {3} gradi, base {4}"),
    NL("Startpaar ({0},{1}): punten {2}, mediane hoek {3} graden, basislijn {4}"),
    RU("Стартовая пара ({0},{1}): точек {2}, медианный угол {3} градусов, база {4}"),
    TR("Başlangıç çifti ({0},{1}): nokta {2}, ortanca açı {3} derece, taban çizgisi {4}"));

SS_MSG(baseline_forward,
    EN("forward"),   JA("前進"),      ZH_HANS("前向"),   ZH_HANT("前向"),
    KO("전진"),       DE("vorwärts"), FR("avant"),      ES("hacia delante"),
    PT("para a frente"), IT("in avanti"), NL("voorwaarts"), RU("вперёд"),
    TR("ileri"));

SS_MSG(baseline_sideways,
    EN("sideways"),  JA("横移動"),    ZH_HANS("横向"),   ZH_HANT("橫向"),
    KO("옆으로"),     DE("seitwärts"), FR("latérale"),  ES("lateral"),
    PT("lateral"),   IT("laterale"), NL("zijwaarts"),  RU("вбок"),
    TR("yana"));

SS_MSG(map_global_ba,
    EN("Global bundle adjustment (cost {0}): filtered {1} observations / {2} points, {3} points remain"),
    JA("全体バンドル調整（コスト {0}）: 観測 {1} 個 / 点 {2} 個を除外、残り {3} 点"),
    ZH_HANS("全局光束法平差（代价 {0}）: 剔除观测 {1} 个 / 点 {2} 个，剩余 {3} 点"),
    ZH_HANT("全域光束法平差（代價 {0}）: 剔除觀測 {1} 個 / 點 {2} 個，剩餘 {3} 點"),
    KO("전역 번들 조정(비용 {0}): 관측 {1} 개 / 점 {2} 개 제거, {3} 점 남음"),
    DE("Globale Bündelausgleichung (Kosten {0}): {1} Beobachtungen / {2} Punkte gefiltert, {3} Punkte bleiben"),
    FR("Ajustement de faisceaux global (coût {0}) : {1} observations / {2} points filtrés, {3} points restants"),
    ES("Ajuste de haces global (coste {0}): filtradas {1} observaciones / {2} puntos, quedan {3} puntos"),
    PT("Ajustamento de feixes global (custo {0}): filtradas {1} observações / {2} pontos, restam {3} pontos"),
    IT("Bundle adjustment globale (costo {0}): filtrate {1} osservazioni / {2} punti, restano {3} punti"),
    NL("Globale bundelaanpassing (kosten {0}): {1} waarnemingen / {2} punten gefilterd, {3} punten over"),
    RU("Глобальное уравнивание связок (стоимость {0}): отсеяно наблюдений {1} / точек {2}, осталось точек: {3}"),
    TR("Genel demet düzeltmesi (maliyet {0}): {1} gözlem / {2} nokta elendi, {3} nokta kaldı"));

SS_MSG(ba_host_fallback,
    EN("Bundle adjustment could not finish on the GPU ({0}). Running it on the CPU instead: "
       "slower, but it finishes. --ba-real cpu --ba-real-coarse cpu starts there next time."),
    JA("バンドル調整を GPU で完了できませんでした（{0}）。代わりに CPU で実行します。"
       "遅くなりますが最後まで進みます。次回から --ba-real cpu --ba-real-coarse cpu を渡すと"
       "最初から CPU で計算します。"),
    ZH_HANS("光束法平差无法在 GPU 上完成（{0}）。改在 CPU 上运行: 更慢，但能跑完。"
            "下次加上 --ba-real cpu --ba-real-coarse cpu 就直接从 CPU 开始。"),
    ZH_HANT("光束法平差無法在 GPU 上完成（{0}）。改在 CPU 上執行: 更慢，但能跑完。"
            "下次加上 --ba-real cpu --ba-real-coarse cpu 就直接從 CPU 開始。"),
    KO("번들 조정을 GPU에서 끝내지 못했습니다({0}). 대신 CPU에서 실행합니다. 느리지만 "
       "끝까지 갑니다. 다음부터는 --ba-real cpu --ba-real-coarse cpu 로 처음부터 CPU에서 "
       "계산합니다."),
    DE("Die Bündelausgleichung konnte auf der GPU nicht abgeschlossen werden ({0}). Sie läuft "
       "stattdessen auf der CPU: langsamer, aber sie kommt zu Ende. Mit --ba-real cpu "
       "--ba-real-coarse cpu beginnt sie beim nächsten Mal dort."),
    FR("L'ajustement de faisceaux n'a pas pu se terminer sur le GPU ({0}). Il tourne sur le CPU "
       "à la place : plus lent, mais il aboutit. --ba-real cpu --ba-real-coarse cpu y commence "
       "la prochaine fois."),
    ES("El ajuste de haces no pudo terminar en la GPU ({0}). Se ejecuta en la CPU: más lento, "
       "pero termina. Con --ba-real cpu --ba-real-coarse cpu empieza allí la próxima vez."),
    PT("O ajustamento de feixes não conseguiu terminar na GPU ({0}). Está rodando na CPU: mais "
       "lento, mas termina. Com --ba-real cpu --ba-real-coarse cpu ele começa aí da próxima vez."),
    IT("Il bundle adjustment non è riuscito a finire sulla GPU ({0}). Ora gira sulla CPU: più "
       "lento, ma arriva in fondo. Con --ba-real cpu --ba-real-coarse cpu parte da lì la "
       "prossima volta."),
    NL("De bundelaanpassing kon niet op de GPU worden afgemaakt ({0}). Hij draait nu op de CPU: "
       "langzamer, maar hij komt klaar. Met --ba-real cpu --ba-real-coarse cpu begint hij daar "
       "de volgende keer."),
    RU("Уравнивание связок не удалось завершить на GPU ({0}). Оно выполняется на CPU: медленнее, "
       "но доходит до конца. С --ba-real cpu --ba-real-coarse cpu оно сразу начнётся там в "
       "следующий раз."),
    TR("Demet düzeltmesi GPU'da tamamlanamadı ({0}). Bunun yerine CPU'da çalışıyor: daha yavaş "
       "ama bitiyor. --ba-real cpu --ba-real-coarse cpu bir dahaki sefere doğrudan orada "
       "başlatır."));

SS_MSG(map_registered,
    EN("Registered image {0} (PnP inliers {1}/{2}); images in the model: {3}"),
    JA("画像 {0} を登録（PnPインライア {1}/{2}）。モデル内の画像: {3}"),
    ZH_HANS("已配准图像 {0}（PnP 内点 {1}/{2}）; 模型中的图像: {3}"),
    ZH_HANT("已註冊影像 {0}（PnP 內點 {1}/{2}）; 模型中的影像: {3}"),
    KO("이미지 {0} 등록(PnP 인라이어 {1}/{2}). 모델 안의 이미지: {3}"),
    DE("Bild {0} registriert (PnP-Inlier {1}/{2}); Bilder im Modell: {3}"),
    FR("Image {0} enregistrée (inliers PnP {1}/{2}) ; images dans le modèle : {3}"),
    ES("Imagen {0} registrada (inliers PnP {1}/{2}); imágenes en el modelo: {3}"),
    PT("Imagem {0} registrada (inliers PnP {1}/{2}); imagens no modelo: {3}"),
    IT("Immagine {0} registrata (inlier PnP {1}/{2}); immagini nel modello: {3}"),
    NL("Afbeelding {0} geregistreerd (PnP-inliers {1}/{2}); afbeeldingen in het model: {3}"),
    RU("Изображение {0} зарегистрировано (инлаеры PnP {1}/{2}); изображений в модели: {3}"),
    TR("{0} numaralı görüntü kaydedildi (PnP içerileri {1}/{2}); modeldeki görüntü: {3}"));

SS_MSG(map_camera_focal,
    EN("Camera {0}: focal {1} -> {2} px (searched and refined, {3} inliers)"),
    JA("カメラ {0}: 焦点距離 {1} -> {2} px（探索と微調整、インライア {3}）"),
    ZH_HANS("相机 {0}: 焦距 {1} -> {2} px（搜索并精化，内点 {3}）"),
    ZH_HANT("相機 {0}: 焦距 {1} -> {2} px（搜尋並精化，內點 {3}）"),
    KO("카메라 {0}: 초점거리 {1} -> {2} px(탐색 후 보정, 인라이어 {3})"),
    DE("Kamera {0}: Brennweite {1} -> {2} px (gesucht und verfeinert, {3} Inlier)"),
    FR("Caméra {0} : focale {1} -> {2} px (recherchée puis affinée, {3} inliers)"),
    ES("Cámara {0}: focal {1} -> {2} px (buscada y refinada, {3} inliers)"),
    PT("Câmera {0}: focal {1} -> {2} px (buscada e refinada, {3} inliers)"),
    IT("Fotocamera {0}: focale {1} -> {2} px (cercata e affinata, {3} inlier)"),
    NL("Camera {0}: brandpunt {1} -> {2} px (gezocht en verfijnd, {3} inliers)"),
    RU("Камера {0}: фокус {1} -> {2} px (найден и уточнён, инлаеров {3})"),
    TR("Kamera {0}: odak {1} -> {2} px (arandı ve iyileştirildi, {3} içeri)"));

SS_MSG(map_runaway_params,
    EN("Camera {0}: reset {1} runaway parameter(s)"),
    JA("カメラ {0}: 発散したパラメータ {1} 個をリセットしました"),
    ZH_HANS("相机 {0}: 重置了 {1} 个发散的参数"),
    ZH_HANT("相機 {0}: 重設了 {1} 個發散的參數"),
    KO("카메라 {0}: 발산한 파라미터 {1} 개를 초기화했습니다"),
    DE("Kamera {0}: {1} entlaufene(r) Parameter zurückgesetzt"),
    FR("Caméra {0} : {1} paramètre(s) divergent(s) réinitialisé(s)"),
    ES("Cámara {0}: se reiniciaron {1} parámetro(s) desbocado(s)"),
    PT("Câmera {0}: {1} parâmetro(s) descontrolado(s) reiniciado(s)"),
    IT("Fotocamera {0}: reimpostati {1} parametro/i fuori controllo"),
    NL("Camera {0}: {1} op hol geslagen parameter(s) hersteld"),
    RU("Камера {0}: сброшено разошедшихся параметров: {1}"),
    TR("Kamera {0}: {1} kaçak parametre sıfırlandı"));

SS_MSG(map_model_too_small,
    EN("The model is too small ({0} images, under the {1} needed; {2} covered by every "
       "attempt so far). Retrying from another seed."),
    JA("モデルが小さすぎます（画像 {0} 枚、必要な {1} 枚未満。これまでの試行で {2} 枚をカバー）。"
       "別の初期ペアからやり直します。"),
    ZH_HANS("模型太小（图像 {0} 张，少于所需的 {1} 张; 到目前为止所有尝试共覆盖 {2} 张）。"
            "将从另一个初始配对重试。"),
    ZH_HANT("模型太小（影像 {0} 張，少於所需的 {1} 張; 到目前為止所有嘗試共涵蓋 {2} 張）。"
            "將從另一個初始配對重試。"),
    KO("모델이 너무 작습니다(이미지 {0} 장으로 필요한 {1} 장 미만. 지금까지의 시도로 {2} 장 포함). "
       "다른 초기 쌍에서 다시 시도합니다."),
    DE("Das Modell ist zu klein ({0} Bilder, unter den nötigen {1}; {2} von allen bisherigen "
       "Versuchen abgedeckt). Neuer Versuch mit einem anderen Startpaar."),
    FR("Le modèle est trop petit ({0} images, sous les {1} requises ; {2} couvertes par toutes "
       "les tentatives jusqu'ici). Nouvel essai depuis une autre amorce."),
    ES("El modelo es demasiado pequeño ({0} imágenes, por debajo de las {1} necesarias; {2} "
       "cubiertas por todos los intentos hasta ahora). Se reintenta desde otra semilla."),
    PT("O modelo é pequeno demais ({0} imagens, abaixo das {1} necessárias; {2} cobertas por "
       "todas as tentativas até agora). Tentando de novo a partir de outra semente."),
    IT("Il modello è troppo piccolo ({0} immagini, sotto le {1} necessarie; {2} coperte da tutti "
       "i tentativi finora). Si riprova da un'altra coppia iniziale."),
    NL("Het model is te klein ({0} afbeeldingen, onder de benodigde {1}; {2} gedekt door alle "
       "pogingen tot nu toe). Opnieuw proberen vanaf een ander startpaar."),
    RU("Модель слишком мала ({0} изображений при необходимых {1}; всеми попытками охвачено {2}). "
       "Пробуем с другой стартовой пары."),
    TR("Model çok küçük ({0} görüntü, gereken {1} altında; şimdiye dek tüm denemeler {2} görüntüyü "
       "kapsadı). Başka bir başlangıç çiftinden yeniden denenecek."));

SS_MSG(map_done,
    EN("Models: {0}, covering {1}/{2} distinct images"),
    JA("モデル: {0} 個、異なる画像 {1}/{2} 枚をカバー"),
    ZH_HANS("模型: {0} 个，覆盖不同图像 {1}/{2} 张"),
    ZH_HANT("模型: {0} 個，涵蓋不同影像 {1}/{2} 張"),
    KO("모델: {0} 개, 서로 다른 이미지 {1}/{2} 장 포함"),
    DE("Modelle: {0}, decken {1}/{2} verschiedene Bilder ab"),
    FR("Modèles : {0}, couvrant {1}/{2} images distinctes"),
    ES("Modelos: {0}, que cubren {1}/{2} imágenes distintas"),
    PT("Modelos: {0}, cobrindo {1}/{2} imagens distintas"),
    IT("Modelli: {0}, che coprono {1}/{2} immagini distinte"),
    NL("Modellen: {0}, die {1}/{2} verschillende afbeeldingen dekken"),
    RU("Моделей: {0}, охвачено различных изображений: {1}/{2}"),
    TR("Model: {0}, {1}/{2} ayrı görüntüyü kapsıyor"));

SS_MSG(map_model_line,
    EN("Model {0}: images {1}, points {2}"),
    JA("モデル {0}: 画像 {1}、点 {2}"),
    ZH_HANS("模型 {0}: 图像 {1}，点 {2}"),
    ZH_HANT("模型 {0}: 影像 {1}，點 {2}"),
    KO("모델 {0}: 이미지 {1}, 점 {2}"),
    DE("Modell {0}: Bilder {1}, Punkte {2}"),
    FR("Modèle {0} : images {1}, points {2}"),
    ES("Modelo {0}: imágenes {1}, puntos {2}"),
    PT("Modelo {0}: imagens {1}, pontos {2}"),
    IT("Modello {0}: immagini {1}, punti {2}"),
    NL("Model {0}: afbeeldingen {1}, punten {2}"),
    RU("Модель {0}: изображений {1}, точек {2}"),
    TR("Model {0}: görüntü {1}, nokta {2}"));

SS_MSG(map_model_line_error,
    EN("Model {0}: images {1}, points {2}, mean reprojection {3} px"),
    JA("モデル {0}: 画像 {1}、点 {2}、再投影誤差の平均 {3} px"),
    ZH_HANS("模型 {0}: 图像 {1}，点 {2}，平均重投影误差 {3} px"),
    ZH_HANT("模型 {0}: 影像 {1}，點 {2}，平均重投影誤差 {3} px"),
    KO("모델 {0}: 이미지 {1}, 점 {2}, 평균 재투영 오차 {3} px"),
    DE("Modell {0}: Bilder {1}, Punkte {2}, mittlerer Rückprojektionsfehler {3} px"),
    FR("Modèle {0} : images {1}, points {2}, reprojection moyenne {3} px"),
    ES("Modelo {0}: imágenes {1}, puntos {2}, reproyección media {3} px"),
    PT("Modelo {0}: imagens {1}, pontos {2}, reprojeção média {3} px"),
    IT("Modello {0}: immagini {1}, punti {2}, riproiezione media {3} px"),
    NL("Model {0}: afbeeldingen {1}, punten {2}, gemiddelde herprojectie {3} px"),
    RU("Модель {0}: изображений {1}, точек {2}, средняя перепроекция {3} px"),
    TR("Model {0}: görüntü {1}, nokta {2}, ortalama yeniden izdüşüm {3} px"));

SS_MSG(map_wrote_model,
    EN("Wrote model {0} ({1} images, {2} points) to {3}"),
    JA("モデル {0}（画像 {1}、点 {2}）を {3} に書き出しました"),
    ZH_HANS("已把模型 {0}（图像 {1}，点 {2}）写入 {3}"),
    ZH_HANT("已把模型 {0}（影像 {1}，點 {2}）寫入 {3}"),
    KO("모델 {0}(이미지 {1}, 점 {2})을 {3} 에 썼습니다"),
    DE("Modell {0} ({1} Bilder, {2} Punkte) nach {3} geschrieben"),
    FR("Modèle {0} ({1} images, {2} points) écrit dans {3}"),
    ES("Modelo {0} ({1} imágenes, {2} puntos) escrito en {3}"),
    PT("Modelo {0} ({1} imagens, {2} pontos) gravado em {3}"),
    IT("Modello {0} ({1} immagini, {2} punti) scritto in {3}"),
    NL("Model {0} ({1} afbeeldingen, {2} punten) geschreven naar {3}"),
    RU("Модель {0} ({1} изображений, {2} точек) записана в {3}"),
    TR("Model {0} ({1} görüntü, {2} nokta) {3} konumuna yazıldı"));

SS_MSG(map_removed_stale,
    EN("Removed {0}, a model directory left by an earlier run"),
    JA("以前の実行が残したモデルのディレクトリ {0} を削除しました"),
    ZH_HANS("已删除 {0}，那是上一次运行留下的模型目录"),
    ZH_HANT("已刪除 {0}，那是上一次執行留下的模型目錄"),
    KO("이전 실행이 남긴 모델 디렉터리 {0} 을(를) 삭제했습니다"),
    DE("{0} entfernt -- ein Modellverzeichnis aus einem früheren Lauf"),
    FR("{0} supprimé : un répertoire de modèle laissé par une exécution précédente"),
    ES("Se eliminó {0}, un directorio de modelo dejado por una ejecución anterior"),
    PT("Removido {0}, um diretório de modelo deixado por uma execução anterior"),
    IT("Rimosso {0}, una directory di modello lasciata da un'esecuzione precedente"),
    NL("{0} verwijderd: een modelmap uit een eerdere run"),
    RU("Удалён {0} — каталог модели, оставшийся от прошлого запуска"),
    TR("Önceki bir çalışmadan kalan model dizini {0} kaldırıldı"));

SS_MSG(map_pp_skipped,
    EN("Camera groups: {0} -- skipping the final principal-point pass, which would move them apart"),
    JA("カメラのグループ: {0} 個。最後の主点調整は各グループを引き離すため省略します"),
    ZH_HANS("相机分组: {0} 组——跳过最后的主点优化，它会把这些分组拉开"),
    ZH_HANT("相機分組: {0} 組——略過最後的主點最佳化，它會把這些分組拉開"),
    KO("카메라 그룹: {0} 개 -- 마지막 주점 보정은 그룹을 서로 벌려 놓으므로 건너뜁니다"),
    DE("Kameragruppen: {0} -- der abschließende Hauptpunkt-Durchgang entfiele, da er sie auseinandertreiben würde"),
    FR("Groupes de caméras : {0} -- la passe finale sur le point principal est omise, elle les écarterait"),
    ES("Grupos de cámaras: {0}: se omite la pasada final del punto principal, que los separaría"),
    PT("Grupos de câmeras: {0} -- pulando a passagem final do ponto principal, que os afastaria"),
    IT("Gruppi di fotocamere: {0}: si salta la passata finale sul punto principale, che li allontanerebbe"),
    NL("Cameragroepen: {0} -- de laatste hoofdpuntronde wordt overgeslagen; die zou ze uit elkaar drijven"),
    RU("Групп камер: {0} — финальный проход по главной точке пропущен: он развёл бы их"),
    TR("Kamera grubu: {0} -- son ana nokta geçişi atlanıyor; grupları birbirinden uzaklaştırırdı"));

SS_MSG(map_final_intrinsics,
    EN("Final intrinsics refinement over {0} model(s): {1} s"),
    JA("{0} 個のモデルに対する最後の内部パラメータ調整: {1} 秒"),
    ZH_HANS("对 {0} 个模型做最后的内参优化: {1} 秒"),
    ZH_HANT("對 {0} 個模型做最後的內參最佳化: {1} 秒"),
    KO("모델 {0} 개에 대한 마지막 내부 파라미터 보정: {1} 초"),
    DE("Abschließende Verfeinerung der inneren Orientierung über {0} Modell(e): {1} s"),
    FR("Affinage final des paramètres internes sur {0} modèle(s) : {1} s"),
    ES("Refinamiento final de los parámetros internos sobre {0} modelo(s): {1} s"),
    PT("Refinamento final dos parâmetros internos em {0} modelo(s): {1} s"),
    IT("Affinamento finale dei parametri interni su {0} modello/i: {1} s"),
    NL("Laatste verfijning van de interne parameters over {0} model(len): {1} s"),
    RU("Финальное уточнение внутренних параметров по {0} моделям: {1} с"),
    TR("{0} model üzerinde son iç parametre iyileştirmesi: {1} s"));

SS_MSG(map_per_image_done,
    EN("Per-image intrinsics refinement over {0} model(s): {1} s"),
    JA("{0} 個のモデルに対する画像ごとの内部パラメータ調整: {1} 秒"),
    ZH_HANS("对 {0} 个模型做逐图像内参优化: {1} 秒"),
    ZH_HANT("對 {0} 個模型做逐影像內參最佳化: {1} 秒"),
    KO("모델 {0} 개에 대한 이미지별 내부 파라미터 보정: {1} 초"),
    DE("Verfeinerung der inneren Orientierung je Bild über {0} Modell(e): {1} s"),
    FR("Affinage des paramètres internes par image sur {0} modèle(s) : {1} s"),
    ES("Refinamiento de los parámetros internos por imagen sobre {0} modelo(s): {1} s"),
    PT("Refinamento dos parâmetros internos por imagem em {0} modelo(s): {1} s"),
    IT("Affinamento dei parametri interni per immagine su {0} modello/i: {1} s"),
    NL("Verfijning van de interne parameters per beeld over {0} model(len): {1} s"),
    RU("Уточнение внутренних параметров по кадрам, по {0} моделям: {1} с"),
    TR("{0} model üzerinde görüntü başına iç parametre iyileştirmesi: {1} s"));

SS_MSG(map_init_failed,
    EN("Initialization failed: {0} candidate pair(s) tried, best median triangulation "
       "angle {1} degrees. Nothing has enough parallax to triangulate on -- the camera "
       "may not have moved, or every pair may be too similar."),
    JA("初期化に失敗しました: 候補ペア {0} 組を試し、三角測量角の中央値は最大 {1} 度でした。"
       "視差が足りず三角測量できません。カメラが動いていないか、どのペアも似すぎている可能性があります。"),
    ZH_HANS("初始化失败: 尝试了 {0} 组候选配对，最佳三角化角度中位数为 {1} 度。视差不足，无法三角化——"
            "可能相机没有移动，或者每一对都太相似。"),
    ZH_HANT("初始化失敗: 嘗試了 {0} 組候選配對，最佳三角化角度中位數為 {1} 度。視差不足，無法三角化——"
            "可能相機沒有移動，或者每一對都太相似。"),
    KO("초기화에 실패했습니다: 후보 쌍 {0} 개를 시도했고 최고 삼각측량 각도 중앙값은 {1} 도였습니다. "
       "시차가 부족해 삼각측량을 할 수 없습니다. 카메라가 움직이지 않았거나 모든 쌍이 너무 비슷할 수 있습니다."),
    DE("Initialisierung fehlgeschlagen: {0} Kandidatenpaar(e) versucht, bester medianer "
       "Triangulationswinkel {1} Grad. Nirgends genug Parallaxe zum Triangulieren -- die Kamera "
       "hat sich vielleicht nicht bewegt, oder alle Paare sind zu ähnlich."),
    FR("Échec de l'initialisation : {0} paire(s) candidate(s) essayée(s), meilleur angle de "
       "triangulation médian {1} degrés. Pas assez de parallaxe pour trianguler -- la caméra n'a "
       "peut-être pas bougé, ou toutes les paires se ressemblent trop."),
    ES("Falló la inicialización: se probaron {0} pareja(s) candidata(s), mejor ángulo de "
       "triangulación mediano {1} grados. No hay paralaje suficiente para triangular: puede que "
       "la cámara no se moviera, o que todas las parejas sean demasiado parecidas."),
    PT("A inicialização falhou: {0} par(es) candidato(s) tentado(s), melhor ângulo de "
       "triangulação mediano {1} graus. Não há paralaxe suficiente para triangular -- talvez a "
       "câmera não tenha se movido, ou todos os pares sejam parecidos demais."),
    IT("Inizializzazione fallita: provate {0} coppie candidate, miglior angolo di triangolazione "
       "mediano {1} gradi. Non c'è parallasse sufficiente per triangolare: forse la fotocamera "
       "non si è mossa, o tutte le coppie sono troppo simili."),
    NL("Initialisatie mislukt: {0} kandidaatpa(a)r(en) geprobeerd, beste mediane "
       "triangulatiehoek {1} graden. Nergens genoeg parallax om te trianguleren -- misschien "
       "bewoog de camera niet, of lijken alle paren te veel op elkaar."),
    RU("Инициализация не удалась: испробовано пар-кандидатов: {0}, лучший медианный угол "
       "триангуляции {1} градусов. Параллакса не хватает для триангуляции — возможно, камера не "
       "двигалась или все пары слишком похожи."),
    TR("Başlatma başarısız: {0} aday çift denendi, en iyi ortanca üçgenleme açısı {1} derece. "
       "Üçgenlemeye yetecek paralaks yok -- kamera hiç hareket etmemiş ya da bütün çiftler "
       "birbirine fazla benziyor olabilir."));

SS_MSG(map_assembled,
    EN("Assembly: {0} s   Models: {1} -> {2} over {3} level(s)   Merged: {4}   Refused: {5}   "
       "Grown: {6} image(s)   Coverage: {7} -> {8} images"),
    JA("組み立て: {0} 秒   モデル: {1} -> {2}（{3} 段階）   統合: {4}   却下: {5}   "
       "追加登録: {6} 枚   カバー: {7} -> {8} 枚"),
    ZH_HANS("装配: {0} 秒   模型: {1} -> {2}（{3} 层）   合并: {4}   拒绝: {5}   "
            "补充注册: {6} 张   覆盖: {7} -> {8} 张"),
    ZH_HANT("組裝: {0} 秒   模型: {1} -> {2}（{3} 層）   合併: {4}   拒絕: {5}   "
            "補充註冊: {6} 張   涵蓋: {7} -> {8} 張"),
    KO("조립: {0} 초   모델: {1} -> {2}({3} 단계)   병합: {4}   거부: {5}   "
       "추가 등록: {6} 장   포함: {7} -> {8} 장"),
    DE("Zusammenbau: {0} s   Modelle: {1} -> {2} über {3} Ebene(n)   Verschmolzen: {4}   "
       "Abgelehnt: {5}   Zugewachsen: {6} Bild(er)   Abdeckung: {7} -> {8} Bilder"),
    FR("Assemblage : {0} s   Modèles : {1} -> {2} sur {3} niveau(x)   Fusionnés : {4}   "
       "Refusés : {5}   Ajoutés : {6} image(s)   Couverture : {7} -> {8} images"),
    ES("Ensamblaje: {0} s   Modelos: {1} -> {2} en {3} nivel(es)   Fusionados: {4}   "
       "Rechazados: {5}   Añadidas: {6} imagen(es)   Cobertura: {7} -> {8} imágenes"),
    PT("Montagem: {0} s   Modelos: {1} -> {2} em {3} nível(is)   Fundidos: {4}   "
       "Recusados: {5}   Acrescentadas: {6} imagem(ns)   Cobertura: {7} -> {8} imagens"),
    IT("Assemblaggio: {0} s   Modelli: {1} -> {2} su {3} livello/i   Fusi: {4}   "
       "Rifiutati: {5}   Aggiunte: {6} immagine/i   Copertura: {7} -> {8} immagini"),
    NL("Assemblage: {0} s   Modellen: {1} -> {2} over {3} niveau(s)   Samengevoegd: {4}   "
       "Geweigerd: {5}   Aangegroeid: {6} afbeelding(en)   Dekking: {7} -> {8} afbeeldingen"),
    RU("Сборка: {0} с   Моделей: {1} -> {2} за уровней: {3}   Объединено: {4}   Отклонено: {5}   "
       "Добавлено изображений: {6}   Охват: {7} -> {8} изображений"),
    TR("Birleştirme: {0} s   Model: {1} -> {2}, {3} düzeyde   Kaynaşan: {4}   Reddedilen: {5}   "
       "Eklenen: {6} görüntü   Kapsama: {7} -> {8} görüntü"));

SS_MSG(map_finishing,
    EN("Finishing passes ({0} s): split {1}, folds cut {2}, reseeded {3}, dropped {4}, "
       "repaired by the audit {5}, dropped by the audit {6}"),
    JA("仕上げ処理（{0} 秒）: 分割 {1}、折り返しの切断 {2}、再シード {3}、除外 {4}、"
       "監査で修復 {5}、監査で除外 {6}"),
    ZH_HANS("收尾处理（{0} 秒）: 拆分 {1}，切开折叠 {2}，重新播种 {3}，丢弃 {4}，"
            "审查修复 {5}，审查丢弃 {6}"),
    ZH_HANT("收尾處理（{0} 秒）: 拆分 {1}，切開折疊 {2}，重新播種 {3}，丟棄 {4}，"
            "稽核修復 {5}，稽核丟棄 {6}"),
    KO("마무리 단계({0} 초): 분할 {1}, 접힘 절단 {2}, 재시드 {3}, 제외 {4}, "
       "감사로 복구 {5}, 감사로 제외 {6}"),
    DE("Abschlussdurchgänge ({0} s): geteilt {1}, Faltungen getrennt {2}, neu gesät {3}, "
       "verworfen {4}, von der Prüfung repariert {5}, von der Prüfung verworfen {6}"),
    FR("Passes finales ({0} s) : scindés {1}, plis coupés {2}, réamorcés {3}, écartés {4}, "
       "réparés par l'audit {5}, écartés par l'audit {6}"),
    ES("Pasadas finales ({0} s): divididos {1}, pliegues cortados {2}, resembrados {3}, "
       "descartados {4}, reparados por la auditoría {5}, descartados por la auditoría {6}"),
    PT("Passagens finais ({0} s): divididos {1}, dobras cortadas {2}, ressemeados {3}, "
       "descartados {4}, reparados pela auditoria {5}, descartados pela auditoria {6}"),
    IT("Passate finali ({0} s): divisi {1}, pieghe tagliate {2}, riseminati {3}, scartati {4}, "
       "riparati dall'audit {5}, scartati dall'audit {6}"),
    NL("Afrondende rondes ({0} s): gesplitst {1}, vouwen doorgesneden {2}, opnieuw gezaaid {3}, "
       "afgevallen {4}, hersteld door de controle {5}, afgevallen door de controle {6}"),
    RU("Завершающие проходы ({0} с): разделено {1}, складок разрезано {2}, пересеяно {3}, "
       "отброшено {4}, исправлено проверкой {5}, отброшено проверкой {6}"),
    TR("Bitirme geçişleri ({0} s): bölünen {1}, kesilen katlanma {2}, yeniden tohumlanan {3}, "
       "elenen {4}, denetimle onarılan {5}, denetimle elenen {6}"));


// ===========================================================================
// The gauge fix, and the summary
// ===========================================================================

SS_MSG(orient_done,
    EN("Model {0}: levelled and centred on the cameras, scaled by {1}"),
    JA("モデル {0}: カメラに合わせて水平・中心を取り、{1} 倍に縮尺しました"),
    ZH_HANS("模型 {0}: 已按相机摆正并居中，缩放 {1} 倍"),
    ZH_HANT("模型 {0}: 已依相機擺正並置中，縮放 {1} 倍"),
    KO("모델 {0}: 카메라에 맞춰 수평·중심을 잡고 {1} 배로 조정했습니다"),
    DE("Modell {0}: an den Kameras ausgerichtet und zentriert, um {1} skaliert"),
    FR("Modèle {0} : mis d'aplomb et centré sur les caméras, mis à l'échelle de {1}"),
    ES("Modelo {0}: nivelado y centrado en las cámaras, escalado por {1}"),
    PT("Modelo {0}: nivelado e centrado nas câmeras, escalado por {1}"),
    IT("Modello {0}: raddrizzato e centrato sulle fotocamere, scalato di {1}"),
    NL("Model {0}: waterpas gezet en op de camera's gecentreerd, geschaald met {1}"),
    RU("Модель {0}: выровнена и центрирована по камерам, масштаб {1}"),
    TR("Model {0}: kameralara göre düzlendi ve ortalandı, {1} ile ölçeklendi"));

SS_MSG(sum_header,
    EN("Summary"),      JA("まとめ"),      ZH_HANS("小结"),    ZH_HANT("小結"),
    KO("요약"),          DE("Zusammenfassung"), FR("Récapitulatif"), ES("Resumen"),
    PT("Resumo"),       IT("Riepilogo"),  NL("Samenvatting"), RU("Итоги"),
    TR("Özet"));

SS_MSG(sum_extract,
    EN("Extraction: {0} s   Images: {1}   Features: {2}"),
    JA("抽出: {0} 秒   画像: {1}   特徴点: {2}"),
    ZH_HANS("提取: {0} 秒   图像: {1}   特征点: {2}"),
    ZH_HANT("擷取: {0} 秒   影像: {1}   特徵點: {2}"),
    KO("추출: {0} 초   이미지: {1}   특징점: {2}"),
    DE("Extraktion: {0} s   Bilder: {1}   Merkmale: {2}"),
    FR("Extraction : {0} s   Images : {1}   Points : {2}"),
    ES("Extracción: {0} s   Imágenes: {1}   Puntos: {2}"),
    PT("Extração: {0} s   Imagens: {1}   Pontos: {2}"),
    IT("Estrazione: {0} s   Immagini: {1}   Punti: {2}"),
    NL("Extractie: {0} s   Afbeeldingen: {1}   Kenmerken: {2}"),
    RU("Извлечение: {0} с   Изображений: {1}   Точек: {2}"),
    TR("Çıkarım: {0} s   Görüntü: {1}   Öznitelik: {2}"));

SS_MSG(sum_masks,
    EN("Masks: {0}/{1} images   Keypoints dropped: {2} ({3}%)"),
    JA("マスク: 画像 {0}/{1}   除外した特徴点: {2}（{3}%）"),
    ZH_HANS("蒙版: 图像 {0}/{1}   被排除的特征点: {2}（{3}%）"),
    ZH_HANT("遮罩: 影像 {0}/{1}   被排除的特徵點: {2}（{3}%）"),
    KO("마스크: 이미지 {0}/{1}   제외된 특징점: {2}({3}%)"),
    DE("Masken: {0}/{1} Bilder   Entfernte Merkmalspunkte: {2} ({3}%)"),
    FR("Masques : {0}/{1} images   Points écartés : {2} ({3}%)"),
    ES("Máscaras: {0}/{1} imágenes   Puntos descartados: {2} ({3}%)"),
    PT("Máscaras: {0}/{1} imagens   Pontos descartados: {2} ({3}%)"),
    IT("Maschere: {0}/{1} immagini   Punti scartati: {2} ({3}%)"),
    NL("Maskers: {0}/{1} afbeeldingen   Weggelaten kenmerkpunten: {2} ({3}%)"),
    RU("Маски: {0}/{1} изображений   Отсечено точек: {2} ({3}%)"),
    TR("Maske: {0}/{1} görüntü   Elenen anahtar nokta: {2} (%{3})"));

SS_MSG(sum_match,
    EN("Matching: {0} s   Pairs kept: {1}/{2}   Inliers: {3}/{4}"),
    JA("照合: {0} 秒   残ったペア: {1}/{2}   インライア: {3}/{4}"),
    ZH_HANS("匹配: {0} 秒   保留的图像对: {1}/{2}   内点: {3}/{4}"),
    ZH_HANT("匹配: {0} 秒   保留的影像對: {1}/{2}   內點: {3}/{4}"),
    KO("정합: {0} 초   남은 쌍: {1}/{2}   인라이어: {3}/{4}"),
    DE("Abgleich: {0} s   Behaltene Paare: {1}/{2}   Inlier: {3}/{4}"),
    FR("Appariement : {0} s   Paires conservées : {1}/{2}   Inliers : {3}/{4}"),
    ES("Emparejamiento: {0} s   Pares conservados: {1}/{2}   Inliers: {3}/{4}"),
    PT("Pareamento: {0} s   Pares mantidos: {1}/{2}   Inliers: {3}/{4}"),
    IT("Accoppiamento: {0} s   Coppie tenute: {1}/{2}   Inlier: {3}/{4}"),
    NL("Koppelen: {0} s   Behouden paren: {1}/{2}   Inliers: {3}/{4}"),
    RU("Сопоставление: {0} с   Оставлено пар: {1}/{2}   Инлаеров: {3}/{4}"),
    TR("Eşleme: {0} s   Tutulan çift: {1}/{2}   İçeri: {3}/{4}"));

SS_MSG(sum_map,
    EN("Mapping: {0} s   Registered: {1}/{2} images   Points: {3}   Cameras: {4}"),
    JA("復元: {0} 秒   登録: 画像 {1}/{2}   点: {3}   カメラ: {4}"),
    ZH_HANS("重建: {0} 秒   已配准: 图像 {1}/{2}   点: {3}   相机: {4}"),
    ZH_HANT("重建: {0} 秒   已註冊: 影像 {1}/{2}   點: {3}   相機: {4}"),
    KO("복원: {0} 초   등록: 이미지 {1}/{2}   점: {3}   카메라: {4}"),
    DE("Kartierung: {0} s   Registriert: {1}/{2} Bilder   Punkte: {3}   Kameras: {4}"),
    FR("Cartographie : {0} s   Enregistrées : {1}/{2} images   Points : {3}   Caméras : {4}"),
    ES("Mapeo: {0} s   Registradas: {1}/{2} imágenes   Puntos: {3}   Cámaras: {4}"),
    PT("Mapeamento: {0} s   Registradas: {1}/{2} imagens   Pontos: {3}   Câmeras: {4}"),
    IT("Mappatura: {0} s   Registrate: {1}/{2} immagini   Punti: {3}   Fotocamere: {4}"),
    NL("Kartering: {0} s   Geregistreerd: {1}/{2} afbeeldingen   Punten: {3}   Camera's: {4}"),
    RU("Построение: {0} с   Зарегистрировано: {1}/{2} изображений   Точек: {3}   Камер: {4}"),
    TR("Haritalama: {0} s   Kaydedilen: {1}/{2} görüntü   Nokta: {3}   Kamera: {4}"));

SS_MSG(sum_total,
    EN("Total: {0} s"),
    JA("合計: {0} 秒"),
    ZH_HANS("合计: {0} 秒"),
    ZH_HANT("合計: {0} 秒"),
    KO("합계: {0} 초"),
    DE("Gesamt: {0} s"),
    FR("Total : {0} s"),
    ES("Total: {0} s"),
    PT("Total: {0} s"),
    IT("Totale: {0} s"),
    NL("Totaal: {0} s"),
    RU("Всего: {0} с"),
    TR("Toplam: {0} s"));

SS_MSG(sum_model_error,
    EN("Reprojection error: mean {0} px, median {1} px, over {2} observations"),
    JA("再投影誤差: 平均 {0} px、中央値 {1} px（観測 {2} 個）"),
    ZH_HANS("重投影误差: 平均 {0} px，中位数 {1} px，共 {2} 个观测"),
    ZH_HANT("重投影誤差: 平均 {0} px，中位數 {1} px，共 {2} 個觀測"),
    KO("재투영 오차: 평균 {0} px, 중앙값 {1} px, 관측 {2} 개"),
    DE("Rückprojektionsfehler: Mittel {0} px, Median {1} px, über {2} Beobachtungen"),
    FR("Erreur de reprojection : moyenne {0} px, médiane {1} px, sur {2} observations"),
    ES("Error de reproyección: media {0} px, mediana {1} px, sobre {2} observaciones"),
    PT("Erro de reprojeção: média {0} px, mediana {1} px, sobre {2} observações"),
    IT("Errore di riproiezione: media {0} px, mediana {1} px, su {2} osservazioni"),
    NL("Herprojectiefout: gemiddeld {0} px, mediaan {1} px, over {2} waarnemingen"),
    RU("Ошибка перепроекции: среднее {0} px, медиана {1} px, по {2} наблюдениям"),
    TR("Yeniden izdüşüm hatası: ortalama {0} px, ortanca {1} px, {2} gözlem üzerinde"));

SS_MSG(sum_components,
    EN("Components: {0}, covering {1}/{2} distinct images"),
    JA("成分: {0} 個、異なる画像 {1}/{2} 枚をカバー"),
    ZH_HANS("连通成分: {0} 个，覆盖不同图像 {1}/{2} 张"),
    ZH_HANT("連通成分: {0} 個，涵蓋不同影像 {1}/{2} 張"),
    KO("구성 요소: {0} 개, 서로 다른 이미지 {1}/{2} 장 포함"),
    DE("Komponenten: {0}, decken {1}/{2} verschiedene Bilder ab"),
    FR("Composantes : {0}, couvrant {1}/{2} images distinctes"),
    ES("Componentes: {0}, que cubren {1}/{2} imágenes distintas"),
    PT("Componentes: {0}, cobrindo {1}/{2} imagens distintas"),
    IT("Componenti: {0}, che coprono {1}/{2} immagini distinte"),
    NL("Componenten: {0}, die {1}/{2} verschillende afbeeldingen dekken"),
    RU("Компонент: {0}, охвачено различных изображений: {1}/{2}"),
    TR("Bileşen: {0}, {1}/{2} ayrı görüntüyü kapsıyor"));

SS_MSG(sum_per_folder,
    EN("Per folder:"),
    JA("フォルダごと:"),
    ZH_HANS("按文件夹:"),
    ZH_HANT("依資料夾:"),
    KO("폴더별:"),
    DE("Pro Ordner:"),
    FR("Par dossier :"),
    ES("Por carpeta:"),
    PT("Por pasta:"),
    IT("Per cartella:"),
    NL("Per map:"),
    RU("По папкам:"),
    TR("Klasör başına:"));

SS_MSG(sum_folder_line,
    EN("{0}: {1}/{2} images registered"),
    JA("{0}: 画像 {1}/{2} を登録"),
    ZH_HANS("{0}: 已配准图像 {1}/{2}"),
    ZH_HANT("{0}: 已註冊影像 {1}/{2}"),
    KO("{0}: 이미지 {1}/{2} 등록"),
    DE("{0}: {1}/{2} Bilder registriert"),
    FR("{0} : {1}/{2} images enregistrées"),
    ES("{0}: {1}/{2} imágenes registradas"),
    PT("{0}: {1}/{2} imagens registradas"),
    IT("{0}: {1}/{2} immagini registrate"),
    NL("{0}: {1}/{2} afbeeldingen geregistreerd"),
    RU("{0}: зарегистрировано изображений {1}/{2}"),
    TR("{0}: {1}/{2} görüntü kaydedildi"));

SS_MSG(sum_folder_empty,
    EN("Not one image under {0} was registered. The model describes the rest of the "
       "capture only: those views may share no features with the others, or their "
       "camera model may be wrong."),
    JA("{0} の画像は1枚も登録されませんでした。モデルは残りの撮影分だけを表しています。"
       "これらの視点は他と特徴点を共有していないか、カメラモデルが違う可能性があります。"),
    ZH_HANS("{0} 下没有任何一张图像被配准。模型只描述了其余部分: 这些视角可能与其他图像没有共同特征，"
            "或者它们的相机模型选错了。"),
    ZH_HANT("{0} 下沒有任何一張影像被註冊。模型只描述了其餘部分: 這些視角可能與其他影像沒有共同特徵，"
            "或者它們的相機模型選錯了。"),
    KO("{0} 아래의 이미지는 한 장도 등록되지 않았습니다. 모델은 나머지 촬영분만 나타냅니다. "
       "이 시점들이 다른 이미지와 공통 특징이 없거나, 카메라 모델이 틀렸을 수 있습니다."),
    DE("Kein einziges Bild unter {0} wurde registriert. Das Modell beschreibt nur den Rest der "
       "Aufnahme: diese Ansichten teilen womöglich keine Merkmale mit den übrigen, oder ihr "
       "Kameramodell stimmt nicht."),
    FR("Aucune image sous {0} n'a été enregistrée. Le modèle ne décrit que le reste de la prise "
       "de vue : ces vues ne partagent peut-être aucun point avec les autres, ou leur modèle de "
       "caméra est erroné."),
    ES("No se registró ni una imagen bajo {0}. El modelo describe solo el resto de la captura: "
       "puede que esas vistas no compartan puntos con las demás, o que su modelo de cámara "
       "sea el equivocado."),
    PT("Nenhuma imagem sob {0} foi registrada. O modelo descreve apenas o resto da captura: "
       "essas vistas podem não compartilhar pontos com as demais, ou o modelo de câmera delas "
       "pode estar errado."),
    IT("Nessuna immagine sotto {0} è stata registrata. Il modello descrive solo il resto della "
       "ripresa: quelle viste potrebbero non condividere punti con le altre, o il loro modello "
       "di fotocamera potrebbe essere sbagliato."),
    NL("Geen enkele afbeelding onder {0} is geregistreerd. Het model beschrijft alleen de rest "
       "van de opname: die beelden delen misschien geen kenmerken met de andere, of hun "
       "cameramodel klopt niet."),
    RU("Ни одно изображение из {0} не зарегистрировано. Модель описывает только остальную часть "
       "съёмки: эти виды могут не иметь общих точек с прочими, либо у них неверная модель камеры."),
    TR("{0} altındaki hiçbir görüntü kaydedilmedi. Model yalnızca çekimin geri kalanını "
       "anlatıyor: bu görünümler diğerleriyle hiç öznitelik paylaşmıyor ya da kamera modelleri "
       "yanlış olabilir."));

SS_MSG(sum_written,
    EN("Written to {0}"),
    JA("{0} に書き出しました"),
    ZH_HANS("已写入 {0}"),
    ZH_HANT("已寫入 {0}"),
    KO("{0} 에 썼습니다"),
    DE("Geschrieben nach {0}"),
    FR("Écrit dans {0}"),
    ES("Escrito en {0}"),
    PT("Gravado em {0}"),
    IT("Scritto in {0}"),
    NL("Geschreven naar {0}"),
    RU("Записано в {0}"),
    TR("{0} konumuna yazıldı"));

SS_MSG(result_ok,
    EN("RESULT: OK -- {0}% of the images registered, {1} px mean reprojection"),
    JA("結果: 良好 -- 画像の {0}% を登録、再投影誤差の平均 {1} px"),
    ZH_HANS("结果: 良好 -- 配准了 {0}% 的图像，平均重投影误差 {1} px"),
    ZH_HANT("結果: 良好 -- 註冊了 {0}% 的影像，平均重投影誤差 {1} px"),
    KO("결과: 양호 -- 이미지의 {0}% 등록, 평균 재투영 오차 {1} px"),
    DE("ERGEBNIS: OK -- {0}% der Bilder registriert, {1} px mittlere Rückprojektion"),
    FR("RÉSULTAT : OK -- {0}% des images enregistrées, reprojection moyenne {1} px"),
    ES("RESULTADO: correcto -- {0}% de las imágenes registradas, reproyección media {1} px"),
    PT("RESULTADO: OK -- {0}% das imagens registradas, reprojeção média {1} px"),
    IT("RISULTATO: OK -- {0}% delle immagini registrate, riproiezione media {1} px"),
    NL("RESULTAAT: OK -- {0}% van de afbeeldingen geregistreerd, gemiddelde herprojectie {1} px"),
    RU("РЕЗУЛЬТАТ: в порядке — зарегистрировано {0}% изображений, средняя перепроекция {1} px"),
    TR("SONUÇ: iyi -- görüntülerin %{0} kadarı kaydedildi, ortalama yeniden izdüşüm {1} px"));

SS_MSG(result_partial,
    EN("RESULT: PARTIAL -- {0}% of the images registered, {1} px mean reprojection. "
       "The model is usable, but it does not describe the whole capture."),
    JA("結果: 部分的 -- 画像の {0}% を登録、再投影誤差の平均 {1} px。"
       "モデルは使えますが、撮影全体を表してはいません。"),
    ZH_HANS("结果: 部分完成 -- 配准了 {0}% 的图像，平均重投影误差 {1} px。"
            "模型可用，但没有覆盖整次拍摄。"),
    ZH_HANT("結果: 部分完成 -- 註冊了 {0}% 的影像，平均重投影誤差 {1} px。"
            "模型可用，但沒有涵蓋整次拍攝。"),
    KO("결과: 부분 -- 이미지의 {0}% 등록, 평균 재투영 오차 {1} px. "
       "모델은 쓸 수 있지만 촬영 전체를 나타내지는 않습니다."),
    DE("ERGEBNIS: TEILWEISE -- {0}% der Bilder registriert, {1} px mittlere Rückprojektion. "
       "Das Modell ist brauchbar, beschreibt aber nicht die ganze Aufnahme."),
    FR("RÉSULTAT : PARTIEL -- {0}% des images enregistrées, reprojection moyenne {1} px. "
       "Le modèle est utilisable, mais il ne décrit pas toute la prise de vue."),
    ES("RESULTADO: parcial -- {0}% de las imágenes registradas, reproyección media {1} px. "
       "El modelo sirve, pero no describe toda la captura."),
    PT("RESULTADO: parcial -- {0}% das imagens registradas, reprojeção média {1} px. "
       "O modelo é utilizável, mas não descreve a captura inteira."),
    IT("RISULTATO: parziale -- {0}% delle immagini registrate, riproiezione media {1} px. "
       "Il modello è utilizzabile, ma non descrive l'intera ripresa."),
    NL("RESULTAAT: GEDEELTELIJK -- {0}% van de afbeeldingen geregistreerd, gemiddelde "
       "herprojectie {1} px. Het model is bruikbaar, maar beschrijft niet de hele opname."),
    RU("РЕЗУЛЬТАТ: частично — зарегистрировано {0}% изображений, средняя перепроекция {1} px. "
       "Модель пригодна, но описывает не всю съёмку."),
    TR("SONUÇ: kısmi -- görüntülerin %{0} kadarı kaydedildi, ortalama yeniden izdüşüm {1} px. "
       "Model kullanılabilir, ama çekimin tamamını anlatmıyor."));

SS_MSG(result_failed,
    EN("RESULT: FAILED -- nothing was reconstructed."),
    JA("結果: 失敗 -- 何も復元できませんでした。"),
    ZH_HANS("结果: 失败 -- 什么也没能重建出来。"),
    ZH_HANT("結果: 失敗 -- 什麼也沒能重建出來。"),
    KO("결과: 실패 -- 아무것도 복원하지 못했습니다."),
    DE("ERGEBNIS: FEHLGESCHLAGEN -- es wurde nichts rekonstruiert."),
    FR("RÉSULTAT : ÉCHEC -- rien n'a été reconstruit."),
    ES("RESULTADO: fallido -- no se reconstruyó nada."),
    PT("RESULTADO: falhou -- nada foi reconstruído."),
    IT("RISULTATO: fallito -- non è stato ricostruito nulla."),
    NL("RESULTAAT: MISLUKT -- er is niets gereconstrueerd."),
    RU("РЕЗУЛЬТАТ: неудача — ничего не восстановлено."),
    TR("SONUÇ: başarısız -- hiçbir şey yeniden oluşturulamadı."));

// How cameras were grouped. The VALUE is a flag spelling and stays as it is
// (`--camera-mode folder`); what is translated is the sentence that says what
// it means, since that is the part somebody reads to check the choice.
SS_MSG(camera_mode_folder,
    EN("folder (one camera per sub-folder, split by resolution)"),
    JA("folder（サブフォルダごとに1台、解像度でさらに分割）"),
    ZH_HANS("folder（每个子文件夹一台相机，再按分辨率拆分）"),
    ZH_HANT("folder（每個子資料夾一台相機，再依解析度拆分）"),
    KO("folder(하위 폴더마다 카메라 하나, 해상도로 다시 분할)"),
    DE("folder (eine Kamera je Unterordner, nach Auflösung getrennt)"),
    FR("folder (une caméra par sous-dossier, séparées par résolution)"),
    ES("folder (una cámara por subcarpeta, separadas por resolución)"),
    PT("folder (uma câmera por subpasta, separadas por resolução)"),
    IT("folder (una fotocamera per sottocartella, divise per risoluzione)"),
    NL("folder (één camera per submap, gesplitst op resolutie)"),
    RU("folder (по одной камере на подпапку, с разделением по разрешению)"),
    TR("folder (her alt klasör için bir kamera, çözünürlüğe göre ayrılmış)"));

SS_MSG(camera_mode_image,
    EN("image (one camera per image)"),
    JA("image（画像ごとに1台）"),
    ZH_HANS("image（每张图像一台相机）"),
    ZH_HANT("image（每張影像一台相機）"),
    KO("image(이미지마다 카메라 하나)"),
    DE("image (eine Kamera je Bild)"),
    FR("image (une caméra par image)"),
    ES("image (una cámara por imagen)"),
    PT("image (uma câmera por imagem)"),
    IT("image (una fotocamera per immagine)"),
    NL("image (één camera per afbeelding)"),
    RU("image (по одной камере на изображение)"),
    TR("image (her görüntü için bir kamera)"));

SS_MSG(camera_mode_single,
    EN("single (one camera per distinct resolution, 2% tolerance)"),
    JA("single（解像度ごとに1台、許容差2%）"),
    ZH_HANS("single（每种分辨率一台相机，容差 2%）"),
    ZH_HANT("single（每種解析度一台相機，容差 2%）"),
    KO("single(해상도마다 카메라 하나, 허용 오차 2%)"),
    DE("single (eine Kamera je Auflösung, 2% Toleranz)"),
    FR("single (une caméra par résolution distincte, tolérance 2%)"),
    ES("single (una cámara por resolución distinta, tolerancia del 2%)"),
    PT("single (uma câmera por resolução distinta, tolerância de 2%)"),
    IT("single (una fotocamera per risoluzione distinta, tolleranza del 2%)"),
    NL("single (één camera per afzonderlijke resolutie, 2% tolerantie)"),
    RU("single (по одной камере на каждое разрешение, допуск 2%)"),
    TR("single (her ayrı çözünürlük için bir kamera, %2 tolerans)"));

// The detector's own per-image counts. Short, because there is one set of them
// per image and they sit under the extraction progress line.
SS_MSG(sift_raw,
    EN("Octaves: {0}   Raw keypoints: {1}"),
    JA("オクターブ: {0}   生の特徴点: {1}"),
    ZH_HANS("八度: {0}   原始关键点: {1}"),
    ZH_HANT("八度: {0}   原始關鍵點: {1}"),
    KO("옥타브: {0}   원시 키포인트: {1}"),
    DE("Oktaven: {0}   Rohe Merkmalspunkte: {1}"),
    FR("Octaves : {0}   Points bruts : {1}"),
    ES("Octavas: {0}   Puntos en bruto: {1}"),
    PT("Oitavas: {0}   Pontos brutos: {1}"),
    IT("Ottave: {0}   Punti grezzi: {1}"),
    NL("Octaven: {0}   Ruwe kenmerkpunten: {1}"),
    RU("Октав: {0}   Исходных точек: {1}"),
    TR("Oktav: {0}   Ham anahtar nokta: {1}"));

SS_MSG(sift_selected,
    EN("Oriented: {0}   Selected: {1} (scale at least {2})"),
    JA("方向付け: {0}   採用: {1}（スケール {2} 以上）"),
    ZH_HANS("已定向: {0}   选中: {1}（尺度不小于 {2}）"),
    ZH_HANT("已定向: {0}   選中: {1}（尺度不小於 {2}）"),
    KO("방향 지정: {0}   선택: {1}(스케일 {2} 이상)"),
    DE("Orientiert: {0}   Ausgewählt: {1} (Skala mindestens {2})"),
    FR("Orientés : {0}   Retenus : {1} (échelle au moins {2})"),
    ES("Orientados: {0}   Seleccionados: {1} (escala mínima {2})"),
    PT("Orientados: {0}   Selecionados: {1} (escala mínima {2})"),
    IT("Orientati: {0}   Selezionati: {1} (scala almeno {2})"),
    NL("Georiënteerd: {0}   Gekozen: {1} (schaal minstens {2})"),
    RU("Ориентировано: {0}   Отобрано: {1} (масштаб не меньше {2})"),
    TR("Yönlendirilen: {0}   Seçilen: {1} (ölçek en az {2})"));

SS_MSG(sift_features,
    EN("Features: {0}"),
    JA("特徴点: {0}"),
    ZH_HANS("特征点: {0}"),
    ZH_HANT("特徵點: {0}"),
    KO("특징점: {0}"),
    DE("Merkmale: {0}"),
    FR("Points : {0}"),
    ES("Puntos: {0}"),
    PT("Pontos: {0}"),
    IT("Punti: {0}"),
    NL("Kenmerken: {0}"),
    RU("Точек: {0}"),
    TR("Öznitelik: {0}"));

SS_MSG(sift_saturated_raw,
    EN("The raw keypoint list filled up ({0} found, room for {1}); raise max_raw_keypoints."),
    JA("生の特徴点リストが上限に達しました（{0} 検出、容量 {1}）。max_raw_keypoints を増やしてください。"),
    ZH_HANS("原始关键点列表已满（检出 {0}，容量 {1}）; 请调大 max_raw_keypoints。"),
    ZH_HANT("原始關鍵點清單已滿（偵測 {0}，容量 {1}）; 請調大 max_raw_keypoints。"),
    KO("원시 키포인트 목록이 가득 찼습니다({0} 개 검출, 용량 {1}). max_raw_keypoints 를 늘리세요."),
    DE("Die Liste roher Merkmalspunkte war voll ({0} gefunden, Platz für {1}); "
       "max_raw_keypoints erhöhen."),
    FR("La liste de points bruts est pleine ({0} trouvés, place pour {1}) ; "
       "augmentez max_raw_keypoints."),
    ES("La lista de puntos en bruto se llenó ({0} encontrados, cabida para {1}); "
       "aumente max_raw_keypoints."),
    PT("A lista de pontos brutos encheu ({0} encontrados, espaço para {1}); "
       "aumente max_raw_keypoints."),
    IT("L'elenco dei punti grezzi si è riempito ({0} trovati, spazio per {1}); "
       "aumenti max_raw_keypoints."),
    NL("De lijst met ruwe kenmerkpunten liep vol ({0} gevonden, plaats voor {1}); "
       "verhoog max_raw_keypoints."),
    RU("Список исходных точек переполнен (найдено {0}, место на {1}); "
       "увеличьте max_raw_keypoints."),
    TR("Ham anahtar nokta listesi doldu ({0} bulundu, {1} yer var); "
       "max_raw_keypoints değerini artırın."));

SS_MSG(sift_saturated_oriented,
    EN("The oriented keypoint list filled up ({0} found, room for {1})."),
    JA("方向付けした特徴点リストが上限に達しました（{0} 検出、容量 {1}）。"),
    ZH_HANS("已定向的关键点列表已满（检出 {0}，容量 {1}）。"),
    ZH_HANT("已定向的關鍵點清單已滿（偵測 {0}，容量 {1}）。"),
    KO("방향을 지정한 키포인트 목록이 가득 찼습니다({0} 개 검출, 용량 {1})."),
    DE("Die Liste orientierter Merkmalspunkte war voll ({0} gefunden, Platz für {1})."),
    FR("La liste de points orientés est pleine ({0} trouvés, place pour {1})."),
    ES("La lista de puntos orientados se llenó ({0} encontrados, cabida para {1})."),
    PT("A lista de pontos orientados encheu ({0} encontrados, espaço para {1})."),
    IT("L'elenco dei punti orientati si è riempito ({0} trovati, spazio per {1})."),
    NL("De lijst met georiënteerde kenmerkpunten liep vol ({0} gevonden, plaats voor {1})."),
    RU("Список ориентированных точек переполнен (найдено {0}, место на {1})."),
    TR("Yönlendirilmiş anahtar nokta listesi doldu ({0} bulundu, {1} yer var)."));


// ===========================================================================
// The rest of what a run prints
// ===========================================================================
// These were the last lines still writing their own printf. They come from the
// stages `auto` runs itself (feature reading, pair selection, verification) and
// from the standalone subcommands, which a person bisecting a failed capture
// runs one at a time.

SS_MSG(match_pairs_scored,
    EN("pairs scored: {0}/{1}"),
    JA("採点したペア: {0}/{1}"),
    ZH_HANS("已评分的像对：{0}/{1}"),
    ZH_HANT("已評分的影像對：{0}/{1}"),
    KO("점수를 매긴 쌍: {0}/{1}"),
    DE("bewertete Paare: {0}/{1}"),
    FR("paires notées : {0}/{1}"),
    ES("pares puntuados: {0}/{1}"),
    PT("pares pontuados: {0}/{1}"),
    IT("coppie valutate: {0}/{1}"),
    NL("beoordeelde paren: {0}/{1}"),
    RU("оценено пар: {0}/{1}"),
    TR("puanlanan çift: {0}/{1}"));

SS_MSG(match_prefilter_kept,
    EN("pair selection kept: {0}/{1}; top features: {2}, neighbours: {3} ({4} s)"),
    JA("ペア選択で残した数: {0}/{1}、上位特徴点: {2}、近傍: {3}（{4} 秒）"),
    ZH_HANS("像对筛选保留：{0}/{1}；取前 {2} 个特征，近邻 {3}（{4} 秒）"),
    ZH_HANT("影像對篩選保留：{0}/{1}；取前 {2} 個特徵，近鄰 {3}（{4} 秒）"),
    KO("쌍 선택으로 남긴 수: {0}/{1}; 상위 특징점: {2}, 이웃: {3}({4}초)"),
    DE("Paarauswahl behielt: {0}/{1}; beste Merkmale: {2}, Nachbarn: {3} ({4} s)"),
    FR("la sélection de paires a gardé : {0}/{1} ; meilleurs points : {2}, "
       "voisins : {3} ({4} s)"),
    ES("la selección de pares conservó: {0}/{1}; mejores rasgos: {2}, "
       "vecinos: {3} ({4} s)"),
    PT("a seleção de pares manteve: {0}/{1}; melhores traços: {2}, "
       "vizinhos: {3} ({4} s)"),
    IT("la selezione delle coppie ha tenuto: {0}/{1}; migliori punti: {2}, "
       "vicini: {3} ({4} s)"),
    NL("paarselectie hield: {0}/{1}; beste kenmerken: {2}, buren: {3} ({4} s)"),
    RU("отбор пар оставил: {0}/{1}; лучших признаков: {2}, соседей: {3} ({4} с)"),
    TR("çift seçimi tuttu: {0}/{1}; en iyi öznitelik: {2}, komşu: {3} ({4} sn)"));

SS_MSG(match_loop_closure_added,
    EN("loop closure added pairs: {0}, on top of sequential pairs: {1} "
       "(selected: {2}, {3} s). --no-loop-closure turns this off."),
    JA("ループ閉じ込みで追加したペア: {0}、逐次ペア: {1}（選択: {2}、{3} 秒）。"
       "--no-loop-closure で無効にできます。"),
    ZH_HANS("回环闭合新增的像对：{0}，此外还有顺序像对：{1}（选出：{2}，{3} 秒）。"
            "用 --no-loop-closure 可关闭。"),
    ZH_HANT("迴環閉合新增的影像對：{0}，此外還有順序影像對：{1}（選出：{2}，{3} 秒）。"
            "用 --no-loop-closure 可關閉。"),
    KO("루프 클로저로 더한 쌍: {0}, 순차 쌍: {1}(선택: {2}, {3}초). "
       "--no-loop-closure 로 끌 수 있습니다."),
    DE("Schleifenschluss ergänzte Paare: {0}, zu sequenziellen Paaren: {1} "
       "(ausgewählt: {2}, {3} s). --no-loop-closure schaltet das ab."),
    FR("la fermeture de boucle a ajouté des paires : {0}, en plus des paires "
       "séquentielles : {1} (sélectionnées : {2}, {3} s). --no-loop-closure "
       "désactive cela."),
    ES("el cierre de bucle añadió pares: {0}, además de los pares "
       "secuenciales: {1} (seleccionados: {2}, {3} s). --no-loop-closure lo "
       "desactiva."),
    PT("o fechamento de laço acrescentou pares: {0}, além dos pares "
       "sequenciais: {1} (selecionados: {2}, {3} s). --no-loop-closure desliga "
       "isso."),
    IT("la chiusura d'anello ha aggiunto coppie: {0}, oltre alle coppie "
       "sequenziali: {1} (selezionate: {2}, {3} s). --no-loop-closure lo "
       "disattiva."),
    NL("lussluiting voegde paren toe: {0}, boven op sequentiële paren: {1} "
       "(geselecteerd: {2}, {3} s). --no-loop-closure zet dit uit."),
    RU("замыкание петли добавило пар: {0}, к последовательным парам: {1} "
       "(отобрано: {2}, {3} с). --no-loop-closure это отключает."),
    TR("döngü kapatma eklenen çift: {0}, sıralı çiftlere ek olarak: {1} "
       "(seçilen: {2}, {3} sn). --no-loop-closure bunu kapatır."));

SS_MSG(match_prefilter_params,
    EN("pair selection -- top features: {0}, neighbours: {1}"),
    JA("ペア選択 -- 上位特徴点: {0}、近傍: {1}"),
    ZH_HANS("像对筛选 —— 取前 {0} 个特征，近邻 {1}"),
    ZH_HANT("影像對篩選 —— 取前 {0} 個特徵，近鄰 {1}"),
    KO("쌍 선택 -- 상위 특징점: {0}, 이웃: {1}"),
    DE("Paarauswahl -- beste Merkmale: {0}, Nachbarn: {1}"),
    FR("sélection de paires -- meilleurs points : {0}, voisins : {1}"),
    ES("selección de pares: mejores rasgos: {0}, vecinos: {1}"),
    PT("seleção de pares -- melhores traços: {0}, vizinhos: {1}"),
    IT("selezione delle coppie -- migliori punti: {0}, vicini: {1}"),
    NL("paarselectie -- beste kenmerken: {0}, buren: {1}"),
    RU("отбор пар -- лучших признаков: {0}, соседей: {1}"),
    TR("çift seçimi -- en iyi öznitelik: {0}, komşu: {1}"));

// The matcher's name is an identifier (`lightglue`), so it stays as it is.
SS_MSG(match_matcher_name,
    EN("matcher: {0}"),
    JA("マッチャー: {0}"),
    ZH_HANS("匹配器：{0}"),
    ZH_HANT("匹配器：{0}"),
    KO("매처: {0}"),
    DE("Matcher: {0}"),
    FR("apparieur : {0}"),
    ES("emparejador: {0}"),
    PT("emparelhador: {0}"),
    IT("abbinatore: {0}"),
    NL("matcher: {0}"),
    RU("сопоставитель: {0}"),
    TR("eşleştirici: {0}"));

SS_MSG(focal_epipolar_search,
    EN("epipolar focal search: {0} s"),
    JA("エピポーラによる焦点距離探索: {0} 秒"),
    ZH_HANS("对极几何焦距搜索：{0} 秒"),
    ZH_HANT("對極幾何焦距搜尋：{0} 秒"),
    KO("에피폴라 초점 거리 탐색: {0}초"),
    DE("epipolare Brennweitensuche: {0} s"),
    FR("recherche épipolaire de focale : {0} s"),
    ES("búsqueda epipolar de la focal: {0} s"),
    PT("busca epipolar da focal: {0} s"),
    IT("ricerca epipolare della focale: {0} s"),
    NL("epipolaire brandpuntszoektocht: {0} s"),
    RU("эпиполярный поиск фокуса: {0} с"),
    TR("epipolar odak arayışı: {0} sn"));

// {2} is a list the caller built ("cam 0: 520.4, cam 1: 519.8"): identifiers
// and numbers, so it is passed through as it is.
SS_MSG(match_bearings,
    EN("calibrated verification on bearings ({0} s, {1} MB); focal lengths: {2}"),
    JA("方位ベクトルでの校正済み検証（{0} 秒、{1} MB）、焦点距離: {2}"),
    ZH_HANS("在方向向量上做标定后验证（{0} 秒，{1} MB）；焦距：{2}"),
    ZH_HANT("在方向向量上做標定後驗證（{0} 秒，{1} MB）；焦距：{2}"),
    KO("방향 벡터에서 보정된 검증({0}초, {1} MB); 초점 거리: {2}"),
    DE("kalibrierte Prüfung auf Richtungsvektoren ({0} s, {1} MB); "
       "Brennweiten: {2}"),
    FR("vérification calibrée sur les directions ({0} s, {1} Mo) ; "
       "focales : {2}"),
    ES("verificación calibrada sobre las direcciones ({0} s, {1} MB); "
       "focales: {2}"),
    PT("verificação calibrada sobre as direções ({0} s, {1} MB); focais: {2}"),
    IT("verifica calibrata sulle direzioni ({0} s, {1} MB); focali: {2}"),
    NL("gekalibreerde verificatie op richtingen ({0} s, {1} MB); "
       "brandpuntsafstanden: {2}"),
    RU("калиброванная проверка по направлениям ({0} с, {1} МБ); "
       "фокусные расстояния: {2}"),
    TR("yön vektörlerinde kalibre doğrulama ({0} sn, {1} MB); odak "
       "uzaklıkları: {2}"));

SS_MSG(match_no_mask_for,
    EN("no mask for {0} in {1}"),
    JA("{1} に {0} のマスクがありません"),
    ZH_HANS("{1} 中没有 {0} 的掩码"),
    ZH_HANT("{1} 中沒有 {0} 的遮罩"),
    KO("{1} 에 {0} 의 마스크가 없습니다"),
    DE("keine Maske für {0} in {1}"),
    FR("aucun masque pour {0} dans {1}"),
    ES("no hay máscara de {0} en {1}"),
    PT("não há máscara de {0} em {1}"),
    IT("nessuna maschera per {0} in {1}"),
    NL("geen masker voor {0} in {1}"),
    RU("в {1} нет маски для {0}"),
    TR("{1} içinde {0} için maske yok"));

SS_MSG(extract_to_gray,
    EN("{0} -> {1}x{2} greyscale"),
    JA("{0} -> {1}x{2} のグレースケール"),
    ZH_HANS("{0} -> {1}x{2} 灰度"),
    ZH_HANT("{0} -> {1}x{2} 灰階"),
    KO("{0} -> {1}x{2} 회색조"),
    DE("{0} -> {1}x{2} Graustufen"),
    FR("{0} -> niveaux de gris {1}x{2}"),
    ES("{0} -> escala de grises de {1}x{2}"),
    PT("{0} -> tons de cinza {1}x{2}"),
    IT("{0} -> scala di grigi {1}x{2}"),
    NL("{0} -> grijswaarden {1}x{2}"),
    RU("{0} -> оттенки серого {1}x{2}"),
    TR("{0} -> {1}x{2} gri tonlama"));

SS_MSG(wrote_file,
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

SS_MSG(extract_dir_done,
    EN("done -- images: {0}, features: {1}"),
    JA("完了 -- 画像: {0}、特徴点: {1}"),
    ZH_HANS("完成 —— 图像：{0}，特征点：{1}"),
    ZH_HANT("完成 —— 影像：{0}，特徵點：{1}"),
    KO("완료 -- 이미지: {0}, 특징점: {1}"),
    DE("fertig -- Bilder: {0}, Merkmale: {1}"),
    FR("terminé -- images : {0}, points : {1}"),
    ES("listo: imágenes: {0}, rasgos: {1}"),
    PT("pronto -- imagens: {0}, traços: {1}"),
    IT("fatto -- immagini: {0}, punti: {1}"),
    NL("klaar -- beelden: {0}, kenmerken: {1}"),
    RU("готово -- изображения: {0}, признаки: {1}"),
    TR("bitti -- görüntü: {0}, öznitelik: {1}"));

SS_MSG(extract_dir_masked,
    EN("masked out: {0}, over masked images: {1}"),
    JA("マスクで除外: {0}、マスク付き画像: {1}"),
    ZH_HANS("被掩码剔除：{0}，涉及带掩码图像：{1}"),
    ZH_HANT("被遮罩剔除：{0}，涉及帶遮罩影像：{1}"),
    KO("마스크로 걸러낸 수: {0}, 마스크가 있는 이미지: {1}"),
    DE("ausmaskiert: {0}, über maskierte Bilder: {1}"),
    FR("écartés par les masques : {0}, sur des images masquées : {1}"),
    ES("descartados por las máscaras: {0}, en imágenes enmascaradas: {1}"),
    PT("descartados pelas máscaras: {0}, em imagens mascaradas: {1}"),
    IT("scartati dalle maschere: {0}, su immagini mascherate: {1}"),
    NL("weggemaskeerd: {0}, over gemaskeerde beelden: {1}"),
    RU("отсечено масками: {0}, по замаскированным изображениям: {1}"),
    TR("maskeyle elenen: {0}, maskeli görüntüde: {1}"));

SS_MSG(extract_dir_failed,
    EN("failed to decode: {0}, unreadable headers: {1}"),
    JA("デコードに失敗: {0}、ヘッダーを読めなかった数: {1}"),
    ZH_HANS("解码失败：{0}，文件头无法读取：{1}"),
    ZH_HANT("解碼失敗：{0}，檔頭無法讀取：{1}"),
    KO("디코딩 실패: {0}, 헤더를 읽지 못한 수: {1}"),
    DE("nicht dekodierbar: {0}, unlesbare Kopfdaten: {1}"),
    FR("échecs de décodage : {0}, en-têtes illisibles : {1}"),
    ES("fallos al descodificar: {0}, cabeceras ilegibles: {1}"),
    PT("falhas ao decodificar: {0}, cabeçalhos ilegíveis: {1}"),
    IT("errori di decodifica: {0}, intestazioni illeggibili: {1}"),
    NL("mislukte decoderingen: {0}, onleesbare kopteksten: {1}"),
    RU("не удалось декодировать: {0}, нечитаемых заголовков: {1}"),
    TR("çözülemeyen: {0}, okunamayan başlık: {1}"));

SS_MSG(extract_one_done,
    EN("features: {0}, dimension: {1}, image: {2}x{3}"),
    JA("特徴点: {0}、次元: {1}、画像: {2}x{3}"),
    ZH_HANS("特征点：{0}，维度：{1}，图像：{2}x{3}"),
    ZH_HANT("特徵點：{0}，維度：{1}，影像：{2}x{3}"),
    KO("특징점: {0}, 차원: {1}, 이미지: {2}x{3}"),
    DE("Merkmale: {0}, Dimension: {1}, Bild: {2}x{3}"),
    FR("points : {0}, dimension : {1}, image : {2}x{3}"),
    ES("rasgos: {0}, dimensión: {1}, imagen: {2}x{3}"),
    PT("traços: {0}, dimensão: {1}, imagem: {2}x{3}"),
    IT("punti: {0}, dimensione: {1}, immagine: {2}x{3}"),
    NL("kenmerken: {0}, dimensie: {1}, beeld: {2}x{3}"),
    RU("признаки: {0}, размерность: {1}, изображение: {2}x{3}"),
    TR("öznitelik: {0}, boyut: {1}, görüntü: {2}x{3}"));

SS_MSG(extract_one_exif_focal,
    EN("EXIF focal length: {0} px"),
    JA("EXIF の焦点距離: {0} px"),
    ZH_HANS("EXIF 焦距：{0} px"),
    ZH_HANT("EXIF 焦距：{0} px"),
    KO("EXIF 초점 거리: {0} px"),
    DE("EXIF-Brennweite: {0} px"),
    FR("focale EXIF : {0} px"),
    ES("focal EXIF: {0} px"),
    PT("focal EXIF: {0} px"),
    IT("focale EXIF: {0} px"),
    NL("EXIF-brandpuntsafstand: {0} px"),
    RU("фокусное расстояние из EXIF: {0} px"),
    TR("EXIF odak uzaklığı: {0} px"));

SS_MSG(extract_one_mask,
    EN("mask: {0}x{1}, masked out: {2}"),
    JA("マスク: {0}x{1}、除外: {2}"),
    ZH_HANS("掩码：{0}x{1}，剔除：{2}"),
    ZH_HANT("遮罩：{0}x{1}，剔除：{2}"),
    KO("마스크: {0}x{1}, 걸러낸 수: {2}"),
    DE("Maske: {0}x{1}, ausmaskiert: {2}"),
    FR("masque : {0}x{1}, écartés : {2}"),
    ES("máscara: {0}x{1}, descartados: {2}"),
    PT("máscara: {0}x{1}, descartados: {2}"),
    IT("maschera: {0}x{1}, scartati: {2}"),
    NL("masker: {0}x{1}, weggemaskeerd: {2}"),
    RU("маска: {0}x{1}, отсечено: {2}"),
    TR("maske: {0}x{1}, elenen: {2}"));

SS_MSG(match_done_inliers,
    EN("matched pairs with at least one inlier: {0}/{1}; inlier matches: {2} of "
       "{3} putative"),
    JA("インライアが 1 つ以上あるペア: {0}/{1}、インライアの対応: 候補 {3} 件中 {2} 件"),
    ZH_HANS("至少含一个内点的像对：{0}/{1}；内点匹配：{3} 个候选中的 {2} 个"),
    ZH_HANT("至少含一個內點的影像對：{0}/{1}；內點匹配：{3} 個候選中的 {2} 個"),
    KO("내부점이 하나 이상인 쌍: {0}/{1}; 내부점 대응: 후보 {3}건 중 {2}건"),
    DE("Paare mit mindestens einem Inlier: {0}/{1}; Inlier-Zuordnungen: {2} von "
       "{3} mutmaßlichen"),
    FR("paires avec au moins un inlier : {0}/{1} ; appariements inliers : {2} "
       "sur {3} présumés"),
    ES("pares con al menos un inlier: {0}/{1}; emparejamientos inliers: {2} de "
       "{3} supuestos"),
    PT("pares com ao menos um inlier: {0}/{1}; correspondências inliers: {2} de "
       "{3} supostas"),
    IT("coppie con almeno un inlier: {0}/{1}; corrispondenze inlier: {2} su {3} "
       "presunte"),
    NL("paren met minstens één inlier: {0}/{1}; inlier-overeenkomsten: {2} van "
       "{3} vermoede"),
    RU("пар хотя бы с одним инлайером: {0}/{1}; инлайерных соответствий: {2} из "
       "{3} предполагаемых"),
    TR("en az bir içleyeni olan çift: {0}/{1}; içleyen eşleşme: {3} adaydan {2} "
       "tanesi"));

SS_MSG(match_done_raw,
    EN("matched pairs with at least one match: {0}/{1}; matches: {2}"),
    JA("対応が 1 つ以上あるペア: {0}/{1}、対応の総数: {2}"),
    ZH_HANS("至少含一个匹配的像对：{0}/{1}；匹配总数：{2}"),
    ZH_HANT("至少含一個匹配的影像對：{0}/{1}；匹配總數：{2}"),
    KO("대응이 하나 이상인 쌍: {0}/{1}; 대응 총수: {2}"),
    DE("Paare mit mindestens einer Zuordnung: {0}/{1}; Zuordnungen: {2}"),
    FR("paires avec au moins un appariement : {0}/{1} ; appariements : {2}"),
    ES("pares con al menos un emparejamiento: {0}/{1}; emparejamientos: {2}"),
    PT("pares com ao menos uma correspondência: {0}/{1}; correspondências: {2}"),
    IT("coppie con almeno una corrispondenza: {0}/{1}; corrispondenze: {2}"),
    NL("paren met minstens één overeenkomst: {0}/{1}; overeenkomsten: {2}"),
    RU("пар хотя бы с одним соответствием: {0}/{1}; соответствий: {2}"),
    TR("en az bir eşleşmesi olan çift: {0}/{1}; eşleşme: {2}"));

SS_MSG(map_read_model,
    EN("read {0} -- images: {1}, points: {2}"),
    JA("{0} を読み込みました -- 画像: {1}、点: {2}"),
    ZH_HANS("已读取 {0} —— 图像：{1}，点：{2}"),
    ZH_HANT("已讀取 {0} —— 影像：{1}，點：{2}"),
    KO("{0} 을(를) 읽었습니다 -- 이미지: {1}, 점: {2}"),
    DE("{0} gelesen -- Bilder: {1}, Punkte: {2}"),
    FR("{0} lu -- images : {1}, points : {2}"),
    ES("se leyó {0}: imágenes: {1}, puntos: {2}"),
    PT("{0} lido -- imagens: {1}, pontos: {2}"),
    IT("letto {0} -- immagini: {1}, punti: {2}"),
    NL("{0} gelezen -- beelden: {1}, punten: {2}"),
    RU("прочитано {0} -- изображения: {1}, точки: {2}"),
    TR("{0} okundu -- görüntü: {1}, nokta: {2}"));

SS_MSG(map_not_a_directory,
    EN("{0} is not a directory"),
    JA("{0} はディレクトリではありません"),
    ZH_HANS("{0} 不是目录"),
    ZH_HANT("{0} 不是目錄"),
    KO("{0} 은(는) 디렉터리가 아닙니다"),
    DE("{0} ist kein Verzeichnis"),
    FR("{0} n'est pas un dossier"),
    ES("{0} no es un directorio"),
    PT("{0} não é um diretório"),
    IT("{0} non è una cartella"),
    NL("{0} is geen map"),
    RU("{0} -- не каталог"),
    TR("{0} bir dizin değil"));

SS_MSG(map_no_model_in,
    EN("{0} holds no model: no cameras.bin and no numbered sub-model"),
    JA("{0} にモデルがありません。cameras.bin も番号付きサブモデルもありません"),
    ZH_HANS("{0} 中没有模型：既没有 cameras.bin，也没有编号的子模型"),
    ZH_HANT("{0} 中沒有模型：既沒有 cameras.bin，也沒有編號的子模型"),
    KO("{0} 에 모델이 없습니다: cameras.bin 도 번호가 붙은 하위 모델도 없습니다"),
    DE("{0} enthält kein Modell: weder cameras.bin noch ein nummeriertes "
       "Untermodell"),
    FR("{0} ne contient aucun modèle : ni cameras.bin ni sous-modèle numéroté"),
    ES("{0} no contiene ningún modelo: ni cameras.bin ni submodelo numerado"),
    PT("{0} não contém nenhum modelo: nem cameras.bin nem submodelo numerado"),
    IT("{0} non contiene alcun modello: né cameras.bin né un sottomodello "
       "numerato"),
    NL("{0} bevat geen model: geen cameras.bin en geen genummerd submodel"),
    RU("в {0} нет модели: ни cameras.bin, ни пронумерованной подмодели"),
    TR("{0} bir model içermiyor: ne cameras.bin ne de numaralı bir alt model"));

SS_MSG(map_cannot_read,
    EN("cannot read {0}: {1}"),
    JA("{0} を読めません: {1}"),
    ZH_HANS("无法读取 {0}：{1}"),
    ZH_HANT("無法讀取 {0}：{1}"),
    KO("{0} 을(를) 읽을 수 없습니다: {1}"),
    DE("{0} kann nicht gelesen werden: {1}"),
    FR("impossible de lire {0} : {1}"),
    ES("no se puede leer {0}: {1}"),
    PT("não é possível ler {0}: {1}"),
    IT("non è possibile leggere {0}: {1}"),
    NL("{0} kan niet gelezen worden: {1}"),
    RU("не удаётся прочитать {0}: {1}"),
    TR("{0} okunamıyor: {1}"));

SS_MSG(map_resumed,
    EN("resumed models: {0}, covering images: {1}/{2}"),
    JA("再開したモデル: {0}、対象画像: {1}/{2}"),
    ZH_HANS("恢复的模型：{0}，覆盖图像：{1}/{2}"),
    ZH_HANT("恢復的模型：{0}，涵蓋影像：{1}/{2}"),
    KO("이어받은 모델: {0}, 포함 이미지: {1}/{2}"),
    DE("wieder aufgenommene Modelle: {0}, erfasste Bilder: {1}/{2}"),
    FR("modèles repris : {0}, images couvertes : {1}/{2}"),
    ES("modelos retomados: {0}, imágenes cubiertas: {1}/{2}"),
    PT("modelos retomados: {0}, imagens cobertas: {1}/{2}"),
    IT("modelli ripresi: {0}, immagini coperte: {1}/{2}"),
    NL("hervatte modellen: {0}, gedekte beelden: {1}/{2}"),
    RU("возобновлено моделей: {0}, охвачено изображений: {1}/{2}"),
    TR("sürdürülen model: {0}, kapsanan görüntü: {1}/{2}"));

SS_MSG(map_no_model_to_work_with,
    EN("there is no model to work with"),
    JA("扱えるモデルがありません"),
    ZH_HANS("没有可处理的模型"),
    ZH_HANT("沒有可處理的模型"),
    KO("다룰 모델이 없습니다"),
    DE("es gibt kein Modell, mit dem sich arbeiten ließe"),
    FR("il n'y a aucun modèle avec lequel travailler"),
    ES("no hay ningún modelo con el que trabajar"),
    PT("não há nenhum modelo com que trabalhar"),
    IT("non c'è alcun modello con cui lavorare"),
    NL("er is geen model om mee te werken"),
    RU("нет модели, с которой можно работать"),
    TR("üzerinde çalışılacak model yok"));

SS_MSG(merge_need_two,
    EN("merging needs at least two models, and there are {0}"),
    JA("統合には最低 2 つのモデルが必要ですが、{0} しかありません"),
    ZH_HANS("合并至少需要两个模型，这里只有 {0} 个"),
    ZH_HANT("合併至少需要兩個模型，這裡只有 {0} 個"),
    KO("병합에는 모델이 최소 2개 필요한데 {0}개뿐입니다"),
    DE("das Zusammenführen braucht mindestens zwei Modelle, hier sind es {0}"),
    FR("la fusion demande au moins deux modèles, et il y en a {0}"),
    ES("la fusión necesita al menos dos modelos, y hay {0}"),
    PT("a fusão precisa de pelo menos dois modelos, e há {0}"),
    IT("la fusione richiede almeno due modelli, e ce ne sono {0}"),
    NL("samenvoegen heeft minstens twee modellen nodig, en dit zijn er {0}"),
    RU("для слияния нужно не меньше двух моделей, а их {0}"),
    TR("birleştirme en az iki model ister, burada {0} tane var"));

SS_MSG(merge_summary,
    EN("merged {0} models into {1} in {2} s (merges: {3}, refused: {4})"),
    JA("{0} 個のモデルを {1} 個に統合しました（{2} 秒、統合: {3}、拒否: {4}）"),
    ZH_HANS("已把 {0} 个模型合并为 {1} 个（{2} 秒，合并：{3}，拒绝：{4}）"),
    ZH_HANT("已把 {0} 個模型合併為 {1} 個（{2} 秒，合併：{3}，拒絕：{4}）"),
    KO("모델 {0}개를 {1}개로 병합했습니다({2}초, 병합: {3}, 거절: {4})"),
    DE("{0} Modelle in {1} zusammengeführt, in {2} s (Zusammenführungen: {3}, "
       "abgelehnt: {4})"),
    FR("{0} modèles fusionnés en {1} en {2} s (fusions : {3}, refus : {4})"),
    ES("se fusionaron {0} modelos en {1} en {2} s (fusiones: {3}, "
       "rechazadas: {4})"),
    PT("{0} modelos fundidos em {1} em {2} s (fusões: {3}, recusadas: {4})"),
    IT("{0} modelli fusi in {1} in {2} s (fusioni: {3}, rifiutate: {4})"),
    NL("{0} modellen samengevoegd tot {1} in {2} s (samenvoegingen: {3}, "
       "geweigerd: {4})"),
    RU("{0} моделей слито в {1} за {2} с (слияний: {3}, отклонено: {4})"),
    TR("{0} model {1} tanesine birleştirildi, {2} sn (birleştirme: {3}, "
       "reddedilen: {4})"));

SS_MSG(merge_ba_seconds,
    EN("bundle adjustment: {0} s"),
    JA("バンドル調整: {0} 秒"),
    ZH_HANS("光束法平差：{0} 秒"),
    ZH_HANT("光束法平差：{0} 秒"),
    KO("번들 조정: {0}초"),
    DE("Bündelausgleich: {0} s"),
    FR("ajustement de faisceaux : {0} s"),
    ES("ajuste de haces: {0} s"),
    PT("ajuste de feixes: {0} s"),
    IT("bundle adjustment: {0} s"),
    NL("bundelaanpassing: {0} s"),
    RU("уравнивание блока: {0} с"),
    TR("demet dengelemesi: {0} sn"));

SS_MSG(merge_model_line,
    EN("model {0} -- images: {1}, points: {2}, mean error: {3} px (median {4}, "
       "observations: {5})"),
    JA("モデル {0} -- 画像: {1}、点: {2}、平均誤差: {3} px（中央値 {4}、観測: {5}）"),
    ZH_HANS("模型 {0} —— 图像：{1}，点：{2}，平均误差：{3} px（中位数 {4}，观测：{5}）"),
    ZH_HANT("模型 {0} —— 影像：{1}，點：{2}，平均誤差：{3} px（中位數 {4}，觀測：{5}）"),
    KO("모델 {0} -- 이미지: {1}, 점: {2}, 평균 오차: {3} px(중앙값 {4}, 관측: {5})"),
    DE("Modell {0} -- Bilder: {1}, Punkte: {2}, mittlerer Fehler: {3} px "
       "(Median {4}, Beobachtungen: {5})"),
    FR("modèle {0} -- images : {1}, points : {2}, erreur moyenne : {3} px "
       "(médiane {4}, observations : {5})"),
    ES("modelo {0}: imágenes: {1}, puntos: {2}, error medio: {3} px "
       "(mediana {4}, observaciones: {5})"),
    PT("modelo {0} -- imagens: {1}, pontos: {2}, erro médio: {3} px "
       "(mediana {4}, observações: {5})"),
    IT("modello {0} -- immagini: {1}, punti: {2}, errore medio: {3} px "
       "(mediana {4}, osservazioni: {5})"),
    NL("model {0} -- beelden: {1}, punten: {2}, gemiddelde fout: {3} px "
       "(mediaan {4}, waarnemingen: {5})"),
    RU("модель {0} -- изображения: {1}, точки: {2}, средняя ошибка: {3} px "
       "(медиана {4}, наблюдений: {5})"),
    TR("model {0} -- görüntü: {1}, nokta: {2}, ortalama hata: {3} px "
       "(ortanca {4}, gözlem: {5})"));

SS_MSG(merge_survived,
    EN("distinct images that survived the merge: {0}/{1}"),
    JA("統合後に残った異なる画像: {0}/{1}"),
    ZH_HANS("合并后留下的不同图像：{0}/{1}"),
    ZH_HANT("合併後留下的不同影像：{0}/{1}"),
    KO("병합 후 남은 서로 다른 이미지: {0}/{1}"),
    DE("verschiedene Bilder, die das Zusammenführen überstanden haben: {0}/{1}"),
    FR("images distinctes ayant survécu à la fusion : {0}/{1}"),
    ES("imágenes distintas que sobrevivieron a la fusión: {0}/{1}"),
    PT("imagens distintas que sobreviveram à fusão: {0}/{1}"),
    IT("immagini distinte sopravvissute alla fusione: {0}/{1}"),
    NL("verschillende beelden die de samenvoeging overleefden: {0}/{1}"),
    RU("различных изображений, переживших слияние: {0}/{1}"),
    TR("birleştirmeden sağ çıkan farklı görüntü: {0}/{1}"));

SS_MSG(merge_removed_absorbed,
    EN("removed {0}, which was absorbed"),
    JA("吸収された {0} を取り除きました"),
    ZH_HANS("已移除被吸收的 {0}"),
    ZH_HANT("已移除被吸收的 {0}"),
    KO("흡수된 {0} 을(를) 없앴습니다"),
    DE("{0} entfernt, es wurde aufgenommen"),
    FR("{0} supprimé, il a été absorbé"),
    ES("se eliminó {0}, que quedó absorbido"),
    PT("{0} removido, pois foi absorvido"),
    IT("rimosso {0}, che è stato assorbito"),
    NL("{0} verwijderd, het is opgenomen"),
    RU("удалён {0}: он был поглощён"),
    TR("emilen {0} kaldırıldı"));

SS_MSG(map_camera_setup_from_db,
    EN("camera setup taken from {0}, as verification recorded it"),
    JA("カメラ構成は {0} から、検証が記録したとおりに読み込みました"),
    ZH_HANS("相机配置取自 {0}，与验证记录的一致"),
    ZH_HANT("相機組態取自 {0}，與驗證記錄的一致"),
    KO("카메라 구성은 검증이 기록한 대로 {0} 에서 가져왔습니다"),
    DE("Kameraaufbau aus {0} übernommen, so wie die Prüfung ihn festhielt"),
    FR("configuration des caméras reprise de {0}, telle que la vérification "
       "l'a notée"),
    ES("configuración de cámaras tomada de {0}, tal como la anotó la "
       "verificación"),
    PT("configuração de câmeras tirada de {0}, tal como a verificação anotou"),
    IT("configurazione delle camere presa da {0}, come l'ha annotata la "
       "verifica"),
    NL("camera-opzet overgenomen uit {0}, zoals de verificatie die noteerde"),
    RU("настройка камер взята из {0} в том виде, в каком её записала проверка"),
    TR("kamera düzeni, doğrulamanın kaydettiği haliyle {0} dosyasından alındı"));

SS_MSG(map_camera_setup_rebuilt,
    EN("camera setup rebuilt from the command line, ignoring the one in {0}"),
    JA("カメラ構成をコマンドラインから作り直しました（{0} のものは無視します）"),
    ZH_HANS("相机配置按命令行重建，忽略 {0} 中的那份"),
    ZH_HANT("相機組態按命令列重建，忽略 {0} 中的那份"),
    KO("카메라 구성을 명령줄에서 다시 만들었습니다({0} 의 것은 무시)"),
    DE("Kameraaufbau von der Kommandozeile neu erstellt; der in {0} bleibt "
       "unberücksichtigt"),
    FR("configuration des caméras reconstruite depuis la ligne de commande, en "
       "ignorant celle de {0}"),
    ES("configuración de cámaras rehecha desde la línea de órdenes, sin tener "
       "en cuenta la de {0}"),
    PT("configuração de câmeras refeita a partir da linha de comando, ignorando "
       "a de {0}"),
    IT("configurazione delle camere ricostruita dalla riga di comando, "
       "ignorando quella in {0}"),
    NL("camera-opzet opnieuw opgebouwd vanaf de opdrachtregel; die in {0} "
       "blijft buiten beschouwing"),
    RU("настройка камер собрана заново из командной строки; та, что в {0}, не "
       "учитывается"),
    TR("kamera düzeni komut satırından yeniden kuruldu; {0} içindeki yok "
       "sayıldı"));

SS_MSG(map_reconstruction_summary,
    EN("reconstruction -- registered images: {0}/{1}, 3D points: {2}, mean "
       "reprojection: {3} px (median {4}, observations: {5})"),
    JA("再構成 -- 登録された画像: {0}/{1}、3D 点: {2}、平均再投影: {3} px"
       "（中央値 {4}、観測: {5}）"),
    ZH_HANS("重建 —— 已注册图像：{0}/{1}，三维点：{2}，平均重投影：{3} px"
            "（中位数 {4}，观测：{5}）"),
    ZH_HANT("重建 —— 已註冊影像：{0}/{1}，三維點：{2}，平均重投影：{3} px"
            "（中位數 {4}，觀測：{5}）"),
    KO("재구성 -- 등록된 이미지: {0}/{1}, 3D 점: {2}, 평균 재투영: {3} px"
       "(중앙값 {4}, 관측: {5})"),
    DE("Rekonstruktion -- registrierte Bilder: {0}/{1}, 3D-Punkte: {2}, "
       "mittlere Rückprojektion: {3} px (Median {4}, Beobachtungen: {5})"),
    FR("reconstruction -- images enregistrées : {0}/{1}, points 3D : {2}, "
       "reprojection moyenne : {3} px (médiane {4}, observations : {5})"),
    ES("reconstrucción: imágenes registradas: {0}/{1}, puntos 3D: {2}, "
       "reproyección media: {3} px (mediana {4}, observaciones: {5})"),
    PT("reconstrução -- imagens registradas: {0}/{1}, pontos 3D: {2}, "
       "reprojeção média: {3} px (mediana {4}, observações: {5})"),
    IT("ricostruzione -- immagini registrate: {0}/{1}, punti 3D: {2}, "
       "riproiezione media: {3} px (mediana {4}, osservazioni: {5})"),
    NL("reconstructie -- geregistreerde beelden: {0}/{1}, 3D-punten: {2}, "
       "gemiddelde herprojectie: {3} px (mediaan {4}, waarnemingen: {5})"),
    RU("реконструкция -- зарегистрировано изображений: {0}/{1}, точек 3D: {2}, "
       "средняя репроекция: {3} px (медиана {4}, наблюдений: {5})"),
    TR("yeniden oluşturma -- kayıtlı görüntü: {0}/{1}, 3B nokta: {2}, ortalama "
       "yeniden izdüşüm: {3} px (ortanca {4}, gözlem: {5})"));

SS_MSG(map_models_covering,
    EN("models: {0}, covering distinct images: {1}/{2}"),
    JA("モデル: {0}、対象となる異なる画像: {1}/{2}"),
    ZH_HANS("模型：{0}，覆盖的不同图像：{1}/{2}"),
    ZH_HANT("模型：{0}，涵蓋的不同影像：{1}/{2}"),
    KO("모델: {0}, 포함하는 서로 다른 이미지: {1}/{2}"),
    DE("Modelle: {0}, erfasste verschiedene Bilder: {1}/{2}"),
    FR("modèles : {0}, images distinctes couvertes : {1}/{2}"),
    ES("modelos: {0}, imágenes distintas cubiertas: {1}/{2}"),
    PT("modelos: {0}, imagens distintas cobertas: {1}/{2}"),
    IT("modelli: {0}, immagini distinte coperte: {1}/{2}"),
    NL("modellen: {0}, gedekte verschillende beelden: {1}/{2}"),
    RU("моделей: {0}, охвачено различных изображений: {1}/{2}"),
    TR("model: {0}, kapsanan farklı görüntü: {1}/{2}"));

SS_MSG(merge_rebundled,
    EN("model {0} re-bundled (cost {1}); filtered observations: {2}, points: "
       "{3}; points remaining: {4}"),
    JA("モデル {0} を再バンドル調整しました（コスト {1}）。除いた観測: {2}、"
       "点: {3}、残った点: {4}"),
    ZH_HANS("模型 {0} 已重新平差（代价 {1}）；滤除观测：{2}，点：{3}；剩余点：{4}"),
    ZH_HANT("模型 {0} 已重新平差（代價 {1}）；濾除觀測：{2}，點：{3}；剩餘點：{4}"),
    KO("모델 {0} 을(를) 다시 번들 조정했습니다(비용 {1}). 걸러낸 관측: {2}, "
       "점: {3}; 남은 점: {4}"),
    DE("Modell {0} neu ausgeglichen (Kosten {1}); gefilterte Beobachtungen: "
       "{2}, Punkte: {3}; verbleibende Punkte: {4}"),
    FR("modèle {0} réajusté (coût {1}) ; observations filtrées : {2}, "
       "points : {3} ; points restants : {4}"),
    ES("modelo {0} reajustado (coste {1}); observaciones filtradas: {2}, "
       "puntos: {3}; puntos restantes: {4}"),
    PT("modelo {0} reajustado (custo {1}); observações filtradas: {2}, "
       "pontos: {3}; pontos restantes: {4}"),
    IT("modello {0} riaggiustato (costo {1}); osservazioni filtrate: {2}, "
       "punti: {3}; punti rimasti: {4}"),
    NL("model {0} opnieuw aangepast (kosten {1}); gefilterde waarnemingen: {2}, "
       "punten: {3}; resterende punten: {4}"),
    RU("модель {0} переуравнена (стоимость {1}); отфильтровано наблюдений: {2}, "
       "точек: {3}; осталось точек: {4}"),
    TR("model {0} yeniden dengelendi (maliyet {1}); elenen gözlem: {2}, "
       "nokta: {3}; kalan nokta: {4}"));

}  // namespace sfm
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
