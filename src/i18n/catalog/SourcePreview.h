#pragma once

// The dataset screen's input preview (app/gui/SourcePreview.h): a video's
// frames and clip, a folder's photos, a scan's points, and the GPS and IMU an
// input carries. Same two rules as Gui.h: no sentence built from fragments,
// no plural-sensitive counting.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace preview {

// ===========================================================================
// The column itself
// ===========================================================================

SS_MSG(pane_input,
    EN("Input"),
    JA("入力"),
    ZH_HANS("输入"),
    ZH_HANT("輸入"),
    KO("입력"),
    DE("Eingabe"),
    FR("Entrée"),
    ES("Entrada"),
    PT("Entrada"),
    IT("Ingresso"),
    NL("Invoer"),
    RU("Вход"),
    TR("Girdi"));

SS_MSG(pane_input_help,
    EN("The input selected in the list on the left: its frames or photos, its "
       "GPS and IMU, its points."),
    JA("左の一覧で選んだ入力です。そのフレームや写真、GPS と IMU、点群を表示しま"
       "す。"),
    ZH_HANS("左侧列表中选中的输入：它的帧或照片、GPS 和 IMU、点云。"),
    ZH_HANT("左側清單中選取的輸入：它的影格或相片、GPS 與 IMU、點雲。"),
    KO("왼쪽 목록에서 고른 입력입니다. 프레임이나 사진, GPS와 IMU, 점들을 보여 "
       "줍니다."),
    DE("Die links in der Liste gewählte Eingabe: ihre Bilder oder Fotos, ihr "
       "GPS und ihre IMU, ihre Punkte."),
    FR("L'entrée choisie dans la liste de gauche : ses images ou ses photos, "
       "son GPS et son IMU, ses points."),
    ES("La entrada elegida en la lista de la izquierda: sus fotogramas o fotos, "
       "su GPS e IMU, sus puntos."),
    PT("A entrada escolhida na lista à esquerda: seus quadros ou fotos, seu GPS "
       "e IMU, seus pontos."),
    IT("L'ingresso scelto nell'elenco a sinistra: i suoi fotogrammi o le sue "
       "foto, il suo GPS e la sua IMU, i suoi punti."),
    NL("De invoer die links in de lijst is gekozen: de beelden of foto's, de "
       "gps en IMU, de punten."),
    RU("Вход, выбранный в списке слева: его кадры или фотографии, GPS и IMU, "
       "точки."),
    TR("Soldaki listede seçilen girdi: kareleri ya da fotoğrafları, GPS ve IMU "
       "verisi, noktaları."));

SS_MSG(pane_run,
    EN("Run"),
    JA("実行"),
    ZH_HANS("运行"),
    ZH_HANT("執行"),
    KO("실행"),
    DE("Lauf"),
    FR("Exécution"),
    ES("Ejecución"),
    PT("Execução"),
    IT("Esecuzione"),
    NL("Uitvoering"),
    RU("Запуск"),
    TR("Çalıştırma"));

SS_MSG(pane_run_help,
    EN("What the run is producing: frames, masks, features, matches and the "
       "model."),
    JA("実行中の処理が生成しているものです。フレーム、マスク、特徴点、マッチ、モ"
       "デル。"),
    ZH_HANS("正在运行的任务产出的内容：帧、蒙版、特征点、匹配和模型。"),
    ZH_HANT("正在執行的工作產出的內容：影格、遮罩、特徵點、匹配與模型。"),
    KO("실행이 만들어 내는 것입니다. 프레임, 마스크, 특징점, 매칭, 모델."),
    DE("Was der Lauf erzeugt: Bilder, Masken, Merkmale, Zuordnungen und das "
       "Modell."),
    FR("Ce que l'exécution produit : images, masques, points, correspondances "
       "et le modèle."),
    ES("Lo que está produciendo la ejecución: fotogramas, máscaras, puntos, "
       "emparejamientos y el modelo."),
    PT("O que a execução está produzindo: quadros, máscaras, pontos, "
       "correspondências e o modelo."),
    IT("Ciò che l'esecuzione sta producendo: fotogrammi, maschere, punti, "
       "corrispondenze e il modello."),
    NL("Wat de uitvoering maakt: beelden, maskers, kenmerken, koppelingen en "
       "het model."),
    RU("То, что создаёт запуск: кадры, маски, особые точки, сопоставления и "
       "модель."),
    TR("Çalıştırmanın ürettikleri: kareler, maskeler, öznitelikler, eşleşmeler "
       "ve model."));

SS_MSG(tab_frames,
    EN("Video"),
    JA("動画"),
    ZH_HANS("视频"),
    ZH_HANT("影片"),
    KO("동영상"),
    DE("Video"),
    FR("Vidéo"),
    ES("Vídeo"),
    PT("Vídeo"),
    IT("Video"),
    NL("Video"),
    RU("Видео"),
    TR("Video"));

SS_MSG(tab_photos,
    EN("Photos"),
    JA("写真"),
    ZH_HANS("照片"),
    ZH_HANT("相片"),
    KO("사진"),
    DE("Fotos"),
    FR("Photos"),
    ES("Fotos"),
    PT("Fotos"),
    IT("Foto"),
    NL("Foto's"),
    RU("Фото"),
    TR("Fotoğraflar"));

SS_MSG(tab_sensors,
    EN("GPS & IMU"),
    JA("GPS と IMU"),
    ZH_HANS("GPS 与 IMU"),
    ZH_HANT("GPS 與 IMU"),
    KO("GPS와 IMU"),
    DE("GPS und IMU"),
    FR("GPS et IMU"),
    ES("GPS e IMU"),
    PT("GPS e IMU"),
    IT("GPS e IMU"),
    NL("Gps en IMU"),
    RU("GPS и IMU"),
    TR("GPS ve IMU"));

SS_MSG(tab_cloud,
    EN("3D"),
    JA("3D"),
    ZH_HANS("3D"),
    ZH_HANT("3D"),
    KO("3D"),
    DE("3D"),
    FR("3D"),
    ES("3D"),
    PT("3D"),
    IT("3D"),
    NL("3D"),
    RU("3D"),
    TR("3D"));

SS_MSG(tab_details,
    EN("Details"),
    JA("詳細"),
    ZH_HANS("详情"),
    ZH_HANT("詳細資訊"),
    KO("자세히"),
    DE("Details"),
    FR("Détails"),
    ES("Detalles"),
    PT("Detalhes"),
    IT("Dettagli"),
    NL("Details"),
    RU("Подробности"),
    TR("Ayrıntılar"));

SS_MSG(sum_video,
    EN("Length: {0} · {1} fps · {2}"),
    JA("長さ: {0} · {1} fps · {2}"),
    ZH_HANS("时长：{0} · {1} fps · {2}"),
    ZH_HANT("長度：{0} · {1} fps · {2}"),
    KO("길이: {0} · {1} fps · {2}"),
    DE("Länge: {0} · {1} fps · {2}"),
    FR("Durée : {0} · {1} fps · {2}"),
    ES("Duración: {0} · {1} fps · {2}"),
    PT("Duração: {0} · {1} fps · {2}"),
    IT("Durata: {0} · {1} fps · {2}"),
    NL("Duur: {0} · {1} fps · {2}"),
    RU("Длительность: {0} · {1} fps · {2}"),
    TR("Süre: {0} · {1} fps · {2}"));

SS_MSG(sum_video_streams,
    EN("Length: {0} · {1} fps · Streams: {2}, {3}"),
    JA("長さ: {0} · {1} fps · ストリーム: {2}、{3}"),
    ZH_HANS("时长：{0} · {1} fps · 视频流：{2}，{3}"),
    ZH_HANT("長度：{0} · {1} fps · 視訊串流：{2}，{3}"),
    KO("길이: {0} · {1} fps · 스트림: {2}, {3}"),
    DE("Länge: {0} · {1} fps · Videospuren: {2}, {3}"),
    FR("Durée : {0} · {1} fps · Flux : {2}, {3}"),
    ES("Duración: {0} · {1} fps · Pistas: {2}, {3}"),
    PT("Duração: {0} · {1} fps · Faixas: {2}, {3}"),
    IT("Durata: {0} · {1} fps · Flussi: {2}, {3}"),
    NL("Duur: {0} · {1} fps · Videosporen: {2}, {3}"),
    RU("Длительность: {0} · {1} fps · Видеопотоков: {2}, {3}"),
    TR("Süre: {0} · {1} fps · Video akışı: {2}, {3}"));

SS_MSG(sum_video_360,
    EN("360° video · Length: {0} · {1} fps"),
    JA("360° 動画 · 長さ: {0} · {1} fps"),
    ZH_HANS("360° 视频 · 时长：{0} · {1} fps"),
    ZH_HANT("360° 影片 · 長度：{0} · {1} fps"),
    KO("360° 동영상 · 길이: {0} · {1} fps"),
    DE("360°-Video · Länge: {0} · {1} fps"),
    FR("Vidéo 360° · Durée : {0} · {1} fps"),
    ES("Vídeo 360° · Duración: {0} · {1} fps"),
    PT("Vídeo 360° · Duração: {0} · {1} fps"),
    IT("Video 360° · Durata: {0} · {1} fps"),
    NL("360°-video · Duur: {0} · {1} fps"),
    RU("Видео 360° · Длительность: {0} · {1} fps"),
    TR("360° video · Süre: {0} · {1} fps"));

SS_MSG(sum_photos,
    EN("Photos: {0} · {1}"),
    JA("写真: {0} · {1}"),
    ZH_HANS("照片：{0} · {1}"),
    ZH_HANT("相片：{0} · {1}"),
    KO("사진: {0} · {1}"),
    DE("Fotos: {0} · {1}"),
    FR("Photos : {0} · {1}"),
    ES("Fotos: {0} · {1}"),
    PT("Fotos: {0} · {1}"),
    IT("Foto: {0} · {1}"),
    NL("Foto's: {0} · {1}"),
    RU("Фотографий: {0} · {1}"),
    TR("Fotoğraf: {0} · {1}"));

SS_MSG(sum_dataset,
    EN("Reconstruction found · Cameras: {0} · Points: {1}"),
    JA("再構成あり · カメラ: {0} · 点: {1}"),
    ZH_HANS("已有重建 · 相机：{0} · 点：{1}"),
    ZH_HANT("已有重建 · 相機：{0} · 點：{1}"),
    KO("재구성 있음 · 카메라: {0} · 점: {1}"),
    DE("Rekonstruktion vorhanden · Kameras: {0} · Punkte: {1}"),
    FR("Reconstruction présente · Caméras : {0} · Points : {1}"),
    ES("Reconstrucción encontrada · Cámaras: {0} · Puntos: {1}"),
    PT("Reconstrução encontrada · Câmeras: {0} · Pontos: {1}"),
    IT("Ricostruzione presente · Fotocamere: {0} · Punti: {1}"),
    NL("Reconstructie aanwezig · Camera's: {0} · Punten: {1}"),
    RU("Есть реконструкция · Камер: {0} · Точек: {1}"),
    TR("Yeniden oluşturma var · Kamera: {0} · Nokta: {1}"));

SS_MSG(sum_scan,
    EN("{0} scan · Points: {1} · Photos with a pose: {2}"),
    JA("{0} スキャン · 点: {1} · 位置姿勢つきの写真: {2}"),
    ZH_HANS("{0} 扫描 · 点：{1} · 带位姿的照片：{2}"),
    ZH_HANT("{0} 掃描 · 點：{1} · 帶位姿的相片：{2}"),
    KO("{0} 스캔 · 점: {1} · 위치와 자세가 있는 사진: {2}"),
    DE("{0}-Scan · Punkte: {1} · Fotos mit Pose: {2}"),
    FR("Scan {0} · Points : {1} · Photos avec une pose : {2}"),
    ES("Escaneo {0} · Puntos: {1} · Fotos con pose: {2}"),
    PT("Varredura {0} · Pontos: {1} · Fotos com pose: {2}"),
    IT("Scansione {0} · Punti: {1} · Foto con posa: {2}"),
    NL("{0}-scan · Punten: {1} · Foto's met pose: {2}"),
    RU("Скан {0} · Точек: {1} · Фотографий с позой: {2}"),
    TR("{0} taraması · Nokta: {1} · Pozlu fotoğraf: {2}"));

// ===========================================================================
// Video
// ===========================================================================

SS_MSG(reading_video,
    EN("Reading the video…"),
    JA("動画を読み込んでいます…"),
    ZH_HANS("正在读取视频…"),
    ZH_HANT("正在讀取影片…"),
    KO("동영상을 읽는 중…"),
    DE("Video wird gelesen…"),
    FR("Lecture de la vidéo…"),
    ES("Leyendo el vídeo…"),
    PT("Lendo o vídeo…"),
    IT("Lettura del video…"),
    NL("Video wordt gelezen…"),
    RU("Чтение видео…"),
    TR("Video okunuyor…"));

SS_MSG(video_unreadable,
    EN("This video could not be read. Without the built-in decoder it needs "
       "ffmpeg ({0})."),
    JA("この動画を読み込めませんでした。内蔵デコーダがない場合は ffmpeg ({0}) が"
       "必要です。"),
    ZH_HANS("无法读取这个视频。没有内置解码器时需要 ffmpeg（{0}）。"),
    ZH_HANT("無法讀取這個影片。沒有內建解碼器時需要 ffmpeg（{0}）。"),
    KO("이 동영상을 읽을 수 없습니다. 내장 디코더가 없으면 ffmpeg({0})가 필요합"
       "니다."),
    DE("Dieses Video ließ sich nicht lesen. Ohne den eingebauten Decoder "
       "braucht es ffmpeg ({0})."),
    FR("Impossible de lire cette vidéo. Sans le décodeur intégré, il faut "
       "ffmpeg ({0})."),
    ES("No se pudo leer este vídeo. Sin el decodificador integrado necesita "
       "ffmpeg ({0})."),
    PT("Não foi possível ler este vídeo. Sem o decodificador embutido, ele "
       "precisa do ffmpeg ({0})."),
    IT("Impossibile leggere questo video. Senza il decodificatore integrato "
       "serve ffmpeg ({0})."),
    NL("Deze video kon niet worden gelezen. Zonder de ingebouwde decoder is "
       "ffmpeg nodig ({0})."),
    RU("Не удалось прочитать это видео. Без встроенного декодера нужен ffmpeg "
       "({0})."),
    TR("Bu video okunamadı. Yerleşik çözücü olmadan ffmpeg ({0}) gerekir."));

SS_MSG(video_needs_gpu,
    EN("Frames are decoded on the GPU, which has not been chosen yet, and "
       "ffmpeg is not installed to decode them instead."),
    JA("フレームは GPU でデコードしますが、GPU がまだ決まっておらず、代わりにデ"
       "コードする ffmpeg もインストールされていません。"),
    ZH_HANS("帧由 GPU 解码，但尚未选定 GPU，也没有安装可代为解码的 ffmpeg。"),
    ZH_HANT("影格由 GPU 解碼，但尚未選定 GPU，也沒有安裝可代為解碼的 ffmpeg。"),
    KO("프레임은 GPU로 디코딩하는데, GPU가 아직 정해지지 않았고 대신 디코딩할 "
       "ffmpeg도 설치되어 있지 않습니다."),
    DE("Die Bilder werden auf der GPU decodiert, die noch nicht gewählt ist, "
       "und ffmpeg ist nicht installiert, um sie stattdessen zu decodieren."),
    FR("Les images sont décodées sur le GPU, qui n'a pas encore été choisi, et "
       "ffmpeg n'est pas installé pour les décoder à sa place."),
    ES("Los fotogramas se decodifican en la GPU, que aún no se ha elegido, y "
       "ffmpeg no está instalado para decodificarlos en su lugar."),
    PT("Os quadros são decodificados na GPU, que ainda não foi escolhida, e o "
       "ffmpeg não está instalado para decodificá-los no lugar dela."),
    IT("I fotogrammi si decodificano sulla GPU, che non è ancora stata scelta, "
       "e ffmpeg non è installato per decodificarli al suo posto."),
    NL("De beelden worden op de GPU gedecodeerd, die nog niet is gekozen, en "
       "ffmpeg is niet geïnstalleerd om ze in plaats daarvan te decoderen."),
    RU("Кадры декодируются на GPU, который ещё не выбран, а ffmpeg, чтобы "
       "декодировать их вместо него, не установлен."),
    TR("Kareler GPU'da çözülür; GPU henüz seçilmedi ve onların yerine çözecek "
       "ffmpeg kurulu değil."));

SS_MSG(decode_on_gpu,
    EN("Decode on the GPU"),
    JA("GPU でデコード"),
    ZH_HANS("用 GPU 解码"),
    ZH_HANT("用 GPU 解碼"),
    KO("GPU로 디코딩"),
    DE("Auf der GPU decodieren"),
    FR("Décoder sur le GPU"),
    ES("Decodificar en la GPU"),
    PT("Decodificar na GPU"),
    IT("Decodifica sulla GPU"),
    NL("Op de GPU decoderen"),
    RU("Декодировать на GPU"),
    TR("GPU'da çöz"));

SS_MSG(decode_on_gpu_help,
    EN("Decodes this preview's frames on the GPU chosen under Settings. That "
       "choice is then fixed until the app restarts, as it is once a run starts."),
    JA("このプレビューのフレームを、設定で選んだ GPU でデコードします。実行を始"
       "めたときと同じく、その選択はアプリを再起動するまで固定されます。"),
    ZH_HANS("用“设置”中选定的 GPU 解码此预览的帧。与开始运行时一样，此后这个选择在应"
            "用重启前不能更改。"),
    ZH_HANT("用「設定」中選定的 GPU 解碼此預覽的影格。與開始執行時一樣，此後這個選擇"
            "在應用程式重新啟動前不能更改。"),
    KO("이 미리보기의 프레임을 설정에서 고른 GPU로 디코딩합니다. 실행을 시작할 "
       "때처럼, 그 선택은 앱을 다시 시작할 때까지 고정됩니다."),
    DE("Decodiert die Bilder dieser Vorschau auf der unter Einstellungen "
       "gewählten GPU. Diese Wahl steht danach bis zum Neustart der App fest, "
       "wie sobald ein Lauf beginnt."),
    FR("Décode les images de cet aperçu sur le GPU choisi dans les Réglages. Ce "
       "choix est ensuite fixé jusqu'au redémarrage de l'application, comme dès "
       "qu'une exécution commence."),
    ES("Decodifica los fotogramas de esta vista previa en la GPU elegida en "
       "Ajustes. Esa elección queda fija hasta reiniciar la aplicación, como "
       "ocurre al empezar una ejecución."),
    PT("Decodifica os quadros desta prévia na GPU escolhida em Configurações. "
       "Essa escolha fica fixa até o app reiniciar, como acontece quando uma "
       "execução começa."),
    IT("Decodifica i fotogrammi di questa anteprima sulla GPU scelta in "
       "Impostazioni. La scelta resta poi fissa fino al riavvio dell'app, come "
       "quando parte un'esecuzione."),
    NL("Decodeert de beelden van dit voorbeeld op de GPU die onder Instellingen "
       "is gekozen. Die keuze ligt daarna vast tot de app opnieuw start, zoals "
       "wanneer een uitvoering begint."),
    RU("Декодирует кадры этого предпросмотра на GPU, выбранном в настройках. "
       "Этот выбор затем закрепляется до перезапуска приложения, как и при "
       "старте запуска."),
    TR("Bu önizlemenin karelerini Ayarlar'da seçilen GPU'da çözer. Bir "
       "çalıştırma başladığındaki gibi, bu seçim uygulama yeniden başlayana dek "
       "sabit kalır."));

SS_MSG(view_stitched,
    EN("Stitched 360°"),
    JA("360° つなぎ合わせ"),
    ZH_HANS("360° 拼接"),
    ZH_HANT("360° 拼接"),
    KO("360° 스티칭"),
    DE("360° zusammengesetzt"),
    FR("360° assemblé"),
    ES("360° unido"),
    PT("360° costurado"),
    IT("360° unito"),
    NL("360° samengevoegd"),
    RU("Сшитое 360°"),
    TR("Birleştirilmiş 360°"));

SS_MSG(view_stitched_help,
    EN("The two lenses joined into one equirectangular panorama, turned the way "
       "the run turns it."),
    JA("2 つのレンズをつないだ 1 枚の正距円筒パノラマです。実行時と同じ向きに回"
       "転しています。"),
    ZH_HANS("两个镜头拼接成一张等距柱状全景图，朝向与运行时的处理一致。"),
    ZH_HANT("兩個鏡頭拼接成一張等距長方全景圖，朝向與執行時的處理一致。"),
    KO("두 렌즈를 이어 붙인 등장방형 파노라마 한 장입니다. 실행이 돌리는 방향 그"
       "대로입니다."),
    DE("Die beiden Objektive zu einem equirektangularen Panorama "
       "zusammengefügt, gedreht wie im Lauf."),
    FR("Les deux objectifs réunis en un seul panorama équirectangulaire, "
       "orienté comme l'exécution l'oriente."),
    ES("Las dos lentes unidas en un panorama equirrectangular, girado como lo "
       "gira la ejecución."),
    PT("As duas lentes unidas em um panorama equirretangular, girado como a "
       "execução o gira."),
    IT("I due obiettivi uniti in un panorama equirettangolare, ruotato come lo "
       "ruota l'esecuzione."),
    NL("De twee lenzen samengevoegd tot één equirectangulair panorama, gedraaid "
       "zoals de uitvoering het draait."),
    RU("Два объектива, сшитые в одну равнопромежуточную панораму и повёрнутые "
       "так, как их поворачивает запуск."),
    TR("İki lens tek bir eşdikdörtgen panoramada birleştirilir, çalıştırmanın "
       "döndürdüğü gibi döndürülür."));

SS_MSG(view_recorded,
    EN("As recorded"),
    JA("記録のまま"),
    ZH_HANS("原始录制"),
    ZH_HANT("原始錄製"),
    KO("녹화된 그대로"),
    DE("Wie aufgenommen"),
    FR("Tel qu'enregistré"),
    ES("Tal como se grabó"),
    PT("Como gravado"),
    IT("Come registrato"),
    NL("Zoals opgenomen"),
    RU("Как записано"),
    TR("Kaydedildiği gibi"));

SS_MSG(view_recorded_help,
    EN("The video streams exactly as the camera wrote them, side by side."),
    JA("カメラが書き込んだままの動画ストリームを並べて表示します。"),
    ZH_HANS("相机写入的原样视频流，并排显示。"),
    ZH_HANT("相機寫入的原樣視訊串流，並排顯示。"),
    KO("카메라가 기록한 그대로의 동영상 스트림을 나란히 보여 줍니다."),
    DE("Die Videospuren genau so, wie die Kamera sie geschrieben hat, "
       "nebeneinander."),
    FR("Les flux vidéo exactement tels que la caméra les a écrits, côte à côte."),
    ES("Las pistas de vídeo exactamente como las escribió la cámara, una junto "
       "a otra."),
    PT("As faixas de vídeo exatamente como a câmera as gravou, lado a lado."),
    IT("I flussi video esattamente come li ha scritti la fotocamera, affiancati."),
    NL("De videosporen precies zoals de camera ze schreef, naast elkaar."),
    RU("Видеопотоки ровно в том виде, в каком их записала камера, рядом друг с "
       "другом."),
    TR("Video akışları, kameranın yazdığı gibi, yan yana."));

SS_MSG(streams_side_by_side,
    EN("Streams: {0}, shown side by side"),
    JA("ストリーム: {0}（並べて表示）"),
    ZH_HANS("视频流：{0}，并排显示"),
    ZH_HANT("視訊串流：{0}，並排顯示"),
    KO("스트림: {0}, 나란히 표시"),
    DE("Videospuren: {0}, nebeneinander gezeigt"),
    FR("Flux : {0}, affichés côte à côte"),
    ES("Pistas: {0}, mostradas una junto a otra"),
    PT("Faixas: {0}, mostradas lado a lado"),
    IT("Flussi: {0}, mostrati affiancati"),
    NL("Videosporen: {0}, naast elkaar getoond"),
    RU("Видеопотоков: {0}, показаны рядом"),
    TR("Video akışı: {0}, yan yana gösteriliyor"));

SS_MSG(camera_toggle_help,
    EN("Include or exclude this camera's frames from the dataset while preserving telemetry."),
    JA("テレメトリを保持したまま、このカメラのフレームをデータセットに含めるか除外します。"),
    ZH_HANS("在保留遥测数据的同时，将此相机的帧包含在数据集中或从中排除。"),
    ZH_HANT("在保留遙測資料的同時，將此相機的影格包含在資料集中或從中排除。"),
    KO("원격 측정을 유지하면서 이 카메라의 프레임을 데이터셋에 포함하거나 제외합니다."),
    DE("Bilder dieser Kamera in den Datensatz aufnehmen oder ausschließen, während Telemetriedaten erhalten bleiben."),
    FR("Inclure ou exclure les images de cette caméra du jeu de données tout en conservant la télémétrie."),
    ES("Incluir o excluir los fotogramas de esta cámara del conjunto de datos conservando la telemetría."),
    PT("Incluir ou excluir os quadros desta câmera do conjunto de dados mantendo a telemetria."),
    IT("Includi o escludi i fotogrammi di questa fotocamera dal set di dati conservando la telemetria."),
    NL("Beelden van deze camera in de dataset opnemen of uitsluiten met behoud van telemetrie."),
    RU("Включить или исключить кадры этой камеры из датасета, сохраняя телеметрию."),
    TR("Telemetriyi koruyarak bu kameranın karelerini veri kümesine dahil edin veya hariç tutun."));

SS_MSG(camera_disabled,
    EN("Excluded"),
    JA("除外"),
    ZH_HANS("已排除"),
    ZH_HANT("已排除"),
    KO("제외됨"),
    DE("Ausgeschlossen"),
    FR("Exclu"),
    ES("Excluido"),
    PT("Excluído"),
    IT("Escluso"),
    NL("Uitgesloten"),
    RU("Исключено"),
    TR("Hariç tutuldu"));

SS_MSG(lenses_side_by_side,
    EN("Lenses in one picture: {0}, cut apart and shown side by side"),
    JA("1 枚に収まったレンズ: {0}（切り分けて並べて表示）"),
    ZH_HANS("同一画面中的镜头：{0}，切开后并排显示"),
    ZH_HANT("同一畫面中的鏡頭：{0}，切開後並排顯示"),
    KO("한 화면에 담긴 렌즈: {0}, 잘라서 나란히 표시"),
    DE("Objektive in einem Bild: {0}, getrennt und nebeneinander gezeigt"),
    FR("Objectifs dans une même image : {0}, découpés et affichés côte à côte"),
    ES("Objetivos en una misma imagen: {0}, separados y mostrados uno junto a otro"),
    PT("Lentes numa mesma imagem: {0}, separadas e mostradas lado a lado"),
    IT("Obiettivi in un'unica immagine: {0}, separati e mostrati affiancati"),
    NL("Lenzen in één beeld: {0}, losgesneden en naast elkaar getoond"),
    RU("Объективов в одном кадре: {0}, разрезаны и показаны рядом"),
    TR("Tek görüntüdeki mercek: {0}, ayrılıp yan yana gösteriliyor"));

SS_MSG(frame_at,
    EN("Frame {0} · {1}"),
    JA("フレーム {0} · {1}"),
    ZH_HANS("第 {0} 帧 · {1}"),
    ZH_HANT("第 {0} 格 · {1}"),
    KO("프레임 {0} · {1}"),
    DE("Bild {0} · {1}"),
    FR("Image {0} · {1}"),
    ES("Fotograma {0} · {1}"),
    PT("Quadro {0} · {1}"),
    IT("Fotogramma {0} · {1}"),
    NL("Beeld {0} · {1}"),
    RU("Кадр {0} · {1}"),
    TR("Kare {0} · {1}"));

SS_MSG(play,
    EN("Play"),
    JA("再生"),
    ZH_HANS("播放"),
    ZH_HANT("播放"),
    KO("재생"),
    DE("Abspielen"),
    FR("Lire"),
    ES("Reproducir"),
    PT("Reproduzir"),
    IT("Riproduci"),
    NL("Afspelen"),
    RU("Воспроизвести"),
    TR("Oynat"));

SS_MSG(pause,
    EN("Pause"),
    JA("一時停止"),
    ZH_HANS("暂停"),
    ZH_HANT("暫停"),
    KO("일시 정지"),
    DE("Pause"),
    FR("Pause"),
    ES("Pausa"),
    PT("Pausar"),
    IT("Pausa"),
    NL("Pauze"),
    RU("Пауза"),
    TR("Duraklat"));

SS_MSG(play_help,
    EN("Plays the video at its own speed (Space)."),
    JA("動画を本来の速さで再生します（Space キー）。"),
    ZH_HANS("以视频本身的速度播放（空格键）。"),
    ZH_HANT("以影片本身的速度播放（空白鍵）。"),
    KO("동영상을 원래 속도로 재생합니다(Space 키)."),
    DE("Spielt das Video in seiner eigenen Geschwindigkeit ab (Leertaste)."),
    FR("Lit la vidéo à sa propre vitesse (Espace)."),
    ES("Reproduce el vídeo a su propia velocidad (Espacio)."),
    PT("Reproduz o vídeo na própria velocidade dele (Espaço)."),
    IT("Riproduce il video alla sua velocità (Spazio)."),
    NL("Speelt de video af op zijn eigen snelheid (spatiebalk)."),
    RU("Воспроизводит видео с его собственной скоростью (пробел)."),
    TR("Videoyu kendi hızında oynatır (Boşluk tuşu)."));

SS_MSG(play_needs_gpu,
    EN("Playing needs the video decoded on the GPU. Here its frames come from "
       "ffmpeg, which takes longer to find a frame than the frame lasts."),
    JA("再生するには GPU で動画をデコードする必要があります。ここではフレームを "
       "ffmpeg で読んでいますが、ffmpeg は 1 フレームを探すのにそのフレームの表"
       "示時間より長くかかります。"),
    ZH_HANS("播放需要在 GPU 上解码视频。这里的帧来自 ffmpeg，而 ffmpeg 找到一帧所需的"
            "时间比这一帧显示的时间还长。"),
    ZH_HANT("播放需要在 GPU 上解碼影片。這裡的影格來自 ffmpeg，而 ffmpeg 找到一個影格"
            "所需的時間比這個影格顯示的時間還長。"),
    KO("재생하려면 GPU에서 동영상을 디코딩해야 합니다. 여기서는 프레임을 "
       "ffmpeg로 읽는데, ffmpeg는 프레임 하나를 찾는 데 그 프레임이 보이는 시간"
       "보다 오래 걸립니다."),
    DE("Abspielen braucht das Video auf der GPU decodiert. Hier kommen seine "
       "Bilder von ffmpeg, das länger braucht, um ein Bild zu finden, als das "
       "Bild dauert."),
    FR("La lecture demande que la vidéo soit décodée sur le GPU. Ici, ses "
       "images viennent de ffmpeg, qui met plus de temps à trouver une image "
       "que l'image ne dure."),
    ES("Para reproducir hay que decodificar el vídeo en la GPU. Aquí sus "
       "fotogramas vienen de ffmpeg, que tarda más en encontrar un fotograma de "
       "lo que dura el fotograma."),
    PT("Reproduzir exige o vídeo decodificado na GPU. Aqui os quadros vêm do "
       "ffmpeg, que leva mais tempo para achar um quadro do que o quadro dura."),
    IT("Per riprodurre il video bisogna decodificarlo sulla GPU. Qui i "
       "fotogrammi arrivano da ffmpeg, che impiega a trovare un fotogramma più "
       "di quanto il fotogramma duri."),
    NL("Afspelen vraagt dat de video op de GPU wordt gedecodeerd. Hier komen de "
       "beelden van ffmpeg, dat er langer over doet om een beeld te vinden dan "
       "het beeld duurt."),
    RU("Для воспроизведения видео нужно декодировать на GPU. Здесь кадры "
       "берутся из ffmpeg, а он ищет кадр дольше, чем этот кадр длится."),
    TR("Oynatmak için videonun GPU'da çözülmesi gerekir. Burada kareler "
       "ffmpeg'den gelir; ffmpeg bir kareyi bulmak için karenin süresinden uzun "
       "zaman harcar."));

SS_MSG(play_gpu_busy,
    EN("Playing needs the video decoded on the GPU, which a run or a mask "
       "preview is using now."),
    JA("再生するには GPU で動画をデコードする必要がありますが、いまは実行中の処"
       "理かマスクのプレビューが GPU を使っています。"),
    ZH_HANS("播放需要在 GPU 上解码视频，而 GPU 现在正被运行任务或蒙版预览占用。"),
    ZH_HANT("播放需要在 GPU 上解碼影片，而 GPU 現在正被執行工作或遮罩預覽佔用。"),
    KO("재생하려면 GPU에서 동영상을 디코딩해야 하는데, 지금은 실행 중인 작업이나"
       " 마스크 미리보기가 GPU를 쓰고 있습니다."),
    DE("Abspielen braucht das Video auf der GPU decodiert, die gerade ein Lauf "
       "oder eine Maskenvorschau belegt."),
    FR("La lecture demande que la vidéo soit décodée sur le GPU, qu'une "
       "exécution ou un aperçu de masque occupe en ce moment."),
    ES("Para reproducir hay que decodificar el vídeo en la GPU, que ahora está "
       "usando una ejecución o una vista previa de máscara."),
    PT("Reproduzir exige o vídeo decodificado na GPU, que uma execução ou uma "
       "prévia de máscara está usando agora."),
    IT("Per riprodurre il video bisogna decodificarlo sulla GPU, che ora è "
       "occupata da un'esecuzione o da un'anteprima delle maschere."),
    NL("Afspelen vraagt dat de video op de GPU wordt gedecodeerd, en die is nu "
       "bezet door een uitvoering of een maskervoorbeeld."),
    RU("Для воспроизведения видео нужно декодировать на GPU, а его сейчас "
       "занимает запуск или предпросмотр масок."),
    TR("Oynatmak için videonun GPU'da çözülmesi gerekir; GPU'yu şu anda bir "
       "çalıştırma ya da maske önizlemesi kullanıyor."));

SS_MSG(play_gpu_cannot,
    EN("Playing needs the video decoded on the GPU, and this GPU cannot decode "
       "this video."),
    JA("再生するには GPU で動画をデコードする必要がありますが、この GPU はこの動"
       "画をデコードできません。"),
    ZH_HANS("播放需要在 GPU 上解码视频，而这个 GPU 无法解码这个视频。"),
    ZH_HANT("播放需要在 GPU 上解碼影片，而這個 GPU 無法解碼這部影片。"),
    KO("재생하려면 GPU에서 동영상을 디코딩해야 하는데, 이 GPU는 이 동영상을 디코"
       "딩할 수 없습니다."),
    DE("Abspielen braucht das Video auf der GPU decodiert, und diese GPU kann "
       "dieses Video nicht decodieren."),
    FR("La lecture demande que la vidéo soit décodée sur le GPU, et ce GPU ne "
       "sait pas décoder cette vidéo."),
    ES("Para reproducir hay que decodificar el vídeo en la GPU, y esta GPU no "
       "puede decodificar este vídeo."),
    PT("Reproduzir exige o vídeo decodificado na GPU, e esta GPU não consegue "
       "decodificar este vídeo."),
    IT("Per riprodurre il video bisogna decodificarlo sulla GPU, e questa GPU "
       "non sa decodificare questo video."),
    NL("Afspelen vraagt dat de video op de GPU wordt gedecodeerd, en deze GPU "
       "kan deze video niet decoderen."),
    RU("Для воспроизведения видео нужно декодировать на GPU, а этот GPU не "
       "может декодировать это видео."),
    TR("Oynatmak için videonun GPU'da çözülmesi gerekir ve bu GPU bu videoyu "
       "çözemiyor."));

SS_MSG(decoding,
    EN("Decoding…"),
    JA("デコード中…"),
    ZH_HANS("正在解码…"),
    ZH_HANT("正在解碼…"),
    KO("디코딩 중…"),
    DE("Wird decodiert…"),
    FR("Décodage…"),
    ES("Decodificando…"),
    PT("Decodificando…"),
    IT("Decodifica…"),
    NL("Decoderen…"),
    RU("Декодирование…"),
    TR("Çözülüyor…"));

SS_MSG(timeline_help,
    EN("Click or drag to look through the video. Drag the green and red handles "
       "to use only part of it."),
    JA("クリックまたはドラッグで動画を見て回れます。緑と赤のハンドルをドラッグす"
       "ると、動画の一部だけを使います。"),
    ZH_HANS("点击或拖动来浏览视频。拖动绿色和红色手柄，只使用其中一段。"),
    ZH_HANT("點按或拖曳來瀏覽影片。拖曳綠色和紅色把手，只使用其中一段。"),
    KO("클릭하거나 끌어서 동영상을 둘러봅니다. 초록색과 빨간색 핸들을 끌면 동영"
       "상의 일부만 씁니다."),
    DE("Klicken oder ziehen, um durch das Video zu gehen. Den grünen und den "
       "roten Griff ziehen, um nur einen Teil davon zu verwenden."),
    FR("Cliquez ou faites glisser pour parcourir la vidéo. Faites glisser les "
       "poignées verte et rouge pour n'en utiliser qu'une partie."),
    ES("Haz clic o arrastra para recorrer el vídeo. Arrastra los tiradores "
       "verde y rojo para usar solo una parte."),
    PT("Clique ou arraste para percorrer o vídeo. Arraste as alças verde e "
       "vermelha para usar só uma parte dele."),
    IT("Fai clic o trascina per scorrere il video. Trascina le maniglie verde e "
       "rossa per usarne solo una parte."),
    NL("Klik of sleep om door de video te gaan. Sleep de groene en rode greep "
       "om maar een deel ervan te gebruiken."),
    RU("Щёлкните или перетащите, чтобы просмотреть видео. Перетащите зелёный и "
       "красный маркеры, чтобы использовать только его часть."),
    TR("Videoda gezinmek için tıklayın ya da sürükleyin. Yalnızca bir bölümünü "
       "kullanmak için yeşil ve kırmızı tutamaçları sürükleyin."));

SS_MSG(clip_start_here,
    EN("Start here"),
    JA("ここから"),
    ZH_HANS("从这里开始"),
    ZH_HANT("從這裡開始"),
    KO("여기서 시작"),
    DE("Hier beginnen"),
    FR("Commencer ici"),
    ES("Empezar aquí"),
    PT("Começar aqui"),
    IT("Inizia qui"),
    NL("Hier beginnen"),
    RU("Начать здесь"),
    TR("Buradan başla"));

SS_MSG(clip_start_here_help,
    EN("The part of the video that is used starts at the frame shown (key I)."),
    JA("使う区間を、表示中のフレームから始めます（I キー）。"),
    ZH_HANS("使用的片段从当前显示的帧开始（I 键）。"),
    ZH_HANT("使用的片段從目前顯示的影格開始（I 鍵）。"),
    KO("쓰는 구간이 지금 보이는 프레임에서 시작합니다(I 키)."),
    DE("Der verwendete Teil des Videos beginnt beim gezeigten Bild (Taste I)."),
    FR("La partie utilisée de la vidéo commence à l'image affichée (touche I)."),
    ES("La parte del vídeo que se usa empieza en el fotograma mostrado (tecla "
       "I)."),
    PT("A parte usada do vídeo começa no quadro mostrado (tecla I)."),
    IT("La parte del video usata inizia dal fotogramma mostrato (tasto I)."),
    NL("Het gebruikte deel van de video begint bij het getoonde beeld (toets I)."),
    RU("Используемая часть видео начинается с показанного кадра (клавиша I)."),
    TR("Videonun kullanılan bölümü gösterilen karede başlar (I tuşu)."));

SS_MSG(clip_end_here,
    EN("End here"),
    JA("ここまで"),
    ZH_HANS("到这里结束"),
    ZH_HANT("到這裡結束"),
    KO("여기서 끝"),
    DE("Hier enden"),
    FR("Finir ici"),
    ES("Terminar aquí"),
    PT("Terminar aqui"),
    IT("Finisci qui"),
    NL("Hier eindigen"),
    RU("Закончить здесь"),
    TR("Burada bitir"));

SS_MSG(clip_end_here_help,
    EN("The part of the video that is used ends at the frame shown (key O)."),
    JA("使う区間を、表示中のフレームで終えます（O キー）。"),
    ZH_HANS("使用的片段在当前显示的帧结束（O 键）。"),
    ZH_HANT("使用的片段在目前顯示的影格結束（O 鍵）。"),
    KO("쓰는 구간이 지금 보이는 프레임에서 끝납니다(O 키)."),
    DE("Der verwendete Teil des Videos endet beim gezeigten Bild (Taste O)."),
    FR("La partie utilisée de la vidéo se termine à l'image affichée (touche O)."),
    ES("La parte del vídeo que se usa termina en el fotograma mostrado (tecla "
       "O)."),
    PT("A parte usada do vídeo termina no quadro mostrado (tecla O)."),
    IT("La parte del video usata finisce al fotogramma mostrato (tasto O)."),
    NL("Het gebruikte deel van de video eindigt bij het getoonde beeld (toets "
       "O)."),
    RU("Используемая часть видео заканчивается на показанном кадре (клавиша O)."),
    TR("Videonun kullanılan bölümü gösterilen karede biter (O tuşu)."));

SS_MSG(clip_whole,
    EN("Whole video"),
    JA("動画全体"),
    ZH_HANS("整个视频"),
    ZH_HANT("整部影片"),
    KO("동영상 전체"),
    DE("Ganzes Video"),
    FR("Vidéo entière"),
    ES("Vídeo completo"),
    PT("Vídeo inteiro"),
    IT("Video intero"),
    NL("Hele video"),
    RU("Всё видео"),
    TR("Tüm video"));

SS_MSG(clip_whole_help,
    EN("Use the video from its start to its end again."),
    JA("動画を最初から最後まで使う状態に戻します。"),
    ZH_HANS("恢复为从头到尾使用整个视频。"),
    ZH_HANT("恢復為從頭到尾使用整部影片。"),
    KO("동영상을 처음부터 끝까지 다시 씁니다."),
    DE("Das Video wieder vom Anfang bis zum Ende verwenden."),
    FR("Utiliser de nouveau la vidéo du début à la fin."),
    ES("Volver a usar el vídeo de principio a fin."),
    PT("Usar de novo o vídeo do início ao fim."),
    IT("Usare di nuovo il video dall'inizio alla fine."),
    NL("De video weer van begin tot eind gebruiken."),
    RU("Снова использовать видео от начала до конца."),
    TR("Videoyu yeniden baştan sona kullan."));

SS_MSG(clip_from,
    EN("Start"),
    JA("開始"),
    ZH_HANS("开始"),
    ZH_HANT("開始"),
    KO("시작"),
    DE("Anfang"),
    FR("Début"),
    ES("Inicio"),
    PT("Início"),
    IT("Inizio"),
    NL("Begin"),
    RU("Начало"),
    TR("Başlangıç"));

SS_MSG(clip_to,
    EN("End"),
    JA("終了"),
    ZH_HANS("结束"),
    ZH_HANT("結束"),
    KO("끝"),
    DE("Ende"),
    FR("Fin"),
    ES("Fin"),
    PT("Fim"),
    IT("Fine"),
    NL("Einde"),
    RU("Конец"),
    TR("Bitiş"));

SS_MSG(clip_length,
    EN("Used: {0} of {1}"),
    JA("使用: {0} / {1}"),
    ZH_HANS("使用：{0} / {1}"),
    ZH_HANT("使用：{0} / {1}"),
    KO("사용: {0} / {1}"),
    DE("Verwendet: {0} von {1}"),
    FR("Utilisé : {0} sur {1}"),
    ES("Usado: {0} de {1}"),
    PT("Usado: {0} de {1}"),
    IT("Usato: {0} su {1}"),
    NL("Gebruikt: {0} van {1}"),
    RU("Используется: {0} из {1}"),
    TR("Kullanılan: {0} / {1}"));

SS_MSG(clip_locked,
    EN("The run reads this part of the video; it cannot be changed while the "
       "run is going."),
    JA("この区間は実行が読み込みます。実行中は変更できません。"),
    ZH_HANS("运行会读取这一段视频；运行期间不能更改。"),
    ZH_HANT("執行會讀取這一段影片；執行期間不能更改。"),
    KO("실행이 이 구간을 읽습니다. 실행 중에는 바꿀 수 없습니다."),
    DE("Der Lauf liest diesen Teil des Videos; solange er läuft, lässt er sich "
       "nicht ändern."),
    FR("L'exécution lit cette partie de la vidéo ; elle ne peut pas être "
       "modifiée pendant l'exécution."),
    ES("La ejecución lee esta parte del vídeo; no se puede cambiar mientras "
       "está en marcha."),
    PT("A execução lê esta parte do vídeo; ela não pode ser alterada enquanto a "
       "execução está em andamento."),
    IT("L'esecuzione legge questa parte del video; non si può cambiare mentre è "
       "in corso."),
    NL("De uitvoering leest dit deel van de video; het kan niet worden "
       "gewijzigd zolang ze loopt."),
    RU("Запуск читает эту часть видео; пока он идёт, её нельзя изменить."),
    TR("Çalıştırma videonun bu bölümünü okur; çalışırken değiştirilemez."));

SS_MSG(clip_frames,
    EN("Frames kept: about {0}"),
    JA("残すフレーム: 約 {0}"),
    ZH_HANS("保留的帧：约 {0}"),
    ZH_HANT("保留的影格：約 {0}"),
    KO("남길 프레임: 약 {0}"),
    DE("Behaltene Bilder: etwa {0}"),
    FR("Images conservées : environ {0}"),
    ES("Fotogramas conservados: unos {0}"),
    PT("Quadros mantidos: cerca de {0}"),
    IT("Fotogrammi tenuti: circa {0}"),
    NL("Behouden beelden: ongeveer {0}"),
    RU("Сохраняется кадров: около {0}"),
    TR("Tutulacak kare: yaklaşık {0}"));

SS_MSG(clip_frames_per_camera,
    EN("Frames kept: about {0} per stream"),
    JA("残すフレーム: ストリームごとに約 {0}"),
    ZH_HANS("保留的帧：每路视频流约 {0}"),
    ZH_HANT("保留的影格：每路視訊串流約 {0}"),
    KO("남길 프레임: 스트림마다 약 {0}"),
    DE("Behaltene Bilder: etwa {0} je Videospur"),
    FR("Images conservées : environ {0} par flux"),
    ES("Fotogramas conservados: unos {0} por pista"),
    PT("Quadros mantidos: cerca de {0} por faixa"),
    IT("Fotogrammi tenuti: circa {0} per flusso"),
    NL("Behouden beelden: ongeveer {0} per videospoor"),
    RU("Сохраняется кадров: около {0} на видеопоток"),
    TR("Tutulacak kare: akış başına yaklaşık {0}"));

SS_MSG(clip_frames_per_lens,
    EN("Frames kept: about {0} per lens"),
    JA("残すフレーム: レンズごとに約 {0}"),
    ZH_HANS("保留的帧：每个镜头约 {0}"),
    ZH_HANT("保留的影格：每個鏡頭約 {0}"),
    KO("남길 프레임: 렌즈마다 약 {0}"),
    DE("Behaltene Bilder: etwa {0} je Objektiv"),
    FR("Images conservées : environ {0} par objectif"),
    ES("Fotogramas conservados: unos {0} por objetivo"),
    PT("Quadros mantidos: cerca de {0} por lente"),
    IT("Fotogrammi tenuti: circa {0} per obiettivo"),
    NL("Behouden beelden: ongeveer {0} per lens"),
    RU("Сохраняется кадров: около {0} на объектив"),
    TR("Tutulacak kare: mercek başına yaklaşık {0}"));

SS_MSG(clip_frames_help,
    EN("At the rate set for this video. With adaptive spacing this is the "
       "average, and the motion decides where they fall."),
    JA("この動画に設定したレートでの数です。動きに合わせて間隔を変える場合は平均"
       "で、どこに置かれるかは動きで決まります。"),
    ZH_HANS("按这个视频设定的帧率计算。启用自适应间隔时这是平均值，具体落在哪里由运动"
            "决定。"),
    ZH_HANT("按這個影片設定的影格率計算。啟用自適應間隔時這是平均值，具體落在哪裡由運"
            "動決定。"),
    KO("이 동영상에 정한 속도로 계산한 수입니다. 적응형 간격일 때는 평균이고, 어"
       "디에 놓일지는 움직임이 정합니다."),
    DE("Bei der für dieses Video eingestellten Rate. Mit adaptivem Abstand ist "
       "das der Durchschnitt, und die Bewegung entscheidet, wo sie liegen."),
    FR("À la cadence réglée pour cette vidéo. Avec l'espacement adaptatif, "
       "c'est la moyenne, et le mouvement décide où elles tombent."),
    ES("Al ritmo fijado para este vídeo. Con espaciado adaptativo es la media, "
       "y el movimiento decide dónde caen."),
    PT("Na taxa definida para este vídeo. Com espaçamento adaptativo, é a "
       "média, e o movimento decide onde eles caem."),
    IT("Alla cadenza impostata per questo video. Con la spaziatura adattiva è "
       "la media, e il movimento decide dove cadono."),
    NL("Bij het tempo dat voor deze video is ingesteld. Met aanpasbare "
       "tussenruimte is dit het gemiddelde, en de beweging bepaalt waar ze "
       "vallen."),
    RU("При частоте, заданной для этого видео. С адаптивным шагом это среднее, "
       "а где они окажутся, решает движение."),
    TR("Bu video için ayarlanan hızda. Uyarlanır aralıkta bu ortalamadır ve "
       "nereye düşeceklerine hareket karar verir."));

// ===========================================================================
// Photos
// ===========================================================================

SS_MSG(back_to_grid,
    EN("Back to grid"),
    JA("一覧に戻る"),
    ZH_HANS("返回网格"),
    ZH_HANT("返回格狀檢視"),
    KO("격자로 돌아가기"),
    DE("Zurück zum Raster"),
    FR("Retour à la grille"),
    ES("Volver a la cuadrícula"),
    PT("Voltar à grade"),
    IT("Torna alla griglia"),
    NL("Terug naar raster"),
    RU("Назад к сетке"),
    TR("Izgaraya dön"));

SS_MSG(photo_count,
    EN("Photos: {0}"),
    JA("写真: {0}"),
    ZH_HANS("照片：{0}"),
    ZH_HANT("相片：{0}"),
    KO("사진: {0}"),
    DE("Fotos: {0}"),
    FR("Photos : {0}"),
    ES("Fotos: {0}"),
    PT("Fotos: {0}"),
    IT("Foto: {0}"),
    NL("Foto's: {0}"),
    RU("Фотографий: {0}"),
    TR("Fotoğraf: {0}"));

SS_MSG(thumb_size_help,
    EN("Thumbnail size"),
    JA("サムネイルの大きさ"),
    ZH_HANS("缩略图大小"),
    ZH_HANT("縮圖大小"),
    KO("미리보기 크기"),
    DE("Größe der Vorschaubilder"),
    FR("Taille des vignettes"),
    ES("Tamaño de las miniaturas"),
    PT("Tamanho das miniaturas"),
    IT("Dimensione delle miniature"),
    NL("Grootte van de miniaturen"),
    RU("Размер миниатюр"),
    TR("Küçük resim boyutu"));

SS_MSG(show_masks,
    EN("Show masks"),
    JA("マスクを表示"),
    ZH_HANS("显示蒙版"),
    ZH_HANT("顯示遮罩"),
    KO("마스크 보기"),
    DE("Masken zeigen"),
    FR("Afficher les masques"),
    ES("Mostrar máscaras"),
    PT("Mostrar máscaras"),
    IT("Mostra maschere"),
    NL("Maskers tonen"),
    RU("Показать маски"),
    TR("Maskeleri göster"));

SS_MSG(show_masks_help,
    EN("Tint what the masks that came with the photos remove."),
    JA("写真といっしょにあったマスクが除く部分に色を付けます。"),
    ZH_HANS("给照片自带的蒙版要去除的区域着色。"),
    ZH_HANT("替相片附帶的遮罩要去除的區域著色。"),
    KO("사진과 함께 온 마스크가 지우는 부분에 색을 입힙니다."),
    DE("Färbt ein, was die mit den Fotos gelieferten Masken entfernen."),
    FR("Colore ce que retirent les masques fournis avec les photos."),
    ES("Colorea lo que quitan las máscaras que venían con las fotos."),
    PT("Colore o que as máscaras que vieram com as fotos removem."),
    IT("Colora ciò che rimuovono le maschere arrivate con le foto."),
    NL("Kleurt wat de maskers die bij de foto's kwamen weghalen."),
    RU("Подсвечивает то, что убирают маски, пришедшие вместе с фотографиями."),
    TR("Fotoğraflarla gelen maskelerin kaldırdığı yerleri renklendirir."));

SS_MSG(loading,
    EN("Loading…"),
    JA("読み込み中…"),
    ZH_HANS("正在加载…"),
    ZH_HANT("正在載入…"),
    KO("불러오는 중…"),
    DE("Wird geladen…"),
    FR("Chargement…"),
    ES("Cargando…"),
    PT("Carregando…"),
    IT("Caricamento…"),
    NL("Laden…"),
    RU("Загрузка…"),
    TR("Yükleniyor…"));

SS_MSG(listing_photos,
    EN("Listing the photos…"),
    JA("写真を一覧にしています…"),
    ZH_HANS("正在列出照片…"),
    ZH_HANT("正在列出相片…"),
    KO("사진 목록을 만드는 중…"),
    DE("Fotos werden aufgelistet…"),
    FR("Inventaire des photos…"),
    ES("Listando las fotos…"),
    PT("Listando as fotos…"),
    IT("Elenco delle foto in corso…"),
    NL("Foto's worden opgesomd…"),
    RU("Составление списка фотографий…"),
    TR("Fotoğraflar listeleniyor…"));

SS_MSG(no_photos,
    EN("No photos were found in this folder."),
    JA("このフォルダに写真は見つかりませんでした。"),
    ZH_HANS("这个文件夹里没有找到照片。"),
    ZH_HANT("這個資料夾裡找不到相片。"),
    KO("이 폴더에서 사진을 찾지 못했습니다."),
    DE("In diesem Ordner wurden keine Fotos gefunden."),
    FR("Aucune photo n'a été trouvée dans ce dossier."),
    ES("No se encontraron fotos en esta carpeta."),
    PT("Nenhuma foto foi encontrada nesta pasta."),
    IT("In questa cartella non è stata trovata nessuna foto."),
    NL("In deze map zijn geen foto's gevonden."),
    RU("В этой папке не найдено фотографий."),
    TR("Bu klasörde fotoğraf bulunamadı."));

SS_MSG(info_focal,
    EN("Focal length: {0} mm"),
    JA("焦点距離: {0} mm"),
    ZH_HANS("焦距：{0} mm"),
    ZH_HANT("焦距：{0} mm"),
    KO("초점 거리: {0} mm"),
    DE("Brennweite: {0} mm"),
    FR("Focale : {0} mm"),
    ES("Distancia focal: {0} mm"),
    PT("Distância focal: {0} mm"),
    IT("Lunghezza focale: {0} mm"),
    NL("Brandpuntsafstand: {0} mm"),
    RU("Фокусное расстояние: {0} мм"),
    TR("Odak uzaklığı: {0} mm"));

SS_MSG(info_focal35,
    EN("Focal length: {0} mm (35 mm equivalent: {1} mm)"),
    JA("焦点距離: {0} mm（35 mm 換算: {1} mm）"),
    ZH_HANS("焦距：{0} mm（35 mm 等效：{1} mm）"),
    ZH_HANT("焦距：{0} mm（35 mm 等效：{1} mm）"),
    KO("초점 거리: {0} mm(35 mm 환산: {1} mm)"),
    DE("Brennweite: {0} mm (Kleinbildäquivalent: {1} mm)"),
    FR("Focale : {0} mm (équivalent 35 mm : {1} mm)"),
    ES("Distancia focal: {0} mm (equivalente a 35 mm: {1} mm)"),
    PT("Distância focal: {0} mm (equivalente a 35 mm: {1} mm)"),
    IT("Lunghezza focale: {0} mm (equivalente 35 mm: {1} mm)"),
    NL("Brandpuntsafstand: {0} mm (35mm-equivalent: {1} mm)"),
    RU("Фокусное расстояние: {0} мм (эквивалент 35 мм: {1} мм)"),
    TR("Odak uzaklığı: {0} mm (35 mm karşılığı: {1} mm)"));

SS_MSG(info_exposure,
    EN("Exposure: {0} s, f/{1}, ISO {2}"),
    JA("露出: {0} 秒、f/{1}、ISO {2}"),
    ZH_HANS("曝光：{0} 秒，f/{1}，ISO {2}"),
    ZH_HANT("曝光：{0} 秒，f/{1}，ISO {2}"),
    KO("노출: {0}초, f/{1}, ISO {2}"),
    DE("Belichtung: {0} s, f/{1}, ISO {2}"),
    FR("Exposition : {0} s, f/{1}, ISO {2}"),
    ES("Exposición: {0} s, f/{1}, ISO {2}"),
    PT("Exposição: {0} s, f/{1}, ISO {2}"),
    IT("Esposizione: {0} s, f/{1}, ISO {2}"),
    NL("Belichting: {0} s, f/{1}, ISO {2}"),
    RU("Экспозиция: {0} с, f/{1}, ISO {2}"),
    TR("Pozlama: {0} sn, f/{1}, ISO {2}"));

// ===========================================================================
// GPS and IMU
// ===========================================================================

SS_MSG(reading_sensors,
    EN("Reading the sensors…"),
    JA("センサーを読み込んでいます…"),
    ZH_HANS("正在读取传感器…"),
    ZH_HANT("正在讀取感測器…"),
    KO("센서를 읽는 중…"),
    DE("Sensoren werden gelesen…"),
    FR("Lecture des capteurs…"),
    ES("Leyendo los sensores…"),
    PT("Lendo os sensores…"),
    IT("Lettura dei sensori…"),
    NL("Sensoren worden gelezen…"),
    RU("Чтение датчиков…"),
    TR("Algılayıcılar okunuyor…"));

SS_MSG(reading_photos,
    EN("Read: {0} of {1}"),
    JA("読み込み済み: {0} / {1}"),
    ZH_HANS("已读取：{0} / {1}"),
    ZH_HANT("已讀取：{0} / {1}"),
    KO("읽음: {0} / {1}"),
    DE("Gelesen: {0} von {1}"),
    FR("Lues : {0} sur {1}"),
    ES("Leídas: {0} de {1}"),
    PT("Lidas: {0} de {1}"),
    IT("Lette: {0} su {1}"),
    NL("Gelezen: {0} van {1}"),
    RU("Прочитано: {0} из {1}"),
    TR("Okunan: {0} / {1}"));

SS_MSG(sensors_not_in_build,
    EN("This build cannot read a video's IMU and GPS: they are read by the "
       "built-in reconstruction, which it does not include."),
    JA("このビルドでは動画の IMU と GPS を読めません。それらは内蔵の再構成が読む"
       "もので、このビルドには含まれていません。"),
    ZH_HANS("这个版本无法读取视频的 IMU 和 GPS：它们由内置重建读取，而这个版本不包含"
            "内置重建。"),
    ZH_HANT("這個版本無法讀取影片的 IMU 與 GPS：它們由內建重建讀取，而這個版本不包含"
            "內建重建。"),
    KO("이 빌드는 동영상의 IMU와 GPS를 읽을 수 없습니다. 그것들은 내장 재구성이 "
       "읽는데, 이 빌드에는 포함되어 있지 않습니다."),
    DE("Dieser Build kann IMU und GPS eines Videos nicht lesen: Sie werden von "
       "der eingebauten Rekonstruktion gelesen, die er nicht enthält."),
    FR("Cette version ne peut pas lire l'IMU et le GPS d'une vidéo : c'est la "
       "reconstruction intégrée qui les lit, et elle n'en fait pas partie."),
    ES("Esta versión no puede leer la IMU ni el GPS de un vídeo: los lee la "
       "reconstrucción integrada, que no incluye."),
    PT("Esta versão não consegue ler a IMU e o GPS de um vídeo: quem os lê é a "
       "reconstrução embutida, que ela não inclui."),
    IT("Questa build non può leggere IMU e GPS di un video: li legge la "
       "ricostruzione integrata, che non contiene."),
    NL("Deze build kan de IMU en gps van een video niet lezen: die leest de "
       "ingebouwde reconstructie, en die zit er niet in."),
    RU("Эта сборка не может читать IMU и GPS видео: их читает встроенная "
       "реконструкция, которой в ней нет."),
    TR("Bu derleme bir videonun IMU ve GPS verisini okuyamaz: bunları yerleşik "
       "yeniden oluşturma okur ve bu derlemede o yok."));

SS_MSG(sensors_none_video,
    EN("This video carries no IMU or GPS that can be read."),
    JA("この動画には読み取れる IMU も GPS もありません。"),
    ZH_HANS("这个视频没有可读取的 IMU 或 GPS。"),
    ZH_HANT("這個影片沒有可讀取的 IMU 或 GPS。"),
    KO("이 동영상에는 읽을 수 있는 IMU나 GPS가 없습니다."),
    DE("Dieses Video enthält kein lesbares IMU oder GPS."),
    FR("Cette vidéo ne contient ni IMU ni GPS lisible."),
    ES("Este vídeo no lleva IMU ni GPS que se puedan leer."),
    PT("Este vídeo não traz IMU nem GPS que possam ser lidos."),
    IT("Questo video non contiene IMU o GPS leggibili."),
    NL("Deze video bevat geen leesbare IMU of gps."),
    RU("В этом видео нет читаемых данных IMU или GPS."),
    TR("Bu videoda okunabilir IMU ya da GPS verisi yok."));

SS_MSG(sensors_none_photos,
    EN("None of these photos records where it was taken."),
    JA("これらの写真には撮影位置の記録がありません。"),
    ZH_HANS("这些照片都没有记录拍摄位置。"),
    ZH_HANT("這些相片都沒有記錄拍攝位置。"),
    KO("이 사진들 중 촬영 위치가 기록된 것이 없습니다."),
    DE("Keines dieser Fotos verzeichnet, wo es aufgenommen wurde."),
    FR("Aucune de ces photos n'indique où elle a été prise."),
    ES("Ninguna de estas fotos registra dónde se tomó."),
    PT("Nenhuma destas fotos registra onde foi tirada."),
    IT("Nessuna di queste foto registra dove è stata scattata."),
    NL("Geen van deze foto's legt vast waar hij is gemaakt."),
    RU("Ни одна из этих фотографий не хранит место съёмки."),
    TR("Bu fotoğrafların hiçbiri nerede çekildiğini kaydetmiyor."));

SS_MSG(gps_none,
    EN("GPS: none"),
    JA("GPS: なし"),
    ZH_HANS("GPS：无"),
    ZH_HANT("GPS：無"),
    KO("GPS: 없음"),
    DE("GPS: keins"),
    FR("GPS : aucun"),
    ES("GPS: ninguno"),
    PT("GPS: nenhum"),
    IT("GPS: nessuno"),
    NL("Gps: geen"),
    RU("GPS: нет"),
    TR("GPS: yok"));

SS_MSG(gps_usable,
    EN("GPS: looks usable"),
    JA("GPS: 使えそうです"),
    ZH_HANS("GPS：看起来可用"),
    ZH_HANT("GPS：看起來可用"),
    KO("GPS: 쓸 만해 보임"),
    DE("GPS: wirkt brauchbar"),
    FR("GPS : semble utilisable"),
    ES("GPS: parece utilizable"),
    PT("GPS: parece utilizável"),
    IT("GPS: sembra utilizzabile"),
    NL("Gps: lijkt bruikbaar"),
    RU("GPS: похоже, пригоден"),
    TR("GPS: kullanılabilir görünüyor"));

SS_MSG(gps_unusable,
    EN("GPS: not usable"),
    JA("GPS: 使えません"),
    ZH_HANS("GPS：不可用"),
    ZH_HANT("GPS：不可用"),
    KO("GPS: 쓸 수 없음"),
    DE("GPS: nicht brauchbar"),
    FR("GPS : inutilisable"),
    ES("GPS: no utilizable"),
    PT("GPS: não utilizável"),
    IT("GPS: non utilizzabile"),
    NL("Gps: niet bruikbaar"),
    RU("GPS: непригоден"),
    TR("GPS: kullanılamaz"));

SS_MSG(gps_unusable_help,
    EN("The track is too short, stuck in one place, or mostly without a fix. "
       "The reconstruction will not take its scale from it."),
    JA("軌跡が短すぎる、1 か所から動かない、またはほとんど測位できていません。再"
       "構成はこれを実寸の基準にしません。"),
    ZH_HANS("轨迹太短、停在一个地方不动，或大多没有定位。重建不会用它来确定尺度。"),
    ZH_HANT("軌跡太短、停在一個地方不動，或大多沒有定位。重建不會用它來決定尺度。"),
    KO("궤적이 너무 짧거나, 한 곳에 멈춰 있거나, 대부분 위치를 잡지 못했습니다. "
       "재구성은 이것으로 실제 크기를 정하지 않습니다."),
    DE("Die Spur ist zu kurz, steht an einer Stelle still oder hat meist keinen "
       "Fix. Die Rekonstruktion nimmt ihren Maßstab nicht daraus."),
    FR("La trace est trop courte, figée en un point ou presque toujours sans "
       "position. La reconstruction n'en tirera pas son échelle."),
    ES("La traza es demasiado corta, está quieta en un punto o casi siempre sin "
       "posición. La reconstrucción no tomará de ella su escala."),
    PT("O trajeto é curto demais, está parado num ponto ou quase sempre sem "
       "posição. A reconstrução não tirará dele a escala."),
    IT("La traccia è troppo corta, ferma in un punto o quasi sempre senza "
       "posizione. La ricostruzione non ne ricaverà la scala."),
    NL("Het spoor is te kort, staat stil op één plek of heeft meestal geen fix. "
       "De reconstructie haalt er haar schaal niet uit."),
    RU("Трек слишком короткий, стоит на месте или почти всё время без фиксации. "
       "Реконструкция не будет брать из него масштаб."),
    TR("İz çok kısa, tek bir yerde takılı ya da çoğunlukla konumsuz. Yeniden "
       "oluşturma ölçeğini bundan almaz."));

SS_MSG(imu_none,
    EN("IMU: none"),
    JA("IMU: なし"),
    ZH_HANS("IMU：无"),
    ZH_HANT("IMU：無"),
    KO("IMU: 없음"),
    DE("IMU: keine"),
    FR("IMU : aucune"),
    ES("IMU: ninguna"),
    PT("IMU: nenhuma"),
    IT("IMU: nessuna"),
    NL("IMU: geen"),
    RU("IMU: нет"),
    TR("IMU: yok"));

SS_MSG(imu_usable,
    EN("IMU: looks usable"),
    JA("IMU: 使えそうです"),
    ZH_HANS("IMU：看起来可用"),
    ZH_HANT("IMU：看起來可用"),
    KO("IMU: 쓸 만해 보임"),
    DE("IMU: wirkt brauchbar"),
    FR("IMU : semble utilisable"),
    ES("IMU: parece utilizable"),
    PT("IMU: parece utilizável"),
    IT("IMU: sembra utilizzabile"),
    NL("IMU: lijkt bruikbaar"),
    RU("IMU: похоже, пригоден"),
    TR("IMU: kullanılabilir görünüyor"));

SS_MSG(imu_unusable,
    EN("IMU: not usable"),
    JA("IMU: 使えません"),
    ZH_HANS("IMU：不可用"),
    ZH_HANT("IMU：不可用"),
    KO("IMU: 쓸 수 없음"),
    DE("IMU: nicht brauchbar"),
    FR("IMU : inutilisable"),
    ES("IMU: no utilizable"),
    PT("IMU: não utilizável"),
    IT("IMU: non utilizzabile"),
    NL("IMU: niet bruikbaar"),
    RU("IMU: непригоден"),
    TR("IMU: kullanılamaz"));

SS_MSG(imu_unusable_help,
    EN("The readings do not look like a working sensor: wrong units, too short, "
       "or out of step with the video."),
    JA("読み取り値が正常なセンサーのものに見えません。単位が違う、短すぎる、また"
       "は動画と時刻が合っていません。"),
    ZH_HANS("读数看起来不像正常工作的传感器：单位不对、太短，或与视频时间对不上。"),
    ZH_HANT("讀數看起來不像正常運作的感測器：單位不對、太短，或與影片時間對不上。"),
    KO("측정값이 정상적인 센서의 것으로 보이지 않습니다. 단위가 틀렸거나, 너무 "
       "짧거나, 동영상과 시간이 맞지 않습니다."),
    DE("Die Messwerte sehen nicht nach einem funktionierenden Sensor aus: "
       "falsche Einheiten, zu kurz oder nicht im Takt mit dem Video."),
    FR("Les mesures ne ressemblent pas à celles d'un capteur qui fonctionne : "
       "mauvaises unités, trop courtes, ou décalées par rapport à la vidéo."),
    ES("Las lecturas no parecen de un sensor que funcione: unidades erróneas, "
       "demasiado cortas o desacompasadas con el vídeo."),
    PT("As leituras não parecem de um sensor funcionando: unidades erradas, "
       "curtas demais ou fora de sincronia com o vídeo."),
    IT("Le letture non sembrano di un sensore funzionante: unità sbagliate, "
       "troppo brevi o sfasate rispetto al video."),
    NL("De metingen lijken niet van een werkende sensor: verkeerde eenheden, te "
       "kort, of niet in de pas met de video."),
    RU("Показания не похожи на работающий датчик: неверные единицы, слишком "
       "короткие или не совпадают по времени с видео."),
    TR("Okumalar çalışan bir algılayıcıya benzemiyor: birimler yanlış, çok kısa "
       "ya da videoyla zamanı uyuşmuyor."));

SS_MSG(gps_photos,
    EN("GPS: {0} of {1} photos"),
    JA("GPS: {0} / {1} 枚"),
    ZH_HANS("GPS：{0} / {1} 张照片"),
    ZH_HANT("GPS：{0} / {1} 張相片"),
    KO("GPS: 사진 {1}장 중 {0}장"),
    DE("GPS: {0} von {1} Fotos"),
    FR("GPS : {0} photos sur {1}"),
    ES("GPS: {0} de {1} fotos"),
    PT("GPS: {0} de {1} fotos"),
    IT("GPS: {0} foto su {1}"),
    NL("Gps: {0} van {1} foto's"),
    RU("GPS: {0} фото из {1}"),
    TR("GPS: {1} fotoğrafın {0} tanesinde"));

SS_MSG(gps_photos_help,
    EN("Photos whose EXIF records a position. The reconstruction uses them for "
       "scale and direction unless GPS is switched off on the input's row."),
    JA("EXIF に位置が記録されている写真です。入力の行で GPS をオフにしない限り、"
       "再構成は実寸と方位にこれを使います。"),
    ZH_HANS("EXIF 中记录了位置的照片。除非在该输入的行上关闭 GPS，重建会用它们确定尺"
            "度和方向。"),
    ZH_HANT("EXIF 中記錄了位置的相片。除非在該輸入的列上關閉 GPS，重建會用它們決定尺"
            "度與方向。"),
    KO("EXIF에 위치가 기록된 사진입니다. 입력 행에서 GPS를 끄지 않는 한, 재구성"
       "은 이것으로 크기와 방향을 정합니다."),
    DE("Fotos, deren EXIF eine Position verzeichnet. Die Rekonstruktion nutzt "
       "sie für Maßstab und Richtung, außer GPS ist in der Zeile der Eingabe "
       "ausgeschaltet."),
    FR("Photos dont l'EXIF indique une position. La reconstruction s'en sert "
       "pour l'échelle et l'orientation, sauf si le GPS est désactivé sur la "
       "ligne de l'entrée."),
    ES("Fotos cuyo EXIF registra una posición. La reconstrucción las usa para "
       "la escala y la orientación salvo que el GPS esté desactivado en la fila "
       "de la entrada."),
    PT("Fotos cujo EXIF registra uma posição. A reconstrução as usa para escala "
       "e direção, a menos que o GPS esteja desligado na linha da entrada."),
    IT("Foto il cui EXIF registra una posizione. La ricostruzione le usa per la "
       "scala e la direzione, a meno che il GPS sia spento nella riga "
       "dell'ingresso."),
    NL("Foto's waarvan de EXIF een positie vastlegt. De reconstructie gebruikt "
       "ze voor schaal en richting, tenzij gps op de rij van de invoer is "
       "uitgezet."),
    RU("Фотографии, в EXIF которых записано положение. Реконструкция использует "
       "их для масштаба и направления, если GPS не выключен в строке входа."),
    TR("EXIF'inde konum kayıtlı fotoğraflar. Girdinin satırında GPS "
       "kapatılmadıkça yeniden oluşturma bunları ölçek ve yön için kullanır."));

SS_MSG(attitude_photos,
    EN("Attitude: {0} of {1} photos"),
    JA("姿勢: {0} / {1} 枚"),
    ZH_HANS("姿态：{0} / {1} 张照片"),
    ZH_HANT("姿態：{0} / {1} 張相片"),
    KO("자세: 사진 {1}장 중 {0}장"),
    DE("Lage: {0} von {1} Fotos"),
    FR("Attitude : {0} photos sur {1}"),
    ES("Actitud: {0} de {1} fotos"),
    PT("Atitude: {0} de {1} fotos"),
    IT("Assetto: {0} foto su {1}"),
    NL("Stand: {0} van {1} foto's"),
    RU("Ориентация: {0} фото из {1}"),
    TR("Duruş: {1} fotoğrafın {0} tanesinde"));

SS_MSG(attitude_photos_help,
    EN("Photos that record which way the camera pointed, as drones write it."),
    JA("ドローンが書き込むような、カメラの向きを記録した写真です。"),
    ZH_HANS("记录了相机朝向的照片，无人机会写入这类信息。"),
    ZH_HANT("記錄了相機朝向的相片，無人機會寫入這類資訊。"),
    KO("드론이 기록하듯 카메라가 향한 방향을 기록한 사진입니다."),
    DE("Fotos, die festhalten, wohin die Kamera zeigte, wie Drohnen es "
       "schreiben."),
    FR("Photos qui indiquent la direction visée par la caméra, comme l'écrivent "
       "les drones."),
    ES("Fotos que registran hacia dónde apuntaba la cámara, como lo escriben "
       "los drones."),
    PT("Fotos que registram para onde a câmera apontava, como os drones gravam."),
    IT("Foto che registrano dove puntava la fotocamera, come lo scrivono i "
       "droni."),
    NL("Foto's die vastleggen waar de camera naartoe wees, zoals drones dat "
       "schrijven."),
    RU("Фотографии, в которых записано, куда смотрела камера, как это делают "
       "дроны."),
    TR("Dronların yazdığı gibi, kameranın nereye baktığını kaydeden fotoğraflar."));

SS_MSG(gps_distinct,
    EN("Distinct positions: {0}"),
    JA("異なる位置: {0}"),
    ZH_HANS("不同位置：{0}"),
    ZH_HANT("不同位置：{0}"),
    KO("서로 다른 위치: {0}"),
    DE("Verschiedene Positionen: {0}"),
    FR("Positions distinctes : {0}"),
    ES("Posiciones distintas: {0}"),
    PT("Posições distintas: {0}"),
    IT("Posizioni distinte: {0}"),
    NL("Verschillende posities: {0}"),
    RU("Разных положений: {0}"),
    TR("Farklı konum: {0}"));

SS_MSG(gps_distinct_help,
    EN("Most photos repeat a position already seen: the receiver kept reporting "
       "an old fix, which says nothing about where they were taken."),
    JA("ほとんどの写真が既出の位置を繰り返しています。受信機が古い測位を出し続け"
       "たもので、撮影場所については何も示しません。"),
    ZH_HANS("大多数照片重复了已经出现过的位置：接收器一直在报告旧的定位，这对拍摄地点"
            "毫无意义。"),
    ZH_HANT("大多數相片重複了已經出現過的位置：接收器一直在回報舊的定位，這對拍攝地點"
            "毫無意義。"),
    KO("대부분의 사진이 이미 나온 위치를 되풀이합니다. 수신기가 옛 위치를 계속 "
       "보고한 것이라 촬영 장소에 대해 아무것도 알려 주지 않습니다."),
    DE("Die meisten Fotos wiederholen eine schon gesehene Position: Der "
       "Empfänger meldete weiter einen alten Fix, der nichts darüber sagt, wo "
       "sie aufgenommen wurden."),
    FR("La plupart des photos répètent une position déjà vue : le récepteur a "
       "continué à donner une ancienne position, qui ne dit rien de l'endroit "
       "où elles ont été prises."),
    ES("La mayoría de las fotos repite una posición ya vista: el receptor "
       "siguió dando una posición antigua, que no dice nada de dónde se tomaron."),
    PT("A maioria das fotos repete uma posição já vista: o receptor continuou "
       "informando uma posição antiga, que não diz nada sobre onde foram "
       "tiradas."),
    IT("La maggior parte delle foto ripete una posizione già vista: il "
       "ricevitore ha continuato a riportare una vecchia posizione, che non "
       "dice nulla su dove sono state scattate."),
    NL("De meeste foto's herhalen een al geziene positie: de ontvanger bleef "
       "een oude fix melden, die niets zegt over waar ze zijn gemaakt."),
    RU("Большинство фотографий повторяет уже встречавшееся положение: приёмник "
       "продолжал выдавать старую фиксацию, и она ничего не говорит о том, где "
       "сделаны снимки."),
    TR("Fotoğrafların çoğu daha önce görülmüş bir konumu tekrarlıyor: alıcı "
       "eski bir konumu bildirmeyi sürdürmüş; bu, nerede çekildikleri hakkında "
       "hiçbir şey söylemez."));

SS_MSG(gps_outliers,
    EN("Dropped as outliers: {0}"),
    JA("外れ値として除外: {0}"),
    ZH_HANS("作为离群值丢弃：{0}"),
    ZH_HANT("作為離群值捨棄：{0}"),
    KO("이상값으로 버림: {0}"),
    DE("Als Ausreißer verworfen: {0}"),
    FR("Écartées comme aberrantes : {0}"),
    ES("Descartadas como atípicas: {0}"),
    PT("Descartadas como discrepantes: {0}"),
    IT("Scartate come anomale: {0}"),
    NL("Als uitschieter weggelaten: {0}"),
    RU("Отброшено как выбросы: {0}"),
    TR("Aykırı diye atılan: {0}"));

SS_MSG(gps_outliers_help,
    EN("Fixes the reconstruction ignores: a dilution of precision over 10, or a "
       "jump faster than 50 m/s. They are marked × on the map."),
    JA("再構成が無視する測位です。精度低下率が 10 を超えるか、50 m/s より速く跳"
       "んだもの。地図上に × で示します。"),
    ZH_HANS("重建会忽略的定位：精度因子超过 10，或跳变速度超过 50 m/s。在地图上用 × "
            "标出。"),
    ZH_HANT("重建會忽略的定位：精度因子超過 10，或跳動速度超過 50 m/s。在地圖上以 × "
            "標出。"),
    KO("재구성이 무시하는 위치입니다. 정밀도 저하율이 10을 넘거나, 50 m/s보다 빠"
       "르게 튄 것. 지도에 ×로 표시합니다."),
    DE("Fixes, die die Rekonstruktion ignoriert: eine Genauigkeitsminderung "
       "über 10 oder ein Sprung schneller als 50 m/s. Auf der Karte mit × "
       "markiert."),
    FR("Positions que la reconstruction ignore : une dilution de précision "
       "au-delà de 10, ou un saut plus rapide que 50 m/s. Marquées × sur la "
       "carte."),
    ES("Posiciones que la reconstrucción ignora: una dilución de precisión "
       "mayor que 10 o un salto más rápido que 50 m/s. Se marcan con × en el "
       "mapa."),
    PT("Posições que a reconstrução ignora: uma diluição de precisão acima de "
       "10 ou um salto mais rápido que 50 m/s. Marcadas com × no mapa."),
    IT("Posizioni che la ricostruzione ignora: una diluizione della precisione "
       "oltre 10 o un salto più rapido di 50 m/s. Segnate con × sulla mappa."),
    NL("Fixes die de reconstructie negeert: een precisieverdunning boven 10, of "
       "een sprong sneller dan 50 m/s. Op de kaart gemarkeerd met ×."),
    RU("Фиксации, которые реконструкция пропускает: снижение точности больше 10 "
       "или скачок быстрее 50 м/с. На карте отмечены ×."),
    TR("Yeniden oluşturmanın yok saydığı konumlar: 10'u aşan hassasiyet kaybı "
       "ya da 50 m/sn'den hızlı bir sıçrama. Haritada × ile işaretlidir."));

SS_MSG(gps_far,
    EN("Far from the rest: {0}"),
    JA("ほかから遠い: {0}"),
    ZH_HANS("远离其他照片：{0}"),
    ZH_HANT("遠離其他相片：{0}"),
    KO("다른 사진과 멂: {0}"),
    DE("Weit von den übrigen: {0}"),
    FR("Loin des autres : {0}"),
    ES("Lejos del resto: {0}"),
    PT("Longe das demais: {0}"),
    IT("Lontane dalle altre: {0}"),
    NL("Ver van de rest: {0}"),
    RU("Далеко от остальных: {0}"),
    TR("Diğerlerinden uzak: {0}"));

SS_MSG(gps_far_help,
    EN("Photos placed far from where the others were taken: a dropped minus "
       "sign, or a fix left over from somewhere else. They are marked × on the "
       "map."),
    JA("ほかの写真の撮影場所から遠くに置かれた写真です。マイナス記号の欠落か、別"
       "の場所の測位が残ったものです。地図上に × で示します。"),
    ZH_HANS("被放在远离其他照片拍摄地点的照片：丢了负号，或残留了别处的定位。在地图上"
            "用 × 标出。"),
    ZH_HANT("被放在遠離其他相片拍攝地點的相片：漏了負號，或殘留了別處的定位。在地圖上"
            "以 × 標出。"),
    KO("다른 사진을 찍은 곳에서 멀리 놓인 사진입니다. 마이너스 부호가 빠졌거나 "
       "다른 곳의 위치가 남은 것입니다. 지도에 ×로 표시합니다."),
    DE("Fotos, die weit von dort liegen, wo die anderen aufgenommen wurden: ein "
       "verlorenes Minuszeichen oder ein übrig gebliebener Fix von anderswo. "
       "Auf der Karte mit × markiert."),
    FR("Photos placées loin de l'endroit où les autres ont été prises : un "
       "signe moins perdu, ou une position restée d'ailleurs. Marquées × sur la "
       "carte."),
    ES("Fotos situadas lejos de donde se tomaron las demás: un signo menos "
       "perdido o una posición que quedó de otro sitio. Se marcan con × en el "
       "mapa."),
    PT("Fotos colocadas longe de onde as outras foram tiradas: um sinal de "
       "menos perdido ou uma posição que sobrou de outro lugar. Marcadas com × "
       "no mapa."),
    IT("Foto collocate lontano da dove sono state scattate le altre: un segno "
       "meno perso o una posizione rimasta da un altro luogo. Segnate con × "
       "sulla mappa."),
    NL("Foto's die ver liggen van waar de andere zijn gemaakt: een verloren "
       "minteken, of een fix die van ergens anders is blijven hangen. Op de "
       "kaart gemarkeerd met ×."),
    RU("Фотографии, оказавшиеся далеко от места, где сняты остальные: "
       "потерянный знак минус или фиксация, оставшаяся от другого места. На "
       "карте отмечены ×."),
    TR("Diğerlerinin çekildiği yerden uzağa düşmüş fotoğraflar: kaybolmuş bir "
       "eksi işareti ya da başka bir yerden kalmış bir konum. Haritada × ile "
       "işaretlidir."));

SS_MSG(gps_zero,
    EN("At 0, 0 (ignored): {0}"),
    JA("0, 0 の位置（無視）: {0}"),
    ZH_HANS("位于 0, 0（已忽略）：{0}"),
    ZH_HANT("位於 0, 0（已忽略）：{0}"),
    KO("0, 0 위치(무시): {0}"),
    DE("Bei 0, 0 (ignoriert): {0}"),
    FR("À 0, 0 (ignorées) : {0}"),
    ES("En 0, 0 (ignoradas): {0}"),
    PT("Em 0, 0 (ignoradas): {0}"),
    IT("A 0, 0 (ignorate): {0}"),
    NL("Op 0, 0 (genegeerd): {0}"),
    RU("В точке 0, 0 (пропущено): {0}"),
    TR("0, 0 konumunda (yok sayıldı): {0}"));

SS_MSG(gps_zero_help,
    EN("A receiver without a fix writes latitude 0, longitude 0. Those "
       "positions are left out here and by the reconstruction."),
    JA("測位できていない受信機は緯度 0、経度 0 を書き込みます。その位置はここで"
       "も再構成でも除外します。"),
    ZH_HANS("没有定位的接收器会写入纬度 0、经度 0。这些位置在这里和重建中都会被排除。"),
    ZH_HANT("沒有定位的接收器會寫入緯度 0、經度 0。這些位置在這裡和重建中都會被排除。"),
    KO("위치를 잡지 못한 수신기는 위도 0, 경도 0을 기록합니다. 그런 위치는 여기"
       "서도 재구성에서도 뺍니다."),
    DE("Ein Empfänger ohne Fix schreibt Breite 0, Länge 0. Solche Positionen "
       "bleiben hier und in der Rekonstruktion außen vor."),
    FR("Un récepteur sans position écrit latitude 0, longitude 0. Ces positions "
       "sont écartées ici comme dans la reconstruction."),
    ES("Un receptor sin posición escribe latitud 0, longitud 0. Esas posiciones "
       "se descartan aquí y en la reconstrucción."),
    PT("Um receptor sem posição grava latitude 0, longitude 0. Essas posições "
       "ficam de fora aqui e na reconstrução."),
    IT("Un ricevitore senza posizione scrive latitudine 0, longitudine 0. "
       "Quelle posizioni vengono escluse qui e nella ricostruzione."),
    NL("Een ontvanger zonder fix schrijft breedte 0, lengte 0. Die posities "
       "blijven hier en in de reconstructie buiten beschouwing."),
    RU("Приёмник без фиксации записывает широту 0, долготу 0. Такие положения "
       "не учитываются ни здесь, ни в реконструкции."),
    TR("Konum bulamayan alıcı enlem 0, boylam 0 yazar. Bu konumlar burada da "
       "yeniden oluşturmada da dışarıda bırakılır."));

SS_MSG(gps_no_fix,
    EN("Not a fix (shown, not used): {0}"),
    JA("測位ではない位置（表示のみ、未使用）: {0}"),
    ZH_HANS("非定位（仅显示，不使用）：{0}"),
    ZH_HANT("非定位（僅顯示，不使用）：{0}"),
    KO("측위 아님(표시만, 사용 안 함): {0}"),
    DE("Kein Fix (gezeigt, nicht verwendet): {0}"),
    FR("Sans position fixée (affichées, non utilisées) : {0}"),
    ES("Sin posición fija (mostradas, no usadas): {0}"),
    PT("Sem posição fixa (mostradas, não usadas): {0}"),
    IT("Senza fix (mostrate, non usate): {0}"),
    NL("Geen fix (getoond, niet gebruikt): {0}"),
    RU("Без фиксации (показано, не используется): {0}"),
    TR("Konum kilidi yok (gösterilir, kullanılmaz): {0}"));

SS_MSG(gps_no_fix_help,
    EN("Positions the receiver itself did not mark as a fix, often the last place it saw the sky.\nThey are marked × in amber on the map, and the reconstruction leaves them out."),
    JA("受信機自身が測位として示さなかった位置です。\n- 多くは最後に空が見えた場所です。\n- 地図上に黄色の × で示し、再構成では使いません。"),
    ZH_HANS("接收器自身未标记为定位的位置，通常是它最后一次看到天空的地方。\n在地图上用黄色 × 标出，重建不会使用。"),
    ZH_HANT("接收器本身未標記為定位的位置，通常是它最後一次看到天空的地方。\n在地圖上以黃色 × 標出，重建不會使用。"),
    KO("수신기가 스스로 측위로 표시하지 않은 위치로, 대개 마지막으로 하늘이 보인 곳입니다.\n지도에 노란색 ×로 표시하며 재구성에는 쓰지 않습니다."),
    DE("Positionen, die der Empfänger selbst nicht als Fix markiert hat, oft der Ort, an dem er zuletzt den Himmel sah. Auf der Karte gelb mit × markiert; die Rekonstruktion lässt sie weg."),
    FR("Positions que le récepteur lui-même n'a pas marquées comme fixées, souvent le dernier endroit où il voyait le ciel.\nMarquées × en jaune sur la carte ; la reconstruction les écarte."),
    ES("Posiciones que el propio receptor no marcó como fijas, a menudo el último lugar donde vio el cielo.\nSe marcan con × en amarillo en el mapa y la reconstrucción las descarta."),
    PT("Posições que o próprio receptor não marcou como fixas, muitas vezes o último lugar onde viu o céu.\nMarcadas com × em amarelo no mapa; a reconstrução deixa-as de fora."),
    IT("Posizioni che il ricevitore stesso non ha segnato come fix, spesso l'ultimo punto in cui vedeva il cielo.\nSegnate con × in giallo sulla mappa; la ricostruzione le esclude."),
    NL("Posities die de ontvanger zelf niet als fix markeerde, vaak de laatste plek waar hij de hemel zag.\nOp de kaart geel gemarkeerd met ×; de reconstructie laat ze weg."),
    RU("Положения, которые сам приёмник не отметил как фиксацию, часто последнее место, где он видел небо.\nНа карте отмечены жёлтым ×, реконструкция их не использует."),
    TR("Alıcının kendisinin konum kilidi olarak işaretlemediği konumlar; çoğu zaman gökyüzünü son gördüğü yer.\nHaritada sarı × ile işaretlidir ve yeniden oluşturma bunları kullanmaz."));

SS_MSG(map_fit,
    EN("Fit"),
    JA("全体表示"),
    ZH_HANS("适应"),
    ZH_HANT("符合"),
    KO("맞추기"),
    DE("Einpassen"),
    FR("Ajuster"),
    ES("Ajustar"),
    PT("Ajustar"),
    IT("Adatta"),
    NL("Passend"),
    RU("Вписать"),
    TR("Sığdır"));

SS_MSG(map_fit_help,
    EN("Fit the map to the track. Drag to move it, scroll to zoom, double-click "
       "to fit."),
    JA("地図を軌跡に合わせます。ドラッグで移動、スクロールで拡大縮小、ダブルクリ"
       "ックで全体表示。"),
    ZH_HANS("让地图适应轨迹。拖动可移动，滚轮可缩放，双击可适应。"),
    ZH_HANT("讓地圖符合軌跡。拖曳可移動，捲動可縮放，按兩下可符合。"),
    KO("지도를 궤적에 맞춥니다. 끌면 이동, 스크롤하면 확대·축소, 더블클릭하면 맞"
       "춥니다."),
    DE("Die Karte an die Spur anpassen. Ziehen verschiebt, Scrollen zoomt, "
       "Doppelklick passt ein."),
    FR("Ajuster la carte à la trace. Faites glisser pour la déplacer, défilez "
       "pour zoomer, double-cliquez pour ajuster."),
    ES("Ajustar el mapa a la traza. Arrastra para moverlo, desplaza para hacer "
       "zoom, doble clic para ajustar."),
    PT("Ajustar o mapa ao trajeto. Arraste para mover, role para dar zoom, "
       "clique duas vezes para ajustar."),
    IT("Adatta la mappa alla traccia. Trascina per spostarla, scorri per lo "
       "zoom, doppio clic per adattarla."),
    NL("De kaart passend maken op het spoor. Slepen verschuift, scrollen zoomt, "
       "dubbelklikken maakt passend."),
    RU("Вписать карту в трек. Перетаскивание сдвигает, прокрутка масштабирует, "
       "двойной щелчок вписывает."),
    TR("Haritayı ize sığdır. Taşımak için sürükleyin, yakınlaştırmak için "
       "kaydırın, sığdırmak için çift tıklayın."));

SS_MSG(map_open_osm,
    EN("Open in OpenStreetMap"),
    JA("OpenStreetMap で開く"),
    ZH_HANS("在 OpenStreetMap 中打开"),
    ZH_HANT("在 OpenStreetMap 中開啟"),
    KO("OpenStreetMap에서 열기"),
    DE("In OpenStreetMap öffnen"),
    FR("Ouvrir dans OpenStreetMap"),
    ES("Abrir en OpenStreetMap"),
    PT("Abrir no OpenStreetMap"),
    IT("Apri in OpenStreetMap"),
    NL("Openen in OpenStreetMap"),
    RU("Открыть в OpenStreetMap"),
    TR("OpenStreetMap'te aç"));

SS_MSG(map_open_osm_help,
    EN("Opens openstreetmap.org in the browser at the middle of the track, to "
       "check it is where the capture was made. This sends that location to the "
       "website."),
    JA("軌跡の中央の地点で openstreetmap.org をブラウザで開き、撮影した場所と合"
       "っているか確かめます。その位置がウェブサイトに送られます。"),
    ZH_HANS("在浏览器中打开 openstreetmap.org 并定位到轨迹中点，用来核对它是否就是拍"
            "摄地点。这会把该位置发送给该网站。"),
    ZH_HANT("在瀏覽器中開啟 openstreetmap.org 並定位到軌跡中點，用來核對它是否就是拍"
            "攝地點。這會把該位置傳送給該網站。"),
    KO("궤적 가운데 지점에서 openstreetmap.org를 브라우저로 열어, 촬영한 곳이 맞"
       "는지 확인합니다. 그 위치가 웹사이트로 전송됩니다."),
    DE("Öffnet openstreetmap.org im Browser in der Mitte der Spur, um zu "
       "prüfen, ob dort aufgenommen wurde. Dabei wird dieser Ort an die Website "
       "geschickt."),
    FR("Ouvre openstreetmap.org dans le navigateur au milieu de la trace, pour "
       "vérifier qu'elle se trouve bien là où la prise de vue a eu lieu. Cette "
       "position est alors envoyée au site."),
    ES("Abre openstreetmap.org en el navegador en el centro de la traza, para "
       "comprobar que está donde se grabó. Esto envía esa ubicación al sitio "
       "web."),
    PT("Abre openstreetmap.org no navegador no meio do trajeto, para conferir "
       "se fica onde a captura foi feita. Isso envia essa localização ao site."),
    IT("Apre openstreetmap.org nel browser al centro della traccia, per "
       "controllare che sia dove è stata fatta la ripresa. Questo invia quella "
       "posizione al sito."),
    NL("Opent openstreetmap.org in de browser in het midden van het spoor, om "
       "te controleren of het ligt waar is opgenomen. Daarbij wordt die locatie "
       "naar de website gestuurd."),
    RU("Открывает openstreetmap.org в браузере в середине трека, чтобы "
       "проверить, что он там, где велась съёмка. Это местоположение при этом "
       "отправляется сайту."),
    TR("İzin ortasında openstreetmap.org'u tarayıcıda açar; çekimin yapıldığı "
       "yer olup olmadığını denetlemek içindir. Bu, o konumu web sitesine "
       "gönderir."));

SS_MSG(track_map,
    EN("Map"),
    JA("地図"),
    ZH_HANS("地图"),
    ZH_HANT("地圖"),
    KO("지도"),
    DE("Karte"),
    FR("Carte"),
    ES("Mapa"),
    PT("Mapa"),
    IT("Mappa"),
    NL("Kaart"),
    RU("Карта"),
    TR("Harita"));

SS_MSG(track_map_help,
    EN("The track seen from above, to scale."),
    JA("真上から見た軌跡です。縮尺は正確です。"),
    ZH_HANS("从正上方看的轨迹，比例真实。"),
    ZH_HANT("從正上方看的軌跡，比例真實。"),
    KO("위에서 내려다본 궤적입니다. 축척은 정확합니다."),
    DE("Die Spur von oben, maßstabsgetreu."),
    FR("La trace vue de dessus, à l'échelle."),
    ES("La traza vista desde arriba, a escala."),
    PT("O trajeto visto de cima, em escala."),
    IT("La traccia vista dall'alto, in scala."),
    NL("Het spoor van bovenaf gezien, op schaal."),
    RU("Трек сверху, в масштабе."),
    TR("Yukarıdan bakılan iz, ölçeğine uygun."));

SS_MSG(track_3d,
    EN("3D"),
    JA("3D"),
    ZH_HANS("3D"),
    ZH_HANT("3D"),
    KO("3D"),
    DE("3D"),
    FR("3D"),
    ES("3D"),
    PT("3D"),
    IT("3D"),
    NL("3D"),
    RU("3D"),
    TR("3D"));

SS_MSG(track_3d_help,
    EN("The track in 3D, its height included: altitude that drifts shows as a slope or a step.\nDrag to turn it, right-drag to move it, scroll to zoom, double-click to fit."),
    JA("高さも含めた 3D の軌跡です。 高度がずれていくと、坂や段差として見えます。\nドラッグで回転、右ドラッグで移動、スクロールで拡大縮小、ダブルクリックで全体表示。"),
    ZH_HANS("包含高度的 3D 轨迹：漂移的高度会显示为斜坡或台阶。拖动可旋转，右键拖动可移动，滚轮可缩放，双击可适应。"),
    ZH_HANT("包含高度的 3D 軌跡：漂移的高度會顯示為斜坡或台階。拖曳可旋轉，右鍵拖曳可移動，捲動可縮放，按兩下可符合。"),
    KO("높이까지 담은 3D 궤적입니다. 고도가 틀어지면 비탈이나 계단으로 보입니다.\n끌면 회전, 오른쪽 버튼으로 끌면 이동, 스크롤하면 확대·축소, 더블클릭하면 맞춥니다."),
    DE("Die Spur in 3D, mit ihrer Höhe: eine wegdriftende Höhe zeigt sich als Steigung oder Stufe.\nZiehen dreht, Ziehen mit der rechten Taste verschiebt, Scrollen zoomt, Doppelklick passt ein."),
    FR("Trace en 3D : la dérive d’altitude forme une pente ou une marche.\nGlisser : tourner ; glisser droit : déplacer ; molette : zoom ; double-clic : ajuster."),
    ES("Traza en 3D: la deriva de altitud forma una pendiente o escalón.\nArrastrar: girar; botón derecho: mover; rueda: zoom; doble clic: ajustar."),
    PT("O trajeto em 3D, com a altura: uma altitude que deriva aparece como uma rampa ou um degrau.\nArraste para girar, arraste com o botão direito para mover, role para dar zoom, clique duas vezes para ajustar."),
    IT("La traccia in 3D, altezza compresa: un'altitudine che deriva si vede come una salita o un gradino.\nTrascina per ruotarla, trascina col tasto destro per spostarla, scorri per lo zoom, doppio clic per adattarla."),
    NL("Het spoor in 3D, met de hoogte erbij: een hoogte die wegdrijft is te zien als een helling of een trede.\nSlepen draait, slepen met de rechterknop verschuift, scrollen zoomt, dubbelklikken maakt passend."),
    RU("Трек в 3D: дрейф высоты виден как склон или ступенька.\nПеретаскивание: вращать; правой кнопкой: сдвигать; колесо: масштаб; двойной щелчок: вписать."),
    TR("3D iz: yükseklik kayması eğim ya da basamak olarak görünür.\nSürükle: döndür; sağ sürükle: taşı; kaydır: yakınlaştır; çift tık: sığdır."));

SS_MSG(height_scale,
    EN("Height"),
    JA("高さ"),
    ZH_HANS("高度"),
    ZH_HANT("高度"),
    KO("높이"),
    DE("Höhe"),
    FR("Hauteur"),
    ES("Altura"),
    PT("Altura"),
    IT("Altezza"),
    NL("Hoogte"),
    RU("Высота"),
    TR("Yükseklik"));

SS_MSG(height_times,
    EN("{0}×"),
    JA("{0}×"),
    ZH_HANS("{0}×"),
    ZH_HANT("{0}×"),
    KO("{0}×"),
    DE("{0}×"),
    FR("{0}×"),
    ES("{0}×"),
    PT("{0}×"),
    IT("{0}×"),
    NL("{0}×"),
    RU("{0}×"),
    TR("{0}×"));

SS_MSG(height_auto,
    EN("Auto ({0})"),
    JA("自動 ({0})"),
    ZH_HANS("自动 ({0})"),
    ZH_HANT("自動 ({0})"),
    KO("자동 ({0})"),
    DE("Automatisch ({0})"),
    FR("Auto ({0})"),
    ES("Auto ({0})"),
    PT("Auto ({0})"),
    IT("Auto ({0})"),
    NL("Automatisch ({0})"),
    RU("Авто ({0})"),
    TR("Otomatik ({0})"));

SS_MSG(height_scale_help,
    EN("Stretches the heights, so that a few metres of drift along a long track can be seen.\n- Auto stretches until the climb is a fifth of the track's width; at 1× the track is true to scale.\n- The ruler is in true metres either way."),
    JA("高さを引き伸ばして、長い軌跡に沿った数メートルの高度のずれを見えるようにします。\n- 自動では、上り下りの幅が軌跡の幅の 5 分の 1 になるまで引き伸ばします。 1× では縮尺どおりです。\n- 目盛りはどちらでも実際のメートルです。"),
    ZH_HANS("拉伸高度，让长轨迹上几米的高度漂移也能看出来。 “自动”会拉伸到高度起伏等于轨迹宽度的五分之一； 1× 时比例真实。\n两种情况下标尺都是真实的米数。"),
    ZH_HANT("拉伸高度，讓長軌跡上幾公尺的高度漂移也能看出來。 「自動」會拉伸到高度起伏等於軌跡寬度的五分之一； 1× 時比例真實。\n兩種情況下尺規都是真實的公尺數。"),
    KO("높이를 늘려서 긴 궤적을 따라 생긴 몇 미터의 고도 틀어짐도 보이게 합니다.\n- 자동은 오르내림이 궤적 폭의 5분의 1이 될 때까지 늘립니다. 1×이면 실제 축척입니다.\n- 눈금자는 어느 경우든 실제 미터입니다."),
    DE("Streckt die Höhen, damit ein paar Meter Drift entlang einer langen Spur zu sehen sind.\n- Automatisch streckt, bis der Höhenunterschied ein Fünftel der Spurbreite ist; bei 1× ist die Spur maßstabsgetreu.\n- Das Lineal zeigt in jedem Fall echte Meter."),
    FR("Étire les hauteurs, pour que quelques mètres de dérive le long d'une longue trace se voient.\n- Auto étire jusqu'à ce que le dénivelé fasse un cinquième de la largeur de la trace ;\n- à 1×, la trace est à l'échelle. La règle est en vrais mètres dans tous les cas."),
    ES("Estira las alturas para que se vean unos pocos metros de deriva a lo largo de una traza larga.\n- Auto estira hasta que el desnivel sea una quinta parte del ancho de la traza; a 1× la traza está a escala.\n- La regla está en metros reales en ambos casos."),
    PT("Estica as alturas, para que alguns metros de deriva ao longo de um trajeto longo apareçam.\n- Auto estica até o desnível ser um quinto da largura do trajeto; em 1× o trajeto está em escala.\n- A régua fica em metros reais nos dois casos."),
    IT("Allunga le altezze, perché si vedano pochi metri di deriva lungo una traccia lunga.\n- Auto allunga finché il dislivello è un quinto della larghezza della traccia; a 1× la traccia è in scala.\n- Il righello è in metri veri in ogni caso."),
    NL("Rekt de hoogtes uit, zodat een paar meter drift langs een lang spoor te zien is.\n- Automatisch rekt uit tot het hoogteverschil een vijfde van de breedte van het spoor is;\n- bij 1× is het spoor op schaal. De liniaal is hoe dan ook in echte meters."),
    RU("Растягивает высоты, чтобы стали видны несколько метров дрейфа вдоль длинного трека.\n- «Авто» растягивает, пока перепад высот не станет пятой частью ширины трека; при 1× трек в масштабе.\n- Линейка в любом случае в настоящих метрах."),
    TR("Uzun bir iz boyunca birkaç metrelik kaymanın görülebilmesi için yükseklikleri uzatır.\n- Otomatik, yükseklik farkı izin genişliğinin beşte biri olana dek uzatır; 1× konumunda iz ölçeğine uygundur.\n- Cetvel her durumda gerçek metredir."));

SS_MSG(no_altitude,
    EN("No altitude was recorded: the track is drawn flat."),
    JA("高度が記録されていないため、軌跡は平らに描かれます。"),
    ZH_HANS("没有记录高度：轨迹画成平的。"),
    ZH_HANT("沒有記錄高度：軌跡畫成平的。"),
    KO("고도가 기록되지 않아 궤적을 평평하게 그립니다."),
    DE("Es wurde keine Höhe aufgezeichnet: Die Spur wird flach gezeichnet."),
    FR("Aucune altitude n'a été enregistrée : la trace est dessinée à plat."),
    ES("No se grabó la altitud: la traza se dibuja plana."),
    PT("Nenhuma altitude foi gravada: o trajeto é desenhado plano."),
    IT("Non è stata registrata nessuna altitudine: la traccia è disegnata "
       "piatta."),
    NL("Er is geen hoogte opgenomen: het spoor wordt plat getekend."),
    RU("Высота не записана: трек нарисован плоским."),
    TR("Yükseklik kaydedilmemiş: iz düz çizilir."));

SS_MSG(plot_speed,
    EN("Speed (m/s)"),
    JA("速度 (m/s)"),
    ZH_HANS("速度 (m/s)"),
    ZH_HANT("速度 (m/s)"),
    KO("속도 (m/s)"),
    DE("Geschwindigkeit (m/s)"),
    FR("Vitesse (m/s)"),
    ES("Velocidad (m/s)"),
    PT("Velocidade (m/s)"),
    IT("Velocità (m/s)"),
    NL("Snelheid (m/s)"),
    RU("Скорость (м/с)"),
    TR("Hız (m/sn)"));

SS_MSG(plot_altitude,
    EN("Altitude (m)"),
    JA("高度 (m)"),
    ZH_HANS("高度 (m)"),
    ZH_HANT("高度 (m)"),
    KO("고도 (m)"),
    DE("Höhe (m)"),
    FR("Altitude (m)"),
    ES("Altitud (m)"),
    PT("Altitude (m)"),
    IT("Altitudine (m)"),
    NL("Hoogte (m)"),
    RU("Высота (м)"),
    TR("Yükseklik (m)"));

SS_MSG(plot_altitude_photos,
    EN("Altitude by photo (m)"),
    JA("写真ごとの高度 (m)"),
    ZH_HANS("每张照片的高度 (m)"),
    ZH_HANT("每張相片的高度 (m)"),
    KO("사진별 고도 (m)"),
    DE("Höhe je Foto (m)"),
    FR("Altitude par photo (m)"),
    ES("Altitud por foto (m)"),
    PT("Altitude por foto (m)"),
    IT("Altitudine per foto (m)"),
    NL("Hoogte per foto (m)"),
    RU("Высота по фотографиям (м)"),
    TR("Fotoğraf başına yükseklik (m)"));

SS_MSG(plot_rotation,
    EN("Rotation rate (°/s)"),
    JA("回転速度 (°/s)"),
    ZH_HANS("角速度 (°/s)"),
    ZH_HANT("角速度 (°/s)"),
    KO("회전 속도 (°/s)"),
    DE("Drehrate (°/s)"),
    FR("Vitesse de rotation (°/s)"),
    ES("Velocidad de giro (°/s)"),
    PT("Taxa de rotação (°/s)"),
    IT("Velocità di rotazione (°/s)"),
    NL("Draaisnelheid (°/s)"),
    RU("Скорость вращения (°/с)"),
    TR("Dönüş hızı (°/sn)"));

SS_MSG(plot_acceleration,
    EN("Acceleration (m/s²)"),
    JA("加速度 (m/s²)"),
    ZH_HANS("加速度 (m/s²)"),
    ZH_HANT("加速度 (m/s²)"),
    KO("가속도 (m/s²)"),
    DE("Beschleunigung (m/s²)"),
    FR("Accélération (m/s²)"),
    ES("Aceleración (m/s²)"),
    PT("Aceleração (m/s²)"),
    IT("Accelerazione (m/s²)"),
    NL("Versnelling (m/s²)"),
    RU("Ускорение (м/с²)"),
    TR("İvme (m/sn²)"));

SS_MSG(plot_dop,
    EN("GPS precision (DOP)"),
    JA("GPS の精度 (DOP)"),
    ZH_HANS("GPS 精度 (DOP)"),
    ZH_HANT("GPS 精度 (DOP)"),
    KO("GPS 정밀도 (DOP)"),
    DE("GPS-Genauigkeit (DOP)"),
    FR("Précision GPS (DOP)"),
    ES("Precisión del GPS (DOP)"),
    PT("Precisão do GPS (DOP)"),
    IT("Precisione del GPS (DOP)"),
    NL("Gps-nauwkeurigheid (DOP)"),
    RU("Точность GPS (DOP)"),
    TR("GPS hassasiyeti (DOP)"));

SS_MSG(plot_gravity,
    EN("Gravity direction"),
    JA("重力の向き"),
    ZH_HANS("重力方向"),
    ZH_HANT("重力方向"),
    KO("중력 방향"),
    DE("Richtung der Schwerkraft"),
    FR("Direction de la gravité"),
    ES("Dirección de la gravedad"),
    PT("Direção da gravidade"),
    IT("Direzione della gravità"),
    NL("Richting van de zwaartekracht"),
    RU("Направление силы тяжести"),
    TR("Yerçekiminin yönü"));

SS_MSG(plot_attitude,
    EN("Attitude (°)"),
    JA("姿勢 (°)"),
    ZH_HANS("姿态 (°)"),
    ZH_HANT("姿態 (°)"),
    KO("자세 (°)"),
    DE("Lage (°)"),
    FR("Attitude (°)"),
    ES("Actitud (°)"),
    PT("Atitude (°)"),
    IT("Assetto (°)"),
    NL("Stand (°)"),
    RU("Ориентация (°)"),
    TR("Duruş (°)"));

SS_MSG(plot_gimbal,
    EN("Camera direction (°)"),
    JA("カメラの向き (°)"),
    ZH_HANS("相机朝向 (°)"),
    ZH_HANT("相機朝向 (°)"),
    KO("카메라 방향 (°)"),
    DE("Kamerarichtung (°)"),
    FR("Direction de la caméra (°)"),
    ES("Dirección de la cámara (°)"),
    PT("Direção da câmera (°)"),
    IT("Direzione della fotocamera (°)"),
    NL("Richting van de camera (°)"),
    RU("Направление камеры (°)"),
    TR("Kameranın yönü (°)"));

SS_MSG(plot_magnet,
    EN("Magnetic field (µT)"),
    JA("磁場 (µT)"),
    ZH_HANS("磁场 (µT)"),
    ZH_HANT("磁場 (µT)"),
    KO("자기장 (µT)"),
    DE("Magnetfeld (µT)"),
    FR("Champ magnétique (µT)"),
    ES("Campo magnético (µT)"),
    PT("Campo magnético (µT)"),
    IT("Campo magnetico (µT)"),
    NL("Magnetisch veld (µT)"),
    RU("Магнитное поле (мкТл)"),
    TR("Manyetik alan (µT)"));

SS_MSG(plot_step,
    EN("Distance from the previous photo (m)"),
    JA("前の写真からの距離 (m)"),
    ZH_HANS("与上一张照片的距离 (m)"),
    ZH_HANT("與上一張相片的距離 (m)"),
    KO("이전 사진과의 거리 (m)"),
    DE("Abstand zum vorigen Foto (m)"),
    FR("Distance à la photo précédente (m)"),
    ES("Distancia a la foto anterior (m)"),
    PT("Distância da foto anterior (m)"),
    IT("Distanza dalla foto precedente (m)"),
    NL("Afstand tot de vorige foto (m)"),
    RU("Расстояние от предыдущего фото (м)"),
    TR("Önceki fotoğrafa uzaklık (m)"));

SS_MSG(plots_button,
    EN("Plots…"),
    JA("グラフ…"),
    ZH_HANS("图表…"),
    ZH_HANT("圖表…"),
    KO("그래프…"),
    DE("Diagramme…"),
    FR("Courbes…"),
    ES("Gráficas…"),
    PT("Gráficos…"),
    IT("Grafici…"),
    NL("Grafieken…"),
    RU("Графики…"),
    TR("Grafikler…"));

SS_MSG(plots_help,
    EN("Which of the streams this input carries are plotted."),
    JA("この入力に含まれるデータのうち、どれをグラフにするかを選びます。"),
    ZH_HANS("选择绘制这个输入所带的哪些数据流。"),
    ZH_HANT("選擇繪製這個輸入所帶的哪些資料流。"),
    KO("이 입력에 담긴 데이터 중 무엇을 그래프로 그릴지 고릅니다."),
    DE("Welche der Datenströme dieser Eingabe gezeichnet werden."),
    FR("Lesquels des flux de cette entrée sont tracés."),
    ES("Cuáles de los flujos que trae esta entrada se dibujan."),
    PT("Quais dos fluxos desta entrada são desenhados."),
    IT("Quali dei flussi di questo ingresso vengono tracciati."),
    NL("Welke van de stromen van deze invoer getekend worden."),
    RU("Какие из потоков этого входа рисуются."),
    TR("Bu girdinin taşıdığı akışlardan hangilerinin çizileceği."));

SS_MSG(whole_length,
    EN("Whole length"),
    JA("全体"),
    ZH_HANS("全长"),
    ZH_HANT("全長"),
    KO("전체 길이"),
    DE("Ganze Länge"),
    FR("Toute la durée"),
    ES("Toda la duración"),
    PT("Duração inteira"),
    IT("Durata intera"),
    NL("Hele lengte"),
    RU("Вся длина"),
    TR("Tüm uzunluk"));

SS_MSG(whole_length_help,
    EN("Show the plots' whole length again."),
    JA("グラフを再び全体の長さで表示します。"),
    ZH_HANS("重新显示图表的全长。"),
    ZH_HANT("重新顯示圖表的全長。"),
    KO("그래프를 다시 전체 길이로 보여 줍니다."),
    DE("Die Diagramme wieder über die ganze Länge zeigen."),
    FR("Afficher de nouveau les courbes sur toute leur durée."),
    ES("Volver a mostrar las gráficas en toda su duración."),
    PT("Mostrar de novo os gráficos na duração inteira."),
    IT("Mostra di nuovo i grafici per tutta la durata."),
    NL("De grafieken weer over de hele lengte tonen."),
    RU("Снова показать графики на всю длину."),
    TR("Grafikleri yeniden tüm uzunluğuyla göster."));

SS_MSG(plot_help,
    EN("Drag to move the video's playhead. Scroll to zoom in on time, "
       "right-drag to move along it, double-click to see the whole length."),
    JA("ドラッグで動画の再生位置を動かします。スクロールで時間を拡大、右ドラッグ"
       "で時間方向に移動、ダブルクリックで全体を表示します。"),
    ZH_HANS("拖动可移动视频的播放位置。滚轮可放大时间，右键拖动可沿时间移动，双击可显"
            "示全长。"),
    ZH_HANT("拖曳可移動影片的播放位置。捲動可放大時間，右鍵拖曳可沿時間移動，按兩下可"
            "顯示全長。"),
    KO("끌면 동영상의 재생 위치가 움직입니다. 스크롤하면 시간을 확대하고, 오른쪽"
       " 버튼으로 끌면 시간을 따라 이동하며, 더블클릭하면 전체 길이를 봅니다."),
    DE("Ziehen verschiebt die Abspielposition des Videos. Scrollen zoomt in die "
       "Zeit, Ziehen mit der rechten Taste bewegt entlang der Zeit, Doppelklick "
       "zeigt die ganze Länge."),
    FR("Faites glisser pour déplacer la tête de lecture de la vidéo. Défilez "
       "pour zoomer dans le temps, glissez avec le bouton droit pour le "
       "parcourir, double-cliquez pour tout voir."),
    ES("Arrastra para mover el cabezal de reproducción del vídeo. Desplaza para "
       "hacer zoom en el tiempo, arrastra con el botón derecho para recorrerlo, "
       "doble clic para verlo entero."),
    PT("Arraste para mover a posição de reprodução do vídeo. Role para dar zoom "
       "no tempo, arraste com o botão direito para percorrê-lo, clique duas "
       "vezes para ver tudo."),
    IT("Trascina per spostare la testina del video. Scorri per lo zoom sul "
       "tempo, trascina col tasto destro per percorrerlo, doppio clic per "
       "vedere tutta la durata."),
    NL("Slepen verplaatst de afspeelpositie van de video. Scrollen zoomt in op "
       "de tijd, slepen met de rechterknop gaat erlangs, dubbelklikken toont de "
       "hele lengte."),
    RU("Перетаскивание двигает позицию воспроизведения видео. Прокрутка "
       "увеличивает время, перетаскивание правой кнопкой сдвигает его, двойной "
       "щелчок показывает всю длину."),
    TR("Videonun oynatma konumunu taşımak için sürükleyin. Zamanı "
       "yakınlaştırmak için kaydırın, boyunca ilerlemek için sağ tuşla "
       "sürükleyin, tüm uzunluğu görmek için çift tıklayın."));

SS_MSG(plot_help_photos,
    EN("Click to open that photo. Scroll to zoom in, right-drag to move along, "
       "double-click to see every photo."),
    JA("クリックするとその写真を開きます。スクロールで拡大、右ドラッグで移動、ダ"
       "ブルクリックですべての写真を表示します。"),
    ZH_HANS("点击打开那张照片。滚轮可放大，右键拖动可移动，双击可显示所有照片。"),
    ZH_HANT("點按以開啟那張相片。捲動可放大，右鍵拖曳可移動，按兩下可顯示所有相片。"),
    KO("클릭하면 그 사진을 엽니다. 스크롤하면 확대, 오른쪽 버튼으로 끌면 이동, "
       "더블클릭하면 모든 사진을 봅니다."),
    DE("Klicken öffnet dieses Foto. Scrollen zoomt, Ziehen mit der rechten "
       "Taste verschiebt, Doppelklick zeigt alle Fotos."),
    FR("Cliquez pour ouvrir cette photo. Défilez pour zoomer, glissez avec le "
       "bouton droit pour parcourir, double-cliquez pour voir toutes les photos."),
    ES("Haz clic para abrir esa foto. Desplaza para hacer zoom, arrastra con el "
       "botón derecho para recorrer, doble clic para ver todas las fotos."),
    PT("Clique para abrir aquela foto. Role para dar zoom, arraste com o botão "
       "direito para percorrer, clique duas vezes para ver todas as fotos."),
    IT("Fai clic per aprire quella foto. Scorri per lo zoom, trascina col tasto "
       "destro per spostarti, doppio clic per vedere tutte le foto."),
    NL("Klik om die foto te openen. Scrollen zoomt in, slepen met de "
       "rechterknop verschuift, dubbelklikken toont alle foto's."),
    RU("Щёлкните, чтобы открыть это фото. Прокрутка увеличивает, перетаскивание "
       "правой кнопкой сдвигает, двойной щелчок показывает все фото."),
    TR("O fotoğrafı açmak için tıklayın. Yakınlaştırmak için kaydırın, "
       "ilerlemek için sağ tuşla sürükleyin, tüm fotoğrafları görmek için çift "
       "tıklayın."));

SS_MSG(ch_from_positions,
    EN("from positions"),
    JA("位置から"),
    ZH_HANS("由位置算出"),
    ZH_HANT("由位置算出"),
    KO("위치에서 계산"),
    DE("aus Positionen"),
    FR("d'après les positions"),
    ES("según las posiciones"),
    PT("pelas posições"),
    IT("dalle posizioni"),
    NL("uit posities"),
    RU("по положениям"),
    TR("konumlardan"));

SS_MSG(ch_reported,
    EN("reported"),
    JA("記録値"),
    ZH_HANS("记录值"),
    ZH_HANT("記錄值"),
    KO("기록값"),
    DE("gemeldet"),
    FR("enregistrée"),
    ES("registrada"),
    PT("registrada"),
    IT("registrata"),
    NL("opgegeven"),
    RU("записанная"),
    TR("kaydedilen"));

SS_MSG(ch_gps,
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
    NL("GPS"),
    RU("GPS"),
    TR("GPS"));

SS_MSG(ch_above_takeoff,
    EN("above take-off"),
    JA("離陸地点から"),
    ZH_HANS("相对起飞点"),
    ZH_HANT("相對起飛點"),
    KO("이륙 지점 기준"),
    DE("über dem Start"),
    FR("au-dessus du décollage"),
    ES("sobre el despegue"),
    PT("acima da decolagem"),
    IT("sopra il decollo"),
    NL("boven het opstijgpunt"),
    RU("над точкой взлёта"),
    TR("kalkış noktasının üstü"));

SS_MSG(ch_yaw,
    EN("yaw"),
    JA("ヨー"),
    ZH_HANS("偏航"),
    ZH_HANT("偏航"),
    KO("요"),
    DE("Gier"),
    FR("lacet"),
    ES("guiñada"),
    PT("guinada"),
    IT("imbardata"),
    NL("gieren"),
    RU("рыскание"),
    TR("sapma"));

SS_MSG(ch_pitch,
    EN("pitch"),
    JA("ピッチ"),
    ZH_HANS("俯仰"),
    ZH_HANT("俯仰"),
    KO("피치"),
    DE("Nick"),
    FR("tangage"),
    ES("cabeceo"),
    PT("arfagem"),
    IT("beccheggio"),
    NL("stampen"),
    RU("тангаж"),
    TR("yunuslama"));

SS_MSG(ch_roll,
    EN("roll"),
    JA("ロール"),
    ZH_HANS("横滚"),
    ZH_HANT("橫滾"),
    KO("롤"),
    DE("Roll"),
    FR("roulis"),
    ES("alabeo"),
    PT("rolagem"),
    IT("rollio"),
    NL("rollen"),
    RU("крен"),
    TR("yuvarlanma"));

SS_MSG(diagnostics,
    EN("Diagnostics"),
    JA("診断"),
    ZH_HANS("诊断"),
    ZH_HANT("診斷"),
    KO("진단"),
    DE("Diagnose"),
    FR("Diagnostic"),
    ES("Diagnóstico"),
    PT("Diagnóstico"),
    IT("Diagnostica"),
    NL("Diagnose"),
    RU("Диагностика"),
    TR("Tanılama"));

SS_MSG(scale_m,
    EN("{0} m"),
    JA("{0} m"),
    ZH_HANS("{0} 米"),
    ZH_HANT("{0} 公尺"),
    KO("{0} m"),
    DE("{0} m"),
    FR("{0} m"),
    ES("{0} m"),
    PT("{0} m"),
    IT("{0} m"),
    NL("{0} m"),
    RU("{0} м"),
    TR("{0} m"));

SS_MSG(scale_km,
    EN("{0} km"),
    JA("{0} km"),
    ZH_HANS("{0} 千米"),
    ZH_HANT("{0} 公里"),
    KO("{0} km"),
    DE("{0} km"),
    FR("{0} km"),
    ES("{0} km"),
    PT("{0} km"),
    IT("{0} km"),
    NL("{0} km"),
    RU("{0} км"),
    TR("{0} km"));

SS_MSG(hz_value,
    EN("{0} Hz"),
    JA("{0} Hz"),
    ZH_HANS("{0} Hz"),
    ZH_HANT("{0} Hz"),
    KO("{0} Hz"),
    DE("{0} Hz"),
    FR("{0} Hz"),
    ES("{0} Hz"),
    PT("{0} Hz"),
    IT("{0} Hz"),
    NL("{0} Hz"),
    RU("{0} Гц"),
    TR("{0} Hz"));

SS_MSG(tip_photo,
    EN("Photo {0}"),
    JA("写真 {0}"),
    ZH_HANS("照片 {0}"),
    ZH_HANT("相片 {0}"),
    KO("사진 {0}"),
    DE("Foto {0}"),
    FR("Photo {0}"),
    ES("Foto {0}"),
    PT("Foto {0}"),
    IT("Foto {0}"),
    NL("Foto {0}"),
    RU("Фото {0}"),
    TR("Fotoğraf {0}"));

SS_MSG(tip_position,
    EN("Position: {0}, {1}"),
    JA("位置: {0}, {1}"),
    ZH_HANS("位置：{0}, {1}"),
    ZH_HANT("位置：{0}, {1}"),
    KO("위치: {0}, {1}"),
    DE("Position: {0}, {1}"),
    FR("Position : {0}, {1}"),
    ES("Posición: {0}, {1}"),
    PT("Posição: {0}, {1}"),
    IT("Posizione: {0}, {1}"),
    NL("Positie: {0}, {1}"),
    RU("Положение: {0}, {1}"),
    TR("Konum: {0}, {1}"));

SS_MSG(tip_altitude,
    EN("Altitude: {0} m"),
    JA("高度: {0} m"),
    ZH_HANS("高度：{0} m"),
    ZH_HANT("高度：{0} m"),
    KO("고도: {0} m"),
    DE("Höhe: {0} m"),
    FR("Altitude : {0} m"),
    ES("Altitud: {0} m"),
    PT("Altitude: {0} m"),
    IT("Altitudine: {0} m"),
    NL("Hoogte: {0} m"),
    RU("Высота: {0} м"),
    TR("Yükseklik: {0} m"));

SS_MSG(tip_speed,
    EN("Speed: {0} m/s"),
    JA("速度: {0} m/s"),
    ZH_HANS("速度：{0} m/s"),
    ZH_HANT("速度：{0} m/s"),
    KO("속도: {0} m/s"),
    DE("Geschwindigkeit: {0} m/s"),
    FR("Vitesse : {0} m/s"),
    ES("Velocidad: {0} m/s"),
    PT("Velocidade: {0} m/s"),
    IT("Velocità: {0} m/s"),
    NL("Snelheid: {0} m/s"),
    RU("Скорость: {0} м/с"),
    TR("Hız: {0} m/sn"));

SS_MSG(tip_dop,
    EN("Dilution of precision: {0}"),
    JA("精度低下率: {0}"),
    ZH_HANS("精度因子：{0}"),
    ZH_HANT("精度因子：{0}"),
    KO("정밀도 저하율: {0}"),
    DE("Genauigkeitsminderung: {0}"),
    FR("Dilution de précision : {0}"),
    ES("Dilución de precisión: {0}"),
    PT("Diluição de precisão: {0}"),
    IT("Diluizione della precisione: {0}"),
    NL("Precisieverdunning: {0}"),
    RU("Снижение точности: {0}"),
    TR("Hassasiyet kaybı: {0}"));

SS_MSG(tip_heading,
    EN("Heading: {0}°"),
    JA("方位: {0}°"),
    ZH_HANS("航向：{0}°"),
    ZH_HANT("航向：{0}°"),
    KO("방위: {0}°"),
    DE("Richtung: {0}°"),
    FR("Cap : {0}°"),
    ES("Rumbo: {0}°"),
    PT("Rumo: {0}°"),
    IT("Direzione: {0}°"),
    NL("Koers: {0}°"),
    RU("Курс: {0}°"),
    TR("Yön: {0}°"));

SS_MSG(tip_pitch,
    EN("Pitch: {0}°"),
    JA("ピッチ: {0}°"),
    ZH_HANS("俯仰：{0}°"),
    ZH_HANT("俯仰：{0}°"),
    KO("피치: {0}°"),
    DE("Neigung: {0}°"),
    FR("Tangage : {0}°"),
    ES("Cabeceo: {0}°"),
    PT("Arfagem: {0}°"),
    IT("Beccheggio: {0}°"),
    NL("Stampen: {0}°"),
    RU("Тангаж: {0}°"),
    TR("Yunuslama: {0}°"));

SS_MSG(tip_outlier,
    EN("Dropped as an outlier"),
    JA("外れ値として除外"),
    ZH_HANS("作为离群值丢弃"),
    ZH_HANT("作為離群值捨棄"),
    KO("이상값으로 버림"),
    DE("Als Ausreißer verworfen"),
    FR("Écartée comme aberrante"),
    ES("Descartada como atípica"),
    PT("Descartada como discrepante"),
    IT("Scartata come anomala"),
    NL("Als uitschieter weggelaten"),
    RU("Отброшено как выброс"),
    TR("Aykırı diye atıldı"));

SS_MSG(tip_no_fix,
    EN("Not a fix, by the receiver's own flag"),
    JA("受信機自身のフラグでは測位ではない"),
    ZH_HANS("按接收器自身的标志，并非定位"),
    ZH_HANT("依接收器本身的旗標，並非定位"),
    KO("수신기 자체 플래그로는 측위 아님"),
    DE("Laut Empfänger selbst kein Fix"),
    FR("Pas une position fixée, selon le récepteur lui-même"),
    ES("Sin posición fija, según el propio receptor"),
    PT("Sem posição fixa, segundo o próprio receptor"),
    IT("Nessun fix, secondo il ricevitore stesso"),
    NL("Geen fix, volgens de ontvanger zelf"),
    RU("Не фиксация, по флагу самого приёмника"),
    TR("Alıcının kendi işaretine göre konum kilidi yok"));

SS_MSG(tip_far,
    EN("Far from the other photos"),
    JA("ほかの写真から遠い"),
    ZH_HANS("远离其他照片"),
    ZH_HANT("遠離其他相片"),
    KO("다른 사진과 멂"),
    DE("Weit von den anderen Fotos"),
    FR("Loin des autres photos"),
    ES("Lejos de las demás fotos"),
    PT("Longe das outras fotos"),
    IT("Lontana dalle altre foto"),
    NL("Ver van de andere foto's"),
    RU("Далеко от остальных фотографий"),
    TR("Diğer fotoğraflardan uzak"));

SS_MSG(tip_click_seek,
    EN("Click to show this moment of the video."),
    JA("クリックすると動画のこの時点を表示します。"),
    ZH_HANS("点击显示视频的这一时刻。"),
    ZH_HANT("點按以顯示影片的這一刻。"),
    KO("클릭하면 동영상의 이 순간을 보여 줍니다."),
    DE("Klicken, um diesen Moment des Videos zu zeigen."),
    FR("Cliquez pour afficher ce moment de la vidéo."),
    ES("Haz clic para ver este momento del vídeo."),
    PT("Clique para mostrar este momento do vídeo."),
    IT("Fai clic per mostrare questo momento del video."),
    NL("Klik om dit moment van de video te tonen."),
    RU("Щёлкните, чтобы показать этот момент видео."),
    TR("Videonun bu anını göstermek için tıklayın."));

SS_MSG(tip_click_photo,
    EN("Click to open this photo."),
    JA("クリックするとこの写真を開きます。"),
    ZH_HANS("点击打开这张照片。"),
    ZH_HANT("點按以開啟這張相片。"),
    KO("클릭하면 이 사진을 엽니다."),
    DE("Klicken, um dieses Foto zu öffnen."),
    FR("Cliquez pour ouvrir cette photo."),
    ES("Haz clic para abrir esta foto."),
    PT("Clique para abrir esta foto."),
    IT("Fai clic per aprire questa foto."),
    NL("Klik om deze foto te openen."),
    RU("Щёлкните, чтобы открыть эту фотографию."),
    TR("Bu fotoğrafı açmak için tıklayın."));

SS_MSG(tip_magnitude,
    EN("Magnitude: {0}"),
    JA("大きさ: {0}"),
    ZH_HANS("大小：{0}"),
    ZH_HANT("大小：{0}"),
    KO("크기: {0}"),
    DE("Betrag: {0}"),
    FR("Norme : {0}"),
    ES("Módulo: {0}"),
    PT("Módulo: {0}"),
    IT("Modulo: {0}"),
    NL("Grootte: {0}"),
    RU("Модуль: {0}"),
    TR("Büyüklük: {0}"));

SS_MSG(tip_gap,
    EN("No readings for {0} s"),
    JA("{0} 秒間、読み取り値がありません"),
    ZH_HANS("{0} 秒内没有读数"),
    ZH_HANT("{0} 秒內沒有讀數"),
    KO("{0}초 동안 측정값 없음"),
    DE("Keine Messwerte für {0} s"),
    FR("Aucune mesure pendant {0} s"),
    ES("Sin lecturas durante {0} s"),
    PT("Sem leituras por {0} s"),
    IT("Nessuna lettura per {0} s"),
    NL("Geen metingen gedurende {0} s"),
    RU("Нет показаний {0} с"),
    TR("{0} sn boyunca okuma yok"));

// ===========================================================================
// 3D
// ===========================================================================

SS_MSG(reading_points,
    EN("Reading the points…"),
    JA("点群を読み込んでいます…"),
    ZH_HANS("正在读取点…"),
    ZH_HANT("正在讀取點…"),
    KO("점을 읽는 중…"),
    DE("Punkte werden gelesen…"),
    FR("Lecture des points…"),
    ES("Leyendo los puntos…"),
    PT("Lendo os pontos…"),
    IT("Lettura dei punti…"),
    NL("Punten worden gelezen…"),
    RU("Чтение точек…"),
    TR("Noktalar okunuyor…"));

SS_MSG(reading_points_of,
    EN("Reading the points: {0} of {1}"),
    JA("点群を読み込んでいます: {0} / {1}"),
    ZH_HANS("正在读取点：{0} / {1}"),
    ZH_HANT("正在讀取點：{0} / {1}"),
    KO("점을 읽는 중: {0} / {1}"),
    DE("Punkte werden gelesen: {0} von {1}"),
    FR("Lecture des points : {0} sur {1}"),
    ES("Leyendo los puntos: {0} de {1}"),
    PT("Lendo os pontos: {0} de {1}"),
    IT("Lettura dei punti: {0} su {1}"),
    NL("Punten worden gelezen: {0} van {1}"),
    RU("Чтение точек: {0} из {1}"),
    TR("Noktalar okunuyor: {0} / {1}"));

SS_MSG(reading_model,
    EN("Reading the reconstruction…"),
    JA("再構成を読み込んでいます…"),
    ZH_HANS("正在读取重建…"),
    ZH_HANT("正在讀取重建…"),
    KO("재구성을 읽는 중…"),
    DE("Rekonstruktion wird gelesen…"),
    FR("Lecture de la reconstruction…"),
    ES("Leyendo la reconstrucción…"),
    PT("Lendo a reconstrução…"),
    IT("Lettura della ricostruzione…"),
    NL("Reconstructie wordt gelezen…"),
    RU("Чтение реконструкции…"),
    TR("Yeniden oluşturma okunuyor…"));

SS_MSG(points_shown,
    EN("Points shown: {0} of {1}"),
    JA("表示中の点: {0} / {1}"),
    ZH_HANS("显示的点：{0} / {1}"),
    ZH_HANT("顯示的點：{0} / {1}"),
    KO("표시한 점: {0} / {1}"),
    DE("Gezeigte Punkte: {0} von {1}"),
    FR("Points affichés : {0} sur {1}"),
    ES("Puntos mostrados: {0} de {1}"),
    PT("Pontos mostrados: {0} de {1}"),
    IT("Punti mostrati: {0} su {1}"),
    NL("Getoonde punten: {0} van {1}"),
    RU("Показано точек: {0} из {1}"),
    TR("Gösterilen nokta: {0} / {1}"));

SS_MSG(model_shown,
    EN("Cameras: {0} · Points: {1}"),
    JA("カメラ: {0} · 点: {1}"),
    ZH_HANS("相机：{0} · 点：{1}"),
    ZH_HANT("相機：{0} · 點：{1}"),
    KO("카메라: {0} · 점: {1}"),
    DE("Kameras: {0} · Punkte: {1}"),
    FR("Caméras : {0} · Points : {1}"),
    ES("Cámaras: {0} · Puntos: {1}"),
    PT("Câmeras: {0} · Pontos: {1}"),
    IT("Fotocamere: {0} · Punti: {1}"),
    NL("Camera's: {0} · Punten: {1}"),
    RU("Камер: {0} · Точек: {1}"),
    TR("Kamera: {0} · Nokta: {1}"));

// ===========================================================================
// Details
// ===========================================================================

SS_MSG(d_path,
    EN("Path"),
    JA("パス"),
    ZH_HANS("路径"),
    ZH_HANT("路徑"),
    KO("경로"),
    DE("Pfad"),
    FR("Chemin"),
    ES("Ruta"),
    PT("Caminho"),
    IT("Percorso"),
    NL("Pad"),
    RU("Путь"),
    TR("Yol"));

SS_MSG(d_duration,
    EN("Length"),
    JA("長さ"),
    ZH_HANS("时长"),
    ZH_HANT("長度"),
    KO("길이"),
    DE("Länge"),
    FR("Durée"),
    ES("Duración"),
    PT("Duração"),
    IT("Durata"),
    NL("Duur"),
    RU("Длительность"),
    TR("Süre"));

SS_MSG(d_frame_rate,
    EN("Frame rate"),
    JA("フレームレート"),
    ZH_HANS("帧率"),
    ZH_HANT("影格率"),
    KO("프레임 속도"),
    DE("Bildrate"),
    FR("Cadence"),
    ES("Fotogramas por segundo"),
    PT("Taxa de quadros"),
    IT("Frequenza dei fotogrammi"),
    NL("Beeldsnelheid"),
    RU("Частота кадров"),
    TR("Kare hızı"));

SS_MSG(d_frames,
    EN("Frames"),
    JA("フレーム数"),
    ZH_HANS("帧数"),
    ZH_HANT("影格數"),
    KO("프레임 수"),
    DE("Bilder"),
    FR("Images"),
    ES("Fotogramas"),
    PT("Quadros"),
    IT("Fotogrammi"),
    NL("Beelden"),
    RU("Кадров"),
    TR("Kare"));

SS_MSG(d_streams,
    EN("Video streams"),
    JA("動画ストリーム"),
    ZH_HANS("视频流"),
    ZH_HANT("視訊串流"),
    KO("동영상 스트림"),
    DE("Videospuren"),
    FR("Flux vidéo"),
    ES("Pistas de vídeo"),
    PT("Faixas de vídeo"),
    IT("Flussi video"),
    NL("Videosporen"),
    RU("Видеопотоки"),
    TR("Video akışları"));

SS_MSG(d_codec,
    EN("Codec"),
    JA("コーデック"),
    ZH_HANS("编码"),
    ZH_HANT("編碼"),
    KO("코덱"),
    DE("Codec"),
    FR("Codec"),
    ES("Códec"),
    PT("Codec"),
    IT("Codec"),
    NL("Codec"),
    RU("Кодек"),
    TR("Kodek"));

SS_MSG(d_pano,
    EN("360° layout"),
    JA("360° の配置"),
    ZH_HANS("360° 布局"),
    ZH_HANT("360° 配置"),
    KO("360° 배치"),
    DE("360°-Anordnung"),
    FR("Disposition 360°"),
    ES("Disposición 360°"),
    PT("Disposição 360°"),
    IT("Disposizione 360°"),
    NL("360°-indeling"),
    RU("Раскладка 360°"),
    TR("360° yerleşimi"));

SS_MSG(d_lut,
    EN("Colour LUT"),
    JA("カラー LUT"),
    ZH_HANS("色彩 LUT"),
    ZH_HANT("色彩 LUT"),
    KO("색상 LUT"),
    DE("Farb-LUT"),
    FR("LUT de couleur"),
    ES("LUT de color"),
    PT("LUT de cor"),
    IT("LUT di colore"),
    NL("Kleur-LUT"),
    RU("Цветовая LUT"),
    TR("Renk LUT'u"));

SS_MSG(d_telemetry,
    EN("Sensor track"),
    JA("センサートラック"),
    ZH_HANS("传感器轨道"),
    ZH_HANT("感測器軌道"),
    KO("센서 트랙"),
    DE("Sensorspur"),
    FR("Piste de capteurs"),
    ES("Pista de sensores"),
    PT("Faixa de sensores"),
    IT("Traccia dei sensori"),
    NL("Sensorspoor"),
    RU("Дорожка датчиков"),
    TR("Algılayıcı izi"));

SS_MSG(d_camera,
    EN("Camera"),
    JA("カメラ"),
    ZH_HANS("相机"),
    ZH_HANT("相機"),
    KO("카메라"),
    DE("Kamera"),
    FR("Caméra"),
    ES("Cámara"),
    PT("Câmera"),
    IT("Fotocamera"),
    NL("Camera"),
    RU("Камера"),
    TR("Kamera"));

SS_MSG(d_gps_fixes,
    EN("GPS fixes"),
    JA("GPS 測位数"),
    ZH_HANS("GPS 定位数"),
    ZH_HANT("GPS 定位數"),
    KO("GPS 위치 수"),
    DE("GPS-Fixes"),
    FR("Positions GPS"),
    ES("Posiciones GPS"),
    PT("Posições de GPS"),
    IT("Posizioni GPS"),
    NL("Gps-fixes"),
    RU("Фиксаций GPS"),
    TR("GPS konumu"));

SS_MSG(d_gps_rate,
    EN("GPS rate"),
    JA("GPS の頻度"),
    ZH_HANS("GPS 频率"),
    ZH_HANT("GPS 頻率"),
    KO("GPS 빈도"),
    DE("GPS-Rate"),
    FR("Fréquence GPS"),
    ES("Frecuencia del GPS"),
    PT("Frequência do GPS"),
    IT("Frequenza del GPS"),
    NL("Gps-frequentie"),
    RU("Частота GPS"),
    TR("GPS sıklığı"));

SS_MSG(d_gps_path,
    EN("GPS path length"),
    JA("GPS の経路長"),
    ZH_HANS("GPS 路径长度"),
    ZH_HANT("GPS 路徑長度"),
    KO("GPS 경로 길이"),
    DE("GPS-Weglänge"),
    FR("Longueur du trajet GPS"),
    ES("Longitud del recorrido GPS"),
    PT("Comprimento do trajeto de GPS"),
    IT("Lunghezza del percorso GPS"),
    NL("Lengte van het gps-spoor"),
    RU("Длина пути GPS"),
    TR("GPS yol uzunluğu"));

SS_MSG(d_gps_spread,
    EN("GPS spread"),
    JA("GPS の広がり"),
    ZH_HANS("GPS 分布范围"),
    ZH_HANT("GPS 分布範圍"),
    KO("GPS 퍼짐"),
    DE("GPS-Streuung"),
    FR("Étalement GPS"),
    ES("Dispersión del GPS"),
    PT("Dispersão do GPS"),
    IT("Dispersione del GPS"),
    NL("Gps-spreiding"),
    RU("Разброс GPS"),
    TR("GPS yayılımı"));

SS_MSG(d_imu_rate,
    EN("IMU rate"),
    JA("IMU の頻度"),
    ZH_HANS("IMU 频率"),
    ZH_HANT("IMU 頻率"),
    KO("IMU 빈도"),
    DE("IMU-Rate"),
    FR("Fréquence de l'IMU"),
    ES("Frecuencia de la IMU"),
    PT("Frequência da IMU"),
    IT("Frequenza dell'IMU"),
    NL("IMU-frequentie"),
    RU("Частота IMU"),
    TR("IMU sıklığı"));

SS_MSG(d_photos,
    EN("Photos"),
    JA("写真"),
    ZH_HANS("照片"),
    ZH_HANT("相片"),
    KO("사진"),
    DE("Fotos"),
    FR("Photos"),
    ES("Fotos"),
    PT("Fotos"),
    IT("Foto"),
    NL("Foto's"),
    RU("Фотографий"),
    TR("Fotoğraf"));

SS_MSG(d_formats,
    EN("File types"),
    JA("ファイルの種類"),
    ZH_HANS("文件类型"),
    ZH_HANT("檔案類型"),
    KO("파일 형식"),
    DE("Dateitypen"),
    FR("Types de fichier"),
    ES("Tipos de archivo"),
    PT("Tipos de arquivo"),
    IT("Tipi di file"),
    NL("Bestandstypen"),
    RU("Типы файлов"),
    TR("Dosya türleri"));

SS_MSG(d_sizes,
    EN("Image sizes"),
    JA("画像サイズ"),
    ZH_HANS("图像尺寸"),
    ZH_HANT("影像尺寸"),
    KO("이미지 크기"),
    DE("Bildgrößen"),
    FR("Tailles d'image"),
    ES("Tamaños de imagen"),
    PT("Tamanhos de imagem"),
    IT("Dimensioni delle immagini"),
    NL("Beeldformaten"),
    RU("Размеры изображений"),
    TR("Görüntü boyutları"));

SS_MSG(d_masks,
    EN("Masks"),
    JA("マスク"),
    ZH_HANS("蒙版"),
    ZH_HANT("遮罩"),
    KO("마스크"),
    DE("Masken"),
    FR("Masques"),
    ES("Máscaras"),
    PT("Máscaras"),
    IT("Maschere"),
    NL("Maskers"),
    RU("Маски"),
    TR("Maskeler"));

SS_MSG(d_gps_photos,
    EN("Photos with GPS"),
    JA("GPS のある写真"),
    ZH_HANS("有 GPS 的照片"),
    ZH_HANT("有 GPS 的相片"),
    KO("GPS가 있는 사진"),
    DE("Fotos mit GPS"),
    FR("Photos avec GPS"),
    ES("Fotos con GPS"),
    PT("Fotos com GPS"),
    IT("Foto con GPS"),
    NL("Foto's met gps"),
    RU("Фотографий с GPS"),
    TR("GPS'li fotoğraf"));

SS_MSG(d_attitude_photos,
    EN("Photos with attitude"),
    JA("姿勢のある写真"),
    ZH_HANS("有姿态的照片"),
    ZH_HANT("有姿態的相片"),
    KO("자세가 있는 사진"),
    DE("Fotos mit Lage"),
    FR("Photos avec attitude"),
    ES("Fotos con actitud"),
    PT("Fotos com atitude"),
    IT("Foto con assetto"),
    NL("Foto's met stand"),
    RU("Фотографий с ориентацией"),
    TR("Duruşlu fotoğraf"));

SS_MSG(d_dataset,
    EN("Dataset"),
    JA("データセット"),
    ZH_HANS("数据集"),
    ZH_HANT("資料集"),
    KO("데이터셋"),
    DE("Datensatz"),
    FR("Jeu de données"),
    ES("Conjunto de datos"),
    PT("Conjunto de dados"),
    IT("Set di dati"),
    NL("Dataset"),
    RU("Набор данных"),
    TR("Veri kümesi"));

SS_MSG(d_model_cameras,
    EN("Cameras in the model"),
    JA("モデルのカメラ"),
    ZH_HANS("模型中的相机"),
    ZH_HANT("模型中的相機"),
    KO("모델의 카메라"),
    DE("Kameras im Modell"),
    FR("Caméras du modèle"),
    ES("Cámaras del modelo"),
    PT("Câmeras no modelo"),
    IT("Fotocamere nel modello"),
    NL("Camera's in het model"),
    RU("Камер в модели"),
    TR("Modeldeki kamera"));

SS_MSG(d_model_points,
    EN("Points in the model"),
    JA("モデルの点"),
    ZH_HANS("模型中的点"),
    ZH_HANT("模型中的點"),
    KO("모델의 점"),
    DE("Punkte im Modell"),
    FR("Points du modèle"),
    ES("Puntos del modelo"),
    PT("Pontos no modelo"),
    IT("Punti nel modello"),
    NL("Punten in het model"),
    RU("Точек в модели"),
    TR("Modeldeki nokta"));

SS_MSG(d_model_error,
    EN("Model"),
    JA("モデル"),
    ZH_HANS("模型"),
    ZH_HANT("模型"),
    KO("모델"),
    DE("Modell"),
    FR("Modèle"),
    ES("Modelo"),
    PT("Modelo"),
    IT("Modello"),
    NL("Model"),
    RU("Модель"),
    TR("Model"));

SS_MSG(d_error,
    EN("Error"),
    JA("エラー"),
    ZH_HANS("错误"),
    ZH_HANT("錯誤"),
    KO("오류"),
    DE("Fehler"),
    FR("Erreur"),
    ES("Error"),
    PT("Erro"),
    IT("Errore"),
    NL("Fout"),
    RU("Ошибка"),
    TR("Hata"));

SS_MSG(d_format,
    EN("Format"),
    JA("形式"),
    ZH_HANS("格式"),
    ZH_HANT("格式"),
    KO("형식"),
    DE("Format"),
    FR("Format"),
    ES("Formato"),
    PT("Formato"),
    IT("Formato"),
    NL("Formaat"),
    RU("Формат"),
    TR("Biçim"));

SS_MSG(d_points,
    EN("Points"),
    JA("点"),
    ZH_HANS("点"),
    ZH_HANT("點"),
    KO("점"),
    DE("Punkte"),
    FR("Points"),
    ES("Puntos"),
    PT("Pontos"),
    IT("Punti"),
    NL("Punten"),
    RU("Точек"),
    TR("Nokta"));

SS_MSG(d_colour,
    EN("Colour"),
    JA("色"),
    ZH_HANS("颜色"),
    ZH_HANT("色彩"),
    KO("색"),
    DE("Farbe"),
    FR("Couleur"),
    ES("Color"),
    PT("Cor"),
    IT("Colore"),
    NL("Kleur"),
    RU("Цвет"),
    TR("Renk"));

SS_MSG(d_yes,
    EN("Yes"),
    JA("あり"),
    ZH_HANS("有"),
    ZH_HANT("有"),
    KO("있음"),
    DE("Ja"),
    FR("Oui"),
    ES("Sí"),
    PT("Sim"),
    IT("Sì"),
    NL("Ja"),
    RU("Да"),
    TR("Var"));

SS_MSG(d_no,
    EN("No"),
    JA("なし"),
    ZH_HANS("无"),
    ZH_HANT("無"),
    KO("없음"),
    DE("Nein"),
    FR("Non"),
    ES("No"),
    PT("Não"),
    IT("No"),
    NL("Nee"),
    RU("Нет"),
    TR("Yok"));

SS_MSG(d_stations,
    EN("Scan positions"),
    JA("スキャン位置"),
    ZH_HANS("扫描站点"),
    ZH_HANT("掃描站點"),
    KO("스캔 위치"),
    DE("Scanstandorte"),
    FR("Stations de scan"),
    ES("Estaciones de escaneo"),
    PT("Estações de varredura"),
    IT("Stazioni di scansione"),
    NL("Scanposities"),
    RU("Точек сканирования"),
    TR("Tarama konumu"));

SS_MSG(d_images,
    EN("Images"),
    JA("画像"),
    ZH_HANS("图像"),
    ZH_HANT("影像"),
    KO("이미지"),
    DE("Bilder"),
    FR("Images"),
    ES("Imágenes"),
    PT("Imagens"),
    IT("Immagini"),
    NL("Beelden"),
    RU("Изображений"),
    TR("Görüntü"));

SS_MSG(d_images_value,
    EN("{0}, with a pose: {1}"),
    JA("{0}、うち位置姿勢つき: {1}"),
    ZH_HANS("{0}，带位姿：{1}"),
    ZH_HANT("{0}，帶位姿：{1}"),
    KO("{0}, 위치와 자세 있음: {1}"),
    DE("{0}, mit Pose: {1}"),
    FR("{0}, avec une pose : {1}"),
    ES("{0}, con pose: {1}"),
    PT("{0}, com pose: {1}"),
    IT("{0}, con posa: {1}"),
    NL("{0}, met pose: {1}"),
    RU("{0}, с позой: {1}"),
    TR("{0}, pozlu: {1}"));

SS_MSG(d_image_kinds,
    EN("Image kinds"),
    JA("画像の種類"),
    ZH_HANS("图像类型"),
    ZH_HANT("影像類型"),
    KO("이미지 종류"),
    DE("Bildarten"),
    FR("Types d'image"),
    ES("Tipos de imagen"),
    PT("Tipos de imagem"),
    IT("Tipi di immagine"),
    NL("Soorten beelden"),
    RU("Виды изображений"),
    TR("Görüntü türleri"));

SS_MSG(d_image_kinds_value,
    EN("Pinhole: {0}, spherical: {1}"),
    JA("ピンホール: {0}、全天球: {1}"),
    ZH_HANS("针孔：{0}，球面：{1}"),
    ZH_HANT("針孔：{0}，球面：{1}"),
    KO("핀홀: {0}, 구면: {1}"),
    DE("Lochkamera: {0}, sphärisch: {1}"),
    FR("Sténopé : {0}, sphériques : {1}"),
    ES("Estenopeicas: {0}, esféricas: {1}"),
    PT("Pinhole: {0}, esféricas: {1}"),
    IT("Stenopeiche: {0}, sferiche: {1}"),
    NL("Pinhole: {0}, bolvormig: {1}"),
    RU("Обскура: {0}, сферические: {1}"),
    TR("İğne deliği: {0}, küresel: {1}"));

// ===========================================================================
// The input rows
// ===========================================================================

SS_MSG(row_clip,
    EN("Clip {0} – {1}"),
    JA("区間 {0} – {1}"),
    ZH_HANS("片段 {0} – {1}"),
    ZH_HANT("片段 {0} – {1}"),
    KO("구간 {0} – {1}"),
    DE("Ausschnitt {0} – {1}"),
    FR("Extrait {0} – {1}"),
    ES("Fragmento {0} – {1}"),
    PT("Trecho {0} – {1}"),
    IT("Spezzone {0} – {1}"),
    NL("Fragment {0} – {1}"),
    RU("Фрагмент {0} – {1}"),
    TR("Kesit {0} – {1}"));

SS_MSG(row_clip_end,
    EN("end"),
    JA("最後"),
    ZH_HANS("结尾"),
    ZH_HANT("結尾"),
    KO("끝"),
    DE("Ende"),
    FR("fin"),
    ES("final"),
    PT("fim"),
    IT("fine"),
    NL("einde"),
    RU("конец"),
    TR("son"));

SS_MSG(row_clip_help,
    EN("Only this part of the video is used. Change it on the preview's "
       "timeline."),
    JA("動画のこの区間だけを使います。プレビューのタイムラインで変更できます。"),
    ZH_HANS("只使用视频的这一段。可在预览的时间轴上更改。"),
    ZH_HANT("只使用影片的這一段。可在預覽的時間軸上更改。"),
    KO("동영상의 이 구간만 씁니다. 미리보기의 타임라인에서 바꿀 수 있습니다."),
    DE("Nur dieser Teil des Videos wird verwendet. Er lässt sich auf der "
       "Zeitleiste der Vorschau ändern."),
    FR("Seule cette partie de la vidéo est utilisée. Modifiez-la sur la frise "
       "de l'aperçu."),
    ES("Solo se usa esta parte del vídeo. Cámbiala en la línea de tiempo de la "
       "vista previa."),
    PT("Só esta parte do vídeo é usada. Altere-a na linha do tempo da prévia."),
    IT("Si usa solo questa parte del video. Si cambia sulla linea temporale "
       "dell'anteprima."),
    NL("Alleen dit deel van de video wordt gebruikt. Wijzig het op de tijdlijn "
       "van het voorbeeld."),
    RU("Используется только эта часть видео. Изменить её можно на шкале времени "
       "в предпросмотре."),
    TR("Videonun yalnızca bu bölümü kullanılır. Önizlemenin zaman çizelgesinde "
       "değiştirin."));

SS_MSG(log_clicks_dropped_clip,
    EN("Mask clicks on {0} were dropped: its clip changed, so the frames they "
       "were drawn on may no longer be extracted."),
    JA("{0} のマスク用クリックを破棄しました。区間が変わったため、クリックしたフ"
       "レームが抽出されなくなる可能性があります。"),
    ZH_HANS("{0} 上的蒙版点击已被丢弃：它的片段变了，点击所在的帧可能不再被抽取。"),
    ZH_HANT("{0} 上的遮罩點按已捨棄：它的片段變了，點按所在的影格可能不再被擷取。"),
    KO("{0}의 마스크 클릭을 버렸습니다. 구간이 바뀌어 클릭한 프레임이 더 이상 추"
       "출되지 않을 수 있습니다."),
    DE("Maskenklicks auf {0} wurden verworfen: Sein Ausschnitt hat sich "
       "geändert, sodass die Bilder, auf denen sie gesetzt wurden, womöglich "
       "nicht mehr entnommen werden."),
    FR("Les clics de masque sur {0} ont été abandonnés : son extrait a changé, "
       "et les images sur lesquelles ils ont été posés pourraient ne plus être "
       "extraites."),
    ES("Se descartaron los clics de máscara sobre {0}: su fragmento cambió, así "
       "que puede que ya no se extraigan los fotogramas en los que se hicieron."),
    PT("Os cliques de máscara em {0} foram descartados: o trecho mudou, então "
       "os quadros em que foram feitos podem não ser mais extraídos."),
    IT("I clic per la maschera su {0} sono stati scartati: lo spezzone è "
       "cambiato, quindi i fotogrammi su cui erano stati fatti potrebbero non "
       "venire più estratti."),
    NL("Maskerklikken op {0} zijn weggelaten: het fragment is veranderd, dus de "
       "beelden waarop ze zijn gezet worden misschien niet meer uitgenomen."),
    RU("Щелчки маски на {0} отброшены: его фрагмент изменился, и кадры, на "
       "которых они сделаны, могут больше не извлекаться."),
    TR("{0} üzerindeki maske tıklamaları atıldı: kesiti değişti, bu yüzden "
       "üzerlerinde tıklandıkları kareler artık çıkarılmayabilir."));

SS_MSG(plan_active_lens,
    EN("Lens extracted"),
    JA("取り出すレンズ"),
    ZH_HANS("提取的镜头"),
    ZH_HANT("提取的鏡頭"),
    KO("추출할 렌즈"),
    DE("Extrahiertes Objektiv"),
    FR("Objectif extrait"),
    ES("Objetivo extraído"),
    PT("Lente extraída"),
    IT("Obiettivo estratto"),
    NL("Geëxtraheerde lens"),
    RU("Извлекаемый объектив"),
    TR("Çıkarılan mercek"));

SS_MSG(plan_clip,
    EN("Clip"),
    JA("区間"),
    ZH_HANS("片段"),
    ZH_HANT("片段"),
    KO("구간"),
    DE("Ausschnitt"),
    FR("Extrait"),
    ES("Fragmento"),
    PT("Trecho"),
    IT("Spezzone"),
    NL("Fragment"),
    RU("Фрагмент"),
    TR("Kesit"));

}  // namespace preview
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
