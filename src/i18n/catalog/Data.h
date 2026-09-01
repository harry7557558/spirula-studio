#pragma once

// What reading a dataset says about it, and what preparing one reports.
//
// These lines come out of src/data/ -- the COLMAP, Nerfstudio and Metashape
// parsers, and the image loader behind them. They are printed by whatever is
// running at the time: `spirula train` in a terminal, or the same code inside
// the GUI, where they land in the log panel. Either way the reader is the
// person who pointed the program at the folder, so they are translated.
//
// Two things here stay as they are in every language, and both are deliberate:
//
//   FILE NAMES AND PATHS. They name something on disk. A translated path is a
//   path that cannot be pasted back.
//
//   FORMAT AND FIELD NAMES -- COLMAP, Metashape, EQUIRECTANGULAR, sparse/0.
//   They are what the other tool calls the thing, and what the reader will
//   search its documentation for.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace data {

// ===========================================================================
// The two words that qualify a line
// ===========================================================================

SS_MSG(word_warning,
    EN("WARNING:"),
    JA("警告:"),
    ZH_HANS("警告："),
    ZH_HANT("警告："),
    KO("경고:"),
    DE("WARNUNG:"),
    FR("AVERTISSEMENT :"),
    ES("AVISO:"),
    PT("AVISO:"),
    IT("AVVISO:"),
    NL("WAARSCHUWING:"),
    RU("ПРЕДУПРЕЖДЕНИЕ:"),
    TR("UYARI:"));

SS_MSG(word_error,
    EN("error:"),
    JA("エラー:"),
    ZH_HANS("错误："),
    ZH_HANT("錯誤："),
    KO("오류:"),
    DE("Fehler:"),
    FR("erreur :"),
    ES("error:"),
    PT("erro:"),
    IT("errore:"),
    NL("fout:"),
    RU("ошибка:"),
    TR("hata:"));

// ===========================================================================
// COLMAP
// ===========================================================================

SS_MSG(colmap_models_found,
    EN("COLMAP models under {1}: {0}; using {2} (registered images: {3})"),
    JA("{1} の下にある COLMAP モデル: {0}。{2} を使います（登録済み画像: {3}）"),
    ZH_HANS("{1} 下的 COLMAP 模型：{0}；采用 {2}（已注册图像：{3}）"),
    ZH_HANT("{1} 下的 COLMAP 模型：{0}；採用 {2}（已註冊影像：{3}）"),
    KO("{1} 아래의 COLMAP 모델: {0}; {2} 을(를) 씁니다(등록된 이미지: {3})"),
    DE("COLMAP-Modelle unter {1}: {0}; verwendet wird {2} (registrierte "
       "Bilder: {3})"),
    FR("modèles COLMAP sous {1} : {0} ; on prend {2} (images enregistrées : {3})"),
    ES("modelos COLMAP bajo {1}: {0}; se usa {2} (imágenes registradas: {3})"),
    PT("modelos COLMAP sob {1}: {0}; usando {2} (imagens registradas: {3})"),
    IT("modelli COLMAP sotto {1}: {0}; si usa {2} (immagini registrate: {3})"),
    NL("COLMAP-modellen onder {1}: {0}; {2} wordt gebruikt (geregistreerde "
       "beelden: {3})"),
    RU("модели COLMAP в {1}: {0}; берётся {2} (зарегистрированные "
       "изображения: {3})"),
    TR("{1} altındaki COLMAP modelleri: {0}; {2} kullanılıyor (kayıtlı "
       "görüntü: {3})"));

SS_MSG(equirect_not_2to1,
    EN("the EQUIRECTANGULAR camera {0} is {1}x{2}, which is not 2:1. The "
       "engine assumes a full 360x180 panorama, so it will use the wrong "
       "vertical scale."),
    JA("EQUIRECTANGULAR カメラ {0} は {1}x{2} で、2:1 ではありません。エンジンは "
       "360x180 の全天球パノラマを前提にするため、垂直方向の尺度が誤ります。"),
    ZH_HANS("EQUIRECTANGULAR 相机 {0} 是 {1}x{2}，并非 2:1。引擎假设这是完整的 "
            "360x180 全景，因而会用错垂直方向的尺度。"),
    ZH_HANT("EQUIRECTANGULAR 相機 {0} 是 {1}x{2}，並非 2:1。引擎假設這是完整的 "
            "360x180 全景，因而會用錯垂直方向的尺度。"),
    KO("EQUIRECTANGULAR 카메라 {0} 은(는) {1}x{2} 로 2:1 이 아닙니다. 엔진은 "
       "360x180 전체 파노라마를 가정하므로 수직 방향 배율이 틀어집니다."),
    DE("die EQUIRECTANGULAR-Kamera {0} ist {1}x{2} und damit nicht 2:1. Die "
       "Engine setzt ein volles 360x180-Panorama voraus und rechnet daher mit "
       "der falschen vertikalen Skala."),
    FR("la caméra EQUIRECTANGULAR {0} fait {1}x{2}, ce qui n'est pas du 2:1. "
       "Le moteur suppose un panorama complet 360x180 et utilisera donc la "
       "mauvaise échelle verticale."),
    ES("la cámara EQUIRECTANGULAR {0} es de {1}x{2}, que no es 2:1. El motor "
       "supone un panorama completo de 360x180, así que usará la escala "
       "vertical equivocada."),
    PT("a câmera EQUIRECTANGULAR {0} é {1}x{2}, o que não é 2:1. O motor supõe "
       "um panorama completo de 360x180, então vai usar a escala vertical "
       "errada."),
    IT("la camera EQUIRECTANGULAR {0} è {1}x{2}, che non è 2:1. Il motore "
       "presuppone un panorama completo 360x180 e userà quindi la scala "
       "verticale sbagliata."),
    NL("de EQUIRECTANGULAR-camera {0} is {1}x{2} en dus niet 2:1. De engine "
       "gaat uit van een volledig 360x180-panorama en gebruikt daardoor de "
       "verkeerde verticale schaal."),
    RU("камера EQUIRECTANGULAR {0} имеет размер {1}x{2}, а это не 2:1. Движок "
       "исходит из полной панорамы 360x180 и потому возьмёт неверный "
       "вертикальный масштаб."),
    TR("EQUIRECTANGULAR kamera {0} {1}x{2} boyutunda, yani 2:1 değil. Motor tam "
       "bir 360x180 panorama varsaydığı için dikey ölçeği yanlış alacak."));

SS_MSG(camera_model_fitted,
    EN("no exact match here for the {0} camera model ({1} in this dataset); "
       "fitted as {2} with {3} distortion, max error {4} px. Its images are "
       "re-distorted to match."),
    JA("{0} カメラモデル（このデータセットに {1} 台）には厳密に一致するものがないため、"
       "{2} と {3} 歪みで近似しました（最大誤差 {4} px）。画像はそれに合わせて"
       "再歪曲されます。"),
    ZH_HANS("{0} 相机模型（本数据集中 {1} 台）没有精确对应的模型，已用 {2} 与 {3} "
            "畸变拟合，最大误差 {4} 像素。其图像会相应重新畸变。"),
    ZH_HANT("{0} 相機模型（本資料集中 {1} 台）沒有精確對應的模型，已用 {2} 與 {3} "
            "畸變擬合，最大誤差 {4} 像素。其影像會相應重新畸變。"),
    KO("{0} 카메라 모델(이 데이터셋에 {1} 대)은 정확히 일치하는 모델이 없어 {2} 와 "
       "{3} 왜곡으로 근사했습니다. 최대 오차 {4} px. 해당 이미지는 그에 맞게 다시 "
       "왜곡됩니다."),
    ES("el modelo de cámara {0} ({1} en este conjunto) no tiene equivalente "
       "exacto; ajustado como {2} con distorsión {3}, error máximo {4} px. Sus "
       "imágenes se redistorsionan."),
    FR("le modèle de caméra {0} ({1} dans ce jeu de données) n'a pas "
       "d'équivalent exact ; ajusté en {2} avec la distorsion {3}, erreur "
       "maximale {4} px. Ses images sont redistordues."),
    DE("für das Kameramodell {0} ({1} in diesem Datensatz) gibt es hier keine "
       "exakte Entsprechung; angepasst als {2} mit {3}-Verzeichnung, maximaler "
       "Fehler {4} px. Ihre Bilder werden entsprechend neu verzeichnet."),
    PT("o modelo de câmera {0} ({1} neste conjunto) não tem equivalente exato; "
       "ajustado como {2} com distorção {3}, erro máximo {4} px. As imagens são "
       "redistorcidas."),
    IT("il modello di camera {0} ({1} in questo dataset) non ha un equivalente "
       "esatto; approssimato come {2} con distorsione {3}, errore massimo {4} "
       "px. Le immagini vengono ridistorte di conseguenza."),
    NL("het cameramodel {0} ({1} in deze dataset) heeft hier geen exacte "
       "tegenhanger; benaderd als {2} met {3}-vervorming, maximale fout {4} px. "
       "De beelden worden opnieuw vervormd."),
    RU("модель камеры {0} ({1} в этом наборе) не имеет точного соответствия; "
       "приближена как {2} с искажением {3}, максимальная ошибка {4} пикс. Её "
       "изображения переискажаются соответственно."),
    TR("{0} kamera modeli (bu veri kumesinde {1} adet) icin birebir karsilik "
       "yok; {2} ve {3} bozulmasi ile yaklasildi, en buyuk hata {4} px. "
       "Goruntuleri buna gore yeniden bozuluyor."));

SS_MSG(camera_model_fitted_exact,
    EN("no exact match here for the {0} camera model ({1} in this dataset); "
       "fitted as {2} with {3} distortion to within {4} px, so its images are "
       "used as they are."),
    JA("{0} カメラモデル（このデータセットに {1} 台）には厳密に一致するものがないため、"
       "{2} と {3} 歪みで近似しました（誤差 {4} px 以内）。画像はそのまま使います。"),
    ZH_HANS("{0} 相机模型（本数据集中 {1} 台）没有精确对应的模型，已用 {2} 与 {3} "
            "畸变拟合，误差在 {4} 像素以内，因此其图像按原样使用。"),
    ZH_HANT("{0} 相機模型（本資料集中 {1} 台）沒有精確對應的模型，已用 {2} 與 {3} "
            "畸變擬合，誤差在 {4} 像素以內，因此其影像按原樣使用。"),
    KO("{0} 카메라 모델(이 데이터셋에 {1} 대)은 정확히 일치하는 모델이 없어 {2} 와 "
       "{3} 왜곡으로 근사했습니다. 오차 {4} px 이내이므로 이미지는 그대로 사용합니다."),
    ES("el modelo de cámara {0} ({1} en este conjunto) no tiene equivalente "
       "exacto; ajustado como {2} con distorsión {3} con un error de {4} px, "
       "así que sus imágenes se usan tal cual."),
    FR("le modèle de caméra {0} ({1} dans ce jeu de données) n'a pas "
       "d'équivalent exact ; ajusté en {2} avec la distorsion {3} à {4} px "
       "près, donc ses images sont utilisées telles quelles."),
    DE("für das Kameramodell {0} ({1} in diesem Datensatz) gibt es hier keine "
       "exakte Entsprechung; angepasst als {2} mit {3}-Verzeichnung auf {4} px "
       "genau, ihre Bilder werden daher unverändert verwendet."),
    PT("o modelo de câmera {0} ({1} neste conjunto) não tem equivalente exato; "
       "ajustado como {2} com distorção {3} com erro de {4} px, então as "
       "imagens são usadas como estão."),
    IT("il modello di camera {0} ({1} in questo dataset) non ha un equivalente "
       "esatto; approssimato come {2} con distorsione {3} entro {4} px, quindi "
       "le immagini sono usate così come sono."),
    NL("het cameramodel {0} ({1} in deze dataset) heeft hier geen exacte "
       "tegenhanger; benaderd als {2} met {3}-vervorming tot op {4} px, dus de "
       "beelden worden ongewijzigd gebruikt."),
    RU("модель камеры {0} ({1} в этом наборе) не имеет точного соответствия; "
       "приближена как {2} с искажением {3} с точностью {4} пикс., поэтому её "
       "изображения используются как есть."),
    TR("{0} kamera modeli (bu veri kumesinde {1} adet) icin birebir karsilik "
       "yok; {2} ve {3} bozulmasi ile {4} px hata payiyla yaklasildi, bu "
       "yuzden goruntuleri oldugu gibi kullaniliyor."));

SS_MSG(camera_sensor_skew,
    EN("the camera for {0} has a {1} px sensor skew, which no supported camera "
       "model carries; its images are re-distorted onto a {2} camera with {3} "
       "distortion, max error {4} px."),
    JA("{0} のカメラには {1} px のセンサースキューがあり、対応するモデルがありません。"
       "画像は {2} カメラ（{3} 歪み）に合わせて再歪曲します（最大誤差 {4} px）。"),
    ZH_HANS("{0} 的相机存在 {1} 像素的传感器切变，没有支持的相机模型能表示；其图像会"
            "重新畸变到 {2} 相机与 {3} 畸变上，最大误差 {4} 像素。"),
    ZH_HANT("{0} 的相機存在 {1} 像素的感光元件切變，沒有支援的相機模型能表示；其影像會"
            "重新畸變到 {2} 相機與 {3} 畸變上，最大誤差 {4} 像素。"),
    KO("{0} 의 카메라에 {1} px 센서 스큐가 있는데 이를 담는 카메라 모델이 없습니다. "
       "이미지는 {2} 카메라와 {3} 왜곡에 맞춰 다시 왜곡됩니다. 최대 오차 {4} px."),
    ES("la cámara de {0} tiene una inclinación de sensor de {1} px que ningún "
       "modelo admitido representa; sus imágenes se redistorsionan a una cámara "
       "{2} con distorsión {3}, error máximo {4} px."),
    FR("la caméra de {0} a un cisaillement de capteur de {1} px qu'aucun modèle "
       "pris en charge ne porte ; ses images sont redistordues vers une caméra "
       "{2} avec la distorsion {3}, erreur maximale {4} px."),
    DE("die Kamera von {0} hat eine Sensorscherung von {1} px, die kein "
       "unterstütztes Kameramodell trägt; ihre Bilder werden auf eine "
       "{2}-Kamera mit {3}-Verzeichnung umverzeichnet, maximaler Fehler {4} px."),
    PT("a câmera de {0} tem uma inclinação de sensor de {1} px que nenhum modelo "
       "suportado carrega; suas imagens são redistorcidas para uma câmera {2} "
       "com distorção {3}, erro máximo {4} px."),
    IT("la camera di {0} ha uno scorrimento del sensore di {1} px che nessun "
       "modello supportato porta; le sue immagini vengono ridistorte su una "
       "camera {2} con distorsione {3}, errore massimo {4} px."),
    NL("de camera van {0} heeft een sensorschuining van {1} px die geen enkel "
       "ondersteund cameramodel draagt; de beelden worden opnieuw vervormd naar "
       "een {2}-camera met {3}-vervorming, maximale fout {4} px."),
    RU("камера для {0} имеет наклон матрицы {1} пикс., которого нет ни в одной "
       "поддерживаемой модели; её изображения переискажаются в камеру {2} с "
       "искажением {3}, максимальная ошибка {4} пикс."),
    TR("{0} icin kamerada {1} px sensor egimi var, bunu tasiyan desteklenen bir "
       "kamera modeli yok; goruntuleri {3} bozulmali bir {2} kameraya yeniden "
       "bozuluyor, en buyuk hata {4} px."));

SS_MSG(camera_model_fit_failed,
    EN("the {0} camera model ({1} in this dataset) projects nothing this reader "
       "understands, so it could not be fitted; falling back to a plain {2}. "
       "What is reconstructed from those cameras will be wrong."),
    JA("{0} カメラモデル（このデータセットに {1} 台）はこのリーダーが解釈できる投影を"
       "返さず、近似できませんでした。素の {2} で代用します。これらのカメラからの"
       "復元は正しくなりません。"),
    ZH_HANS("{0} 相机模型（本数据集中 {1} 台）未给出本读取器能理解的投影，无法拟合；"
            "改用普通的 {2}。由这些相机重建的结果会是错的。"),
    ZH_HANT("{0} 相機模型（本資料集中 {1} 台）未給出本讀取器能理解的投影，無法擬合；"
            "改用普通的 {2}。由這些相機重建的結果會是錯的。"),
    KO("{0} 카메라 모델(이 데이터셋에 {1} 대)은 이 리더가 이해할 수 있는 투영을 "
       "내놓지 않아 근사할 수 없었습니다. 기본 {2} 로 대체합니다. 이 카메라들로부터의 "
       "복원은 잘못됩니다."),
    ES("el modelo de cámara {0} ({1} en este conjunto) no proyecta nada que "
       "este lector entienda, así que no se pudo ajustar; se usa una {2} "
       "simple. Lo que se reconstruya desde esas cámaras estará mal."),
    FR("le modèle de caméra {0} ({1} dans ce jeu de données) ne projette rien "
       "que ce lecteur comprenne, il n'a donc pas pu être ajusté ; repli sur "
       "une {2} simple. Ce qui sera reconstruit depuis ces caméras sera faux."),
    DE("das Kameramodell {0} ({1} in diesem Datensatz) liefert keine Projektion, "
       "die dieser Leser versteht, und ließ sich daher nicht anpassen; "
       "ersatzweise eine schlichte {2}. Was aus diesen Kameras rekonstruiert "
       "wird, ist falsch."),
    PT("o modelo de câmera {0} ({1} neste conjunto) não projeta nada que este "
       "leitor entenda, então não pôde ser ajustado; usando uma {2} simples. O "
       "que for reconstruído a partir dessas câmeras ficará errado."),
    IT("il modello di camera {0} ({1} in questo dataset) non proietta nulla che "
       "questo lettore capisca, quindi non è stato possibile approssimarlo; si "
       "ripiega su una {2} semplice. Ciò che viene ricostruito da queste camere "
       "sarà sbagliato."),
    NL("het cameramodel {0} ({1} in deze dataset) projecteert niets wat deze "
       "lezer begrijpt en kon dus niet benaderd worden; er wordt teruggevallen "
       "op een gewone {2}. Wat uit die camera's gereconstrueerd wordt, klopt "
       "niet."),
    RU("модель камеры {0} ({1} в этом наборе) не даёт проекции, понятной этому "
       "загрузчику, поэтому приблизить её не удалось; берётся обычная {2}. "
       "Всё, что будет восстановлено с этих камер, окажется неверным."),
    TR("{0} kamera modeli (bu veri kumesinde {1} adet) bu okuyucunun anladigi "
       "bir izdusum vermiyor, bu yuzden yaklasilamadi; duz bir {2} "
       "kullaniliyor. Bu kameralardan yeniden olusturulan sey yanlis olacak."));

SS_MSG(colmap_parse_failed,
    EN("the COLMAP data could not be read: {0}"),
    JA("COLMAP のデータを読めませんでした: {0}"),
    ZH_HANS("无法读取 COLMAP 数据：{0}"),
    ZH_HANT("無法讀取 COLMAP 資料：{0}"),
    KO("COLMAP 데이터를 읽지 못했습니다: {0}"),
    DE("die COLMAP-Daten ließen sich nicht lesen: {0}"),
    FR("les données COLMAP n'ont pas pu être lues : {0}"),
    ES("no se pudieron leer los datos de COLMAP: {0}"),
    PT("não foi possível ler os dados do COLMAP: {0}"),
    IT("non è stato possibile leggere i dati COLMAP: {0}"),
    NL("de COLMAP-gegevens konden niet gelezen worden: {0}"),
    RU("не удалось прочитать данные COLMAP: {0}"),
    TR("COLMAP verileri okunamadı: {0}"));

SS_MSG(trying_metashape,
    EN("trying to read the folder as Metashape data instead..."),
    JA("代わりに Metashape のデータとして読んでみます..."),
    ZH_HANS("改按 Metashape 数据读取试试……"),
    ZH_HANT("改按 Metashape 資料讀取試試……"),
    KO("대신 Metashape 데이터로 읽어 봅니다..."),
    DE("stattdessen wird der Ordner als Metashape-Daten gelesen ..."),
    FR("nouvelle tentative : lecture du dossier en données Metashape..."),
    ES("se intenta leer la carpeta como datos de Metashape..."),
    PT("tentando ler a pasta como dados do Metashape..."),
    IT("si prova a leggere la cartella come dati Metashape..."),
    NL("de map wordt in plaats daarvan als Metashape-gegevens gelezen..."),
    RU("пробуем прочитать папку как данные Metashape..."),
    TR("klasör bunun yerine Metashape verisi olarak okunmaya çalışılıyor..."));

// ===========================================================================
// Nerfstudio
// ===========================================================================

SS_MSG(image_format_unsupported,
    EN("{0} is in an image format this build cannot read; skipping it"),
    JA("{0} はこのビルドが読めない画像形式です。飛ばします"),
    ZH_HANS("{0} 的图像格式本版本无法读取，跳过"),
    ZH_HANT("{0} 的影像格式本版本無法讀取，略過"),
    KO("{0} 은(는) 이 빌드가 읽지 못하는 이미지 형식입니다. 건너뜁니다"),
    DE("{0} liegt in einem Bildformat vor, das dieser Build nicht lesen kann; "
       "wird übersprungen"),
    FR("{0} est dans un format d'image que cette version ne sait pas lire ; "
       "ignoré"),
    ES("{0} tiene un formato de imagen que esta compilación no sabe leer; se "
       "omite"),
    PT("{0} está num formato de imagem que esta compilação não consegue ler; "
       "será ignorado"),
    IT("{0} è in un formato di immagine che questa build non sa leggere; viene "
       "saltato"),
    NL("{0} heeft een beeldformaat dat deze build niet kan lezen; wordt "
       "overgeslagen"),
    RU("{0} записан в формате изображения, который эта сборка не читает; "
       "пропускается"),
    TR("{0} bu derlemenin okuyamadığı bir görüntü biçiminde; atlanıyor"));

SS_MSG(image_missing,
    EN("{0} does not exist; skipping it"),
    JA("{0} が存在しません。飛ばします"),
    ZH_HANS("{0} 不存在，跳过"),
    ZH_HANT("{0} 不存在，略過"),
    KO("{0} 이(가) 없습니다. 건너뜁니다"),
    DE("{0} gibt es nicht; wird übersprungen"),
    FR("{0} n'existe pas ; ignoré"),
    ES("{0} no existe; se omite"),
    PT("{0} não existe; será ignorado"),
    IT("{0} non esiste; viene saltato"),
    NL("{0} bestaat niet; wordt overgeslagen"),
    RU("{0} не существует; пропускается"),
    TR("{0} yok; atlanıyor"));

// ===========================================================================
// Metashape
// ===========================================================================

SS_MSG(ms_using_file,
    EN("using the {0} file found at {1}"),
    JA("見つかった {0} ファイル {1} を使います"),
    ZH_HANS("使用找到的 {0} 文件：{1}"),
    ZH_HANT("使用找到的 {0} 檔案：{1}"),
    KO("찾은 {0} 파일 {1} 을(를) 씁니다"),
    DE("die gefundene {0}-Datei {1} wird verwendet"),
    FR("utilisation du fichier {0} trouvé : {1}"),
    ES("se usa el archivo {0} encontrado: {1}"),
    PT("usando o arquivo {0} encontrado: {1}"),
    IT("si usa il file {0} trovato: {1}"),
    NL("het gevonden {0}-bestand {1} wordt gebruikt"),
    RU("используется найденный файл {0}: {1}"),
    TR("bulunan {0} dosyası kullanılıyor: {1}"));

SS_MSG(ms_no_camera_table,
    EN("no camera table under {0}; falling back to matching by label"),
    JA("{0} の下にカメラ表がありません。ラベルによる照合に切り替えます"),
    ZH_HANS("{0} 下没有相机表，改用按标签匹配"),
    ZH_HANT("{0} 下沒有相機表，改用按標籤比對"),
    KO("{0} 아래에 카메라 표가 없습니다. 라벨로 맞추는 방식으로 넘어갑니다"),
    DE("keine Kameratabelle unter {0}; es wird über die Beschriftung zugeordnet"),
    FR("aucune table de caméras sous {0} ; on retombe sur l'appariement par "
       "étiquette"),
    ES("no hay tabla de cámaras bajo {0}; se recurre a emparejar por etiqueta"),
    PT("não há tabela de câmeras sob {0}; recorrendo à correspondência por "
       "rótulo"),
    IT("nessuna tabella delle camere sotto {0}; si ripiega sull'abbinamento per "
       "etichetta"),
    NL("geen cameratabel onder {0}; er wordt op label gematcht"),
    RU("под {0} нет таблицы камер; переходим к сопоставлению по метке"),
    TR("{0} altında kamera tablosu yok; etikete göre eşlemeye dönülüyor"));

SS_MSG(ms_camera_not_in_table,
    EN("the camera {0} is not in the project's camera table; skipping it"),
    JA("カメラ {0} はプロジェクトのカメラ表にありません。飛ばします"),
    ZH_HANS("相机 {0} 不在项目的相机表中，跳过"),
    ZH_HANT("相機 {0} 不在專案的相機表中，略過"),
    KO("카메라 {0} 이(가) 프로젝트의 카메라 표에 없습니다. 건너뜁니다"),
    DE("die Kamera {0} steht nicht in der Kameratabelle des Projekts; wird "
       "übersprungen"),
    FR("la caméra {0} n'est pas dans la table de caméras du projet ; ignorée"),
    ES("la cámara {0} no está en la tabla de cámaras del proyecto; se omite"),
    PT("a câmera {0} não está na tabela de câmeras do projeto; será ignorada"),
    IT("la camera {0} non è nella tabella delle camere del progetto; viene "
       "saltata"),
    NL("de camera {0} staat niet in de cameratabel van het project; wordt "
       "overgeslagen"),
    RU("камеры {0} нет в таблице камер проекта; пропускается"),
    TR("{0} kamerası projenin kamera tablosunda yok; atlanıyor"));

SS_MSG(ms_ambiguous,
    EN("the file name for {0} is ambiguous (candidates: {1}); skipping it"),
    JA("{0} に対応するファイル名が一意に決まりません（候補: {1}）。飛ばします"),
    ZH_HANS("{0} 对应的文件名不唯一（候选：{1}），跳过"),
    ZH_HANT("{0} 對應的檔名不唯一（候選：{1}），略過"),
    KO("{0} 의 파일 이름이 하나로 정해지지 않습니다(후보: {1}). 건너뜁니다"),
    DE("der Dateiname für {0} ist mehrdeutig (Kandidaten: {1}); wird "
       "übersprungen"),
    FR("le nom de fichier pour {0} est ambigu (candidats : {1}) ; ignoré"),
    ES("el nombre de archivo de {0} es ambiguo (candidatos: {1}); se omite"),
    PT("o nome de arquivo de {0} é ambíguo (candidatos: {1}); será ignorado"),
    IT("il nome di file per {0} è ambiguo (candidati: {1}); viene saltato"),
    NL("de bestandsnaam voor {0} is dubbelzinnig (kandidaten: {1}); wordt "
       "overgeslagen"),
    RU("имя файла для {0} неоднозначно (кандидаты: {1}); пропускается"),
    TR("{0} için dosya adı belirsiz (aday: {1}); atlanıyor"));

// The same case, with the one thing that would resolve it. A separate message
// rather than a tail glued onto the previous one: the clause lands in a
// different place in a verb-final language.
SS_MSG(ms_ambiguous_psx,
    EN("the file name for {0} is ambiguous (candidates: {1}); skipping it. A "
       "Metashape .psx project file would resolve it."),
    JA("{0} に対応するファイル名が一意に決まりません（候補: {1}）。飛ばします。"
       "Metashape の .psx プロジェクトファイルがあれば解決できます。"),
    ZH_HANS("{0} 对应的文件名不唯一（候选：{1}），跳过。提供 Metashape 的 .psx "
            "工程文件即可消除歧义。"),
    ZH_HANT("{0} 對應的檔名不唯一（候選：{1}），略過。提供 Metashape 的 .psx "
            "專案檔即可消除歧義。"),
    KO("{0} 의 파일 이름이 하나로 정해지지 않습니다(후보: {1}). 건너뜁니다. "
       "Metashape 의 .psx 프로젝트 파일이 있으면 해결됩니다."),
    DE("der Dateiname für {0} ist mehrdeutig (Kandidaten: {1}); wird "
       "übersprungen. Eine Metashape-.psx-Projektdatei würde das auflösen."),
    FR("le nom de fichier pour {0} est ambigu (candidats : {1}) ; ignoré. Un "
       "fichier de projet Metashape .psx lèverait l'ambiguïté."),
    ES("el nombre de archivo de {0} es ambiguo (candidatos: {1}); se omite. Un "
       "archivo de proyecto .psx de Metashape lo resolvería."),
    PT("o nome de arquivo de {0} é ambíguo (candidatos: {1}); será ignorado. Um "
       "arquivo de projeto .psx do Metashape resolveria isso."),
    IT("il nome di file per {0} è ambiguo (candidati: {1}); viene saltato. Un "
       "file di progetto Metashape .psx lo risolverebbe."),
    NL("de bestandsnaam voor {0} is dubbelzinnig (kandidaten: {1}); wordt "
       "overgeslagen. Een Metashape-.psx-projectbestand zou dit oplossen."),
    RU("имя файла для {0} неоднозначно (кандидаты: {1}); пропускается. Файл "
       "проекта Metashape .psx снял бы неоднозначность."),
    TR("{0} için dosya adı belirsiz (aday: {1}); atlanıyor. Bir Metashape .psx "
       "proje dosyası bunu çözerdi."));

SS_MSG(ms_missing_sensor,
    EN("no sensor calibration for {0}; skipping it"),
    JA("{0} のセンサーキャリブレーションがありません。飛ばします"),
    ZH_HANS("{0} 没有传感器标定，跳过"),
    ZH_HANT("{0} 沒有感測器標定，略過"),
    KO("{0} 의 센서 보정값이 없습니다. 건너뜁니다"),
    DE("keine Sensorkalibrierung für {0}; wird übersprungen"),
    FR("aucun étalonnage de capteur pour {0} ; ignoré"),
    ES("no hay calibración de sensor para {0}; se omite"),
    PT("não há calibração de sensor para {0}; será ignorado"),
    IT("nessuna calibrazione del sensore per {0}; viene saltato"),
    NL("geen sensorkalibratie voor {0}; wordt overgeslagen"),
    RU("нет калибровки датчика для {0}; пропускается"),
    TR("{0} için sensör kalibrasyonu yok; atlanıyor"));

SS_MSG(ms_missing_transform,
    EN("no transform data for {0}; skipping it"),
    JA("{0} の変換データがありません。飛ばします"),
    ZH_HANS("{0} 没有变换数据，跳过"),
    ZH_HANT("{0} 沒有變換資料，略過"),
    KO("{0} 의 변환 데이터가 없습니다. 건너뜁니다"),
    DE("keine Transformationsdaten für {0}; wird übersprungen"),
    FR("aucune donnée de transformation pour {0} ; ignoré"),
    ES("no hay datos de transformación para {0}; se omite"),
    PT("não há dados de transformação para {0}; será ignorado"),
    IT("nessun dato di trasformazione per {0}; viene saltato"),
    NL("geen transformatiegegevens voor {0}; wordt overgeslagen"),
    RU("нет данных преобразования для {0}; пропускается"),
    TR("{0} için dönüşüm verisi yok; atlanıyor"));

SS_MSG(ms_malformed_transform,
    EN("the transform for {0} is malformed; skipping it"),
    JA("{0} の変換データが壊れています。飛ばします"),
    ZH_HANS("{0} 的变换数据格式有误，跳过"),
    ZH_HANT("{0} 的變換資料格式有誤，略過"),
    KO("{0} 의 변환 데이터가 잘못되었습니다. 건너뜁니다"),
    DE("die Transformation für {0} ist fehlerhaft; wird übersprungen"),
    FR("la transformation pour {0} est mal formée ; ignoré"),
    ES("la transformación de {0} está mal formada; se omite"),
    PT("a transformação de {0} está malformada; será ignorado"),
    IT("la trasformazione per {0} è malformata; viene saltato"),
    NL("de transformatie voor {0} is misvormd; wordt overgeslagen"),
    RU("преобразование для {0} испорчено; пропускается"),
    TR("{0} için dönüşüm bozuk; atlanıyor"));

SS_MSG(ms_components,
    EN("components in the Metashape export: {0}. Training on the largest ({1} "
       "of {2} aligned cameras). Tidy the project up if that is not what you "
       "want."),
    JA("Metashape の書き出しにある成分: {0}。最大の成分で学習します"
       "（整列済みカメラ {2} 台のうち {1} 台）。意図と違う場合はプロジェクトを"
       "整理してください。"),
    ZH_HANS("Metashape 导出中的分块：{0}。将用最大的一个训练（{2} 台已对齐相机中的 "
            "{1} 台）。若这不是你想要的，请先整理工程。"),
    ZH_HANT("Metashape 匯出中的分塊：{0}。將用最大的一個訓練（{2} 台已對齊相機中的 "
            "{1} 台）。若這不是你想要的，請先整理專案。"),
    KO("Metashape 내보내기의 성분: {0}. 가장 큰 것으로 학습합니다(정렬된 카메라 "
       "{2}대 중 {1}대). 원하는 바가 아니라면 프로젝트를 정리하세요."),
    DE("Komponenten im Metashape-Export: {0}. Trainiert wird auf der größten "
       "({1} von {2} ausgerichteten Kameras). Räumen Sie das Projekt auf, wenn "
       "das nicht gewollt ist."),
    FR("composantes dans l'export Metashape : {0}. L'entraînement porte sur la "
       "plus grande ({1} caméras alignées sur {2}). Rangez le projet si ce "
       "n'est pas ce que vous vouliez."),
    ES("componentes en la exportación de Metashape: {0}. Se entrena con la "
       "mayor ({1} de {2} cámaras alineadas). Ordena el proyecto si no es lo "
       "que querías."),
    PT("componentes na exportação do Metashape: {0}. O treino usa o maior ({1} "
       "de {2} câmeras alinhadas). Arrume o projeto se não for isso que você "
       "queria."),
    IT("componenti nell'esportazione Metashape: {0}. Si addestra sulla più "
       "grande ({1} di {2} camere allineate). Metta in ordine il progetto se "
       "non è ciò che voleva."),
    NL("componenten in de Metashape-export: {0}. Er wordt op de grootste "
       "getraind ({1} van {2} uitgelijnde camera's). Ruim het project op als "
       "dat niet de bedoeling is."),
    RU("компоненты в экспорте Metashape: {0}. Обучение идёт на самой большой "
       "({1} из {2} выровненных камер). Приведите проект в порядок, если это не "
       "то, что нужно."),
    TR("Metashape dışa aktarımındaki bileşenler: {0}. En büyüğüyle eğitiliyor "
       "({2} hizalanmış kameradan {1} tanesi). İstediğiniz bu değilse projeyi "
       "toparlayın."));

SS_MSG(ms_skipped,
    EN("images skipped: {0}"),
    JA("飛ばした画像: {0}"),
    ZH_HANS("跳过的图像：{0}"),
    ZH_HANT("略過的影像：{0}"),
    KO("건너뛴 이미지: {0}"),
    DE("übersprungene Bilder: {0}"),
    FR("images ignorées : {0}"),
    ES("imágenes omitidas: {0}"),
    PT("imagens ignoradas: {0}"),
    IT("immagini saltate: {0}"),
    NL("overgeslagen beelden: {0}"),
    RU("пропущено изображений: {0}"),
    TR("atlanan görüntü: {0}"));

SS_MSG(final_frames,
    EN("frames in the finished dataset: {0}"),
    JA("できあがったデータセットのフレーム数: {0}"),
    ZH_HANS("最终数据集的帧数：{0}"),
    ZH_HANT("最終資料集的影格數：{0}"),
    KO("완성된 데이터셋의 프레임 수: {0}"),
    DE("Einzelbilder im fertigen Datensatz: {0}"),
    FR("images du jeu de données final : {0}"),
    ES("fotogramas del conjunto de datos final: {0}"),
    PT("quadros no conjunto de dados final: {0}"),
    IT("fotogrammi nel dataset finito: {0}"),
    NL("beelden in de voltooide dataset: {0}"),
    RU("кадров в готовом наборе данных: {0}"),
    TR("tamamlanan veri kümesindeki kare: {0}"));

// ===========================================================================
// Fitting the cameras to the images
// ===========================================================================

SS_MSG(camera_fit_to_image,
    EN("'{0}' is {1}, but its camera says {2}. Training at {3}. Further "
       "messages for this pair are suppressed."),
    JA("'{0}' は {1} ですが、カメラは {2} と言っています。{3} で学習します。"
       "この組み合わせの以降のメッセージは出しません。"),
    ZH_HANS("'{0}' 是 {1}，但其相机记为 {2}。将以 {3} 训练。"
            "该组合的后续消息不再输出。"),
    ZH_HANT("'{0}' 是 {1}，但其相機記為 {2}。將以 {3} 訓練。"
            "該組合的後續訊息不再輸出。"),
    KO("'{0}' 은(는) {1} 인데 카메라는 {2} 라고 합니다. {3} 으로 학습합니다. "
       "이 조합의 이후 메시지는 생략합니다."),
    DE("'{0}' ist {1}, seine Kamera sagt aber {2}. Training läuft mit {3}. "
       "Weitere Meldungen für diese Kombination entfallen."),
    FR("'{0}' fait {1}, mais sa caméra annonce {2}. L'entraînement se fera en "
       "{3}. Les messages suivants pour cette combinaison sont supprimés."),
    ES("'{0}' es de {1}, pero su cámara dice {2}. Se entrenará a {3}. Se omiten "
       "los mensajes siguientes de esta combinación."),
    PT("'{0}' é {1}, mas a sua câmera diz {2}. O treino será em {3}. As "
       "mensagens seguintes desta combinação são omitidas."),
    IT("'{0}' è {1}, ma la sua camera dice {2}. L'addestramento userà {3}. I "
       "messaggi successivi per questa combinazione sono soppressi."),
    NL("'{0}' is {1}, maar zijn camera zegt {2}. Er wordt getraind op {3}. "
       "Verdere meldingen voor deze combinatie vervallen."),
    RU("'{0}' имеет размер {1}, а его камера говорит {2}. Обучение пойдёт в "
       "{3}. Дальнейшие сообщения по этой паре подавлены."),
    TR("'{0}' {1}, ama kamerası {2} diyor. Eğitim {3} çözünürlüğünde "
       "yapılacak. Bu ikili için sonraki iletiler bastırılıyor."));

SS_MSG(camera_image_aspect,
    EN("'{0}' is {1} and its camera {2} -- a different shape, not just a "
       "different size. Check that the images belong to this reconstruction."),
    JA("'{0}' は {1}、カメラは {2} で、大きさだけでなく縦横比も違います。"
       "画像がこの復元結果のものか確認してください。"),
    ZH_HANS("'{0}' 是 {1}，其相机是 {2}：不只是尺寸不同，长宽比也不同。"
            "请确认这些图像属于该重建结果。"),
    ZH_HANT("'{0}' 是 {1}，其相機是 {2}：不只是尺寸不同，長寬比也不同。"
            "請確認這些影像屬於該重建結果。"),
    KO("'{0}' 은(는) {1}, 카메라는 {2} 입니다. 크기뿐 아니라 가로세로 비율도 "
       "다릅니다. 이 이미지가 해당 복원 결과의 것인지 확인하세요."),
    DE("'{0}' ist {1}, seine Kamera {2} -- nicht nur eine andere Größe, "
       "sondern ein anderes Seitenverhältnis. Prüfen Sie, ob die Bilder zu "
       "dieser Rekonstruktion gehören."),
    FR("'{0}' fait {1} et sa caméra {2} : le rapport d'aspect diffère, pas "
       "seulement la taille. Vérifiez que les images correspondent à cette "
       "reconstruction."),
    ES("'{0}' es de {1} y su cámara de {2}: cambia la relación de aspecto, no "
       "solo el tamaño. Compruebe que las imágenes pertenecen a esta "
       "reconstrucción."),
    PT("'{0}' é {1} e a sua câmera {2}: muda a proporção, não apenas o "
       "tamanho. Verifique se as imagens pertencem a esta reconstrução."),
    IT("'{0}' è {1} e la sua camera {2}: cambia il rapporto d'aspetto, non "
       "solo la dimensione. Verifica che le immagini appartengano a questa "
       "ricostruzione."),
    NL("'{0}' is {1} en zijn camera {2}: een andere beeldverhouding, niet "
       "alleen een ander formaat. Controleer of de beelden bij deze "
       "reconstructie horen."),
    RU("'{0}' имеет размер {1}, а его камера {2}: отличается не только "
       "размер, но и соотношение сторон. Проверьте, относятся ли эти "
       "изображения к данной реконструкции."),
    TR("'{0}' {1}, kamerası ise {2}: yalnızca boyut değil, en-boy oranı da "
       "farklı. Görüntülerin bu rekonstrüksiyona ait olduğunu doğrulayın."));


// ===========================================================================
// Loading the images
// ===========================================================================

SS_MSG(rgb_shape_mismatch,
    EN("'{0}' is {1}x{2} but its camera says {3}x{4}. The file is being resized "
       "to match. Further warnings for this group are suppressed."),
    JA("'{0}' は {1}x{2} ですが、カメラは {3}x{4} と言っています。合わせるために"
       "ファイル側を拡大縮小します。このグループの以降の警告は出しません。"),
    ZH_HANS("'{0}' 是 {1}x{2}，但其相机记为 {3}x{4}。将缩放文件以匹配。"
            "该组的后续警告不再输出。"),
    ZH_HANT("'{0}' 是 {1}x{2}，但其相機記為 {3}x{4}。將縮放檔案以符合。"
            "該組的後續警告不再輸出。"),
    KO("'{0}' 은(는) {1}x{2} 인데 카메라는 {3}x{4} 라고 합니다. 맞추기 위해 "
       "파일 쪽을 크기 조정합니다. 이 그룹의 이후 경고는 생략합니다."),
    DE("'{0}' ist {1}x{2}, seine Kamera sagt aber {3}x{4}. Die Datei wird "
       "passend skaliert. Weitere Warnungen für diese Gruppe entfallen."),
    FR("'{0}' fait {1}x{2}, mais sa caméra annonce {3}x{4}. Le fichier est "
       "redimensionné pour correspondre. Les avertissements suivants pour ce "
       "groupe sont supprimés."),
    ES("'{0}' es de {1}x{2}, pero su cámara dice {3}x{4}. Se redimensiona el "
       "archivo para que cuadre. Se omiten los avisos siguientes de este grupo."),
    PT("'{0}' é {1}x{2}, mas a sua câmera diz {3}x{4}. O arquivo está sendo "
       "redimensionado para bater. Os avisos seguintes deste grupo são omitidos."),
    IT("'{0}' è {1}x{2}, ma la sua camera dice {3}x{4}. Il file viene "
       "ridimensionato per farlo combaciare. Gli avvisi successivi per questo "
       "gruppo sono soppressi."),
    NL("'{0}' is {1}x{2}, maar zijn camera zegt {3}x{4}. Het bestand wordt "
       "passend geschaald. Verdere waarschuwingen voor deze groep vervallen."),
    RU("'{0}' имеет размер {1}x{2}, а его камера говорит {3}x{4}. Файл "
       "масштабируется под камеру. Дальнейшие предупреждения по этой группе "
       "подавлены."),
    TR("'{0}' {1}x{2}, ama kamerası {3}x{4} diyor. Dosya uyacak şekilde yeniden "
       "boyutlandırılıyor. Bu grup için sonraki uyarılar bastırılıyor."));

SS_MSG(mask_shape_mismatch,
    EN("mask shapes differ within a {0}x{1} group; upsampling them to {2}x{3} "
       "(bilinear, then nearest). Further mask warnings are suppressed."),
    JA("{0}x{1} のグループ内でマスクの形が揃っていません。{2}x{3} に拡大します"
       "（バイリニアのあと最近傍）。以降のマスク警告は出しません。"),
    ZH_HANS("{0}x{1} 组内的掩码尺寸不一致，将上采样到 {2}x{3}（先双线性后最近邻）。"
            "后续掩码警告不再输出。"),
    ZH_HANT("{0}x{1} 群組內的遮罩尺寸不一致，將上採樣到 {2}x{3}（先雙線性後最近鄰）。"
            "後續遮罩警告不再輸出。"),
    KO("{0}x{1} 그룹 안에서 마스크 크기가 다릅니다. {2}x{3} 으로 키웁니다"
       "(이중선형 후 최근접). 이후 마스크 경고는 생략합니다."),
    DE("die Maskenformate innerhalb einer {0}x{1}-Gruppe stimmen nicht überein; "
       "sie werden auf {2}x{3} hochgerechnet (bilinear, dann nächster Nachbar). "
       "Weitere Maskenwarnungen entfallen."),
    FR("les tailles de masque diffèrent au sein d'un groupe {0}x{1} ; "
       "suréchantillonnage vers {2}x{3} (bilinéaire, puis plus proche voisin). "
       "Les avertissements de masque suivants sont supprimés."),
    ES("las máscaras de un grupo {0}x{1} tienen tamaños distintos; se amplían a "
       "{2}x{3} (bilineal y luego vecino más cercano). Se omiten los avisos "
       "siguientes sobre máscaras."),
    PT("as máscaras de um grupo {0}x{1} têm tamanhos diferentes; serão ampliadas "
       "para {2}x{3} (bilinear e depois vizinho mais próximo). Os avisos "
       "seguintes sobre máscaras são omitidos."),
    IT("le maschere in un gruppo {0}x{1} hanno dimensioni diverse; vengono "
       "ingrandite a {2}x{3} (bilineare, poi vicino più prossimo). Gli avvisi "
       "successivi sulle maschere sono soppressi."),
    NL("de maskergroottes binnen een {0}x{1}-groep verschillen; ze worden "
       "opgeschaald naar {2}x{3} (bilineair, dan dichtstbijzijnde). Verdere "
       "maskerwaarschuwingen vervallen."),
    RU("в группе {0}x{1} размеры масок различаются; они увеличиваются до {2}x{3} "
       "(билинейно, затем ближайший сосед). Дальнейшие предупреждения о масках "
       "подавлены."),
    TR("{0}x{1} grubunda maske boyutları farklı; {2}x{3} boyutuna büyütülüyor "
       "(çift doğrusal, sonra en yakın komşu). Sonraki maske uyarıları "
       "bastırılıyor."));

SS_MSG(depth_shape_mismatch,
    EN("depth shapes differ within a {0}x{1} group; upsampling them to {2}x{3} "
       "(bilinear). Further depth warnings are suppressed."),
    JA("{0}x{1} のグループ内で深度の形が揃っていません。{2}x{3} に拡大します"
       "（バイリニア）。以降の深度警告は出しません。"),
    ZH_HANS("{0}x{1} 组内的深度图尺寸不一致，将上采样到 {2}x{3}（双线性）。"
            "后续深度警告不再输出。"),
    ZH_HANT("{0}x{1} 群組內的深度圖尺寸不一致，將上採樣到 {2}x{3}（雙線性）。"
            "後續深度警告不再輸出。"),
    KO("{0}x{1} 그룹 안에서 깊이 크기가 다릅니다. {2}x{3} 으로 키웁니다"
       "(이중선형). 이후 깊이 경고는 생략합니다."),
    DE("die Tiefenformate innerhalb einer {0}x{1}-Gruppe stimmen nicht überein; "
       "sie werden auf {2}x{3} hochgerechnet (bilinear). Weitere "
       "Tiefenwarnungen entfallen."),
    FR("les tailles de profondeur diffèrent au sein d'un groupe {0}x{1} ; "
       "suréchantillonnage vers {2}x{3} (bilinéaire). Les avertissements de "
       "profondeur suivants sont supprimés."),
    ES("los mapas de profundidad de un grupo {0}x{1} tienen tamaños distintos; "
       "se amplían a {2}x{3} (bilineal). Se omiten los avisos siguientes sobre "
       "profundidad."),
    PT("os mapas de profundidade de um grupo {0}x{1} têm tamanhos diferentes; "
       "serão ampliados para {2}x{3} (bilinear). Os avisos seguintes sobre "
       "profundidade são omitidos."),
    IT("le profondità in un gruppo {0}x{1} hanno dimensioni diverse; vengono "
       "ingrandite a {2}x{3} (bilineare). Gli avvisi successivi sulla "
       "profondità sono soppressi."),
    NL("de dieptegroottes binnen een {0}x{1}-groep verschillen; ze worden "
       "opgeschaald naar {2}x{3} (bilineair). Verdere dieptewaarschuwingen "
       "vervallen."),
    RU("в группе {0}x{1} размеры карт глубины различаются; они увеличиваются до "
       "{2}x{3} (билинейно). Дальнейшие предупреждения о глубине подавлены."),
    TR("{0}x{1} grubunda derinlik boyutları farklı; {2}x{3} boyutuna büyütülüyor "
       "(çift doğrusal). Sonraki derinlik uyarıları bastırılıyor."));

SS_MSG(normal_shape_mismatch,
    EN("normal-map shapes differ within a {0}x{1} group; upsampling them to "
       "{2}x{3} (bilinear). Further normal-map warnings are suppressed."),
    JA("{0}x{1} のグループ内で法線マップの形が揃っていません。{2}x{3} に拡大します"
       "（バイリニア）。以降の法線マップ警告は出しません。"),
    ZH_HANS("{0}x{1} 组内的法线图尺寸不一致，将上采样到 {2}x{3}（双线性）。"
            "后续法线图警告不再输出。"),
    ZH_HANT("{0}x{1} 群組內的法線圖尺寸不一致，將上採樣到 {2}x{3}（雙線性）。"
            "後續法線圖警告不再輸出。"),
    KO("{0}x{1} 그룹 안에서 법선 맵 크기가 다릅니다. {2}x{3} 으로 키웁니다"
       "(이중선형). 이후 법선 맵 경고는 생략합니다."),
    DE("die Normalenkarten innerhalb einer {0}x{1}-Gruppe stimmen im Format "
       "nicht überein; sie werden auf {2}x{3} hochgerechnet (bilinear). Weitere "
       "Warnungen zu Normalenkarten entfallen."),
    FR("les tailles de carte de normales diffèrent au sein d'un groupe {0}x{1} ; "
       "suréchantillonnage vers {2}x{3} (bilinéaire). Les avertissements "
       "suivants sur les normales sont supprimés."),
    ES("los mapas de normales de un grupo {0}x{1} tienen tamaños distintos; se "
       "amplían a {2}x{3} (bilineal). Se omiten los avisos siguientes sobre "
       "normales."),
    PT("os mapas de normais de um grupo {0}x{1} têm tamanhos diferentes; serão "
       "ampliados para {2}x{3} (bilinear). Os avisos seguintes sobre normais "
       "são omitidos."),
    IT("le mappe di normali in un gruppo {0}x{1} hanno dimensioni diverse; "
       "vengono ingrandite a {2}x{3} (bilineare). Gli avvisi successivi sulle "
       "normali sono soppressi."),
    NL("de normaalkaarten binnen een {0}x{1}-groep verschillen in grootte; ze "
       "worden opgeschaald naar {2}x{3} (bilineair). Verdere waarschuwingen "
       "over normaalkaarten vervallen."),
    RU("в группе {0}x{1} размеры карт нормалей различаются; они увеличиваются до "
       "{2}x{3} (билинейно). Дальнейшие предупреждения о картах нормалей "
       "подавлены."),
    TR("{0}x{1} grubunda normal haritası boyutları farklı; {2}x{3} boyutuna "
       "büyütülüyor (çift doğrusal). Sonraki normal haritası uyarıları "
       "bastırılıyor."));

// Redrawn in place on one line, so it stays short.
SS_MSG(loading_images,
    EN("Loading images {0}/{1}"),
    JA("画像を読み込み中 {0}/{1}"),
    ZH_HANS("正在加载图像 {0}/{1}"),
    ZH_HANT("正在載入影像 {0}/{1}"),
    KO("이미지 불러오는 중 {0}/{1}"),
    DE("Bilder werden geladen {0}/{1}"),
    FR("Chargement des images {0}/{1}"),
    ES("Cargando imágenes {0}/{1}"),
    PT("Carregando imagens {0}/{1}"),
    IT("Caricamento immagini {0}/{1}"),
    NL("Beelden laden {0}/{1}"),
    RU("Загрузка изображений {0}/{1}"),
    TR("Görüntüler yükleniyor {0}/{1}"));

// ===========================================================================
// Preparing a dataset from a video: the timing table
// ===========================================================================
// A label column and a value column. The labels are padded to a common
// display width at print time (i18n::pad_to), so a language whose words are
// longer -- or twice as wide, in CJK -- still lines its numbers up.

SS_MSG(xs_decoded,
    EN("frames decoded"),
    JA("デコードしたフレーム"),
    ZH_HANS("已解码的帧"),
    ZH_HANT("已解碼的影格"),
    KO("디코딩한 프레임"),
    DE("dekodierte Einzelbilder"),
    FR("images décodées"),
    ES("fotogramas descodificados"),
    PT("quadros decodificados"),
    IT("fotogrammi decodificati"),
    NL("gedecodeerde beelden"),
    RU("декодировано кадров"),
    TR("çözülen kare"));

SS_MSG(xs_measured,
    EN("frames measured"),
    JA("測定したフレーム"),
    ZH_HANS("已评估的帧"),
    ZH_HANT("已評估的影格"),
    KO("측정한 프레임"),
    DE("gemessene Einzelbilder"),
    FR("images mesurées"),
    ES("fotogramas medidos"),
    PT("quadros medidos"),
    IT("fotogrammi misurati"),
    NL("gemeten beelden"),
    RU("измерено кадров"),
    TR("ölçülen kare"));

SS_MSG(xs_written,
    EN("frames written"),
    JA("書き出したフレーム"),
    ZH_HANS("已写出的帧"),
    ZH_HANT("已寫出的影格"),
    KO("저장한 프레임"),
    DE("geschriebene Einzelbilder"),
    FR("images écrites"),
    ES("fotogramas escritos"),
    PT("quadros escritos"),
    IT("fotogrammi scritti"),
    NL("geschreven beelden"),
    RU("записано кадров"),
    TR("yazılan kare"));

SS_MSG(xs_decode,
    EN("decode"),
    JA("デコード"),
    ZH_HANS("解码"),
    ZH_HANT("解碼"),
    KO("디코딩"),
    DE("Dekodierung"),
    FR("décodage"),
    ES("descodificación"),
    PT("decodificação"),
    IT("decodifica"),
    NL("decoderen"),
    RU("декодирование"),
    TR("çözme"));

SS_MSG(xs_sharpness,
    EN("sharpness"),
    JA("鮮鋭度"),
    ZH_HANS("清晰度"),
    ZH_HANT("清晰度"),
    KO("선명도"),
    DE("Schärfe"),
    FR("netteté"),
    ES("nitidez"),
    PT("nitidez"),
    IT("nitidezza"),
    NL("scherpte"),
    RU("резкость"),
    TR("keskinlik"));

SS_MSG(xs_convert,
    EN("convert and download"),
    JA("変換とダウンロード"),
    ZH_HANS("转换与回传"),
    ZH_HANT("轉換與回傳"),
    KO("변환과 내려받기"),
    DE("Umwandlung und Übertragung"),
    FR("conversion et transfert"),
    ES("conversión y transferencia"),
    PT("conversão e transferência"),
    IT("conversione e trasferimento"),
    NL("omzetten en overhalen"),
    RU("преобразование и выгрузка"),
    TR("dönüştürme ve aktarma"));

SS_MSG(xs_segmentation,
    EN("segmentation"),
    JA("セグメンテーション"),
    ZH_HANS("分割"),
    ZH_HANT("分割"),
    KO("분할"),
    DE("Segmentierung"),
    FR("segmentation"),
    ES("segmentación"),
    PT("segmentação"),
    IT("segmentazione"),
    NL("segmentatie"),
    RU("сегментация"),
    TR("bölütleme"));

SS_MSG(xs_encode,
    EN("encode (threads: {0})"),
    JA("エンコード（スレッド: {0}）"),
    ZH_HANS("编码（线程：{0}）"),
    ZH_HANT("編碼（執行緒：{0}）"),
    KO("인코딩(스레드: {0})"),
    DE("Kodierung (Threads: {0})"),
    FR("encodage (threads : {0})"),
    ES("codificación (hilos: {0})"),
    PT("codificação (threads: {0})"),
    IT("codifica (thread: {0})"),
    NL("coderen (threads: {0})"),
    RU("кодирование (потоки: {0})"),
    TR("kodlama (iş parçacığı: {0})"));

SS_MSG(xs_stalled,
    EN("  stalled on the queue"),
    JA("  キュー待ち"),
    ZH_HANS("  队列等待"),
    ZH_HANT("  佇列等待"),
    KO("  큐 대기"),
    DE("  Warten an der Warteschlange"),
    FR("  attente sur la file"),
    ES("  espera en la cola"),
    PT("  espera na fila"),
    IT("  attesa in coda"),
    NL("  wachten op de wachtrij"),
    RU("  ожидание очереди"),
    TR("  kuyrukta bekleme"));

SS_MSG(xs_drain,
    EN("  drain at exit"),
    JA("  終了時の掃き出し"),
    ZH_HANS("  收尾清空"),
    ZH_HANT("  收尾清空"),
    KO("  종료 시 비우기"),
    DE("  Leerlaufen am Ende"),
    FR("  vidage à la sortie"),
    ES("  vaciado al salir"),
    PT("  esvaziamento ao sair"),
    IT("  svuotamento all'uscita"),
    NL("  leeglopen bij afsluiten"),
    RU("  дослив при выходе"),
    TR("  çıkışta boşaltma"));

SS_MSG(xs_total,
    EN("total"),
    JA("合計"),
    ZH_HANS("合计"),
    ZH_HANT("合計"),
    KO("합계"),
    DE("gesamt"),
    FR("total"),
    ES("total"),
    PT("total"),
    IT("totale"),
    NL("totaal"),
    RU("итого"),
    TR("toplam"));

SS_MSG(xs_ms,
    EN("{0} ms"),
    JA("{0} ミリ秒"),
    ZH_HANS("{0} 毫秒"),
    ZH_HANT("{0} 毫秒"),
    KO("{0} 밀리초"),
    DE("{0} ms"),
    FR("{0} ms"),
    ES("{0} ms"),
    PT("{0} ms"),
    IT("{0} ms"),
    NL("{0} ms"),
    RU("{0} мс"),
    TR("{0} ms"));

SS_MSG(xs_ms_fps,
    EN("{0} ms ({1} frames per second)"),
    JA("{0} ミリ秒（毎秒 {1} フレーム）"),
    ZH_HANS("{0} 毫秒（每秒 {1} 帧）"),
    ZH_HANT("{0} 毫秒（每秒 {1} 影格）"),
    KO("{0} 밀리초(초당 {1} 프레임)"),
    DE("{0} ms ({1} Einzelbilder je Sekunde)"),
    FR("{0} ms ({1} images par seconde)"),
    ES("{0} ms ({1} fotogramas por segundo)"),
    PT("{0} ms ({1} quadros por segundo)"),
    IT("{0} ms ({1} fotogrammi al secondo)"),
    NL("{0} ms ({1} beelden per seconde)"),
    RU("{0} мс ({1} кадров в секунду)"),
    TR("{0} ms (saniyede {1} kare)"));

SS_MSG(xs_ms_cpu,
    EN("{0} ms of CPU across the workers"),
    JA("{0} ミリ秒（ワーカー全体の CPU 時間）"),
    ZH_HANS("{0} 毫秒（各工作线程的 CPU 时间合计）"),
    ZH_HANT("{0} 毫秒（各工作執行緒的 CPU 時間合計）"),
    KO("{0} 밀리초(워커 전체의 CPU 시간)"),
    DE("{0} ms CPU-Zeit über alle Arbeiter"),
    FR("{0} ms de CPU sur l'ensemble des ouvriers"),
    ES("{0} ms de CPU en el conjunto de los trabajadores"),
    PT("{0} ms de CPU no conjunto dos trabalhadores"),
    IT("{0} ms di CPU sull'insieme dei worker"),
    NL("{0} ms CPU over alle werkers"),
    RU("{0} мс процессорного времени по всем рабочим потокам"),
    TR("{0} ms, tüm işçilerdeki CPU süresi"));

SS_MSG(xs_ms_written_rate,
    EN("{0} ms ({1} frames written per second)"),
    JA("{0} ミリ秒（毎秒 {1} フレームを書き出し）"),
    ZH_HANS("{0} 毫秒（每秒写出 {1} 帧）"),
    ZH_HANT("{0} 毫秒（每秒寫出 {1} 影格）"),
    KO("{0} 밀리초(초당 {1} 프레임 저장)"),
    DE("{0} ms ({1} geschriebene Einzelbilder je Sekunde)"),
    FR("{0} ms ({1} images écrites par seconde)"),
    ES("{0} ms ({1} fotogramas escritos por segundo)"),
    PT("{0} ms ({1} quadros escritos por segundo)"),
    IT("{0} ms ({1} fotogrammi scritti al secondo)"),
    NL("{0} ms ({1} geschreven beelden per seconde)"),
    RU("{0} мс ({1} кадров записывается в секунду)"),
    TR("{0} ms (saniyede {1} kare yazıldı)"));

SS_MSG(xs_written_to,
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

SS_MSG(write_failures,
    EN("images that failed to write: {0}"),
    JA("書き出しに失敗した画像: {0}"),
    ZH_HANS("写出失败的图像：{0}"),
    ZH_HANT("寫出失敗的影像：{0}"),
    KO("저장에 실패한 이미지: {0}"),
    DE("Bilder, die nicht geschrieben werden konnten: {0}"),
    FR("images qui n'ont pas pu être écrites : {0}"),
    ES("imágenes que no se pudieron escribir: {0}"),
    PT("imagens que não puderam ser escritas: {0}"),
    IT("immagini che non è stato possibile scrivere: {0}"),
    NL("beelden die niet geschreven konden worden: {0}"),
    RU("изображений не удалось записать: {0}"),
    TR("yazılamayan görüntü: {0}"));

// ===========================================================================
// One line from further down the stack that a normal run can still reach
// ===========================================================================

SS_MSG(vk_device_lacks_features,
    EN("the requested device '{0}' does not have the features this needs "
       "(Vulkan 1.2 with bufferDeviceAddress and timelineSemaphore)"),
    JA("指定されたデバイス '{0}' には必要な機能がありません"
       "（Vulkan 1.2 と bufferDeviceAddress、timelineSemaphore）"),
    ZH_HANS("所选设备 '{0}' 不具备所需特性（Vulkan 1.2，且需 bufferDeviceAddress "
            "与 timelineSemaphore）"),
    ZH_HANT("所選裝置 '{0}' 不具備所需特性（Vulkan 1.2，且需 bufferDeviceAddress "
            "與 timelineSemaphore）"),
    KO("요청한 장치 '{0}' 에는 필요한 기능이 없습니다"
       "(Vulkan 1.2 와 bufferDeviceAddress, timelineSemaphore)"),
    DE("das angeforderte Gerät '{0}' hat die benötigten Funktionen nicht "
       "(Vulkan 1.2 mit bufferDeviceAddress und timelineSemaphore)"),
    FR("le périphérique demandé '{0}' n'a pas les fonctions nécessaires "
       "(Vulkan 1.2 avec bufferDeviceAddress et timelineSemaphore)"),
    ES("el dispositivo solicitado '{0}' no tiene las funciones necesarias "
       "(Vulkan 1.2 con bufferDeviceAddress y timelineSemaphore)"),
    PT("o dispositivo pedido '{0}' não tem os recursos necessários (Vulkan 1.2 "
       "com bufferDeviceAddress e timelineSemaphore)"),
    IT("il dispositivo richiesto '{0}' non ha le funzioni necessarie (Vulkan "
       "1.2 con bufferDeviceAddress e timelineSemaphore)"),
    NL("het gevraagde apparaat '{0}' heeft de benodigde functies niet (Vulkan "
       "1.2 met bufferDeviceAddress en timelineSemaphore)"),
    RU("у запрошенного устройства '{0}' нет нужных возможностей (Vulkan 1.2 с "
       "bufferDeviceAddress и timelineSemaphore)"),
    TR("istenen aygıt '{0}' gereken özelliklere sahip değil (bufferDeviceAddress "
       "ve timelineSemaphore ile Vulkan 1.2)"));

// ===========================================================================
// Decode faults
// ===========================================================================

SS_MSG(file_gone,
    EN("{0} is no longer there — moved, renamed or deleted while the run was "
       "using it"),
    JA("{0} が見つかりません。実行中に移動・改名・削除されました"),
    ZH_HANS("{0} 已经不在了 — 运行期间它被移动、改名或删除了"),
    ZH_HANT("{0} 已經不在了 — 執行期間它被移動、改名或刪除了"),
    KO("{0} 이(가) 없습니다 — 실행 중에 옮겨졌거나 이름이 바뀌었거나 "
       "지워졌습니다"),
    DE("{0} ist nicht mehr da — während des Laufs verschoben, umbenannt oder "
       "gelöscht"),
    FR("{0} n'est plus là — déplacé, renommé ou supprimé pendant l'exécution"),
    ES("{0} ya no está — se movió, se renombró o se borró durante la "
       "ejecución"),
    PT("{0} já não está lá — foi movido, renomeado ou apagado durante a "
       "execução"),
    IT("{0} non c'è più — spostato, rinominato o eliminato durante "
       "l'esecuzione"),
    NL("{0} is er niet meer — tijdens de run verplaatst, hernoemd of "
       "verwijderd"),
    RU("{0} больше нет — файл переместили, переименовали или удалили во время "
       "прогона"),
    TR("{0} artık yok — çalışma sırasında taşınmış, adı değiştirilmiş ya da "
       "silinmiş"));

SS_MSG(file_unreadable,
    EN("{0} could not be read"),
    JA("{0} を読み込めませんでした"),
    ZH_HANS("无法读取 {0}"),
    ZH_HANT("無法讀取 {0}"),
    KO("{0} 을(를) 읽지 못했습니다"),
    DE("{0} konnte nicht gelesen werden"),
    FR("{0} n'a pas pu être lu"),
    ES("No se pudo leer {0}"),
    PT("Não foi possível ler {0}"),
    IT("Non è stato possibile leggere {0}"),
    NL("{0} kon niet worden gelezen"),
    RU("Не удалось "
       "прочитать {0}"),
    TR("{0} okunamadı"));

}  // namespace data
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
