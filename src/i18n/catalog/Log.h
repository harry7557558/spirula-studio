#pragma once

// What the progress line and the log panel say while a job runs.
//
// Two kinds of text end up in that panel and only one of them is here:
//
//   OURS -- the stage names, the notes, the "here is what I did with your
//   folder" lines. They are written for the person watching, so they are
//   translated, and the stage names doubly so: the same string is the caption
//   above the progress bar.
//
//   THE CHILD PROCESS'S -- every line COLMAP or ffmpeg prints. Passed through
//   verbatim. They are English, they are what a bug report is pasted from, and
//   they are not ours to rewrite.
//
// So a log in Japanese is Japanese around English, which is honest: the
// English parts are the ones that came from somewhere else.
//
// `spirula sfm` is the exception that proves the rule -- it is a child process
// but it is OURS, so it translates itself, out of i18n/catalog/Sfm.h and
// through src/sfm/core/Log.h.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace log {

// ===========================================================================
// Stages -- also the caption above the progress bar
// ===========================================================================

SS_MSG(stage_collecting_photos,
    EN("Collecting photos"),
    JA("写真を集めています"),
    ZH_HANS("正在收集照片"),
    ZH_HANT("正在收集照片"),
    KO("사진을 모으는 중"),
    DE("Fotos werden zusammengetragen"),
    FR("Collecte des photos"),
    ES("Recopilando las fotos"),
    PT("Reunindo as fotos"),
    IT("Raccolta delle foto"),
    NL("Foto's verzamelen"),
    RU("Сбор фотографий"),
    TR("Fotoğraflar toplanıyor"));

SS_MSG(stage_extract_gpu,
    EN("Extracting frames (GPU decode)"),
    JA("フレームを取り出しています（GPUデコード）"),
    ZH_HANS("正在提取帧（GPU 解码）"),
    ZH_HANT("正在擷取影格（GPU 解碼）"),
    KO("프레임을 뽑는 중(GPU 디코딩)"),
    DE("Einzelbilder werden entnommen (GPU-Dekodierung)"),
    FR("Extraction des images (décodage GPU)"),
    ES("Extrayendo fotogramas (decodificación por GPU)"),
    PT("Extraindo quadros (decodificação na GPU)"),
    IT("Estrazione dei fotogrammi (decodifica GPU)"),
    NL("Beelden uitpakken (GPU-decodering)"),
    RU("Извлечение кадров (декодирование на GPU)"),
    TR("Kareler çıkarılıyor (GPU çözümü)"));

SS_MSG(stage_extract_mask_gpu,
    EN("Extracting frames and masking (GPU)"),
    JA("フレームの取り出しとマスク作成をしています（GPU）"),
    ZH_HANS("正在提取帧并生成蒙版（GPU）"),
    ZH_HANT("正在擷取影格並產生遮罩（GPU）"),
    KO("프레임을 뽑고 마스크를 만드는 중(GPU)"),
    DE("Einzelbilder entnehmen und maskieren (GPU)"),
    FR("Extraction des images et masquage (GPU)"),
    ES("Extrayendo fotogramas y enmascarando (GPU)"),
    PT("Extraindo quadros e mascarando (GPU)"),
    IT("Estrazione dei fotogrammi e mascheratura (GPU)"),
    NL("Beelden uitpakken en maskeren (GPU)"),
    RU("Извлечение кадров и маскирование (GPU)"),
    TR("Kareler çıkarılıyor ve maskeleniyor (GPU)"));

SS_MSG(stage_extract_candidates,
    EN("Extracting candidate frames (ffmpeg)"),
    JA("候補となるフレームを取り出しています（ffmpeg）"),
    ZH_HANS("正在提取候选帧（ffmpeg）"),
    ZH_HANT("正在擷取候選影格（ffmpeg）"),
    KO("후보 프레임을 뽑는 중(ffmpeg)"),
    DE("Kandidatenbilder werden entnommen (ffmpeg)"),
    FR("Extraction des images candidates (ffmpeg)"),
    ES("Extrayendo fotogramas candidatos (ffmpeg)"),
    PT("Extraindo quadros candidatos (ffmpeg)"),
    IT("Estrazione dei fotogrammi candidati (ffmpeg)"),
    NL("Kandidaatbeelden uitpakken (ffmpeg)"),
    RU("Извлечение кадров-кандидатов (ffmpeg)"),
    TR("Aday kareler çıkarılıyor (ffmpeg)"));

SS_MSG(stage_extract_ffmpeg,
    EN("Extracting frames (ffmpeg)"),
    JA("フレームを取り出しています（ffmpeg）"),
    ZH_HANS("正在提取帧（ffmpeg）"),
    ZH_HANT("正在擷取影格（ffmpeg）"),
    KO("프레임을 뽑는 중(ffmpeg)"),
    DE("Einzelbilder werden entnommen (ffmpeg)"),
    FR("Extraction des images (ffmpeg)"),
    ES("Extrayendo fotogramas (ffmpeg)"),
    PT("Extraindo quadros (ffmpeg)"),
    IT("Estrazione dei fotogrammi (ffmpeg)"),
    NL("Beelden uitpakken (ffmpeg)"),
    RU("Извлечение кадров (ffmpeg)"),
    TR("Kareler çıkarılıyor (ffmpeg)"));

// {0} is the track number inside a multi-track file (an Insta360 .insv holds
// one per lens).
SS_MSG(stage_split_track,
    EN("Splitting video track {0}"),
    JA("動画のトラック {0} を分けています"),
    ZH_HANS("正在拆分视频轨道 {0}"),
    ZH_HANT("正在拆分影片軌道 {0}"),
    KO("영상 트랙 {0} 분리 중"),
    DE("Videospur {0} wird aufgeteilt"),
    FR("Séparation de la piste vidéo {0}"),
    ES("Separando la pista de vídeo {0}"),
    PT("Separando a trilha de vídeo {0}"),
    IT("Separazione della traccia video {0}"),
    NL("Videospoor {0} splitsen"),
    RU("Разделение видеодорожки {0}"),
    TR("{0} numaralı video izi ayrılıyor"));

SS_MSG(stage_select_sharpest,
    EN("Selecting sharpest frames (multithreaded)"),
    JA("いちばん鮮明なフレームを選んでいます（マルチスレッド）"),
    ZH_HANS("正在挑选最清晰的帧（多线程）"),
    ZH_HANT("正在挑選最清晰的影格（多執行緒）"),
    KO("가장 선명한 프레임을 고르는 중(멀티스레드)"),
    DE("Schärfste Einzelbilder werden ausgewählt (mehrere Threads)"),
    FR("Sélection des images les plus nettes (multithread)"),
    ES("Seleccionando los fotogramas más nítidos (multihilo)"),
    PT("Selecionando os quadros mais nítidos (multithread)"),
    IT("Selezione dei fotogrammi più nitidi (multithread)"),
    NL("Scherpste beelden kiezen (meerdere threads)"),
    RU("Выбор самых резких кадров (в несколько потоков)"),
    TR("En net kareler seçiliyor (çok iş parçacıklı)"));

SS_MSG(stage_masks_builtin,
    EN("Generating masks (segmentation)"),
    JA("マスクを作成しています（セグメンテーション）"),
    ZH_HANS("正在生成蒙版（分割）"),
    ZH_HANT("正在產生遮罩（分割）"),
    KO("마스크를 만드는 중(분할)"),
    DE("Masken werden erzeugt (Segmentierung)"),
    FR("Génération des masques (segmentation)"),
    ES("Generando las máscaras (segmentación)"),
    PT("Gerando as máscaras (segmentação)"),
    IT("Generazione delle maschere (segmentazione)"),
    NL("Maskers maken (segmentatie)"),
    RU("Создание масок (сегментация)"),
    TR("Maskeler oluşturuluyor (bölütleme)"));

SS_MSG(stage_frame_mask,
    EN("Masking the fixed areas of the frame"),
    JA("画面の決まった位置を隠しています"),
    ZH_HANS("正在遮住画面中固定的区域"),
    ZH_HANT("正在遮住畫面中固定的區域"),
    KO("화면에서 늘 같은 자리를 가리는 중"),
    DE("Die festen Bereiche des Bildes werden maskiert"),
    FR("Masquage des zones fixes de l'image"),
    ES("Enmascarando las zonas fijas del fotograma"),
    PT("Mascarando as áreas fixas do quadro"),
    IT("Mascheratura delle zone fisse del fotogramma"),
    NL("De vaste gebieden van het beeld maskeren"),
    RU("Маскирование постоянных участков кадра"),
    TR("Karenin sabit alanları maskeleniyor"));

SS_MSG(frame_mask_border,
    EN("{0}: border found -- {1}% of the frame is kept"),
    JA("{0}: 枠が見つかりました -- 画面の {1}% を残します"),
    ZH_HANS("{0}：找到边框 —— 保留画面的 {1}%"),
    ZH_HANT("{0}：找到邊框 —— 保留畫面的 {1}%"),
    KO("{0}: 테두리를 찾았습니다 -- 화면의 {1}% 를 남깁니다"),
    DE("{0}: Rand gefunden -- {1} % des Bildes bleiben"),
    FR("{0} : bord trouvé -- {1} % de l'image est conservé"),
    ES("{0}: borde encontrado; se conserva el {1} % del fotograma"),
    PT("{0}: borda encontrada -- {1}% do quadro fica"),
    IT("{0}: bordo trovato -- resta il {1}% del fotogramma"),
    NL("{0}: rand gevonden -- {1}% van het beeld blijft"),
    RU("{0}: край найден -- остаётся {1} % кадра"),
    TR("{0}: kenar bulundu -- karenin %{1} kadarı kalıyor"));

SS_MSG(frame_mask_no_border,
    EN("{0}: no border found, so none was cut"),
    JA("{0}: 枠が見つからなかったので、何も切り取っていません"),
    ZH_HANS("{0}：没找到边框，因此没有裁掉任何东西"),
    ZH_HANT("{0}：沒找到邊框，因此沒有裁掉任何東西"),
    KO("{0}: 테두리를 찾지 못해 아무것도 잘라내지 않았습니다"),
    DE("{0}: kein Rand gefunden, also wurde keiner weggenommen"),
    FR("{0} : aucun bord trouvé, donc rien n'a été retiré"),
    ES("{0}: no se encontró borde, así que no se quitó ninguno"),
    PT("{0}: nenhuma borda encontrada, então nada foi tirado"),
    IT("{0}: nessun bordo trovato, quindi non ne è stato tolto alcuno"),
    NL("{0}: geen rand gevonden, dus er is er geen weggehaald"),
    RU("{0}: край не найден, поэтому ничего не срезано"),
    TR("{0}: kenar bulunamadı, bu yüzden hiçbiri kesilmedi"));

SS_MSG(masks_combined_in_place,
    EN("The masks in {0} are replaced by their combination with the frame mask"),
    JA("{0} のマスクは、フレームマスクと合成したもので置き換わります"),
    ZH_HANS("{0} 里的蒙版将被它与画面蒙版合并后的结果替换"),
    ZH_HANT("{0} 裡的遮罩將被它與畫面遮罩合併後的結果取代"),
    KO("{0} 의 마스크는 프레임 마스크와 합친 결과로 바뀝니다"),
    DE("Die Masken in {0} werden durch ihre Verbindung mit der Bildmaske ersetzt"),
    FR("Les masques de {0} sont remplacés par leur combinaison avec le masque d'image"),
    ES("Las máscaras de {0} se sustituyen por su combinación con la máscara de imagen"),
    PT("As máscaras em {0} são substituídas pela combinação com a máscara de imagem"),
    IT("Le maschere in {0} sono sostituite dalla loro unione con la maschera di inquadratura"),
    NL("De maskers in {0} worden vervangen door hun combinatie met het beeldmasker"),
    RU("Маски в {0} заменяются их сочетанием с маской кадра"),
    TR("{0} içindeki maskeler, kare maskesiyle birleştirilmiş hâlleriyle değiştirilir"));

SS_MSG(stage_masks_python,
    EN("Generating masks (external Python)"),
    JA("マスクを作成しています（外部のPython）"),
    ZH_HANS("正在生成蒙版（外部 Python）"),
    ZH_HANT("正在產生遮罩（外部 Python）"),
    KO("마스크를 만드는 중(외부 Python)"),
    DE("Masken werden erzeugt (externes Python)"),
    FR("Génération des masques (Python externe)"),
    ES("Generando las máscaras (Python externo)"),
    PT("Gerando as máscaras (Python externo)"),
    IT("Generazione delle maschere (Python esterno)"),
    NL("Maskers maken (extern Python)"),
    RU("Создание масок (внешний Python)"),
    TR("Maskeler oluşturuluyor (harici Python)"));

SS_MSG(stage_finding_features,
    EN("Finding features"),
    JA("特徴点を探しています"),
    ZH_HANS("正在寻找特征点"),
    ZH_HANT("正在尋找特徵點"),
    KO("특징점을 찾는 중"),
    DE("Merkmale werden gesucht"),
    FR("Recherche des points caractéristiques"),
    ES("Buscando puntos característicos"),
    PT("Procurando pontos característicos"),
    IT("Ricerca dei punti caratteristici"),
    NL("Kenmerken zoeken"),
    RU("Поиск особых точек"),
    TR("Öznitelikler aranıyor"));

SS_MSG(stage_matching_images,
    EN("Matching images"),
    JA("画像どうしを照合しています"),
    ZH_HANS("正在匹配图像"),
    ZH_HANT("正在比對影像"),
    KO("이미지끼리 맞춰 보는 중"),
    DE("Bilder werden einander zugeordnet"),
    FR("Mise en correspondance des images"),
    ES("Emparejando las imágenes"),
    PT("Correspondendo as imagens"),
    IT("Corrispondenza tra le immagini"),
    NL("Beelden aan elkaar koppelen"),
    RU("Сопоставление снимков"),
    TR("Görüntüler eşleştiriliyor"));

SS_MSG(stage_reconstructing,
    EN("Reconstructing cameras (the slow part)"),
    JA("カメラ位置を復元しています（時間のかかる工程）"),
    ZH_HANS("正在恢复相机位姿（最慢的一步）"),
    ZH_HANT("正在還原相機位姿（最慢的一步）"),
    KO("카메라 위치를 복원하는 중(가장 오래 걸리는 단계)"),
    DE("Kamerastandpunkte werden rekonstruiert (der langsame Teil)"),
    FR("Reconstruction des caméras (l'étape lente)"),
    ES("Reconstruyendo las cámaras (la parte lenta)"),
    PT("Reconstruindo as câmeras (a parte lenta)"),
    IT("Ricostruzione delle fotocamere (la parte lenta)"),
    NL("Camerastandpunten reconstrueren (het trage deel)"),
    RU("Восстановление положений камер (самый долгий этап)"),
    TR("Kamera konumları yeniden kuruluyor (yavaş kısım)"));

SS_MSG(stage_reconstructing_features,
    EN("Reconstructing (finding features)"),
    JA("復元しています（特徴点の抽出）"),
    ZH_HANS("正在重建（寻找特征点）"),
    ZH_HANT("正在重建（尋找特徵點）"),
    KO("복원하는 중(특징점 찾기)"),
    DE("Rekonstruktion (Merkmalssuche)"),
    FR("Reconstruction (recherche des points caractéristiques)"),
    ES("Reconstruyendo (buscando puntos característicos)"),
    PT("Reconstruindo (procurando pontos característicos)"),
    IT("Ricostruzione (ricerca dei punti caratteristici)"),
    NL("Reconstructie (kenmerken zoeken)"),
    RU("Восстановление (поиск особых точек)"),
    TR("Yeniden kuruluyor (öznitelik arama)"));

SS_MSG(stage_cleaning_up,
    EN("Cleaning up"),
    JA("後片付けをしています"),
    ZH_HANS("正在清理"),
    ZH_HANT("正在清理"),
    KO("정리하는 중"),
    DE("Aufräumen"),
    FR("Nettoyage"),
    ES("Limpiando"),
    PT("Limpando"),
    IT("Pulizia"),
    NL("Opruimen"),
    RU("Очистка"),
    TR("Temizleniyor"));

SS_MSG(stage_done,
    EN("Done"),
    JA("完了"),
    ZH_HANS("完成"),
    ZH_HANT("完成"),
    KO("완료"),
    DE("Fertig"),
    FR("Terminé"),
    ES("Listo"),
    PT("Concluído"),
    IT("Fatto"),
    NL("Klaar"),
    RU("Готово"),
    TR("Bitti"));

// ---- COLMAP's stages ------------------------------------------------------

SS_MSG(stage_vocab_download,
    EN("Downloading vocabulary tree (one-time, ~150 MB)"),
    JA("ボキャブラリツリーをダウンロードしています（初回のみ、約150 MB）"),
    ZH_HANS("正在下载词汇树（仅一次，约 150 MB）"),
    ZH_HANT("正在下載詞彙樹（僅一次，約 150 MB）"),
    KO("어휘 트리를 내려받는 중(최초 1회, 약 150 MB)"),
    DE("Vokabelbaum wird heruntergeladen (einmalig, ca. 150 MB)"),
    FR("Téléchargement de l'arbre de vocabulaire (une seule fois, ~150 Mo)"),
    ES("Descargando el árbol de vocabulario (una sola vez, ~150 MB)"),
    PT("Baixando a árvore de vocabulário (uma única vez, ~150 MB)"),
    IT("Download dell'albero di vocabolario (una sola volta, ~150 MB)"),
    NL("Vocabulaireboom downloaden (eenmalig, ~150 MB)"),
    RU("Загрузка словарного дерева (один раз, ~150 МБ)"),
    TR("Sözcük ağacı indiriliyor (tek seferlik, ~150 MB)"));

SS_MSG(stage_colmap_features,
    EN("Extracting features (colmap)"),
    JA("特徴点を抽出しています（colmap）"),
    ZH_HANS("正在提取特征点（colmap）"),
    ZH_HANT("正在擷取特徵點（colmap）"),
    KO("특징점을 뽑는 중(colmap)"),
    DE("Merkmale werden extrahiert (colmap)"),
    FR("Extraction des points caractéristiques (colmap)"),
    ES("Extrayendo puntos característicos (colmap)"),
    PT("Extraindo pontos característicos (colmap)"),
    IT("Estrazione dei punti caratteristici (colmap)"),
    NL("Kenmerken extraheren (colmap)"),
    RU("Извлечение особых точек (colmap)"),
    TR("Öznitelikler çıkarılıyor (colmap)"));

SS_MSG(stage_colmap_features_aliked,
    EN("Extracting features (colmap, ALIKED)"),
    JA("特徴点を抽出しています（colmap、ALIKED）"),
    ZH_HANS("正在提取特征点（colmap、ALIKED）"),
    ZH_HANT("正在擷取特徵點（colmap、ALIKED）"),
    KO("특징점을 뽑는 중(colmap, ALIKED)"),
    DE("Merkmale werden extrahiert (colmap, ALIKED)"),
    FR("Extraction des points caractéristiques (colmap, ALIKED)"),
    ES("Extrayendo puntos característicos (colmap, ALIKED)"),
    PT("Extraindo pontos característicos (colmap, ALIKED)"),
    IT("Estrazione dei punti caratteristici (colmap, ALIKED)"),
    NL("Kenmerken extraheren (colmap, ALIKED)"),
    RU("Извлечение особых точек (colmap, ALIKED)"),
    TR("Öznitelikler çıkarılıyor (colmap, ALIKED)"));

SS_MSG(stage_match_vocab,
    EN("Matching features (vocabulary tree)"),
    JA("特徴点を照合しています（ボキャブラリツリー）"),
    ZH_HANS("正在匹配特征点（词汇树）"),
    ZH_HANT("正在比對特徵點（詞彙樹）"),
    KO("특징점을 맞춰 보는 중(어휘 트리)"),
    DE("Merkmale werden zugeordnet (Vokabelbaum)"),
    FR("Mise en correspondance (arbre de vocabulaire)"),
    ES("Emparejando puntos característicos (árbol de vocabulario)"),
    PT("Correspondendo pontos característicos (árvore de vocabulário)"),
    IT("Corrispondenza dei punti caratteristici (albero di vocabolario)"),
    NL("Kenmerken koppelen (vocabulaireboom)"),
    RU("Сопоставление особых точек (словарное дерево)"),
    TR("Öznitelikler eşleştiriliyor (sözcük ağacı)"));

SS_MSG(stage_match_sequential,
    EN("Matching features (sequential)"),
    JA("特徴点を照合しています（連続フレーム）"),
    ZH_HANS("正在匹配特征点（顺序）"),
    ZH_HANT("正在比對特徵點（順序）"),
    KO("특징점을 맞춰 보는 중(순차)"),
    DE("Merkmale werden zugeordnet (fortlaufend)"),
    FR("Mise en correspondance (séquentielle)"),
    ES("Emparejando puntos característicos (secuencial)"),
    PT("Correspondendo pontos característicos (sequencial)"),
    IT("Corrispondenza dei punti caratteristici (sequenziale)"),
    NL("Kenmerken koppelen (opeenvolgend)"),
    RU("Сопоставление особых точек (последовательное)"),
    TR("Öznitelikler eşleştiriliyor (ardışık)"));

SS_MSG(stage_match_exhaustive,
    EN("Matching features (exhaustive)"),
    JA("特徴点を照合しています（総当たり）"),
    ZH_HANS("正在匹配特征点（穷举）"),
    ZH_HANT("正在比對特徵點（窮舉）"),
    KO("특징점을 맞춰 보는 중(전수 비교)"),
    DE("Merkmale werden zugeordnet (vollständig)"),
    FR("Mise en correspondance (exhaustive)"),
    ES("Emparejando puntos característicos (exhaustivo)"),
    PT("Correspondendo pontos característicos (exaustivo)"),
    IT("Corrispondenza dei punti caratteristici (esaustiva)"),
    NL("Kenmerken koppelen (uitputtend)"),
    RU("Сопоставление особых точек (полный перебор)"),
    TR("Öznitelikler eşleştiriliyor (tam karşılaştırma)"));

SS_MSG(stage_colmap_mapper,
    EN("Reconstructing cameras (mapper; this is the slow part)"),
    JA("カメラ位置を復元しています（mapper。時間のかかる工程です）"),
    ZH_HANS("正在恢复相机位姿（mapper，这一步最慢）"),
    ZH_HANT("正在還原相機位姿（mapper，這一步最慢）"),
    KO("카메라 위치를 복원하는 중(mapper, 가장 오래 걸리는 단계)"),
    DE("Kamerastandpunkte werden rekonstruiert (mapper; der langsame Teil)"),
    FR("Reconstruction des caméras (mapper ; c'est l'étape lente)"),
    ES("Reconstruyendo las cámaras (mapper; esta es la parte lenta)"),
    PT("Reconstruindo as câmeras (mapper; esta é a parte lenta)"),
    IT("Ricostruzione delle fotocamere (mapper; è la parte lenta)"),
    NL("Camerastandpunten reconstrueren (mapper; dit is het trage deel)"),
    RU("Восстановление положений камер (mapper; это самый долгий этап)"),
    TR("Kamera konumları yeniden kuruluyor (mapper; yavaş kısım budur)"));

SS_MSG(stage_merge_models,
    EN("Merging partial models"),
    JA("部分的なモデルをつなぎ合わせています"),
    ZH_HANS("正在合并零散的模型"),
    ZH_HANT("正在合併零散的模型"),
    KO("조각난 모델을 합치는 중"),
    DE("Teilmodelle werden zusammengeführt"),
    FR("Fusion des modèles partiels"),
    ES("Uniendo los modelos parciales"),
    PT("Juntando os modelos parciais"),
    IT("Unione dei modelli parziali"),
    NL("Deelmodellen samenvoegen"),
    RU("Объединение частичных моделей"),
    TR("Parçalı modeller birleştiriliyor"));

SS_MSG(stage_bundle_adjust,
    EN("Refining cameras (bundle adjustment)"),
    JA("カメラ位置を微調整しています（バンドル調整）"),
    ZH_HANS("正在微调相机位姿（光束法平差）"),
    ZH_HANT("正在微調相機位姿（光束法平差）"),
    KO("카메라 위치를 다듬는 중(번들 조정)"),
    DE("Kamerastandpunkte werden verfeinert (Bündelblockausgleichung)"),
    FR("Affinement des caméras (ajustement de faisceaux)"),
    ES("Afinando las cámaras (ajuste de haces)"),
    PT("Refinando as câmeras (ajuste de feixes)"),
    IT("Affinamento delle fotocamere (bundle adjustment)"),
    NL("Camerastandpunten verfijnen (bundeladjustering)"),
    RU("Уточнение положений камер (уравнивание связок)"),
    TR("Kamera konumları iyileştiriliyor (demet dengelemesi)"));

// ===========================================================================
// Notes -- what happened to the user's folder, and what to do about it
// ===========================================================================

// {0} the folder, {1} how many images.
SS_MSG(found_images,
    EN("Found {0} images in {1}"),
    JA("{1} に画像が {0} 枚見つかりました"),
    ZH_HANS("在 {1} 中找到 {0} 张图像"),
    ZH_HANT("在 {1} 中找到 {0} 張影像"),
    KO("{1}에서 이미지 {0}장을 찾았습니다"),
    DE("{0} Bilder in {1} gefunden"),
    FR("{0} images trouvées dans {1}"),
    ES("Se encontraron {0} imágenes en {1}"),
    PT("Encontradas {0} imagens em {1}"),
    IT("Trovate {0} immagini in {1}"),
    NL("{0} beelden gevonden in {1}"),
    RU("Найдено изображений: {0} (в {1})"),
    TR("{1} içinde {0} görüntü bulundu"));

SS_MSG(using_bundled_masks,
    EN("Using the masks that came with the photos: {0}"),
    JA("写真に付いていたマスクを使います: {0}"),
    ZH_HANS("使用随照片一起提供的蒙版：{0}"),
    ZH_HANT("使用隨照片一起提供的遮罩：{0}"),
    KO("사진에 딸려 온 마스크를 사용합니다: {0}"),
    DE("Die mitgelieferten Masken werden verwendet: {0}"),
    FR("Utilisation des masques fournis avec les photos : {0}"),
    ES("Se usan las máscaras que venían con las fotos: {0}"),
    PT("Usando as máscaras que vieram com as fotos: {0}"),
    IT("Si usano le maschere fornite con le foto: {0}"),
    NL("De meegeleverde maskers worden gebruikt: {0}"),
    RU("Используются маски, приложенные к фотографиям: {0}"),
    TR("Fotoğraflarla birlikte gelen maskeler kullanılıyor: {0}"));

SS_MSG(video_input,
    EN("Video: {0}"),
    JA("動画: {0}"),
    ZH_HANS("视频：{0}"),
    ZH_HANT("影片：{0}"),
    KO("영상: {0}"),
    DE("Video: {0}"),
    FR("Vidéo : {0}"),
    ES("Vídeo: {0}"),
    PT("Vídeo: {0}"),
    IT("Video: {0}"),
    NL("Video: {0}"),
    RU("Видео: {0}"),
    TR("Video: {0}"));

// {0} how many, {1} the folder.
SS_MSG(resume_keep_frames,
    EN("Resume: keeping {0} extracted frames in {1} (delete the folder to "
       "re-extract)"),
    JA("再開: {1} にある取り出し済みのフレーム {0} 枚をそのまま使います"
       "（取り直すにはフォルダーを削除してください）"),
    ZH_HANS("继续：保留 {1} 中已提取的 {0} 帧（想重新提取请删除该文件夹）"),
    ZH_HANT("繼續：保留 {1} 中已擷取的 {0} 個影格（想重新擷取請刪除該資料夾）"),
    KO("이어서 진행: {1}에 이미 뽑아 둔 프레임 {0}장을 그대로 씁니다(다시 "
       "뽑으려면 폴더를 지우세요)"),
    DE("Fortsetzen: die {0} bereits entnommenen Einzelbilder in {1} bleiben "
       "(zum erneuten Entnehmen den Ordner löschen)"),
    FR("Reprise : les {0} images déjà extraites dans {1} sont conservées "
       "(supprimez le dossier pour les réextraire)"),
    ES("Reanudar: se conservan los {0} fotogramas ya extraídos en {1} (borre "
       "la carpeta para volver a extraerlos)"),
    PT("Retomar: mantendo os {0} quadros já extraídos em {1} (apague a pasta "
       "para extrair de novo)"),
    IT("Ripresa: si tengono i {0} fotogrammi già estratti in {1} (cancelli la "
       "cartella per riestrarli)"),
    NL("Hervatten: de {0} al uitgepakte beelden in {1} blijven staan (verwijder "
       "de map om opnieuw uit te pakken)"),
    RU("Продолжение: оставляем {0} уже извлечённых кадров в {1} (чтобы извлечь "
       "заново, удалите папку)"),
    TR("Sürdürme: {1} içindeki {0} çıkarılmış kare korunuyor (yeniden çıkarmak "
       "için klasörü silin)"));

SS_MSG(resume_keep_masks,
    EN("Resume: keeping the masks in {0}"),
    JA("再開: {0} にあるマスクをそのまま使います"),
    ZH_HANS("继续：保留 {0} 中的蒙版"),
    ZH_HANT("繼續：保留 {0} 中的遮罩"),
    KO("이어서 진행: {0}에 있는 마스크를 그대로 씁니다"),
    DE("Fortsetzen: die Masken in {0} bleiben"),
    FR("Reprise : les masques dans {0} sont conservés"),
    ES("Reanudar: se conservan las máscaras en {0}"),
    PT("Retomar: mantendo as máscaras em {0}"),
    IT("Ripresa: si tengono le maschere in {0}"),
    NL("Hervatten: de maskers in {0} blijven staan"),
    RU("Продолжение: маски в {0} остаются"),
    TR("Sürdürme: {0} içindeki maskeler korunuyor"));

SS_MSG(resume_keep_frames_dir,
    EN("Resume: keeping the frames already in {0}"),
    JA("再開: すでに {0} にあるフレームをそのまま使います"),
    ZH_HANS("继续：保留 {0} 中已有的帧"),
    ZH_HANT("繼續：保留 {0} 中已有的影格"),
    KO("이어서 진행: {0}에 이미 있는 프레임을 그대로 씁니다"),
    DE("Fortsetzen: die Einzelbilder in {0} bleiben"),
    FR("Reprise : les images déjà présentes dans {0} sont conservées"),
    ES("Reanudar: se conservan los fotogramas que ya hay en {0}"),
    PT("Retomar: mantendo os quadros que já estão em {0}"),
    IT("Ripresa: si tengono i fotogrammi già presenti in {0}"),
    NL("Hervatten: de beelden die al in {0} staan blijven staan"),
    RU("Продолжение: кадры, уже лежащие в {0}, остаются"),
    TR("Sürdürme: {0} içinde zaten bulunan kareler korunuyor"));

// {0} the decoder's own message, English.
SS_MSG(decode_fallback_ffmpeg,
    EN("Built-in decoding could not handle this file ({0}); falling back to "
       "ffmpeg"),
    JA("内蔵のデコーダではこのファイルを扱えませんでした（{0}）。ffmpeg に"
       "切り替えます"),
    ZH_HANS("内置解码无法处理这个文件（{0}），改用 ffmpeg"),
    ZH_HANT("內建解碼無法處理這個檔案（{0}），改用 ffmpeg"),
    KO("내장 디코더로는 이 파일을 다룰 수 없었습니다({0}). ffmpeg으로 "
       "넘어갑니다"),
    DE("Die eingebaute Dekodierung kam mit dieser Datei nicht zurecht ({0}); es "
       "wird auf ffmpeg zurückgegriffen"),
    FR("Le décodage intégré n'a pas su traiter ce fichier ({0}) ; repli sur "
       "ffmpeg"),
    ES("La decodificación integrada no pudo con este archivo ({0}); se recurre "
       "a ffmpeg"),
    PT("A decodificação integrada não deu conta deste arquivo ({0}); recorrendo "
       "ao ffmpeg"),
    IT("La decodifica integrata non ha gestito questo file ({0}); si ripiega su "
       "ffmpeg"),
    NL("De ingebouwde decodering kon dit bestand niet aan ({0}); er wordt "
       "teruggevallen op ffmpeg"),
    RU("Встроенный декодер не справился с этим файлом ({0}); переходим на "
       "ffmpeg"),
    TR("Yerleşik çözücü bu dosyayla baş edemedi ({0}); ffmpeg'e geçiliyor"));

SS_MSG(kept_frames,
    EN("Kept {0} frames -> {1}"),
    JA("フレームを {0} 枚残しました -> {1}"),
    ZH_HANS("保留了 {0} 帧 -> {1}"),
    ZH_HANT("保留了 {0} 個影格 -> {1}"),
    KO("프레임 {0}장을 남겼습니다 -> {1}"),
    DE("{0} Einzelbilder behalten -> {1}"),
    FR("{0} images conservées -> {1}"),
    ES("Se conservaron {0} fotogramas -> {1}"),
    PT("Mantidos {0} quadros -> {1}"),
    IT("Tenuti {0} fotogrammi -> {1}"),
    NL("{0} beelden bewaard -> {1}"),
    RU("Оставлено кадров: {0} -> {1}"),
    TR("{0} kare tutuldu -> {1}"));

SS_MSG(linked_copied_kept,
    EN("  {0} linked, {1} copied, {2} already there"),
    JA("  リンク {0} 件、コピー {1} 件、既存 {2} 件"),
    ZH_HANS("  链接 {0} 个，复制 {1} 个，已有 {2} 个"),
    ZH_HANT("  連結 {0} 個，複製 {1} 個，已有 {2} 個"),
    KO("  링크 {0}개, 복사 {1}개, 이미 있던 것 {2}개"),
    DE("  {0} verknüpft, {1} kopiert, {2} schon vorhanden"),
    FR("  {0} liées, {1} copiées, {2} déjà présentes"),
    ES("  {0} enlazadas, {1} copiadas, {2} ya estaban"),
    PT("  {0} vinculadas, {1} copiadas, {2} já estavam lá"),
    IT("  {0} collegate, {1} copiate, {2} già presenti"),
    NL("  {0} gekoppeld, {1} gekopieerd, {2} stonden er al"),
    RU("  связано: {0}, скопировано: {1}, уже было: {2}"),
    TR("  {0} bağlandı, {1} kopyalandı, {2} zaten vardı"));

SS_MSG(err_inputs_without_prompt,
    EN("Nothing to mask by for {0}. A clicked object prompts only the input it "
       "was drawn on, so either click the object on every input, or add a text "
       "prompt."),
    JA("{0} をマスクする手がかりがありません。クリックした対象はそれを描いた"
       "入力にしか効かないため、すべての入力で対象をクリックするか、文字の"
       "プロンプトを追加してください。"),
    ZH_HANS("没有可用来给 {0} 生成蒙版的提示。点选的对象只对标注它的那个输入"
            "有效，请在每个输入上都点选一次对象，或者加上文字提示。"),
    ZH_HANT("沒有可用來給 {0} 產生遮罩的提示。點選的對象只對標註它的那個輸入"
            "有效，請在每個輸入上都點選一次對象，或者加上文字提示。"),
    KO("{0}을(를) 마스킹할 근거가 없습니다. 클릭한 대상은 그것을 그린 입력에만 "
       "적용되므로, 모든 입력에서 대상을 클릭하거나 텍스트 프롬프트를 "
       "추가하세요."),
    DE("Für {0} gibt es nichts, wonach maskiert werden könnte. Ein angeklicktes "
       "Objekt gilt nur für die Eingabe, auf der es eingezeichnet wurde -- "
       "klicken Sie es also auf jeder Eingabe an, oder ergänzen Sie einen "
       "Texthinweis."),
    FR("Rien pour masquer {0}. Un objet cliqué ne vaut que pour l'entrée sur "
       "laquelle il a été tracé : cliquez-le sur chaque entrée, ou ajoutez une "
       "invite textuelle."),
    ES("No hay con qué enmascarar {0}. Un objeto marcado solo vale para la "
       "entrada en la que se marcó: márquelo en cada entrada, o añada una "
       "indicación de texto."),
    PT("Não há com que mascarar {0}. Um objeto clicado só vale para a entrada "
       "em que foi marcado: marque-o em cada entrada, ou acrescente um comando "
       "de texto."),
    IT("Non c'è nulla con cui mascherare {0}. Un oggetto cliccato vale solo per "
       "l'ingresso su cui è stato tracciato: lo clicchi su ogni ingresso, "
       "oppure aggiunga un testo."),
    NL("Er is niets om {0} mee te maskeren. Een aangeklikt object geldt alleen "
       "voor de invoer waarop het is gezet: klik het op elke invoer aan, of "
       "voeg een tekstprompt toe."),
    RU("Нечем маскировать {0}. Отмеченный объект действует только на том входе, "
       "где его указали: отметьте его на каждом входе или добавьте текстовый "
       "запрос."),
    TR("{0} için maskeleyecek bir şey yok. Tıklanan nesne yalnızca "
       "işaretlendiği girdi için geçerlidir: nesneyi her girdide tıklayın ya da "
       "bir metin istemi ekleyin."));

SS_MSG(warn_unreadable_skipped,
    EN("warning: could not read {0}; skipped"),
    JA("警告: {0} を読み込めませんでした。とばします"),
    ZH_HANS("警告：无法读取 {0}，已跳过"),
    ZH_HANT("警告：無法讀取 {0}，已略過"),
    KO("경고: {0}을(를) 읽지 못해 건너뜁니다"),
    DE("Warnung: {0} konnte nicht gelesen werden; übersprungen"),
    FR("Avertissement : {0} n'a pas pu être lu ; ignoré"),
    ES("Aviso: no se pudo leer {0}; omitido"),
    PT("Aviso: não foi possível ler {0}; ignorado"),
    IT("Avviso: non è stato possibile leggere {0}; saltato"),
    NL("Waarschuwing: {0} kon niet worden gelezen; overgeslagen"),
    RU("Предупреждение: не удалось прочитать {0}; пропущено"),
    TR("Uyarı: {0} okunamadı; atlandı"));

// ---- the built-in reconstruction's notes ----------------------------------

SS_MSG(sfm_focal_unreadable,
    EN("warning: could not read an image in {0}; leaving its focal length to "
       "be guessed"),
    JA("警告: {0} の画像を読み込めませんでした。焦点距離は推測に任せます"),
    ZH_HANS("警告：无法读取 {0} 里的图像，焦距交给自动推测"),
    ZH_HANT("警告：無法讀取 {0} 裡的影像，焦距交給自動推測"),
    KO("경고: {0}의 이미지를 읽지 못했습니다. 초점 거리는 추정에 맡깁니다"),
    DE("Warnung: In {0} konnte kein Bild gelesen werden; die Brennweite wird "
       "geschätzt"),
    FR("Avertissement : aucune image lisible dans {0} ; la focale sera devinée"),
    ES("Aviso: no se pudo leer ninguna imagen en {0}; la focal se dejará "
       "adivinar"),
    PT("Aviso: não foi possível ler nenhuma imagem em {0}; a distância focal "
       "será adivinhada"),
    IT("Avviso: non è stato possibile leggere alcuna immagine in {0}; la "
       "focale sarà indovinata"),
    NL("Waarschuwing: geen leesbaar beeld in {0}; de brandpuntsafstand wordt "
       "geraden"),
    RU("Предупреждение: не удалось прочитать ни одного изображения в {0}; "
       "фокусное расстояние будет угадано"),
    TR("Uyarı: {0} içinde okunabilir görüntü yok; odak uzaklığı tahmin "
       "edilecek"));

// {0} which input, {1} the focal in px, {2} the factor, {3} the image width.
SS_MSG(sfm_initial_focal,
    EN("Initial focal length for {0}: {1} px ({2} x {3} px wide)"),
    JA("{0} の初期焦点距離: {1} px（横幅 {3} px の {2} 倍）"),
    ZH_HANS("{0} 的初始焦距：{1} px（宽 {3} px 的 {2} 倍）"),
    ZH_HANT("{0} 的初始焦距：{1} px（寬 {3} px 的 {2} 倍）"),
    KO("{0}의 초기 초점 거리: {1} px(가로 {3} px의 {2}배)"),
    DE("Anfangsbrennweite für {0}: {1} px ({2} x {3} px Breite)"),
    FR("Focale initiale pour {0} : {1} px ({2} x {3} px de large)"),
    ES("Distancia focal inicial de {0}: {1} px ({2} x {3} px de ancho)"),
    PT("Distância focal inicial de {0}: {1} px ({2} x {3} px de largura)"),
    IT("Focale iniziale per {0}: {1} px ({2} x {3} px di larghezza)"),
    NL("Beginbrandpuntsafstand voor {0}: {1} px ({2} x {3} px breed)"),
    RU("Начальное фокусное расстояние для {0}: {1} px ({2} x {3} px ширины)"),
    TR("{0} için başlangıç odak uzaklığı: {1} px ({2} x {3} px genişlik)"));

SS_MSG(sfm_the_capture,
    EN("the capture"),
    JA("この撮影"),
    ZH_HANS("本次拍摄"),
    ZH_HANT("本次拍攝"),
    KO("이번 촬영"),
    DE("die Aufnahme"),
    FR("la prise de vue"),
    ES("la captura"),
    PT("a captura"),
    IT("la ripresa"),
    NL("de opname"),
    RU("эта съёмка"),
    TR("bu çekim"));

SS_MSG(sfm_resuming,
    EN("Resuming the previous run in {0}"),
    JA("{0} にある前回の実行を再開します"),
    ZH_HANS("从 {0} 中上一次的运行继续"),
    ZH_HANT("從 {0} 中上一次的執行繼續"),
    KO("{0}에 있는 지난번 실행을 이어서 진행합니다"),
    DE("Der vorherige Lauf in {0} wird fortgesetzt"),
    FR("Reprise de l'exécution précédente dans {0}"),
    ES("Se reanuda la ejecución anterior en {0}"),
    PT("Retomando a execução anterior em {0}"),
    IT("Si riprende l'esecuzione precedente in {0}"),
    NL("De vorige uitvoering in {0} wordt hervat"),
    RU("Продолжаем предыдущий запуск в {0}"),
    TR("{0} içindeki önceki çalıştırma sürdürülüyor"));

SS_MSG(one_camera_per_folder,
    EN("images/ holds one folder per camera: switching to one camera per "
       "folder"),
    JA("images/ はカメラごとに1つのフォルダーになっています。フォルダーごとに"
       "1台のカメラとして扱います"),
    ZH_HANS("images/ 里每台相机一个文件夹：改为每个文件夹一台相机"),
    ZH_HANT("images/ 裡每台相機一個資料夾：改為每個資料夾一台相機"),
    KO("images/ 안에 카메라별로 폴더가 하나씩 있습니다: 폴더당 카메라 하나로 "
       "바꿉니다"),
    DE("images/ enthält einen Ordner je Kamera: es wird auf eine Kamera pro "
       "Ordner umgestellt"),
    FR("images/ contient un dossier par appareil : passage à un appareil par "
       "dossier"),
    ES("images/ tiene una carpeta por cámara: se cambia a una cámara por "
       "carpeta"),
    PT("images/ tem uma pasta por câmera: mudando para uma câmera por pasta"),
    IT("images/ ha una cartella per fotocamera: si passa a una fotocamera per "
       "cartella"),
    NL("images/ bevat één map per camera: er wordt overgeschakeld op één camera "
       "per map"),
    RU("В images/ по одной папке на камеру: переходим к режиму «одна камера на "
       "папку»"),
    TR("images/ her kamera için bir klasör içeriyor: klasör başına bir kameraya "
       "geçiliyor"));

SS_MSG(colmap_split_frame_sizes,
    EN("Images of several frame sizes share a camera group, which COLMAP cannot "
       "do -- extracting one camera per size instead. Camera groups: {0}"),
    JA("1つのカメラのまとまりに複数の画像サイズが混ざっています。COLMAP はこれを"
       "扱えないため、サイズごとに1台のカメラとして特徴点を抽出します。"
       "カメラのまとまり: {0}"),
    ZH_HANS("同一相机分组里混有多种画幅尺寸，COLMAP 无法处理：改为每种尺寸一台相机"
            "提取特征。相机分组: {0}"),
    ZH_HANT("同一相機分組裡混有多種畫幅尺寸，COLMAP 無法處理：改為每種尺寸一台相機"
            "擷取特徵。相機分組: {0}"),
    KO("한 카메라 묶음에 여러 이미지 크기가 섞여 있습니다. COLMAP은 이렇게 하지 "
       "못하므로 크기마다 카메라를 하나씩 두고 특징점을 추출합니다. 카메라 묶음: {0}"),
    DE("In einer Kameragruppe stecken mehrere Bildgrößen, was COLMAP nicht kann "
       "-- es wird stattdessen je Größe eine Kamera extrahiert. Kameragruppen: {0}"),
    FR("Plusieurs tailles d'image se trouvent dans un même groupe de caméras, ce "
       "que COLMAP ne sait pas faire : extraction avec une caméra par taille. "
       "Groupes de caméras : {0}"),
    ES("En un mismo grupo de cámaras hay varios tamaños de imagen, algo que "
       "COLMAP no admite: se extrae con una cámara por tamaño. "
       "Grupos de cámaras: {0}"),
    PT("Um mesmo grupo de câmeras tem vários tamanhos de imagem, o que o COLMAP "
       "não aceita: a extração usa uma câmera por tamanho. Grupos de câmeras: {0}"),
    IT("In uno stesso gruppo di fotocamere ci sono più dimensioni di immagine, "
       "cosa che COLMAP non ammette: l'estrazione usa una fotocamera per "
       "dimensione. Gruppi di fotocamere: {0}"),
    NL("Eén cameragroep bevat meerdere beeldformaten, wat COLMAP niet kan -- er "
       "wordt per formaat één camera uitgelezen. Cameragroepen: {0}"),
    RU("В одной группе камер оказались изображения разных размеров, чего COLMAP "
       "не допускает: извлечение идёт по одной камере на размер. Групп камер: {0}"),
    TR("Aynı kamera grubunda birden çok görüntü boyutu var; COLMAP bunu yapamaz, "
       "bu yüzden her boyut için ayrı kamera ile çıkarım yapılıyor. "
       "Kamera grubu sayısı: {0}"));

SS_MSG(sfm_partial,
    EN("Note: only part of the capture reconstructed. It will still train, but "
       "expect gaps."),
    JA("メモ: 撮影の一部しか復元できませんでした。学習はできますが、"
       "欠けが出ます。"),
    ZH_HANS("提示：只重建出了拍摄内容的一部分。仍然可以训练，但会有缺口。"),
    ZH_HANT("提示：只重建出了拍攝內容的一部分。仍然可以訓練，但會有缺口。"),
    KO("참고: 촬영분의 일부만 복원되었습니다. 학습은 되지만 빈 곳이 생깁니다."),
    DE("Hinweis: Nur ein Teil der Aufnahme wurde rekonstruiert. Das Training "
       "läuft trotzdem, aber mit Lücken."),
    FR("Note : seule une partie de la prise de vue a été reconstruite. "
       "L'entraînement fonctionnera, mais avec des trous."),
    ES("Nota: solo se reconstruyó parte de la captura. Se puede entrenar igual, "
       "pero habrá huecos."),
    PT("Nota: só parte da captura foi reconstruída. Ainda dá para treinar, mas "
       "haverá lacunas."),
    IT("Nota: è stata ricostruita solo una parte della ripresa. Si può "
       "addestrare lo stesso, ma con dei buchi."),
    NL("Let op: slechts een deel van de opname is gereconstrueerd. Trainen kan "
       "nog steeds, maar met gaten."),
    RU("Примечание: восстановлена лишь часть съёмки. Обучение пойдёт, но с "
       "пробелами."),
    TR("Not: çekimin yalnızca bir bölümü yeniden kuruldu. Yine de eğitilebilir, "
       "ama boşluklar olacak."));

SS_MSG(photos_referenced_in_place,
    EN("Note: the photos are referenced where they are. If you reopen this "
       "dataset later, set image_dir to {0} under the dataset options."),
    JA("メモ: 写真は元の場所を参照しています。あとでこのデータセットを開き直す"
       "ときは、データセット設定の image_dir に {0} を指定してください。"),
    ZH_HANS("提示：照片是就地引用的。以后重新打开这个数据集时，请在数据集选项里"
            "把 image_dir 设为 {0}。"),
    ZH_HANT("提示：照片是就地引用的。以後重新開啟這個資料集時，請在資料集選項裡"
            "把 image_dir 設為 {0}。"),
    KO("참고: 사진은 원래 자리를 참조합니다. 나중에 이 데이터셋을 다시 열 때는 "
       "데이터셋 옵션의 image_dir을 {0}(으)로 지정하세요."),
    DE("Hinweis: Die Fotos werden dort referenziert, wo sie liegen. Wenn Sie "
       "diesen Datensatz später wieder öffnen, setzen Sie image_dir in den "
       "Datensatzoptionen auf {0}."),
    FR("Note : les photos sont référencées là où elles sont. Si vous rouvrez ce "
       "jeu de données plus tard, réglez image_dir sur {0} dans ses options."),
    ES("Nota: las fotos se referencian donde están. Si vuelve a abrir este "
       "conjunto de datos más adelante, ponga image_dir en {0} dentro de sus "
       "opciones."),
    PT("Nota: as fotos são referenciadas onde estão. Se reabrir este conjunto "
       "de dados mais tarde, defina image_dir como {0} nas opções dele."),
    IT("Nota: le foto sono referenziate dove si trovano. Se riapre questo set "
       "di dati più avanti, imposti image_dir su {0} tra le sue opzioni."),
    NL("Let op: de foto's worden op hun eigen plek aangehaald. Als u deze "
       "dataset later opnieuw opent, zet image_dir dan op {0} bij de "
       "datasetopties."),
    RU("Примечание: фотографии используются там, где лежат. Если позже откроете "
       "этот набор данных снова, укажите в его параметрах image_dir = {0}."),
    TR("Not: fotoğraflar bulundukları yerden kullanılıyor. Bu veri kümesini "
       "sonra yeniden açarsanız, veri kümesi seçeneklerinde image_dir değerini "
       "{0} yapın."));

// {0} the port.
SS_MSG(web_viewer_at,
    EN("Web viewer at http://localhost:{0}/"),
    JA("ウェブビューア: http://localhost:{0}/"),
    ZH_HANS("网页查看器：http://localhost:{0}/"),
    ZH_HANT("網頁檢視器：http://localhost:{0}/"),
    KO("웹 뷰어: http://localhost:{0}/"),
    DE("Web-Betrachter unter http://localhost:{0}/"),
    FR("Visionneuse web sur http://localhost:{0}/"),
    ES("Visor web en http://localhost:{0}/"),
    PT("Visualizador web em http://localhost:{0}/"),
    IT("Visualizzatore web su http://localhost:{0}/"),
    NL("Webviewer op http://localhost:{0}/"),
    RU("Веб-просмотрщик: http://localhost:{0}/"),
    TR("Web görüntüleyici: http://localhost:{0}/"));

// ---- training ------------------------------------------------------------
//
// Only the lines that report progress to whoever is watching. The warnings
// about unported flags next to these in TrainerCore.cpp stay English on
// purpose: they name command-line flags and files under docs/notes/, so they
// are addressed to someone working on this program rather than using it.

// {0} cameras parsed, {1} after splitting panoramas, {2} seed points,
// {3} the frame scale.
SS_MSG(parsed_dataset,
    EN("Cameras: {0} ({1} after splitting), seed points: {2} "
       "(train_frame_scale={3})"),
    JA("カメラ: {0}（分割後 {1}）、初期点: {2}（train_frame_scale={3}）"),
    ZH_HANS("相机：{0}（拆分后 {1}），初始点：{2}（train_frame_scale={3}）"),
    ZH_HANT("相機：{0}（拆分後 {1}），初始點：{2}（train_frame_scale={3}）"),
    KO("카메라: {0}(분할 후 {1}), 초기 점: {2}(train_frame_scale={3})"),
    DE("Kameras: {0} ({1} nach dem Aufteilen), Startpunkte: {2} "
       "(train_frame_scale={3})"),
    FR("Caméras : {0} ({1} après découpage), points initiaux : {2} "
       "(train_frame_scale={3})"),
    ES("Cámaras: {0} ({1} tras dividir), puntos iniciales: {2} "
       "(train_frame_scale={3})"),
    PT("Câmeras: {0} ({1} após dividir), pontos iniciais: {2} "
       "(train_frame_scale={3})"),
    IT("Fotocamere: {0} ({1} dopo la divisione), punti iniziali: {2} "
       "(train_frame_scale={3})"),
    NL("Camera's: {0} ({1} na het splitsen), startpunten: {2} "
       "(train_frame_scale={3})"),
    RU("Камер: {0} ({1} после разделения), начальных точек: {2} "
       "(train_frame_scale={3})"),
    TR("Kamera: {0} (bölmeden sonra {1}), başlangıç noktası: {2} "
       "(train_frame_scale={3})"));

// {0} the --init-ply file, {1} its splats after the cap, {2} seeds the
// dataset's point cloud added on top (0 without --init-ply-add-points).
SS_MSG(seeded_from_ply,
    EN("Seeded from {0}: {1} splats from the PLY, {2} from the point cloud"),
    JA("{0} から初期化しました。PLY のスプラット {1}、点群から {2}"),
    ZH_HANS("已从 {0} 初始化：来自 PLY 的泼溅 {1} 个，来自点云 {2} 个"),
    ZH_HANT("已從 {0} 初始化：來自 PLY 的潑濺 {1} 個，來自點雲 {2} 個"),
    KO("{0}에서 초기화했습니다. PLY의 스플랫 {1}개, 점 구름에서 {2}개"),
    DE("Initialisiert aus {0}: {1} Splats aus dem PLY, {2} aus der Punktwolke"),
    FR("Initialisé depuis {0} : {1} splats du PLY, {2} du nuage de points"),
    ES("Inicializado desde {0}: {1} splats del PLY, {2} de la nube de puntos"),
    PT("Inicializado a partir de {0}: {1} splats do PLY, {2} da nuvem de pontos"),
    IT("Inizializzato da {0}: {1} splat dal PLY, {2} dalla nuvola di punti"),
    NL("Geïnitialiseerd vanuit {0}: {1} splats uit het PLY, {2} uit de puntenwolk"),
    RU("Инициализировано из {0}: сплатов из PLY {1}, из облака точек {2}"),
    TR("{0} dosyasından başlatıldı: PLY'den {1} splat, nokta bulutundan {2}"));

// Printed only when --input-depth-is-ray-depth was left unset and there are
// depth maps to read; {0} is the convention the lens picked.
SS_MSG(ray_depth_resolved,
    EN("Depth maps read as {0} (--input-depth-is-ray-depth)"),
    JA("深度マップを{0}として読み込みます（--input-depth-is-ray-depth）"),
    ZH_HANS("深度图按{0}读取（--input-depth-is-ray-depth）"),
    ZH_HANT("深度圖依{0}讀取（--input-depth-is-ray-depth）"),
    KO("깊이 맵을 {0}(으)로 읽습니다(--input-depth-is-ray-depth)"),
    DE("Tiefenkarten werden als {0} gelesen (--input-depth-is-ray-depth)"),
    FR("Cartes de profondeur lues comme {0} (--input-depth-is-ray-depth)"),
    ES("Los mapas de profundidad se leen como {0} (--input-depth-is-ray-depth)"),
    PT("Mapas de profundidade lidos como {0} (--input-depth-is-ray-depth)"),
    IT("Mappe di profondità lette come {0} (--input-depth-is-ray-depth)"),
    NL("Dieptekaarten gelezen als {0} (--input-depth-is-ray-depth)"),
    RU("Карты глубины читаются как {0} (--input-depth-is-ray-depth)"),
    TR("Derinlik haritaları {0} olarak okunuyor (--input-depth-is-ray-depth)"));

SS_MSG(ray_depth_along_ray,
    EN("distance along the ray"), JA("光線に沿った距離"),
    ZH_HANS("沿光线的距离"), ZH_HANT("沿光線的距離"),
    KO("광선을 따라 잰 거리"),
    DE("Abstand entlang des Strahls"),
    FR("distance le long du rayon"),
    ES("distancia a lo largo del rayo"),
    PT("distância ao longo do raio"),
    IT("distanza lungo il raggio"),
    NL("afstand langs de straal"),
    RU("расстояние вдоль луча"),
    TR("ışın boyunca mesafe"));

SS_MSG(ray_depth_straight_ahead,
    EN("distance straight ahead"), JA("正面方向の距離"),
    ZH_HANS("正前方的距离"), ZH_HANT("正前方的距離"),
    KO("정면 거리"),
    DE("Abstand geradeaus"),
    FR("distance droit devant"),
    ES("distancia hacia delante"),
    PT("distância em frente"),
    IT("distanza in avanti"),
    NL("afstand recht vooruit"),
    RU("расстояние прямо вперёд"),
    TR("ileri doğru mesafe"));

SS_MSG(ppisp_exif_exposure,
    EN("PPISP exposure initialized from EXIF ({0} of {1} photos)"),
    JA("PPISP の露出を EXIF から初期化しました（{1} 枚中 {0} 枚）"),
    ZH_HANS("已从 EXIF 初始化 PPISP 曝光（{1} 张照片中的 {0} 张）"),
    ZH_HANT("已從 EXIF 初始化 PPISP 曝光（{1} 張照片中的 {0} 張）"),
    KO("EXIF에서 PPISP 노출을 초기화했습니다({1}장 중 {0}장)"),
    DE("PPISP-Belichtung aus EXIF initialisiert ({0} von {1} Fotos)"),
    FR("Exposition PPISP initialisée depuis l'EXIF ({0} photos sur {1})"),
    ES("Exposición PPISP inicializada desde EXIF ({0} de {1} fotos)"),
    PT("Exposição PPISP inicializada a partir do EXIF ({0} de {1} fotos)"),
    IT("Esposizione PPISP inizializzata dall'EXIF ({0} foto su {1})"),
    NL("PPISP-belichting geïnitialiseerd uit EXIF ({0} van {1} foto's)"),
    RU("Экспозиция PPISP инициализирована из EXIF ({0} из {1} фото)"),
    TR("PPISP pozlaması EXIF'ten başlatıldı ({1} fotoğraftan {0})"));

// Printed when the input images are EXRs and no colour space was given on the
// command line; {0} is the gamut read out of the file.
SS_MSG(exr_color_space,
    EN("EXR input read as linear {0} (--image-color-gamut, --image-color-is-linear)"),
    JA("EXR 入力を線形 {0} として読み込みます"
       "（--image-color-gamut, --image-color-is-linear）"),
    ZH_HANS("EXR 输入按线性 {0} 读取（--image-color-gamut、--image-color-is-linear）"),
    ZH_HANT("EXR 輸入依線性 {0} 讀取（--image-color-gamut、--image-color-is-linear）"),
    KO("EXR 입력을 선형 {0}(으)로 읽습니다"
       "(--image-color-gamut, --image-color-is-linear)"),
    DE("EXR-Eingabe wird als lineares {0} gelesen "
       "(--image-color-gamut, --image-color-is-linear)"),
    FR("Entrée EXR lue comme {0} linéaire "
       "(--image-color-gamut, --image-color-is-linear)"),
    ES("Entrada EXR leída como {0} lineal "
       "(--image-color-gamut, --image-color-is-linear)"),
    PT("Entrada EXR lida como {0} linear "
       "(--image-color-gamut, --image-color-is-linear)"),
    IT("Ingresso EXR letto come {0} lineare "
       "(--image-color-gamut, --image-color-is-linear)"),
    NL("EXR-invoer gelezen als lineair {0} "
       "(--image-color-gamut, --image-color-is-linear)"),
    RU("Вход EXR читается как линейный {0} "
       "(--image-color-gamut, --image-color-is-linear)"),
    TR("EXR girdisi doğrusal {0} olarak okunuyor "
       "(--image-color-gamut, --image-color-is-linear)"));

// The same, for a run that declared the transfer itself and left only the
// primaries to the file.
SS_MSG(exr_gamut_from_file,
    EN("EXR colour space {0}, from the file (--image-color-gamut)"),
    JA("EXR の色空間は {0} です（ファイルの情報、--image-color-gamut）"),
    ZH_HANS("EXR 色彩空间为 {0}（取自文件，--image-color-gamut）"),
    ZH_HANT("EXR 色彩空間為 {0}（取自檔案，--image-color-gamut）"),
    KO("EXR 색 공간은 {0}입니다(파일에서 읽음, --image-color-gamut)"),
    DE("EXR-Farbraum {0}, aus der Datei (--image-color-gamut)"),
    FR("Espace colorimétrique EXR {0}, d'après le fichier (--image-color-gamut)"),
    ES("Espacio de color EXR {0}, según el archivo (--image-color-gamut)"),
    PT("Espaço de cor EXR {0}, conforme o arquivo (--image-color-gamut)"),
    IT("Spazio colore EXR {0}, dal file (--image-color-gamut)"),
    NL("EXR-kleurruimte {0}, uit het bestand (--image-color-gamut)"),
    RU("Цветовое пространство EXR {0}, из файла (--image-color-gamut)"),
    TR("EXR renk uzayı {0}, dosyadan (--image-color-gamut)"));

SS_MSG(exr_gamut_unknown,
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

SS_MSG(output_directory,
    EN("Output directory: {0}"),
    JA("出力先フォルダー: {0}"),
    ZH_HANS("输出文件夹：{0}"),
    ZH_HANT("輸出資料夾：{0}"),
    KO("출력 폴더: {0}"),
    DE("Ausgabeordner: {0}"),
    FR("Dossier de sortie : {0}"),
    ES("Carpeta de salida: {0}"),
    PT("Pasta de saída: {0}"),
    IT("Cartella di uscita: {0}"),
    NL("Uitvoermap: {0}"),
    RU("Папка вывода: {0}"),
    TR("Çıktı klasörü: {0}"));

SS_MSG(ckpt_adapting,
    EN("Checkpoint layout differs from this run's; adapting on the host (no "
       "extra VRAM)..."),
    JA("チェックポイントの構成が今回の実行と違います。ホスト側で合わせます"
       "（VRAMは追加で使いません）…"),
    ZH_HANS("检查点的结构与本次运行不同，正在主机端做适配（不额外占用显存）…"),
    ZH_HANT("檢查點的結構與本次執行不同，正在主機端做調整（不額外佔用顯示記憶體）…"),
    KO("체크포인트 구성이 이번 실행과 달라 호스트에서 맞추는 중입니다"
       "(VRAM을 더 쓰지 않습니다)…"),
    DE("Der Aufbau des Checkpoints weicht von diesem Lauf ab; er wird auf dem "
       "Host angepasst (kein zusätzlicher VRAM) …"),
    FR("La structure du point de reprise diffère de celle de cette exécution ; "
       "adaptation côté hôte (sans VRAM supplémentaire)…"),
    ES("La estructura del punto de control difiere de la de esta ejecución; se "
       "adapta en el anfitrión (sin VRAM adicional)…"),
    PT("A estrutura do ponto de verificação difere da desta execução; "
       "adaptando no host (sem VRAM extra)…"),
    IT("La struttura del checkpoint è diversa da quella di questa esecuzione; "
       "adattamento sull'host (senza VRAM aggiuntiva)…"),
    NL("De opbouw van het controlepunt wijkt af van die van deze uitvoering; "
       "aanpassen op de host (zonder extra VRAM)…"),
    RU("Структура контрольной точки отличается от текущего запуска; "
       "подгоняем на хосте (без дополнительной видеопамяти)…"),
    TR("Denetim noktasının yapısı bu çalıştırmadan farklı; ana makinede "
       "uyarlanıyor (ek VRAM kullanmadan)…"));

SS_MSG(resumed_from,
    EN("Resumed from {0} at step {1}"),
    JA("{0} のステップ {1} から再開しました"),
    ZH_HANS("已从 {0} 的第 {1} 步继续"),
    ZH_HANT("已從 {0} 的第 {1} 步繼續"),
    KO("{0}의 {1}단계부터 이어서 진행합니다"),
    DE("Fortgesetzt ab {0}, Schritt {1}"),
    FR("Reprise depuis {0} à l'étape {1}"),
    ES("Reanudado desde {0} en el paso {1}"),
    PT("Retomado de {0} no passo {1}"),
    IT("Ripreso da {0} al passo {1}"),
    NL("Hervat vanaf {0} bij stap {1}"),
    RU("Продолжено с {0}, шаг {1}"),
    TR("{0} konumundan {1}. adımda sürdürüldü"));

SS_MSG(checkpoint_saved,
    EN("Checkpoint saved to: {0}"),
    JA("チェックポイントを保存しました: {0}"),
    ZH_HANS("检查点已保存到：{0}"),
    ZH_HANT("檢查點已儲存到：{0}"),
    KO("체크포인트를 저장했습니다: {0}"),
    DE("Checkpoint gespeichert unter: {0}"),
    FR("Point de reprise enregistré dans : {0}"),
    ES("Punto de control guardado en: {0}"),
    PT("Ponto de verificação salvo em: {0}"),
    IT("Checkpoint salvato in: {0}"),
    NL("Controlepunt opgeslagen in: {0}"),
    RU("Контрольная точка сохранена: {0}"),
    TR("Denetim noktası şuraya kaydedildi: {0}"));

// Labelled rather than inflected ("Steps: 3", not "3 steps") -- see
// src/i18n/README.md. Steps counts what THIS run did, so it pairs with a
// time that also excludes whatever a resumed checkpoint already had.
SS_MSG(train_finished,
    EN("Training complete. Steps: {0}   Time: {1}"),
    JA("学習が完了しました。ステップ: {0}   所要時間: {1}"),
    ZH_HANS("训练完成。步数：{0}   用时：{1}"),
    ZH_HANT("訓練完成。步數：{0}   用時：{1}"),
    KO("학습이 끝났습니다. 스텝: {0}   소요 시간: {1}"),
    DE("Training abgeschlossen. Schritte: {0}   Zeit: {1}"),
    FR("Entraînement terminé. Étapes : {0}   Durée : {1}"),
    ES("Entrenamiento terminado. Pasos: {0}   Tiempo: {1}"),
    PT("Treinamento concluído. Passos: {0}   Tempo: {1}"),
    IT("Addestramento completato. Passi: {0}   Tempo: {1}"),
    NL("Training klaar. Stappen: {0}   Tijd: {1}"),
    RU("Обучение завершено. Шагов: {0}   Время: {1}"),
    TR("Eğitim tamamlandı. Adım: {0}   Süre: {1}"));

SS_MSG(eval_split_empty,
    EN("Eval: the eval split is empty; nothing to score."),
    JA("評価: 評価用の分割が空です。採点するものがありません。"),
    ZH_HANS("评估：评估集为空，没有可评分的内容。"),
    ZH_HANT("評估：評估集為空，沒有可評分的內容。"),
    KO("평가: 평가용 분할이 비어 있어 점수를 낼 것이 없습니다."),
    DE("Auswertung: Der Auswertungsanteil ist leer; nichts zu bewerten."),
    FR("Évaluation : la partie d'évaluation est vide ; rien à noter."),
    ES("Evaluación: la partición de evaluación está vacía; nada que puntuar."),
    PT("Avaliação: a divisão de avaliação está vazia; nada a pontuar."),
    IT("Valutazione: la parte di valutazione è vuota; niente da valutare."),
    NL("Evaluatie: het evaluatiedeel is leeg; niets te scoren."),
    RU("Оценка: набор для оценки пуст; оценивать нечего."),
    TR("Değerlendirme: değerlendirme bölümü boş; puanlanacak bir şey yok."));

SS_MSG(eval_views,
    EN("Eval: views to score: {0}"),
    JA("評価: 採点する視点: {0}"),
    ZH_HANS("评估：待评分的视角：{0}"),
    ZH_HANT("評估：待評分的視角：{0}"),
    KO("평가: 점수를 낼 시점: {0}"),
    DE("Auswertung: zu bewertende Ansichten: {0}"),
    FR("Évaluation : vues à noter : {0}"),
    ES("Evaluación: vistas a puntuar: {0}"),
    PT("Avaliação: vistas a pontuar: {0}"),
    IT("Valutazione: viste da valutare: {0}"),
    NL("Evaluatie: te scoren aanzichten: {0}"),
    RU("Оценка: видов для оценки: {0}"),
    TR("Değerlendirme: puanlanacak görünüm: {0}"));

SS_MSG(eval_no_views,
    EN("Eval: no views were rendered."),
    JA("評価: 描画された視点がありませんでした。"),
    ZH_HANS("评估：没有渲染出任何视角。"),
    ZH_HANT("評估：沒有算繪出任何視角。"),
    KO("평가: 렌더링된 시점이 없습니다."),
    DE("Auswertung: Es wurden keine Ansichten gerendert."),
    FR("Évaluation : aucune vue n'a été rendue."),
    ES("Evaluación: no se renderizó ninguna vista."),
    PT("Avaliação: nenhuma vista foi renderizada."),
    IT("Valutazione: nessuna vista è stata renderizzata."),
    NL("Evaluatie: er zijn geen aanzichten gerenderd."),
    RU("Оценка: ни один вид не был отрисован."),
    TR("Değerlendirme: hiçbir görünüm işlenmedi."));

SS_MSG(eval_metrics_written,
    EN("Eval metrics written to {0}"),
    JA("評価指標を {0} に書き出しました"),
    ZH_HANS("评估指标已写入 {0}"),
    ZH_HANT("評估指標已寫入 {0}"),
    KO("평가 지표를 {0}에 기록했습니다"),
    DE("Auswertungskennzahlen geschrieben nach {0}"),
    FR("Mesures d'évaluation écrites dans {0}"),
    ES("Métricas de evaluación escritas en {0}"),
    PT("Métricas de avaliação escritas em {0}"),
    IT("Metriche di valutazione scritte in {0}"),
    NL("Evaluatiecijfers weggeschreven naar {0}"),
    RU("Показатели оценки записаны в {0}"),
    TR("Değerlendirme ölçütleri {0} dosyasına yazıldı"));

// ===========================================================================
// Failures -- what the screen says when a run stops
// ===========================================================================

SS_MSG(err_cancelled,
    EN("cancelled"),
    JA("中止しました"),
    ZH_HANS("已取消"),
    ZH_HANT("已取消"),
    KO("취소했습니다"),
    DE("abgebrochen"),
    FR("annulé"),
    ES("cancelado"),
    PT("cancelado"),
    IT("annullato"),
    NL("geannuleerd"),
    RU("отменено"),
    TR("iptal edildi"));

SS_MSG(err_unfinished_run,
    EN("the output folder already holds an unfinished run (extracted frames / "
       "features / masks); tick \"Resume previous run\" to reuse it, or pick "
       "another folder"),
    JA("出力先のフォルダーには終わっていない実行の残り（取り出したフレーム、"
       "特徴点、マスク）があります。「前回の続きから」を入れて使い回すか、"
       "別のフォルダーを選んでください"),
    ZH_HANS("输出文件夹里已有一次未完成运行的残留（提取的帧／特征点／蒙版）。"
            "勾选“接着上次继续”来复用，或换一个文件夹"),
    ZH_HANT("輸出資料夾裡已有一次未完成執行的殘留（擷取的影格／特徵點／遮罩）。"
            "勾選「接著上次繼續」來重用，或換一個資料夾"),
    KO("출력 폴더에 끝나지 않은 실행의 흔적(뽑아 둔 프레임·특징점·마스크)이 "
       "있습니다. \"이전 작업 이어서\"를 켜서 재사용하거나 다른 폴더를 "
       "고르세요"),
    DE("Der Ausgabeordner enthält bereits einen unfertigen Lauf (entnommene "
       "Einzelbilder / Merkmale / Masken); haken Sie \"Vorherigen Lauf "
       "fortsetzen\" an, um ihn weiterzuverwenden, oder wählen Sie einen "
       "anderen Ordner"),
    FR("Le dossier de sortie contient déjà une exécution inachevée (images "
       "extraites, points caractéristiques, masques) ; cochez « Reprendre "
       "l'exécution précédente » pour la réutiliser, ou choisissez un autre "
       "dossier"),
    ES("La carpeta de salida ya contiene una ejecución sin terminar "
       "(fotogramas extraídos, puntos característicos, máscaras); marque "
       "\"Reanudar la ejecución anterior\" para aprovecharla, o elija otra "
       "carpeta"),
    PT("A pasta de saída já contém uma execução inacabada (quadros extraídos, "
       "pontos característicos, máscaras); marque \"Retomar a execução "
       "anterior\" para reaproveitá-la, ou escolha outra pasta"),
    IT("La cartella di uscita contiene già un'esecuzione incompiuta "
       "(fotogrammi estratti, punti caratteristici, maschere); spunti "
       "\"Riprendi l'esecuzione precedente\" per riusarla, oppure scelga "
       "un'altra cartella"),
    NL("De uitvoermap bevat al een onafgemaakte uitvoering (uitgepakte beelden "
       "/ kenmerken / maskers); vink \"Vorige uitvoering hervatten\" aan om die "
       "te hergebruiken, of kies een andere map"),
    RU("В папке вывода уже есть незавершённый запуск (извлечённые кадры, особые "
       "точки, маски); отметьте «Продолжить предыдущий запуск», чтобы "
       "использовать его, или выберите другую папку"),
    TR("Çıktı klasöründe yarım kalmış bir çalıştırma var (çıkarılmış kareler / "
       "öznitelikler / maskeler); yeniden kullanmak için \"Önceki çalıştırmayı "
       "sürdür\" seçeneğini işaretleyin ya da başka bir klasör seçin"));

// {0} the program that would not start.
SS_MSG(err_spawn_recon,
    EN("could not start the reconstruction ({0})"),
    JA("復元処理を起動できませんでした（{0}）"),
    ZH_HANS("无法启动重建程序（{0}）"),
    ZH_HANT("無法啟動重建程式（{0}）"),
    KO("복원 프로그램을 시작하지 못했습니다({0})"),
    DE("Die Rekonstruktion konnte nicht gestartet werden ({0})"),
    FR("Impossible de lancer la reconstruction ({0})"),
    ES("No se pudo iniciar la reconstrucción ({0})"),
    PT("Não foi possível iniciar a reconstrução ({0})"),
    IT("Non è stato possibile avviare la ricostruzione ({0})"),
    NL("De reconstructie kon niet worden gestart ({0})"),
    RU("Не удалось запустить реконструкцию ({0})"),
    TR("Yeniden kurulum başlatılamadı ({0})"));

SS_MSG(err_recon_gpu,
    EN("the GPU could not finish the reconstruction -- the driver reported a "
       "lost device or ran out of memory (the log has its error). Turn on "
       "\"{0}\" under Advanced and run it again: that step is slower on the "
       "CPU, but it does not put the whole model on the GPU at once."),
    JA("GPU が復元を完了できませんでした。デバイスが失われた、またはメモリが"
       "足りないとドライバが報告しています（詳しくはログを見てください）。「詳細設定」で「{0}」を"
       "オンにして、もう一度実行してください。CPU では遅くなりますが、モデル全体を"
       "一度に GPU へ載せずに済みます。"),
    ZH_HANS("GPU 无法完成重建: 驱动报告设备丢失或显存不足（错误在日志里）。"
            "请在「高级」中打开「{0}」后重新运行: 这一步在 CPU 上更慢，"
            "但不必把整个模型一次性放到 GPU 上。"),
    ZH_HANT("GPU 無法完成重建: 驅動回報裝置失效或記憶體不足（錯誤在日誌裡）。"
            "請在「進階」中開啟「{0}」後重新執行: 這一步在 CPU 上較慢，"
            "但不必把整個模型一次放到 GPU 上。"),
    KO("GPU가 복원을 끝내지 못했습니다. 드라이버가 장치 손실이나 메모리 부족을 "
       "알렸습니다(오류는 로그에 있습니다). 「고급」에서 「{0}」을 켜고 다시 "
       "실행하세요. CPU에서는 느리지만 모델 전체를 한 번에 GPU에 올리지 않습니다."),
    DE("Die GPU konnte die Rekonstruktion nicht abschließen -- der Treiber "
       "meldete ein verlorenes Gerät oder zu wenig Speicher (der Fehler steht "
       "im Protokoll). Schalten Sie unter \"Erweitert\" \"{0}\" ein und "
       "starten Sie erneut: Auf der CPU ist dieser Schritt langsamer, legt "
       "aber nicht das ganze Modell auf einmal auf die GPU."),
    FR("Le GPU n'a pas pu terminer la reconstruction : le pilote a signalé un "
       "périphérique perdu ou un manque de mémoire (son erreur est dans le "
       "journal). Activez \"{0}\" sous \"Avancé\" et relancez : cette "
       "étape est plus lente sur le CPU, mais elle ne met pas tout le modèle "
       "sur le GPU d'un coup."),
    ES("La GPU no pudo terminar la reconstrucción: el controlador informó de "
       "un dispositivo perdido o de falta de memoria (su error está en el "
       "registro). Active \"{0}\" en \"Avanzado\" y vuelva a ejecutarlo: "
       "ese paso es más lento en la CPU, pero no pone todo el modelo en la "
       "GPU de una vez."),
    PT("A GPU não conseguiu terminar a reconstrução: o driver relatou "
       "dispositivo perdido ou falta de memória (o erro está no registro). "
       "Ative \"{0}\" em \"Avançado\" e execute de novo: esse passo é "
       "mais lento na CPU, mas não coloca o modelo inteiro na GPU de uma vez."),
    IT("La GPU non è riuscita a terminare la ricostruzione: il driver ha "
       "segnalato un dispositivo perso o memoria esaurita (l'errore è nel "
       "registro). Attivi \"{0}\" in \"Avanzate\" e riprovi: sulla CPU "
       "quel passaggio è più lento, ma non mette tutto il modello sulla GPU "
       "in una volta."),
    NL("De GPU kon de reconstructie niet afmaken: het stuurprogramma meldde "
       "een verloren apparaat of te weinig geheugen (de fout staat in het "
       "logboek). Zet \"{0}\" aan onder \"Geavanceerd\" en voer het "
       "opnieuw uit: die stap is trager op de CPU, maar zet niet het hele "
       "model in één keer op de GPU."),
    RU("GPU не смог завершить реконструкцию: драйвер сообщил о потере "
       "устройства или нехватке памяти (его ошибка есть в журнале). Включите "
       "\"{0}\" в разделе \"Дополнительно\" и запустите снова: на CPU "
       "этот шаг медленнее, но не требует держать всю модель на GPU сразу."),
    TR("GPU yeniden kurulumu tamamlayamadı: sürücü aygıt kaybı veya bellek "
       "yetersizliği bildirdi (hatası günlükte). \"Gelişmiş\" altında "
       "\"{0}\" seçeneğini açıp yeniden çalıştırın: bu adım CPU'da daha "
       "yavaştır ama modelin tamamını bir kerede GPU'ya koymaz."));

SS_MSG(err_recon_failed,
    EN("reconstruction failed (see the log). Common causes: too few "
       "overlapping images, not enough overlap between them, or the wrong "
       "camera model for the lens."),
    JA("復元に失敗しました（ログを見てください）。よくある原因は、重なりのある"
       "画像が少なすぎる、画像どうしの重なりが足りない、レンズに合わない"
       "カメラモデルを選んでいる、などです。"),
    ZH_HANS("重建失败（请看日志）。常见原因：有重叠的图像太少、图像之间重叠不够、"
            "或者选的相机模型与镜头不匹配。"),
    ZH_HANT("重建失敗（請看日誌）。常見原因：有重疊的影像太少、影像之間重疊不夠、"
            "或者選的相機模型與鏡頭不符。"),
    KO("복원에 실패했습니다(로그를 보세요). 흔한 원인은 겹치는 이미지가 너무 "
       "적음, 이미지끼리 겹침이 부족함, 렌즈에 맞지 않는 카메라 모델 선택 "
       "등입니다."),
    DE("Die Rekonstruktion ist fehlgeschlagen (siehe Protokoll). Häufige "
       "Ursachen: zu wenige überlappende Bilder, zu geringe Überlappung, oder "
       "das falsche Kameramodell für das Objektiv."),
    FR("La reconstruction a échoué (voir le journal). Causes fréquentes : trop "
       "peu d'images qui se recouvrent, recouvrement insuffisant entre elles, "
       "ou un modèle de caméra qui ne correspond pas à l'objectif."),
    ES("La reconstrucción falló (vea el registro). Causas frecuentes: "
       "demasiadas pocas imágenes solapadas, solape insuficiente entre ellas, "
       "o un modelo de cámara que no corresponde al objetivo."),
    PT("A reconstrução falhou (veja o registro). Causas comuns: imagens "
       "sobrepostas de menos, sobreposição insuficiente entre elas, ou o modelo "
       "de câmera errado para a lente."),
    IT("La ricostruzione è fallita (veda il registro). Cause frequenti: troppe "
       "poche immagini sovrapposte, sovrapposizione insufficiente fra loro, "
       "oppure il modello di fotocamera sbagliato per l'obiettivo."),
    NL("De reconstructie is mislukt (zie het logboek). Veelvoorkomende "
       "oorzaken: te weinig overlappende beelden, te weinig overlap ertussen, "
       "of het verkeerde cameramodel voor de lens."),
    RU("Реконструкция не удалась (смотрите журнал). Обычные причины: слишком "
       "мало перекрывающихся снимков, недостаточное перекрытие между ними или "
       "неподходящая модель камеры для объектива."),
    TR("Yeniden kurulum başarısız oldu (günlüğe bakın). Sık görülen nedenler: "
       "örtüşen görüntünün çok az olması, aralarındaki örtüşmenin yetersizliği "
       "veya objektife uymayan kamera modeli."));

SS_MSG(err_no_reconstruction,
    EN("no reconstruction was produced -- the images may not overlap enough. "
       "Try more photos around the subject, or a higher quality setting."),
    JA("復元結果ができませんでした。画像どうしの重なりが足りない可能性が"
       "あります。被写体のまわりで写真を増やすか、品質を上げてみてください。"),
    ZH_HANS("没有产生任何重建结果——图像之间的重叠可能不够。试着围着拍摄对象多拍"
            "一些照片，或者把质量调高。"),
    ZH_HANT("沒有產生任何重建結果——影像之間的重疊可能不夠。試著繞著拍攝對象多拍"
            "一些照片，或者把品質調高。"),
    KO("복원 결과가 나오지 않았습니다. 이미지끼리 겹침이 부족할 수 있습니다. "
       "피사체 주위에서 사진을 더 찍거나 품질을 높여 보세요."),
    DE("Es wurde keine Rekonstruktion erzeugt -- die Bilder überlappen "
       "vermutlich zu wenig. Nehmen Sie mehr Fotos rund um das Motiv auf, oder "
       "wählen Sie eine höhere Qualitätsstufe."),
    FR("Aucune reconstruction n'a été produite -- les images ne se recouvrent "
       "peut-être pas assez. Prenez plus de photos autour du sujet, ou montez "
       "le réglage de qualité."),
    ES("No se produjo ninguna reconstrucción: puede que las imágenes no se "
       "solapen lo bastante. Haga más fotos alrededor del motivo, o suba el "
       "ajuste de calidad."),
    PT("Nenhuma reconstrução foi produzida -- talvez as imagens não se "
       "sobreponham o bastante. Tire mais fotos ao redor do objeto, ou aumente "
       "a qualidade."),
    IT("Non è stata prodotta alcuna ricostruzione: forse le immagini non si "
       "sovrappongono abbastanza. Scatti più foto attorno al soggetto, oppure "
       "alzi il livello di qualità."),
    NL("Er is geen reconstructie ontstaan -- de beelden overlappen wellicht te "
       "weinig. Maak meer foto's rond het onderwerp, of zet de kwaliteit "
       "hoger."),
    RU("Реконструкция не получена — вероятно, снимки перекрываются слишком "
       "мало. Снимите больше кадров вокруг объекта или поднимите уровень "
       "качества."),
    TR("Hiçbir yeniden kurulum üretilmedi -- görüntüler yeterince örtüşmüyor "
       "olabilir. Nesnenin çevresinde daha çok fotoğraf çekin ya da kalite "
       "ayarını yükseltin."));

SS_MSG(err_mesh_failed,
    EN("mesh extraction failed (see the log)."),
    JA("メッシュの抽出に失敗しました（ログを見てください）。"),
    ZH_HANS("网格提取失败（请看日志）。"),
    ZH_HANT("網格擷取失敗（請看日誌）。"),
    KO("메시 추출에 실패했습니다(로그를 보세요)."),
    DE("Die Netzerzeugung ist fehlgeschlagen (siehe Protokoll)."),
    FR("L'extraction du maillage a échoué (voir le journal)."),
    ES("La extracción de la malla falló (consulta el registro)."),
    PT("A extração da malha falhou (veja o registro)."),
    IT("L'estrazione della mesh non è riuscita (vedi il registro)."),
    NL("Het maken van de mesh is mislukt (zie het logboek)."),
    RU("Не удалось построить меш (смотрите журнал)."),
    TR("Ağ çıkarma başarısız oldu (günlüğe bakın)."));


// ===========================================================================
// What the trainer says about its own limits
// ===========================================================================
//
// The flag NAME in each of these is an identifier -- `--use-bvh` is
// `--use-bvh` in every language -- so it arrives as {0} and is printed
// verbatim. These reach the GUI's batch pre-flight as well as the log, which
// is why they are messages and not sentences built in place.

SS_MSG(not_supported_yet,
    EN("{0} is not supported yet"),
    JA("{0} はまだ対応していません"),
    ZH_HANS("{0} 还不支持"),
    ZH_HANT("{0} 還不支援"),
    KO("{0}은(는) 아직 지원하지 않습니다"),
    DE("{0} wird noch nicht unterstützt"),
    FR("{0} n'est pas encore pris en charge"),
    ES("{0} todavía no está admitido"),
    PT("{0} ainda não é aceito"),
    IT("{0} non è ancora supportato"),
    NL("{0} wordt nog niet ondersteund"),
    RU("{0} пока не поддерживается"),
    TR("{0} henüz desteklenmiyor"));
SS_MSG(ppisp_before_color_space_order,
    EN("{0} needs {1}: the per-photo color correction is always applied in sRGB"),
    JA("{0} には {1} が必要です。写真ごとの色補正はつねに sRGB で適用されます"),
    ZH_HANS("{0} 需要同时开启 {1}：逐张照片的颜色校正始终在 sRGB 中进行"),
    ZH_HANT("{0} 需要同時開啟 {1}：逐張照片的色彩校正始終在 sRGB 中進行"),
    KO("{0}에는 {1}이(가) 필요합니다. 사진별 색 보정은 항상 sRGB에서 적용됩니다"),
    DE("{0} setzt {1} voraus: Die Farbkorrektur pro Foto wird immer in sRGB "
       "angewendet"),
    FR("{0} requiert {1} : la correction de couleur par photo est toujours "
       "appliquée en sRGB"),
    ES("{0} requiere {1}: la corrección de color por foto siempre se aplica en "
       "sRGB"),
    PT("{0} exige {1}: a correção de cor por foto é sempre aplicada em sRGB"),
    IT("{0} richiede {1}: la correzione del colore per foto è sempre applicata "
       "in sRGB"),
    NL("{0} vereist {1}: de kleurcorrectie per foto wordt altijd in sRGB "
       "toegepast"),
    RU("{0} требует {1}: покадровая коррекция цвета всегда применяется в sRGB"),
    TR("{0}, {1} gerektirir: fotoğraf başına renk düzeltmesi her zaman sRGB'de "
       "uygulanır"));
SS_MSG(bad_quantization_level,
    EN("quantization_level must be 0 or 1"),
    JA("quantization_level は 0 か 1 にしてください"),
    ZH_HANS("quantization_level 必须是 0 或 1"),
    ZH_HANT("quantization_level 必須是 0 或 1"),
    KO("quantization_level은 0 또는 1이어야 합니다"),
    DE("quantization_level muss 0 oder 1 sein"),
    FR("quantization_level doit valoir 0 ou 1"),
    ES("quantization_level debe ser 0 o 1"),
    PT("quantization_level precisa ser 0 ou 1"),
    IT("quantization_level deve essere 0 o 1"),
    NL("quantization_level moet 0 of 1 zijn"),
    RU("quantization_level должен быть 0 или 1"),
    TR("quantization_level 0 ya da 1 olmalı"));
SS_MSG(warn_init_ply_ignored,
    EN("warning: --resume restores the checkpoint's own splats, so --init-ply "
       "is ignored"),
    JA("警告: --resume はチェックポイント自身のスプラットを復元するため、"
       "--init-ply は無視されます"),
    ZH_HANS("警告：--resume 会恢复检查点自己的泼溅，因此 --init-ply 会被忽略"),
    ZH_HANT("警告：--resume 會恢復檢查點自己的潑濺，因此 --init-ply 會被忽略"),
    KO("경고: --resume은 체크포인트 자체의 스플랫을 되살리므로 --init-ply는 "
       "무시됩니다"),
    DE("Warnung: --resume stellt die Splats des Checkpoints selbst wieder her, "
       "--init-ply wird ignoriert"),
    FR("Avertissement : --resume restaure les splats du point de reprise "
       "lui-même, donc --init-ply est ignoré"),
    ES("Aviso: --resume restaura los splats del propio punto de control, así "
       "que --init-ply se ignora"),
    PT("Aviso: --resume restaura os splats do próprio ponto de verificação, "
       "então --init-ply é ignorado"),
    IT("Avviso: --resume ripristina gli splat del checkpoint stesso, quindi "
       "--init-ply viene ignorato"),
    NL("Waarschuwing: --resume herstelt de splats van het controlepunt zelf, "
       "dus --init-ply wordt genegeerd"),
    RU("Предупреждение: --resume восстанавливает сплаты самой контрольной "
       "точки, поэтому --init-ply игнорируется"),
    TR("Uyarı: --resume denetim noktasının kendi splat'larını geri yükler, bu "
       "yüzden --init-ply yok sayılır"));
SS_MSG(warn_validation_unported,
    EN("warning: validation images are held out but early stopping / eval is "
       "not ported yet"),
    JA("警告: 検証用の画像は取り分けられますが、早期終了と評価はまだ移植されて"
       "いません"),
    ZH_HANS("警告：验证图像已经留出，但提前停止和评估还没有移植过来"),
    ZH_HANT("警告：驗證影像已經留出，但提前停止和評估還沒有移植過來"),
    KO("경고: 검증용 이미지는 떼어 두지만, 조기 종료와 평가는 아직 이식되지 "
       "않았습니다"),
    DE("Warnung: Validierungsbilder werden zurückgehalten, aber vorzeitiges "
       "Beenden und die Auswertung sind noch nicht portiert"),
    FR("Avertissement : des images de validation sont mises de côté, mais "
       "l'arrêt anticipé et l'évaluation ne sont pas encore portés"),
    ES("Aviso: se apartan imágenes de validación, pero la parada temprana y la "
       "evaluación todavía no están portadas"),
    PT("Aviso: imagens de validação são separadas, mas a parada antecipada e a "
       "avaliação ainda não foram portadas"),
    IT("Avviso: le immagini di validazione vengono messe da parte, ma "
       "l'arresto anticipato e la valutazione non sono ancora stati portati"),
    NL("Waarschuwing: er worden validatiebeelden apart gehouden, maar vroegtijdig "
       "stoppen en evalueren zijn nog niet overgezet"),
    RU("Предупреждение: проверочные снимки откладываются, но ранняя остановка и "
       "оценка ещё не перенесены"),
    TR("Uyarı: doğrulama görüntüleri ayrılıyor ama erken durdurma ve "
       "değerlendirme henüz taşınmadı"));
// {0} is --orientation-method's value and {1} is --center-method's; both are
// identifiers ('up', 'poses') and print verbatim.
SS_MSG(warn_pose_normalization_approx,
    EN("warning: orientation/center method '{0}'/'{1}' approximated as "
       "'up'/'poses' (affects only train_frame_scale; see "
       "docs/notes/pose-normalization.md for the unported reference "
       "implementation)"),
    JA("警告: 向き／中心の求め方 '{0}'／'{1}' は 'up'／'poses' で近似します"
       "（影響するのは train_frame_scale だけです。未移植の参照実装は "
       "docs/notes/pose-normalization.md にあります）"),
    ZH_HANS("警告：朝向／中心方法 '{0}'／'{1}' 用 'up'／'poses' 近似"
            "（只影响 train_frame_scale；未移植的参考实现见 "
            "docs/notes/pose-normalization.md）"),
    ZH_HANT("警告：朝向／中心方法 '{0}'／'{1}' 用 'up'／'poses' 近似"
            "（只影響 train_frame_scale；未移植的參考實作見 "
            "docs/notes/pose-normalization.md）"),
    KO("경고: 방향/중심 방식 '{0}'/'{1}'을(를) 'up'/'poses'로 근사합니다"
       "(train_frame_scale에만 영향을 줍니다. 이식되지 않은 참조 구현은 "
       "docs/notes/pose-normalization.md에 있습니다)"),
    DE("Warnung: Ausrichtungs-/Zentrierungsverfahren '{0}'/'{1}' wird durch "
       "'up'/'poses' angenähert (betrifft nur train_frame_scale; die nicht "
       "portierte Referenzimplementierung steht in "
       "docs/notes/pose-normalization.md)"),
    FR("Avertissement : les méthodes d'orientation/centrage '{0}'/'{1}' sont "
       "approchées par 'up'/'poses' (n'affecte que train_frame_scale ; "
       "l'implémentation de référence, non portée, est dans "
       "docs/notes/pose-normalization.md)"),
    ES("Aviso: los métodos de orientación/centrado '{0}'/'{1}' se aproximan "
       "por 'up'/'poses' (solo afecta a train_frame_scale; la implementación "
       "de referencia, sin portar, está en docs/notes/pose-normalization.md)"),
    PT("Aviso: os métodos de orientação/centralização '{0}'/'{1}' são "
       "aproximados por 'up'/'poses' (afeta só train_frame_scale; a "
       "implementação de referência, não portada, está em "
       "docs/notes/pose-normalization.md)"),
    IT("Avviso: i metodi di orientamento/centratura '{0}'/'{1}' sono "
       "approssimati con 'up'/'poses' (riguarda solo train_frame_scale; "
       "l'implementazione di riferimento, non portata, è in "
       "docs/notes/pose-normalization.md)"),
    NL("Waarschuwing: de oriëntatie-/centreermethode '{0}'/'{1}' wordt benaderd "
       "met 'up'/'poses' (raakt alleen train_frame_scale; de niet overgezette "
       "referentie-implementatie staat in docs/notes/pose-normalization.md)"),
    RU("Предупреждение: способы ориентации/центрирования '{0}'/'{1}' заменены "
       "приближением 'up'/'poses' (влияет только на train_frame_scale; "
       "непереносённая эталонная реализация -- в "
       "docs/notes/pose-normalization.md)"),
    TR("Uyarı: yönlendirme/merkezleme yöntemi '{0}'/'{1}', 'up'/'poses' ile "
       "yaklaşık olarak karşılanıyor (yalnızca train_frame_scale'i etkiler; "
       "taşınmamış referans gerçekleme docs/notes/pose-normalization.md "
       "dosyasında)"));

// ===========================================================================
// Meshing stages -- the caption under the GUI's mesh progress bar
// ===========================================================================
//
// `spirula mesh` runs as a child process and prints "[meshing] <stage> ..."
// lines that are DIAGNOSTICS: numbers, timings, chart counts, read by whoever
// is tuning the pipeline, and English like the rest of that layer. The GUI
// reads those same lines to move its progress bar (src/app/gui/MeshRunner.cpp),
// which is the second reason they do not get translated -- they are a protocol
// between two processes, and a protocol in thirteen languages is thirteen
// protocols.
//
// What the GUI SHOWS, though, is addressed to whoever is waiting for a mesh.
// So MeshRunner matches the English stage word and displays the message below
// it. One entry per row of kStages there; adding a stage there without one
// here shows nothing, which is why the two lists sit next to each other in
// that file.

SS_MSG(mesh_stage_loading,
    EN("Loading the model"),
    JA("モデルを読み込んでいます"),
    ZH_HANS("正在加载模型"),
    ZH_HANT("正在載入模型"),
    KO("모델을 불러오는 중"),
    DE("Modell wird geladen"),
    FR("Chargement du modèle"),
    ES("Cargando el modelo"),
    PT("Carregando o modelo"),
    IT("Caricamento del modello"),
    NL("Model laden"),
    RU("Загрузка модели"),
    TR("Model yükleniyor"));
SS_MSG(mesh_stage_point_cloud,
    EN("Sampling a point cloud"),
    JA("点群をサンプリングしています"),
    ZH_HANS("正在采样点云"),
    ZH_HANT("正在取樣點雲"),
    KO("점 구름을 표본으로 뽑는 중"),
    DE("Punktwolke wird abgetastet"),
    FR("Échantillonnage d'un nuage de points"),
    ES("Muestreando una nube de puntos"),
    PT("Amostrando uma nuvem de pontos"),
    IT("Campionamento di una nuvola di punti"),
    NL("Puntenwolk bemonsteren"),
    RU("Выборка облака точек"),
    TR("Nokta bulutu örnekleniyor"));
SS_MSG(mesh_stage_delaunay,
    EN("Triangulating the points"),
    JA("点を三角形分割しています"),
    ZH_HANS("正在对点做三角剖分"),
    ZH_HANT("正在對點做三角剖分"),
    KO("점을 삼각 분할하는 중"),
    DE("Punkte werden trianguliert"),
    FR("Triangulation des points"),
    ES("Triangulando los puntos"),
    PT("Triangulando os pontos"),
    IT("Triangolazione dei punti"),
    NL("Punten trianguleren"),
    RU("Триангуляция точек"),
    TR("Noktalar üçgenleniyor"));
SS_MSG(mesh_stage_occupancy,
    EN("Measuring where the surface is"),
    JA("面がどこにあるかを調べています"),
    ZH_HANS("正在判断表面在哪里"),
    ZH_HANT("正在判斷表面在哪裡"),
    KO("표면이 어디인지 재는 중"),
    DE("Es wird ermittelt, wo die Oberfläche liegt"),
    FR("Repérage de la surface"),
    ES("Midiendo dónde está la superficie"),
    PT("Medindo onde está a superfície"),
    IT("Individuazione della superficie"),
    NL("Bepalen waar het oppervlak ligt"),
    RU("Определение положения поверхности"),
    TR("Yüzeyin nerede olduğu ölçülüyor"));
SS_MSG(mesh_stage_cut_edges,
    EN("Finding the surface crossings"),
    JA("面と交わる辺を探しています"),
    ZH_HANS("正在寻找与表面相交的边"),
    ZH_HANT("正在尋找與表面相交的邊"),
    KO("표면과 만나는 모서리를 찾는 중"),
    DE("Die Kanten durch die Oberfläche werden gesucht"),
    FR("Recherche des arêtes traversant la surface"),
    ES("Buscando las aristas que cruzan la superficie"),
    PT("Procurando as arestas que cruzam a superfície"),
    IT("Ricerca degli spigoli che attraversano la superficie"),
    NL("Zoeken naar de ribben door het oppervlak"),
    RU("Поиск рёбер, пересекающих поверхность"),
    TR("Yüzeyi kesen kenarlar aranıyor"));
SS_MSG(mesh_stage_bisection,
    EN("Pinning the surface down"),
    JA("面の位置を絞り込んでいます"),
    ZH_HANS("正在把表面位置收紧"),
    ZH_HANT("正在把表面位置收緊"),
    KO("표면 위치를 좁히는 중"),
    DE("Die Lage der Oberfläche wird eingegrenzt"),
    FR("Localisation précise de la surface"),
    ES("Afinando la posición de la superficie"),
    PT("Afinando a posição da superfície"),
    IT("Individuazione precisa della superficie"),
    NL("De ligging van het oppervlak nauwkeuriger bepalen"),
    RU("Уточнение положения поверхности"),
    TR("Yüzeyin yeri daraltılıyor"));
SS_MSG(mesh_stage_marching_tets,
    EN("Building the triangles"),
    JA("三角形を組み立てています"),
    ZH_HANS("正在生成三角面"),
    ZH_HANT("正在產生三角面"),
    KO("삼각형을 만드는 중"),
    DE("Dreiecke werden gebaut"),
    FR("Construction des triangles"),
    ES("Construyendo los triángulos"),
    PT("Construindo os triângulos"),
    IT("Costruzione dei triangoli"),
    NL("Driehoeken bouwen"),
    RU("Построение треугольников"),
    TR("Üçgenler oluşturuluyor"));
SS_MSG(mesh_stage_merge,
    EN("Merging short edges"),
    JA("短い辺をまとめています"),
    ZH_HANS("正在合并过短的边"),
    ZH_HANT("正在合併過短的邊"),
    KO("짧은 모서리를 합치는 중"),
    DE("Kurze Kanten werden zusammengefasst"),
    FR("Fusion des arêtes courtes"),
    ES("Fusionando las aristas cortas"),
    PT("Juntando as arestas curtas"),
    IT("Unione degli spigoli corti"),
    NL("Korte ribben samenvoegen"),
    RU("Слияние коротких рёбер"),
    TR("Kısa kenarlar birleştiriliyor"));
SS_MSG(mesh_stage_cleanup,
    EN("Cleaning up the mesh"),
    JA("メッシュを整理しています"),
    ZH_HANS("正在清理网格"),
    ZH_HANT("正在清理網格"),
    KO("메시를 정리하는 중"),
    DE("Das Netz wird aufgeräumt"),
    FR("Nettoyage du maillage"),
    ES("Limpiando la malla"),
    PT("Limpando a malha"),
    IT("Pulizia della mesh"),
    NL("De mesh opschonen"),
    RU("Очистка меша"),
    TR("Ağ temizleniyor"));
SS_MSG(mesh_stage_cull_unseen,
    EN("Dropping what no camera saw"),
    JA("どのカメラからも見えない部分を落としています"),
    ZH_HANS("正在丢掉没有相机看到的部分"),
    ZH_HANT("正在丟掉沒有相機看到的部分"),
    KO("어느 카메라에도 보이지 않은 부분을 버리는 중"),
    DE("Was keine Kamera gesehen hat, wird entfernt"),
    FR("Suppression de ce qu'aucune caméra n'a vu"),
    ES("Descartando lo que ninguna cámara vio"),
    PT("Descartando o que nenhuma câmera viu"),
    IT("Rimozione di ciò che nessuna camera ha visto"),
    NL("Weggooien wat geen camera zag"),
    RU("Удаление того, чего не видела ни одна камера"),
    TR("Hiçbir kameranın görmediği kısımlar atılıyor"));
SS_MSG(mesh_stage_quality,
    EN("Improving the triangles"),
    JA("三角形の質を上げています"),
    ZH_HANS("正在改善三角面质量"),
    ZH_HANT("正在改善三角面品質"),
    KO("삼각형 품질을 높이는 중"),
    DE("Die Dreiecke werden verbessert"),
    FR("Amélioration des triangles"),
    ES("Mejorando los triángulos"),
    PT("Melhorando os triângulos"),
    IT("Miglioramento dei triangoli"),
    NL("De driehoeken verbeteren"),
    RU("Улучшение треугольников"),
    TR("Üçgenler iyileştiriliyor"));
SS_MSG(mesh_stage_orient,
    EN("Orienting the surface"),
    JA("面の向きをそろえています"),
    ZH_HANS("正在统一表面朝向"),
    ZH_HANT("正在統一表面朝向"),
    KO("표면 방향을 맞추는 중"),
    DE("Die Oberfläche wird ausgerichtet"),
    FR("Orientation de la surface"),
    ES("Orientando la superficie"),
    PT("Orientando a superfície"),
    IT("Orientamento della superficie"),
    NL("Het oppervlak oriënteren"),
    RU("Ориентирование поверхности"),
    TR("Yüzey yönlendiriliyor"));
SS_MSG(mesh_stage_color,
    EN("Coloring the vertices"),
    JA("頂点に色を付けています"),
    ZH_HANS("正在给顶点上色"),
    ZH_HANT("正在給頂點上色"),
    KO("정점에 색을 입히는 중"),
    DE("Die Eckpunkte werden eingefärbt"),
    FR("Coloration des sommets"),
    ES("Coloreando los vértices"),
    PT("Colorindo os vértices"),
    IT("Colorazione dei vertici"),
    NL("De hoekpunten kleuren"),
    RU("Раскраска вершин"),
    TR("Köşeler renklendiriliyor"));
SS_MSG(mesh_stage_uv,
    EN("Laying out the texture map"),
    JA("テクスチャの配置を決めています"),
    ZH_HANS("正在排布纹理贴图"),
    ZH_HANT("正在排布紋理貼圖"),
    KO("텍스처 배치를 잡는 중"),
    DE("Die Texturbelegung wird angeordnet"),
    FR("Disposition de la carte de texture"),
    ES("Distribuyendo el mapa de textura"),
    PT("Distribuindo o mapa de textura"),
    IT("Disposizione della mappa di texture"),
    NL("De textuurkaart indelen"),
    RU("Раскладка текстурной карты"),
    TR("Doku haritası yerleştiriliyor"));
SS_MSG(mesh_stage_bake,
    EN("Baking the texture"),
    JA("テクスチャを焼き付けています"),
    ZH_HANS("正在烘焙纹理"),
    ZH_HANT("正在烘焙紋理"),
    KO("텍스처를 굽는 중"),
    DE("Die Textur wird gebacken"),
    FR("Cuisson de la texture"),
    ES("Horneando la textura"),
    PT("Assando a textura"),
    IT("Baking della texture"),
    NL("De textuur bakken"),
    RU("Запекание текстуры"),
    TR("Doku pişiriliyor"));
SS_MSG(mesh_stage_texture,
    EN("Finishing the texture"),
    JA("テクスチャを仕上げています"),
    ZH_HANS("正在完成纹理"),
    ZH_HANT("正在完成紋理"),
    KO("텍스처를 마무리하는 중"),
    DE("Die Textur wird fertiggestellt"),
    FR("Finition de la texture"),
    ES("Terminando la textura"),
    PT("Finalizando a textura"),
    IT("Rifinitura della texture"),
    NL("De textuur afwerken"),
    RU("Завершение текстуры"),
    TR("Doku tamamlanıyor"));
SS_MSG(mesh_stage_stats,
    EN("Measuring the mesh"),
    JA("メッシュを計測しています"),
    ZH_HANS("正在统计网格"),
    ZH_HANT("正在統計網格"),
    KO("메시를 재는 중"),
    DE("Das Netz wird vermessen"),
    FR("Mesure du maillage"),
    ES("Midiendo la malla"),
    PT("Medindo a malha"),
    IT("Misurazione della mesh"),
    NL("De mesh opmeten"),
    RU("Измерение меша"),
    TR("Ağ ölçülüyor"));
SS_MSG(mesh_stage_wrote,
    EN("Writing the files"),
    JA("ファイルを書き出しています"),
    ZH_HANS("正在写出文件"),
    ZH_HANT("正在寫出檔案"),
    KO("파일을 쓰는 중"),
    DE("Die Dateien werden geschrieben"),
    FR("Écriture des fichiers"),
    ES("Escribiendo los archivos"),
    PT("Escrevendo os arquivos"),
    IT("Scrittura dei file"),
    NL("De bestanden schrijven"),
    RU("Запись файлов"),
    TR("Dosyalar yazılıyor"));


// Both `split_batch` and `use_fused_proj_bwd_optim` on at once is a
// contradiction the engine resolves for you; these say which way it went. The
// two flag names are identifiers and stay as they are -- what is translated is
// the reason.

SS_MSG(warn_split_batch_noop,
    EN("warning: both `split_batch` and `use_fused_proj_bwd_optim` are "
       "enabled, but this dataset never puts more than one camera in a batch, "
       "so `split_batch` would do nothing. Turning it off and keeping "
       "`use_fused_proj_bwd_optim`."),
    JA("警告: `split_batch` と `use_fused_proj_bwd_optim` が両方とも有効ですが、"
       "このデータセットでは 1 バッチに 1 台のカメラしか入らないため "
       "`split_batch` は何もしません。`split_batch` を無効にし、"
       "`use_fused_proj_bwd_optim` を使います。"),
    ZH_HANS("警告：`split_batch` 和 `use_fused_proj_bwd_optim` 同时开启，但这个"
            "数据集每批最多只有一台相机，`split_batch` 起不到作用。已关闭 "
            "`split_batch`，保留 `use_fused_proj_bwd_optim`。"),
    ZH_HANT("警告：`split_batch` 和 `use_fused_proj_bwd_optim` 同時開啟，但這個"
            "資料集每批最多只有一台相機，`split_batch` 起不到作用。已關閉 "
            "`split_batch`，保留 `use_fused_proj_bwd_optim`。"),
    KO("경고: `split_batch`와 `use_fused_proj_bwd_optim`이 모두 켜져 있지만, 이 "
       "데이터셋은 한 배치에 카메라가 하나뿐이라 `split_batch`는 아무 일도 하지 "
       "않습니다. `split_batch`를 끄고 `use_fused_proj_bwd_optim`을 씁니다."),
    DE("Warnung: `split_batch` und `use_fused_proj_bwd_optim` sind beide an, "
       "aber dieser Datensatz hat nie mehr als eine Kamera pro Stapel, also "
       "täte `split_batch` nichts. Es wird abgeschaltet, "
       "`use_fused_proj_bwd_optim` bleibt."),
    FR("Avertissement : `split_batch` et `use_fused_proj_bwd_optim` sont tous "
       "deux activés, mais ce jeu de données ne met jamais plus d'une caméra "
       "par lot, donc `split_batch` ne ferait rien. Il est désactivé et "
       "`use_fused_proj_bwd_optim` est conservé."),
    ES("Aviso: `split_batch` y `use_fused_proj_bwd_optim` están activados los "
       "dos, pero este conjunto de datos nunca pone más de una cámara por "
       "lote, así que `split_batch` no haría nada. Se desactiva y se mantiene "
       "`use_fused_proj_bwd_optim`."),
    PT("Aviso: `split_batch` e `use_fused_proj_bwd_optim` estão ambos ligados, "
       "mas este conjunto de dados nunca põe mais de uma câmera por lote, "
       "então `split_batch` não faria nada. Ele é desligado e "
       "`use_fused_proj_bwd_optim` fica."),
    IT("Avviso: `split_batch` e `use_fused_proj_bwd_optim` sono entrambi "
       "attivi, ma questo set di dati non mette mai più di una camera per "
       "batch, quindi `split_batch` non farebbe nulla. Viene disattivato e "
       "resta `use_fused_proj_bwd_optim`."),
    NL("Waarschuwing: `split_batch` en `use_fused_proj_bwd_optim` staan allebei "
       "aan, maar deze dataset zet nooit meer dan één camera in een batch, dus "
       "`split_batch` zou niets doen. Het gaat uit en "
       "`use_fused_proj_bwd_optim` blijft."),
    RU("Предупреждение: включены и `split_batch`, и "
       "`use_fused_proj_bwd_optim`, но в этом наборе данных в пакете никогда не "
       "бывает больше одной камеры, так что `split_batch` ничего не даст. Он "
       "выключается, `use_fused_proj_bwd_optim` остаётся."),
    TR("Uyarı: `split_batch` ile `use_fused_proj_bwd_optim` birlikte açık ama "
       "bu veri kümesi bir yığına asla birden fazla kamera koymuyor, yani "
       "`split_batch` bir işe yaramazdı. Kapatıldı, `use_fused_proj_bwd_optim` "
       "korunuyor."));
// {0} is the largest batch this dataset produces.
SS_MSG(warn_fpbo_incompatible,
    EN("warning: both `split_batch` and `use_fused_proj_bwd_optim` are "
       "enabled, but a batch here can hold more than one camera (at most {0}), "
       "which `use_fused_proj_bwd_optim` cannot accumulate gradients across. "
       "Turning it off and keeping `split_batch`."),
    JA("警告: `split_batch` と `use_fused_proj_bwd_optim` が両方とも有効ですが、"
       "ここでは 1 バッチに複数のカメラが入りえます（最大 {0} 台）。"
       "`use_fused_proj_bwd_optim` はその間で勾配を足し合わせられないため、"
       "無効にし、`split_batch` を使います。"),
    ZH_HANS("警告：`split_batch` 和 `use_fused_proj_bwd_optim` 同时开启，但这里"
            "一批里可能有多台相机（最多 {0} 台），而 `use_fused_proj_bwd_optim` "
            "无法跨相机累积梯度。已关闭它，保留 `split_batch`。"),
    ZH_HANT("警告：`split_batch` 和 `use_fused_proj_bwd_optim` 同時開啟，但這裡"
            "一批裡可能有多台相機（最多 {0} 台），而 `use_fused_proj_bwd_optim` "
            "無法跨相機累積梯度。已關閉它，保留 `split_batch`。"),
    KO("경고: `split_batch`와 `use_fused_proj_bwd_optim`이 모두 켜져 있지만, "
       "여기서는 한 배치에 카메라가 여러 대 들어올 수 있고(최대 {0}대), "
       "`use_fused_proj_bwd_optim`은 그 사이로 기울기를 누적하지 못합니다. "
       "이를 끄고 `split_batch`를 씁니다."),
    DE("Warnung: `split_batch` und `use_fused_proj_bwd_optim` sind beide an, "
       "aber ein Stapel kann hier mehr als eine Kamera enthalten (höchstens "
       "{0}), und darüber kann `use_fused_proj_bwd_optim` keine Gradienten "
       "aufsummieren. Es wird abgeschaltet, `split_batch` bleibt."),
    FR("Avertissement : `split_batch` et `use_fused_proj_bwd_optim` sont tous "
       "deux activés, mais un lot peut contenir ici plusieurs caméras ({0} au "
       "plus), et `use_fused_proj_bwd_optim` ne sait pas cumuler les gradients "
       "à travers elles. Il est désactivé et `split_batch` est conservé."),
    ES("Aviso: `split_batch` y `use_fused_proj_bwd_optim` están activados los "
       "dos, pero aquí un lote puede tener varias cámaras ({0} como máximo), y "
       "`use_fused_proj_bwd_optim` no sabe acumular gradientes entre ellas. Se "
       "desactiva y se mantiene `split_batch`."),
    PT("Aviso: `split_batch` e `use_fused_proj_bwd_optim` estão ambos ligados, "
       "mas aqui um lote pode ter mais de uma câmera (no máximo {0}), e "
       "`use_fused_proj_bwd_optim` não acumula gradientes entre elas. Ele é "
       "desligado e `split_batch` fica."),
    IT("Avviso: `split_batch` e `use_fused_proj_bwd_optim` sono entrambi "
       "attivi, ma qui un batch può contenere più camere (al massimo {0}), e "
       "`use_fused_proj_bwd_optim` non sa accumulare gradienti fra di esse. "
       "Viene disattivato e resta `split_batch`."),
    NL("Waarschuwing: `split_batch` en `use_fused_proj_bwd_optim` staan allebei "
       "aan, maar een batch kan hier meer dan één camera bevatten (hoogstens "
       "{0}), en daarover kan `use_fused_proj_bwd_optim` geen gradiënten "
       "optellen. Het gaat uit en `split_batch` blijft."),
    RU("Предупреждение: включены и `split_batch`, и "
       "`use_fused_proj_bwd_optim`, но пакет здесь может содержать несколько "
       "камер (не больше {0}), а по ним `use_fused_proj_bwd_optim` не умеет "
       "накапливать градиенты. Он выключается, `split_batch` остаётся."),
    TR("Uyarı: `split_batch` ile `use_fused_proj_bwd_optim` birlikte açık ama "
       "burada bir yığında birden çok kamera olabiliyor (en fazla {0}) ve "
       "`use_fused_proj_bwd_optim` bunlar arasında gradyan biriktiremiyor. "
       "Kapatıldı, `split_batch` korunuyor."));

SS_MSG(warn_diverged_loss,
    EN("warning: training diverged at step {0} (`{1}` = {2}). It will not "
       "recover, and the result will render black. Please report it with the "
       "run's config.json."),
    JA("警告: ステップ {0} で学習が発散しました（`{1}` = {2}）。これは元に戻らず、"
       "結果は真っ黒に描画されます。実行の config.json を添えて報告してください。"),
    ZH_HANS("警告：训练在第 {0} 步发散（`{1}` = {2}）。它无法恢复，结果会渲染成"
            "全黑。请附上本次运行的 config.json 报告此问题。"),
    ZH_HANT("警告：訓練在第 {0} 步發散（`{1}` = {2}）。它無法恢復，結果會算繪成"
            "全黑。請附上本次執行的 config.json 回報此問題。"),
    KO("경고: {0}단계에서 학습이 발산했습니다(`{1}` = {2}). 회복되지 않으며 "
       "결과는 검게 렌더링됩니다. 실행의 config.json과 함께 보고해 주세요."),
    DE("Warnung: Das Training ist in Schritt {0} divergiert (`{1}` = {2}). Es "
       "erholt sich nicht, und das Ergebnis wird schwarz gerendert. Bitte mit "
       "der config.json des Laufs melden."),
    FR("Avertissement : l'entraînement a divergé à l'étape {0} (`{1}` = {2}). "
       "Il ne s'en remettra pas et le résultat s'affichera en noir. Merci de "
       "le signaler avec le config.json de l'exécution."),
    ES("Aviso: el entrenamiento divergió en el paso {0} (`{1}` = {2}). No se "
       "recuperará y el resultado se verá negro. Comunícalo junto con el "
       "config.json de la ejecución."),
    PT("Aviso: o treino divergiu no passo {0} (`{1}` = {2}). Não se recupera "
       "e o resultado será renderizado preto. Relate o caso junto com o "
       "config.json da execução."),
    IT("Avviso: l'addestramento è divergito al passo {0} (`{1}` = {2}). Non "
       "si riprende e il risultato verrà reso nero. Segnalalo insieme al "
       "config.json dell'esecuzione."),
    NL("Waarschuwing: de training is bij stap {0} gedivergeerd (`{1}` = {2}). "
       "Het herstelt niet en het resultaat wordt zwart weergegeven. Meld het "
       "met de config.json van de run."),
    RU("Предупреждение: на шаге {0} обучение разошлось (`{1}` = {2}). Оно не "
       "восстановится, а результат отрисуется чёрным. Сообщите об этом, "
       "приложив config.json запуска."),
    TR("Uyarı: eğitim {0}. adımda ıraksadı (`{1}` = {2}). Toparlanmaz ve "
       "sonuç siyah görüntülenir. Lütfen çalışmanın config.json dosyasıyla "
       "birlikte bildirin."));


// ===========================================================================
// Progress while a long pass runs
// ===========================================================================
// "how long is this going to take" is the only question a user has during a
// twenty-minute masking pass, so the line answers it. Each shape is a whole
// sentence rather than a stem with clauses glued on: the rate and the estimate
// land in different places in a verb-final language, and "about ... left"
// cannot be appended to a Japanese noun phrase and still parse.

SS_MSG(prog_count,
    EN("  {0}: {1}"),
    JA("  {0}: {1}"),
    ZH_HANS("  {0}：{1}"),
    ZH_HANT("  {0}：{1}"),
    KO("  {0}: {1}"),
    DE("  {0}: {1}"),
    FR("  {0} : {1}"),
    ES("  {0}: {1}"),
    PT("  {0}: {1}"),
    IT("  {0}: {1}"),
    NL("  {0}: {1}"),
    RU("  {0}: {1}"),
    TR("  {0}: {1}"));

SS_MSG(prog_count_total,
    EN("  {0}: {1} / {2}"),
    JA("  {0}: {1} / {2}"),
    ZH_HANS("  {0}：{1} / {2}"),
    ZH_HANT("  {0}：{1} / {2}"),
    KO("  {0}: {1} / {2}"),
    DE("  {0}: {1} / {2}"),
    FR("  {0} : {1} / {2}"),
    ES("  {0}: {1} / {2}"),
    PT("  {0}: {1} / {2}"),
    IT("  {0}: {1} / {2}"),
    NL("  {0}: {1} / {2}"),
    RU("  {0}: {1} / {2}"),
    TR("  {0}: {1} / {2}"));

SS_MSG(prog_count_rate,
    EN("  {0}: {1}  ({2})"),
    JA("  {0}: {1}（{2}）"),
    ZH_HANS("  {0}：{1}（{2}）"),
    ZH_HANT("  {0}：{1}（{2}）"),
    KO("  {0}: {1}({2})"),
    DE("  {0}: {1}  ({2})"),
    FR("  {0} : {1}  ({2})"),
    ES("  {0}: {1}  ({2})"),
    PT("  {0}: {1}  ({2})"),
    IT("  {0}: {1}  ({2})"),
    NL("  {0}: {1}  ({2})"),
    RU("  {0}: {1}  ({2})"),
    TR("  {0}: {1}  ({2})"));

SS_MSG(prog_count_total_rate,
    EN("  {0}: {1} / {2}  ({3})"),
    JA("  {0}: {1} / {2}（{3}）"),
    ZH_HANS("  {0}：{1} / {2}（{3}）"),
    ZH_HANT("  {0}：{1} / {2}（{3}）"),
    KO("  {0}: {1} / {2}({3})"),
    DE("  {0}: {1} / {2}  ({3})"),
    FR("  {0} : {1} / {2}  ({3})"),
    ES("  {0}: {1} / {2}  ({3})"),
    PT("  {0}: {1} / {2}  ({3})"),
    IT("  {0}: {1} / {2}  ({3})"),
    NL("  {0}: {1} / {2}  ({3})"),
    RU("  {0}: {1} / {2}  ({3})"),
    TR("  {0}: {1} / {2}  ({3})"));

SS_MSG(prog_count_total_rate_eta,
    EN("  {0}: {1} / {2}  ({3}, about {4} left)"),
    JA("  {0}: {1} / {2}（{3}、残り約 {4}）"),
    ZH_HANS("  {0}：{1} / {2}（{3}，大约还剩 {4}）"),
    ZH_HANT("  {0}：{1} / {2}（{3}，大約還剩 {4}）"),
    KO("  {0}: {1} / {2}({3}, 약 {4} 남음)"),
    DE("  {0}: {1} / {2}  ({3}, noch etwa {4})"),
    FR("  {0} : {1} / {2}  ({3}, encore {4} environ)"),
    ES("  {0}: {1} / {2}  ({3}, quedan unos {4})"),
    PT("  {0}: {1} / {2}  ({3}, faltam cerca de {4})"),
    IT("  {0}: {1} / {2}  ({3}, mancano circa {4})"),
    NL("  {0}: {1} / {2}  ({3}, nog ongeveer {4})"),
    RU("  {0}: {1} / {2}  ({3}, осталось около {4})"),
    TR("  {0}: {1} / {2}  ({3}, yaklaşık {4} kaldı)"));

SS_MSG(rate_each,
    EN("{0} s each"),
    JA("1 件あたり {0} 秒"),
    ZH_HANS("每个 {0} 秒"),
    ZH_HANT("每個 {0} 秒"),
    KO("개당 {0}초"),
    DE("{0} s je Stück"),
    FR("{0} s chacun"),
    ES("{0} s cada uno"),
    PT("{0} s cada"),
    IT("{0} s ciascuno"),
    NL("{0} s per stuk"),
    RU("по {0} с"),
    TR("her biri {0} sn"));

SS_MSG(rate_per_second,
    EN("{0} per second"),
    JA("毎秒 {0} 件"),
    ZH_HANS("每秒 {0} 个"),
    ZH_HANT("每秒 {0} 個"),
    KO("초당 {0}개"),
    DE("{0} je Sekunde"),
    FR("{0} par seconde"),
    ES("{0} por segundo"),
    PT("{0} por segundo"),
    IT("{0} al secondo"),
    NL("{0} per seconde"),
    RU("{0} в секунду"),
    TR("saniyede {0}"));

SS_MSG(dur_moment,
    EN("a moment"),
    JA("わずか"),
    ZH_HANS("一会儿"),
    ZH_HANT("一會兒"),
    KO("잠깐"),
    DE("einen Augenblick"),
    FR("un instant"),
    ES("un momento"),
    PT("um instante"),
    IT("un istante"),
    NL("een ogenblik"),
    RU("мгновение"),
    TR("bir an"));

SS_MSG(dur_seconds,
    EN("{0} s"),
    JA("{0} 秒"),
    ZH_HANS("{0} 秒"),
    ZH_HANT("{0} 秒"),
    KO("{0}초"),
    DE("{0} s"),
    FR("{0} s"),
    ES("{0} s"),
    PT("{0} s"),
    IT("{0} s"),
    NL("{0} s"),
    RU("{0} с"),
    TR("{0} sn"));

SS_MSG(dur_minutes,
    EN("{0} min"),
    JA("{0} 分"),
    ZH_HANS("{0} 分钟"),
    ZH_HANT("{0} 分鐘"),
    KO("{0}분"),
    DE("{0} min"),
    FR("{0} min"),
    ES("{0} min"),
    PT("{0} min"),
    IT("{0} min"),
    NL("{0} min"),
    RU("{0} мин"),
    TR("{0} dk"));

SS_MSG(dur_hours,
    EN("{0} h"),
    JA("{0} 時間"),
    ZH_HANS("{0} 小时"),
    ZH_HANT("{0} 小時"),
    KO("{0}시간"),
    DE("{0} h"),
    FR("{0} h"),
    ES("{0} h"),
    PT("{0} h"),
    IT("{0} h"),
    NL("{0} u"),
    RU("{0} ч"),
    TR("{0} sa"));

// What is being counted. A label, so no plural agreement is needed.
SS_MSG(noun_frames_written,
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

SS_MSG(noun_frames_written_masked,
    EN("frames written and masked"),
    JA("書き出してマスクしたフレーム"),
    ZH_HANS("已写出并遮罩的帧"),
    ZH_HANT("已寫出並遮罩的影格"),
    KO("저장하고 마스크한 프레임"),
    DE("geschriebene und maskierte Einzelbilder"),
    FR("images écrites et masquées"),
    ES("fotogramas escritos y enmascarados"),
    PT("quadros escritos e mascarados"),
    IT("fotogrammi scritti e mascherati"),
    NL("geschreven en gemaskeerde beelden"),
    RU("записано и замаскировано кадров"),
    TR("yazılan ve maskelenen kare"));

SS_MSG(noun_photos_collected,
    EN("photos collected"),
    JA("集めた写真"),
    ZH_HANS("已收集的照片"),
    ZH_HANT("已收集的照片"),
    KO("모은 사진"),
    DE("gesammelte Fotos"),
    FR("photos rassemblées"),
    ES("fotos recopiladas"),
    PT("fotos reunidas"),
    IT("foto raccolte"),
    NL("verzamelde foto's"),
    RU("собрано фотографий"),
    TR("toplanan fotoğraf"));

SS_MSG(noun_masks_collected,
    EN("masks collected"),
    JA("集めたマスク"),
    ZH_HANS("已收集的掩码"),
    ZH_HANT("已收集的遮罩"),
    KO("모은 마스크"),
    DE("gesammelte Masken"),
    FR("masques rassemblés"),
    ES("máscaras recopiladas"),
    PT("máscaras reunidas"),
    IT("maschere raccolte"),
    NL("verzamelde maskers"),
    RU("собрано масок"),
    TR("toplanan maske"));

SS_MSG(noun_images_masked,
    EN("images masked"),
    JA("マスクした画像"),
    ZH_HANS("已遮罩的图像"),
    ZH_HANT("已遮罩的影像"),
    KO("마스크한 이미지"),
    DE("maskierte Bilder"),
    FR("images masquées"),
    ES("imágenes enmascaradas"),
    PT("imagens mascaradas"),
    IT("immagini mascherate"),
    NL("gemaskeerde beelden"),
    RU("замаскировано изображений"),
    TR("maskelenen görüntü"));

SS_MSG(copying_photos,
    EN("photos: {0} -> {1}"),
    JA("写真: {0} -> {1}"),
    ZH_HANS("照片：{0} -> {1}"),
    ZH_HANT("照片：{0} -> {1}"),
    KO("사진: {0} -> {1}"),
    DE("Fotos: {0} -> {1}"),
    FR("photos : {0} -> {1}"),
    ES("fotos: {0} -> {1}"),
    PT("fotos: {0} -> {1}"),
    IT("foto: {0} -> {1}"),
    NL("foto's: {0} -> {1}"),
    RU("фотографии: {0} -> {1}"),
    TR("fotoğraflar: {0} -> {1}"));

SS_MSG(copying_masks,
    EN("masks: {0} -> {1}"),
    JA("マスク: {0} -> {1}"),
    ZH_HANS("掩码：{0} -> {1}"),
    ZH_HANT("遮罩：{0} -> {1}"),
    KO("마스크: {0} -> {1}"),
    DE("Masken: {0} -> {1}"),
    FR("masques : {0} -> {1}"),
    ES("máscaras: {0} -> {1}"),
    PT("máscaras: {0} -> {1}"),
    IT("maschere: {0} -> {1}"),
    NL("maskers: {0} -> {1}"),
    RU("маски: {0} -> {1}"),
    TR("maskeler: {0} -> {1}"));

SS_MSG(download_percent_of,
    EN("{0}% of {1}"),
    JA("{1} のうち {0}%"),
    ZH_HANS("{0}%，共 {1}"),
    ZH_HANT("{0}%，共 {1}"),
    KO("{1} 중 {0}%"),
    DE("{0} % von {1}"),
    FR("{0} % de {1}"),
    ES("{0} % de {1}"),
    PT("{0}% de {1}"),
    IT("{0}% di {1}"),
    NL("{0}% van {1}"),
    RU("{0} % из {1}"),
    TR("{1} içinden %{0}"));

SS_MSG(colmap_reproj_error,
    EN("Mean reprojection error: {0} px -> {1} px"),
    JA("平均再投影誤差: {0} px -> {1} px"),
    ZH_HANS("平均重投影误差：{0} px -> {1} px"),
    ZH_HANT("平均重投影誤差：{0} px -> {1} px"),
    KO("평균 재투영 오차: {0} px -> {1} px"),
    DE("mittlerer Rückprojektionsfehler: {0} px -> {1} px"),
    FR("erreur de reprojection moyenne : {0} px -> {1} px"),
    ES("error medio de reproyección: {0} px -> {1} px"),
    PT("erro médio de reprojeção: {0} px -> {1} px"),
    IT("errore medio di riproiezione: {0} px -> {1} px"),
    NL("gemiddelde herprojectiefout: {0} px -> {1} px"),
    RU("средняя ошибка репроекции: {0} px -> {1} px"),
    TR("ortalama yeniden izdüşüm hatası: {0} px -> {1} px"));

// ===========================================================================
// What went wrong
// ===========================================================================

SS_MSG(err_nothing_to_prepare,
    EN("Nothing to prepare: no video and no photo folder was picked."),
    JA("準備するものがありません。動画も写真フォルダーも選ばれていません。"),
    ZH_HANS("没有可准备的内容：既没有选视频，也没有选照片文件夹。"),
    ZH_HANT("沒有可準備的內容：既沒有選影片，也沒有選照片資料夾。"),
    KO("준비할 것이 없습니다. 동영상도 사진 폴더도 고르지 않았습니다."),
    DE("Es gibt nichts vorzubereiten: weder ein Video noch ein Fotoordner wurde "
       "gewählt."),
    FR("Rien à préparer : ni vidéo ni dossier de photos n'a été choisi."),
    ES("No hay nada que preparar: no se eligió ni un vídeo ni una carpeta de "
       "fotos."),
    PT("Não há nada a preparar: nem um vídeo nem uma pasta de fotos foi "
       "escolhida."),
    IT("Non c'è nulla da preparare: non è stato scelto né un video né una "
       "cartella di foto."),
    NL("Er valt niets voor te bereiden: er is geen video en geen fotomap "
       "gekozen."),
    RU("Готовить нечего: не выбраны ни видео, ни папка с фотографиями."),
    TR("Hazırlanacak bir şey yok: ne video ne de fotoğraf klasörü seçildi."));

SS_MSG(err_too_few_images,
    EN("At least 3 images are needed, and this has {0}."),
    JA("画像は最低 3 枚必要ですが、{0} 枚しかありません。"),
    ZH_HANS("至少需要 3 张图像，这里只有 {0} 张。"),
    ZH_HANT("至少需要 3 張影像，這裡只有 {0} 張。"),
    KO("이미지가 최소 3장 필요한데 {0}장뿐입니다."),
    DE("Es werden mindestens 3 Bilder gebraucht, hier sind es {0}."),
    FR("Il faut au moins 3 images, et il y en a {0}."),
    ES("Hacen falta al menos 3 imágenes, y aquí hay {0}."),
    PT("São necessárias pelo menos 3 imagens, e aqui há {0}."),
    IT("Servono almeno 3 immagini, e qui ce ne sono {0}."),
    NL("Er zijn minstens 3 beelden nodig, en dit zijn er {0}."),
    RU("Нужно не меньше 3 изображений, а здесь их {0}."),
    TR("En az 3 görüntü gerekiyor, burada {0} tane var."));

SS_MSG(err_mask_no_target,
    EN("Masking is on, but nothing says what to mask. Type a prompt (\"people; "
       "cars\"), or open \"Try the mask\" and click the object."),
    JA("マスクは有効ですが、何をマスクするかが指定されていません。プロンプトを"
       "入力するか（\"people; cars\" など）、「マスクを試す」を開いて対象を"
       "クリックしてください。"),
    ZH_HANS("遮罩已开启，但没有说明要遮罩什么。请输入提示词（如 \"people; cars\"），"
            "或打开“试一下遮罩”并点击目标。"),
    ZH_HANT("遮罩已開啟，但沒有說明要遮罩什麼。請輸入提示詞（如 \"people; cars\"），"
            "或開啟「試一下遮罩」並點選目標。"),
    KO("마스크가 켜져 있지만 무엇을 마스크할지 정해지지 않았습니다. 프롬프트를 "
       "입력하거나(\"people; cars\") \"마스크 시험\"을 열어 대상을 클릭하세요."),
    DE("Die Maskierung ist an, aber nichts sagt, was maskiert werden soll. "
       "Geben Sie einen Prompt ein (\"people; cars\") oder öffnen Sie \"Maske "
       "ausprobieren\" und klicken Sie das Objekt an."),
    FR("Le masquage est activé, mais rien n'indique quoi masquer. Saisissez une "
       "consigne (\"people; cars\"), ou ouvrez « Essayer le masque » et cliquez "
       "sur l'objet."),
    ES("El enmascarado está activado, pero nada dice qué enmascarar. Escribe una "
       "indicación (\"people; cars\"), o abre «Probar la máscara» y haz clic en "
       "el objeto."),
    PT("A máscara está ligada, mas nada diz o que mascarar. Escreva um comando "
       "(\"people; cars\"), ou abra \"Experimentar a máscara\" e clique no "
       "objeto."),
    IT("La mascheratura è attiva, ma nulla dice cosa mascherare. Scriva un "
       "prompt (\"people; cars\"), oppure apra \"Prova la maschera\" e clicchi "
       "l'oggetto."),
    NL("Maskeren staat aan, maar niets zegt wat er gemaskeerd moet worden. Typ "
       "een prompt (\"people; cars\"), of open \"Masker proberen\" en klik het "
       "object aan."),
    RU("Маскирование включено, но не сказано, что маскировать. Введите запрос "
       "(\"people; cars\") или откройте «Проверить маску» и щёлкните по объекту."),
    TR("Maskeleme açık, ama neyin maskeleneceğini söyleyen bir şey yok. Bir "
       "istem yazın (\"people; cars\") ya da \"Maskeyi dene\" bölümünü açıp "
       "nesneye tıklayın."));

SS_MSG(err_no_frames_extracted,
    EN("No frames came out of the video."),
    JA("動画からフレームが 1 枚も取り出せませんでした。"),
    ZH_HANS("没有从视频中提取到任何帧。"),
    ZH_HANT("沒有從影片中擷取到任何影格。"),
    KO("동영상에서 프레임이 하나도 나오지 않았습니다."),
    DE("Aus dem Video kamen keine Einzelbilder."),
    FR("Aucune image n'est sortie de la vidéo."),
    ES("No salió ningún fotograma del vídeo."),
    PT("Nenhum quadro saiu do vídeo."),
    IT("Dal video non è uscito alcun fotogramma."),
    NL("Er kwamen geen beelden uit de video."),
    RU("Из видео не получилось ни одного кадра."),
    TR("Videodan hiç kare çıkmadı."));

SS_MSG(err_ffmpeg_missing,
    EN("ffmpeg was not found ('{0}'). Install it, set its path under Tool "
       "locations, or build with -DSS_ENABLE_PATENTED=ON to decode in-process."),
    JA("ffmpeg が見つかりません（'{0}'）。インストールするか、「ツールの場所」で"
       "パスを設定するか、-DSS_ENABLE_PATENTED=ON でビルドしてプロセス内で"
       "デコードしてください。"),
    ZH_HANS("找不到 ffmpeg（'{0}'）。请安装它、在“工具位置”中设置其路径，"
            "或以 -DSS_ENABLE_PATENTED=ON 构建以在进程内解码。"),
    ZH_HANT("找不到 ffmpeg（'{0}'）。請安裝它、在「工具位置」中設定其路徑，"
            "或以 -DSS_ENABLE_PATENTED=ON 建置以在行程內解碼。"),
    KO("ffmpeg 을 찾지 못했습니다('{0}'). 설치하거나 \"도구 위치\"에서 경로를 "
       "지정하거나, -DSS_ENABLE_PATENTED=ON 으로 빌드해 프로세스 안에서 "
       "디코딩하세요."),
    DE("ffmpeg wurde nicht gefunden ('{0}'). Installieren Sie es, tragen Sie "
       "seinen Pfad unter Werkzeugpfade ein, oder bauen Sie mit "
       "-DSS_ENABLE_PATENTED=ON, um im Prozess zu dekodieren."),
    FR("ffmpeg est introuvable ('{0}'). Installez-le, indiquez son chemin sous "
       "Emplacements des outils, ou compilez avec -DSS_ENABLE_PATENTED=ON pour "
       "décoder dans le processus."),
    ES("No se encontró ffmpeg ('{0}'). Instálalo, indica su ruta en Ubicaciones "
       "de herramientas, o compila con -DSS_ENABLE_PATENTED=ON para descodificar "
       "en el propio proceso."),
    PT("O ffmpeg não foi encontrado ('{0}'). Instale-o, informe o caminho em "
       "Locais das ferramentas, ou compile com -DSS_ENABLE_PATENTED=ON para "
       "decodificar no próprio processo."),
    IT("ffmpeg non è stato trovato ('{0}'). Lo installi, ne indichi il percorso "
       "in Posizioni degli strumenti, oppure compili con "
       "-DSS_ENABLE_PATENTED=ON per decodificare nel processo."),
    NL("ffmpeg is niet gevonden ('{0}'). Installeer het, geef het pad op onder "
       "Gereedschapslocaties, of bouw met -DSS_ENABLE_PATENTED=ON om in het "
       "proces te decoderen."),
    RU("ffmpeg не найден ('{0}'). Установите его, укажите путь в «Расположение "
       "инструментов» или соберите с -DSS_ENABLE_PATENTED=ON, чтобы "
       "декодировать внутри процесса."),
    TR("ffmpeg bulunamadı ('{0}'). Kurun, yolunu Araç konumları altında "
       "belirtin ya da süreç içinde çözmek için -DSS_ENABLE_PATENTED=ON ile "
       "derleyin."));

SS_MSG(err_ffmpeg_split_failed,
    EN("ffmpeg could not split the tracks (see the log)."),
    JA("ffmpeg がトラックを分割できませんでした（ログを参照）。"),
    ZH_HANS("ffmpeg 无法拆分轨道（详见日志）。"),
    ZH_HANT("ffmpeg 無法拆分軌道（詳見記錄）。"),
    KO("ffmpeg 이 트랙을 나누지 못했습니다(로그 참고)."),
    DE("ffmpeg konnte die Spuren nicht trennen (siehe Protokoll)."),
    FR("ffmpeg n'a pas pu séparer les pistes (voir le journal)."),
    ES("ffmpeg no pudo separar las pistas (mira el registro)."),
    PT("O ffmpeg não conseguiu separar as trilhas (veja o registro)."),
    IT("ffmpeg non è riuscito a separare le tracce (veda il registro)."),
    NL("ffmpeg kon de sporen niet splitsen (zie het logboek)."),
    RU("ffmpeg не смог разделить дорожки (см. журнал)."),
    TR("ffmpeg izleri ayıramadı (günlüğe bakın)."));

SS_MSG(err_ffmpeg_extract_failed,
    EN("ffmpeg could not extract the frames (see the log)."),
    JA("ffmpeg がフレームを取り出せませんでした（ログを参照）。"),
    ZH_HANS("ffmpeg 无法提取帧（详见日志）。"),
    ZH_HANT("ffmpeg 無法擷取影格（詳見記錄）。"),
    KO("ffmpeg 이 프레임을 뽑아내지 못했습니다(로그 참고)."),
    DE("ffmpeg konnte die Einzelbilder nicht entnehmen (siehe Protokoll)."),
    FR("ffmpeg n'a pas pu extraire les images (voir le journal)."),
    ES("ffmpeg no pudo extraer los fotogramas (mira el registro)."),
    PT("O ffmpeg não conseguiu extrair os quadros (veja o registro)."),
    IT("ffmpeg non è riuscito a estrarre i fotogrammi (veda il registro)."),
    NL("ffmpeg kon de beelden niet uitpakken (zie het logboek)."),
    RU("ffmpeg не смог извлечь кадры (см. журнал)."),
    TR("ffmpeg kareleri çıkaramadı (günlüğe bakın)."));

SS_MSG(err_not_a_folder,
    EN("Not a folder: {0}"),
    JA("フォルダーではありません: {0}"),
    ZH_HANS("不是文件夹：{0}"),
    ZH_HANT("不是資料夾：{0}"),
    KO("폴더가 아닙니다: {0}"),
    DE("Kein Ordner: {0}"),
    FR("Ce n'est pas un dossier : {0}"),
    ES("No es una carpeta: {0}"),
    PT("Não é uma pasta: {0}"),
    IT("Non è una cartella: {0}"),
    NL("Geen map: {0}"),
    RU("Это не папка: {0}"),
    TR("Klasör değil: {0}"));

SS_MSG(err_no_photos_in,
    EN("There are no photos in {0}."),
    JA("{0} に写真がありません。"),
    ZH_HANS("{0} 中没有照片。"),
    ZH_HANT("{0} 中沒有照片。"),
    KO("{0} 에 사진이 없습니다."),
    DE("In {0} sind keine Fotos."),
    FR("Il n'y a aucune photo dans {0}."),
    ES("No hay fotos en {0}."),
    PT("Não há fotos em {0}."),
    IT("In {0} non ci sono foto."),
    NL("Er staan geen foto's in {0}."),
    RU("В {0} нет фотографий."),
    TR("{0} içinde fotoğraf yok."));

SS_MSG(err_no_masks_in,
    EN("There are no masks in {0}."),
    JA("{0} にマスクがありません。"),
    ZH_HANS("{0} 中没有掩码。"),
    ZH_HANT("{0} 中沒有遮罩。"),
    KO("{0} 에 마스크가 없습니다."),
    DE("In {0} sind keine Masken."),
    FR("Il n'y a aucun masque dans {0}."),
    ES("No hay máscaras en {0}."),
    PT("Não há máscaras em {0}."),
    IT("In {0} non ci sono maschere."),
    NL("Er staan geen maskers in {0}."),
    RU("В {0} нет масок."),
    TR("{0} içinde maske yok."));

SS_MSG(err_copy_failed,
    EN("{0} could not be put into {1} ({2})."),
    JA("{0} を {1} に入れられませんでした（{2}）。"),
    ZH_HANS("无法把 {0} 放入 {1}（{2}）。"),
    ZH_HANT("無法把 {0} 放入 {1}（{2}）。"),
    KO("{0} 을(를) {1} 에 넣지 못했습니다({2})."),
    DE("{0} konnte nicht nach {1} gebracht werden ({2})."),
    FR("{0} n'a pas pu être placé dans {1} ({2})."),
    ES("No se pudo poner {0} en {1} ({2})."),
    PT("Não foi possível colocar {0} em {1} ({2})."),
    IT("Non è stato possibile mettere {0} in {1} ({2})."),
    NL("{0} kon niet in {1} gezet worden ({2})."),
    RU("Не удалось поместить {0} в {1} ({2})."),
    TR("{0}, {1} içine konulamadı ({2})."));

SS_MSG(err_clicks_need_builtin,
    EN("Clicked objects need the built-in segmentation; the external Python "
       "masker only understands text prompts. Turn off \"external masking\", or "
       "describe the object in words."),
    JA("クリックで選んだ対象には内蔵のセグメンテーションが必要です。外部の Python "
       "マスカーはテキストのプロンプトしか解釈できません。「外部マスク」をオフに"
       "するか、対象を言葉で説明してください。"),
    ZH_HANS("点击选中的目标需要内置分割；外部的 Python 遮罩器只认文本提示。"
            "请关闭“外部遮罩”，或用文字描述目标。"),
    ZH_HANT("點選選中的目標需要內建分割；外部的 Python 遮罩器只認文字提示。"
            "請關閉「外部遮罩」，或用文字描述目標。"),
    KO("클릭으로 고른 대상에는 내장 분할이 필요합니다. 외부 Python 마스커는 텍스트 "
       "프롬프트만 이해합니다. \"외부 마스킹\"을 끄거나 대상을 말로 설명하세요."),
    DE("Angeklickte Objekte brauchen die eingebaute Segmentierung; der externe "
       "Python-Masker versteht nur Textprompts. Schalten Sie \"externe "
       "Maskierung\" ab, oder beschreiben Sie das Objekt in Worten."),
    FR("Les objets cliqués ont besoin de la segmentation intégrée ; le masqueur "
       "Python externe ne comprend que des consignes textuelles. Désactivez le "
       "« masquage externe », ou décrivez l'objet avec des mots."),
    ES("Los objetos señalados con clic necesitan la segmentación integrada; el "
       "enmascarador externo de Python solo entiende indicaciones de texto. "
       "Desactiva el «enmascarado externo», o describe el objeto con palabras."),
    PT("Objetos clicados precisam da segmentação embutida; o mascarador externo "
       "em Python só entende comandos de texto. Desligue a \"máscara externa\", "
       "ou descreva o objeto em palavras."),
    IT("Gli oggetti cliccati richiedono la segmentazione integrata; il "
       "mascheratore Python esterno capisce solo prompt testuali. Disattivi la "
       "\"mascheratura esterna\", oppure descriva l'oggetto a parole."),
    NL("Aangeklikte objecten hebben de ingebouwde segmentatie nodig; de externe "
       "Python-masker begrijpt alleen tekstprompts. Zet \"extern maskeren\" uit, "
       "of beschrijf het object in woorden."),
    RU("Объекты, выбранные щелчком, требуют встроенной сегментации; внешний "
       "маскировщик на Python понимает только текстовые запросы. Отключите "
       "«внешнее маскирование» или опишите объект словами."),
    TR("Tıklanan nesneler yerleşik bölütlemeyi gerektirir; harici Python "
       "maskeleyici yalnızca metin istemlerini anlar. \"Harici maskeleme\"yi "
       "kapatın ya da nesneyi sözle anlatın."));

SS_MSG(err_no_images_to_mask,
    EN("There are no images to mask."),
    JA("マスクする画像がありません。"),
    ZH_HANS("没有可遮罩的图像。"),
    ZH_HANT("沒有可遮罩的影像。"),
    KO("마스크할 이미지가 없습니다."),
    DE("Es gibt keine Bilder zum Maskieren."),
    FR("Il n'y a aucune image à masquer."),
    ES("No hay imágenes que enmascarar."),
    PT("Não há imagens para mascarar."),
    IT("Non ci sono immagini da mascherare."),
    NL("Er zijn geen beelden om te maskeren."),
    RU("Маскировать нечего."),
    TR("Maskelenecek görüntü yok."));

SS_MSG(err_masking_failed_on,
    EN("Masking failed on {0}: {1}"),
    JA("{0} のマスクに失敗しました: {1}"),
    ZH_HANS("对 {0} 的遮罩失败：{1}"),
    ZH_HANT("對 {0} 的遮罩失敗：{1}"),
    KO("{0} 의 마스크에 실패했습니다: {1}"),
    DE("Das Maskieren von {0} ist fehlgeschlagen: {1}"),
    FR("Le masquage de {0} a échoué : {1}"),
    ES("Falló el enmascarado de {0}: {1}"),
    PT("A máscara de {0} falhou: {1}"),
    IT("La mascheratura di {0} non è riuscita: {1}"),
    NL("Het maskeren van {0} is mislukt: {1}"),
    RU("Не удалось замаскировать {0}: {1}"),
    TR("{0} maskelenemedi: {1}"));

SS_MSG(err_python_missing,
    EN("Python was not found ('{0}'). External masking needs Python with the "
       "lang-segment-anything package. Set the Python path under Tool "
       "locations, or use the built-in segmentation."),
    JA("Python が見つかりません（'{0}'）。外部マスクには lang-segment-anything "
       "パッケージを入れた Python が必要です。「ツールの場所」で Python のパスを"
       "設定するか、内蔵のセグメンテーションを使ってください。"),
    ZH_HANS("找不到 Python（'{0}'）。外部遮罩需要装有 lang-segment-anything 包的 "
            "Python。请在“工具位置”中设置 Python 路径，或改用内置分割。"),
    ZH_HANT("找不到 Python（'{0}'）。外部遮罩需要裝有 lang-segment-anything 套件的 "
            "Python。請在「工具位置」中設定 Python 路徑，或改用內建分割。"),
    KO("Python 을 찾지 못했습니다('{0}'). 외부 마스킹에는 lang-segment-anything "
       "패키지가 있는 Python 이 필요합니다. \"도구 위치\"에서 Python 경로를 "
       "지정하거나 내장 분할을 쓰세요."),
    DE("Python wurde nicht gefunden ('{0}'). Externe Maskierung braucht Python "
       "mit dem Paket lang-segment-anything. Tragen Sie den Python-Pfad unter "
       "Werkzeugpfade ein, oder nutzen Sie die eingebaute Segmentierung."),
    FR("Python est introuvable ('{0}'). Le masquage externe a besoin de Python "
       "avec le paquet lang-segment-anything. Indiquez le chemin de Python sous "
       "Emplacements des outils, ou utilisez la segmentation intégrée."),
    ES("No se encontró Python ('{0}'). El enmascarado externo necesita Python "
       "con el paquete lang-segment-anything. Indica la ruta de Python en "
       "Ubicaciones de herramientas, o usa la segmentación integrada."),
    PT("O Python não foi encontrado ('{0}'). A máscara externa precisa de Python "
       "com o pacote lang-segment-anything. Informe o caminho do Python em "
       "Locais das ferramentas, ou use a segmentação embutida."),
    IT("Python non è stato trovato ('{0}'). La mascheratura esterna richiede "
       "Python con il pacchetto lang-segment-anything. Indichi il percorso di "
       "Python in Posizioni degli strumenti, oppure usi la segmentazione "
       "integrata."),
    NL("Python is niet gevonden ('{0}'). Extern maskeren heeft Python met het "
       "pakket lang-segment-anything nodig. Geef het Python-pad op onder "
       "Gereedschapslocaties, of gebruik de ingebouwde segmentatie."),
    RU("Python не найден ('{0}'). Внешнему маскированию нужен Python с пакетом "
       "lang-segment-anything. Укажите путь к Python в «Расположении "
       "инструментов» или используйте встроенную сегментацию."),
    TR("Python bulunamadı ('{0}'). Harici maskeleme, lang-segment-anything "
       "paketinin kurulu olduğu bir Python ister. Python yolunu Araç konumları "
       "altında belirtin ya da yerleşik bölütlemeyi kullanın."));

SS_MSG(err_cannot_write,
    EN("Cannot write {0}."),
    JA("{0} を書き出せません。"),
    ZH_HANS("无法写入 {0}。"),
    ZH_HANT("無法寫入 {0}。"),
    KO("{0} 을(를) 쓸 수 없습니다."),
    DE("{0} kann nicht geschrieben werden."),
    FR("Impossible d'écrire {0}."),
    ES("No se puede escribir {0}."),
    PT("Não é possível escrever {0}."),
    IT("Non è possibile scrivere {0}."),
    NL("{0} kan niet geschreven worden."),
    RU("Не удаётся записать {0}."),
    TR("{0} yazılamıyor."));

SS_MSG(err_mask_generation_failed,
    EN("Mask generation failed."),
    JA("マスクの生成に失敗しました。"),
    ZH_HANS("生成掩码失败。"),
    ZH_HANT("產生遮罩失敗。"),
    KO("마스크 생성에 실패했습니다."),
    DE("Die Maskenerzeugung ist fehlgeschlagen."),
    FR("La génération des masques a échoué."),
    ES("Falló la generación de máscaras."),
    PT("A geração de máscaras falhou."),
    IT("La generazione delle maschere non è riuscita."),
    NL("Het maken van de maskers is mislukt."),
    RU("Не удалось создать маски."),
    TR("Maske üretimi başarısız oldu."));

SS_MSG(err_mask_model_not_downloaded,
    EN("The masking model has not been downloaded yet. Get it under the masking "
       "options -- it is a one-time download -- and start the run again."),
    JA("マスク用のモデルがまだダウンロードされていません。マスクの設定から取得して"
       "ください。ダウンロードは初回だけです。そのうえで、もう一度実行してください。"),
    ZH_HANS("遮罩用的模型还没有下载。请在遮罩选项里获取它，只需下载一次，"
            "然后重新开始运行。"),
    ZH_HANT("遮罩用的模型還沒有下載。請在遮罩選項裡取得它，只需下載一次，"
            "然後重新開始執行。"),
    KO("마스킹에 쓸 모델을 아직 내려받지 않았습니다. 마스크 설정에서 받으세요. "
       "내려받기는 한 번뿐입니다. 그런 다음 다시 실행하세요."),
    DE("Das Maskierungsmodell ist noch nicht heruntergeladen. Holen Sie es "
       "unter den Maskierungsoptionen -- einmalig -- und starten Sie den Lauf "
       "erneut."),
    FR("Le modèle de masquage n'est pas encore téléchargé. Obtenez-le dans les "
       "options de masquage -- c'est un téléchargement unique -- puis relancez "
       "le traitement."),
    ES("El modelo de enmascarado todavía no está descargado. Obténgalo en las "
       "opciones de enmascarado -- es una descarga única -- y vuelva a iniciar "
       "la ejecución."),
    PT("O modelo de máscara ainda não foi baixado. Obtenha-o nas opções de "
       "máscara -- é um download único -- e inicie a execução de novo."),
    IT("Il modello per le maschere non è ancora stato scaricato. Lo ottenga "
       "dalle opzioni delle maschere -- si scarica una volta sola -- e avvii di "
       "nuovo l'elaborazione."),
    NL("Het maskeermodel is nog niet gedownload. Haal het op bij de "
       "maskeeropties -- eenmalig -- en start de verwerking opnieuw."),
    RU("Модель для масок ещё не загружена. Получите её в настройках масок -- "
       "загрузка нужна только один раз -- и запустите обработку снова."),
    TR("Maskeleme modeli henüz indirilmedi. Maskeleme seçeneklerinden getirin "
       "-- bir kez indirilir -- ve işlemi yeniden başlatın."));

SS_MSG(err_geometry_model_not_downloaded,
    EN("The geometry model has not been downloaded yet. Get it under the depth "
       "and normal options -- it is a one-time download -- and try again."),
    JA("ジオメトリのモデルがまだダウンロードされていません。深度と法線の設定から"
       "取得してください。ダウンロードは初回だけです。そのうえで、もう一度お試し"
       "ください。"),
    ZH_HANS("几何模型还没有下载。请在深度与法线选项里获取它，只需下载一次，"
            "然后再试一次。"),
    ZH_HANT("幾何模型還沒有下載。請在深度與法線選項裡取得它，只需下載一次，"
            "然後再試一次。"),
    KO("기하 모델을 아직 내려받지 않았습니다. 깊이와 법선 설정에서 받으세요. "
       "내려받기는 한 번뿐입니다. 그런 다음 다시 시도하세요."),
    DE("Das Geometriemodell ist noch nicht heruntergeladen. Holen Sie es unter "
       "den Tiefen- und Normalenoptionen -- einmalig -- und versuchen Sie es "
       "erneut."),
    FR("Le modèle de géométrie n'est pas encore téléchargé. Obtenez-le dans les "
       "options de profondeur et de normales -- c'est un téléchargement "
       "unique -- puis réessayez."),
    ES("El modelo de geometría todavía no está descargado. Obténgalo en las "
       "opciones de profundidad y normales -- es una descarga única -- e "
       "inténtelo de nuevo."),
    PT("O modelo de geometria ainda não foi baixado. Obtenha-o nas opções de "
       "profundidade e normais -- é um download único -- e tente de novo."),
    IT("Il modello di geometria non è ancora stato scaricato. Lo ottenga dalle "
       "opzioni di profondità e normali -- si scarica una volta sola -- e "
       "riprovi."),
    NL("Het geometriemodel is nog niet gedownload. Haal het op bij de opties "
       "voor diepte en normalen -- eenmalig -- en probeer het opnieuw."),
    RU("Модель геометрии ещё не загружена. Получите её в настройках глубины и "
       "нормалей -- загрузка нужна только один раз -- и попробуйте снова."),
    TR("Geometri modeli henüz indirilmedi. Derinlik ve normal seçeneklerinden "
       "getirin -- bir kez indirilir -- ve yeniden deneyin."));

SS_MSG(err_mask_missing_packages,
    EN("Mask generation failed: the Python packages are missing. Install "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, which needs CUDA PyTorch)."),
    JA("マスクの生成に失敗しました。Python パッケージが足りません。"
       "lang-segment-anything を入れてください（pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything。CUDA 版 PyTorch が必要です）。"),
    ZH_HANS("生成掩码失败：缺少 Python 包。请安装 lang-segment-anything"
            "（pip install git+https://github.com/luca-medeiros/lang-segment-anything，"
            "需要 CUDA 版 PyTorch）。"),
    ZH_HANT("產生遮罩失敗：缺少 Python 套件。請安裝 lang-segment-anything"
            "（pip install git+https://github.com/luca-medeiros/lang-segment-anything，"
            "需要 CUDA 版 PyTorch）。"),
    KO("마스크 생성에 실패했습니다: Python 패키지가 없습니다. "
       "lang-segment-anything 을 설치하세요(pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, CUDA PyTorch 필요)."),
    DE("Die Maskenerzeugung ist fehlgeschlagen: die Python-Pakete fehlen. "
       "Installieren Sie lang-segment-anything (pip install git+https://"
       "github.com/luca-medeiros/lang-segment-anything, braucht CUDA-PyTorch)."),
    FR("La génération des masques a échoué : les paquets Python manquent. "
       "Installez lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, qui a besoin de PyTorch CUDA)."),
    ES("Falló la generación de máscaras: faltan los paquetes de Python. Instala "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, que necesita PyTorch con CUDA)."),
    PT("A geração de máscaras falhou: faltam os pacotes de Python. Instale o "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, que precisa de PyTorch com CUDA)."),
    IT("La generazione delle maschere non è riuscita: mancano i pacchetti "
       "Python. Installi lang-segment-anything (pip install git+https://"
       "github.com/luca-medeiros/lang-segment-anything, richiede PyTorch CUDA)."),
    NL("Het maken van de maskers is mislukt: de Python-pakketten ontbreken. "
       "Installeer lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, met CUDA-PyTorch)."),
    RU("Не удалось создать маски: отсутствуют пакеты Python. Установите "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, нужен PyTorch с CUDA)."),
    TR("Maske üretimi başarısız oldu: Python paketleri eksik. "
       "lang-segment-anything kurun (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, CUDA'lı PyTorch ister)."));

SS_MSG(err_mask_missing_packages_sam3,
    EN("Mask generation failed: the Python packages are missing. Install "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, which needs CUDA PyTorch), or for "
       "SAM 3: https://github.com/facebookresearch/sam3"),
    JA("マスクの生成に失敗しました。Python パッケージが足りません。"
       "lang-segment-anything を入れてください（pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything。CUDA 版 PyTorch が必要です）。"
       "SAM 3 の場合は https://github.com/facebookresearch/sam3 を参照。"),
    ZH_HANS("生成掩码失败：缺少 Python 包。请安装 lang-segment-anything"
            "（pip install git+https://github.com/luca-medeiros/lang-segment-anything，"
            "需要 CUDA 版 PyTorch）；若用 SAM 3，见 "
            "https://github.com/facebookresearch/sam3"),
    ZH_HANT("產生遮罩失敗：缺少 Python 套件。請安裝 lang-segment-anything"
            "（pip install git+https://github.com/luca-medeiros/lang-segment-anything，"
            "需要 CUDA 版 PyTorch）；若用 SAM 3，見 "
            "https://github.com/facebookresearch/sam3"),
    KO("마스크 생성에 실패했습니다: Python 패키지가 없습니다. "
       "lang-segment-anything 을 설치하세요(pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, CUDA PyTorch 필요). SAM 3 은 "
       "https://github.com/facebookresearch/sam3 을 보세요."),
    DE("Die Maskenerzeugung ist fehlgeschlagen: die Python-Pakete fehlen. "
       "Installieren Sie lang-segment-anything (pip install git+https://"
       "github.com/luca-medeiros/lang-segment-anything, braucht CUDA-PyTorch), "
       "oder für SAM 3: https://github.com/facebookresearch/sam3"),
    FR("La génération des masques a échoué : les paquets Python manquent. "
       "Installez lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, qui a besoin de PyTorch CUDA), ou "
       "pour SAM 3 : https://github.com/facebookresearch/sam3"),
    ES("Falló la generación de máscaras: faltan los paquetes de Python. Instala "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, que necesita PyTorch con CUDA), o "
       "para SAM 3: https://github.com/facebookresearch/sam3"),
    PT("A geração de máscaras falhou: faltam os pacotes de Python. Instale o "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, que precisa de PyTorch com CUDA), "
       "ou para o SAM 3: https://github.com/facebookresearch/sam3"),
    IT("La generazione delle maschere non è riuscita: mancano i pacchetti "
       "Python. Installi lang-segment-anything (pip install git+https://"
       "github.com/luca-medeiros/lang-segment-anything, richiede PyTorch CUDA), "
       "oppure per SAM 3: https://github.com/facebookresearch/sam3"),
    NL("Het maken van de maskers is mislukt: de Python-pakketten ontbreken. "
       "Installeer lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, met CUDA-PyTorch), of voor SAM 3: "
       "https://github.com/facebookresearch/sam3"),
    RU("Не удалось создать маски: отсутствуют пакеты Python. Установите "
       "lang-segment-anything (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, нужен PyTorch с CUDA), либо для "
       "SAM 3: https://github.com/facebookresearch/sam3"),
    TR("Maske üretimi başarısız oldu: Python paketleri eksik. "
       "lang-segment-anything kurun (pip install git+https://github.com/"
       "luca-medeiros/lang-segment-anything, CUDA'lı PyTorch ister) ya da SAM 3 "
       "için: https://github.com/facebookresearch/sam3"));

SS_MSG(err_no_builtin_segmentation,
    EN("This build has no built-in segmentation (-DSS_BUILD_SAM=OFF)."),
    JA("このビルドには内蔵のセグメンテーションがありません（-DSS_BUILD_SAM=OFF）。"),
    ZH_HANS("本版本没有内置分割（-DSS_BUILD_SAM=OFF）。"),
    ZH_HANT("本版本沒有內建分割（-DSS_BUILD_SAM=OFF）。"),
    KO("이 빌드에는 내장 분할이 없습니다(-DSS_BUILD_SAM=OFF)."),
    DE("Dieser Build hat keine eingebaute Segmentierung (-DSS_BUILD_SAM=OFF)."),
    FR("Cette version n'a pas de segmentation intégrée (-DSS_BUILD_SAM=OFF)."),
    ES("Esta compilación no tiene segmentación integrada (-DSS_BUILD_SAM=OFF)."),
    PT("Esta compilação não tem segmentação embutida (-DSS_BUILD_SAM=OFF)."),
    IT("Questa build non ha la segmentazione integrata (-DSS_BUILD_SAM=OFF)."),
    NL("Deze build heeft geen ingebouwde segmentatie (-DSS_BUILD_SAM=OFF)."),
    RU("В этой сборке нет встроенной сегментации (-DSS_BUILD_SAM=OFF)."),
    TR("Bu derlemede yerleşik bölütleme yok (-DSS_BUILD_SAM=OFF)."));

SS_MSG(err_no_model_selected,
    EN("No model is selected -- download one first."),
    JA("モデルが選ばれていません。まずダウンロードしてください。"),
    ZH_HANS("没有选择模型 —— 请先下载一个。"),
    ZH_HANT("沒有選擇模型 —— 請先下載一個。"),
    KO("모델이 선택되지 않았습니다 -- 먼저 내려받으세요."),
    DE("Es ist kein Modell gewählt -- laden Sie zuerst eines herunter."),
    FR("Aucun modèle n'est choisi -- téléchargez-en un d'abord."),
    ES("No hay ningún modelo elegido: descarga uno primero."),
    PT("Nenhum modelo está escolhido -- baixe um primeiro."),
    IT("Non è scelto alcun modello -- ne scarichi prima uno."),
    NL("Er is geen model gekozen -- download er eerst een."),
    RU("Модель не выбрана -- сначала загрузите её."),
    TR("Seçili model yok -- önce bir tane indirin."));

SS_MSG(err_prompt_matched_nothing,
    EN("Nothing on this frame matched the prompt."),
    JA("このフレームにはプロンプトに合うものがありませんでした。"),
    ZH_HANS("这一帧上没有与提示词匹配的内容。"),
    ZH_HANT("這一格上沒有與提示詞相符的內容。"),
    KO("이 프레임에서 프롬프트에 맞는 것이 없습니다."),
    DE("Auf diesem Bild passte nichts zum Prompt."),
    FR("Rien sur cette image ne correspond à la consigne."),
    ES("Nada de este fotograma coincidió con la indicación."),
    PT("Nada neste quadro correspondeu ao comando."),
    IT("In questo fotogramma nulla corrisponde al prompt."),
    NL("Niets op dit beeld kwam overeen met de prompt."),
    RU("На этом кадре ничего не подошло под запрос."),
    TR("Bu karede isteme uyan bir şey çıkmadı."));

SS_MSG(err_clicks_matched_nothing,
    EN("The clicks on this frame did not select anything."),
    JA("このフレームでのクリックは何も選びませんでした。"),
    ZH_HANS("这一帧上的点击没有选中任何东西。"),
    ZH_HANT("這一格上的點選沒有選中任何東西。"),
    KO("이 프레임에서 한 클릭이 아무것도 고르지 못했습니다."),
    DE("Die Klicks auf diesem Bild haben nichts ausgewählt."),
    FR("Les clics sur cette image n'ont rien sélectionné."),
    ES("Los clics en este fotograma no seleccionaron nada."),
    PT("Os cliques neste quadro não selecionaram nada."),
    IT("I clic su questo fotogramma non hanno selezionato nulla."),
    NL("De klikken op dit beeld hebben niets geselecteerd."),
    RU("Щелчки на этом кадре ничего не выделили."),
    TR("Bu karedeki tıklamalar hiçbir şey seçmedi."));

SS_MSG(err_preview_unavailable,
    EN("The preview renderer is unavailable (OpenGL 3.2 is required)."),
    JA("プレビューの描画が使えません（OpenGL 3.2 が必要です）。"),
    ZH_HANS("预览渲染器不可用（需要 OpenGL 3.2）。"),
    ZH_HANT("預覽算繪器不可用（需要 OpenGL 3.2）。"),
    KO("미리보기 렌더러를 쓸 수 없습니다(OpenGL 3.2 필요)."),
    DE("Die Vorschau-Darstellung steht nicht zur Verfügung (OpenGL 3.2 wird "
       "gebraucht)."),
    FR("Le rendu d'aperçu n'est pas disponible (OpenGL 3.2 est requis)."),
    ES("El renderizador de vista previa no está disponible (hace falta OpenGL "
       "3.2)."),
    PT("O renderizador de pré-visualização não está disponível (é preciso "
       "OpenGL 3.2)."),
    IT("Il rendering dell'anteprima non è disponibile (serve OpenGL 3.2)."),
    NL("De voorbeeldweergave is niet beschikbaar (OpenGL 3.2 is vereist)."),
    RU("Просмотр недоступен (нужен OpenGL 3.2)."),
    TR("Önizleme işleyicisi kullanılamıyor (OpenGL 3.2 gerekiyor)."));

SS_MSG(err_unknown_preset,
    EN("Unknown preset: {0}"),
    JA("知らないプリセットです: {0}"),
    ZH_HANS("未知的预设：{0}"),
    ZH_HANT("未知的預設：{0}"),
    KO("모르는 프리셋입니다: {0}"),
    DE("Unbekannte Voreinstellung: {0}"),
    FR("Préréglage inconnu : {0}"),
    ES("Ajuste preestablecido desconocido: {0}"),
    PT("Predefinição desconhecida: {0}"),
    IT("Preimpostazione sconosciuta: {0}"),
    NL("Onbekende voorinstelling: {0}"),
    RU("Неизвестная предустановка: {0}"),
    TR("Bilinmeyen ön ayar: {0}"));

SS_MSG(err_bad_flag_value,
    EN("{0}: '{1}' is not a value this accepts."),
    JA("{0}: '{1}' は受け付けられない値です。"),
    ZH_HANS("{0}：'{1}' 不是可接受的值。"),
    ZH_HANT("{0}：'{1}' 不是可接受的值。"),
    KO("{0}: '{1}' 은(는) 받을 수 없는 값입니다."),
    DE("{0}: '{1}' ist kein Wert, den das annimmt."),
    FR("{0} : « {1} » n'est pas une valeur acceptée."),
    ES("{0}: «{1}» no es un valor admitido."),
    PT("{0}: '{1}' não é um valor aceito."),
    IT("{0}: '{1}' non è un valore accettato."),
    NL("{0}: '{1}' is geen waarde die hier kan."),
    RU("{0}: «{1}» -- недопустимое значение."),
    TR("{0}: '{1}' kabul edilen bir değer değil."));

SS_MSG(err_glfw_init,
    EN("GLFW could not start. Is a display available?"),
    JA("GLFW を起動できませんでした。ディスプレイはありますか。"),
    ZH_HANS("GLFW 无法启动。是否有可用的显示设备？"),
    ZH_HANT("GLFW 無法啟動。是否有可用的顯示裝置？"),
    KO("GLFW 를 시작하지 못했습니다. 디스플레이가 있습니까?"),
    DE("GLFW konnte nicht starten. Gibt es eine Anzeige?"),
    FR("GLFW n'a pas pu démarrer. Y a-t-il un affichage ?"),
    ES("GLFW no pudo arrancar. ¿Hay alguna pantalla disponible?"),
    PT("O GLFW não conseguiu iniciar. Há algum monitor disponível?"),
    IT("GLFW non è riuscito ad avviarsi. C'è uno schermo disponibile?"),
    NL("GLFW kon niet starten. Is er een scherm beschikbaar?"),
    RU("GLFW не запустился. Есть ли доступный дисплей?"),
    TR("GLFW başlatılamadı. Kullanılabilir bir ekran var mı?"));

SS_MSG(err_window_create,
    EN("The window or the GL context could not be created."),
    JA("ウィンドウまたは GL コンテキストを作成できませんでした。"),
    ZH_HANS("无法创建窗口或 GL 上下文。"),
    ZH_HANT("無法建立視窗或 GL 內容。"),
    KO("창이나 GL 컨텍스트를 만들지 못했습니다."),
    DE("Das Fenster oder der GL-Kontext ließ sich nicht anlegen."),
    FR("La fenêtre ou le contexte GL n'a pas pu être créé."),
    ES("No se pudo crear la ventana o el contexto de GL."),
    PT("Não foi possível criar a janela ou o contexto GL."),
    IT("Non è stato possibile creare la finestra o il contesto GL."),
    NL("Het venster of de GL-context kon niet gemaakt worden."),
    RU("Не удалось создать окно или контекст GL."),
    TR("Pencere ya da GL bağlamı oluşturulamadı."));

SS_MSG(warn_font_unreadable,
    EN("{0} could not be read."),
    JA("{0} を読めませんでした。"),
    ZH_HANS("无法读取 {0}。"),
    ZH_HANT("無法讀取 {0}。"),
    KO("{0} 을(를) 읽지 못했습니다."),
    DE("{0} konnte nicht gelesen werden."),
    FR("{0} n'a pas pu être lu."),
    ES("No se pudo leer {0}."),
    PT("Não foi possível ler {0}."),
    IT("Non è stato possibile leggere {0}."),
    NL("{0} kon niet gelezen worden."),
    RU("Не удалось прочитать {0}."),
    TR("{0} okunamadı."));

SS_MSG(err_no_sfm_module,
    EN("This build has no structure-from-motion module (-DSS_BUILD_SFM=OFF); "
       "use COLMAP instead."),
    JA("このビルドには Structure from Motion のモジュールがありません"
       "（-DSS_BUILD_SFM=OFF）。代わりに COLMAP を使ってください。"),
    ZH_HANS("本版本没有运动恢复结构模块（-DSS_BUILD_SFM=OFF），请改用 COLMAP。"),
    ZH_HANT("本版本沒有運動恢復結構模組（-DSS_BUILD_SFM=OFF），請改用 COLMAP。"),
    KO("이 빌드에는 Structure from Motion 모듈이 없습니다(-DSS_BUILD_SFM=OFF). "
       "대신 COLMAP 을 쓰세요."),
    DE("Dieser Build hat kein Structure-from-Motion-Modul "
       "(-DSS_BUILD_SFM=OFF); verwenden Sie stattdessen COLMAP."),
    FR("Cette version n'a pas de module structure-from-motion "
       "(-DSS_BUILD_SFM=OFF) ; utilisez COLMAP à la place."),
    ES("Esta compilación no tiene módulo de structure-from-motion "
       "(-DSS_BUILD_SFM=OFF); usa COLMAP en su lugar."),
    PT("Esta compilação não tem módulo de structure-from-motion "
       "(-DSS_BUILD_SFM=OFF); use o COLMAP no lugar."),
    IT("Questa build non ha il modulo structure-from-motion "
       "(-DSS_BUILD_SFM=OFF); usi COLMAP al suo posto."),
    NL("Deze build heeft geen structure-from-motion-module "
       "(-DSS_BUILD_SFM=OFF); gebruik in plaats daarvan COLMAP."),
    RU("В этой сборке нет модуля structure-from-motion (-DSS_BUILD_SFM=OFF); "
       "используйте COLMAP."),
    TR("Bu derlemede structure-from-motion modülü yok (-DSS_BUILD_SFM=OFF); "
       "bunun yerine COLMAP kullanın."));

SS_MSG(err_no_exe_path,
    EN("This program could not work out its own path, so it cannot run the "
       "reconstruction step."),
    JA("このプログラムは自分自身のパスを特定できなかったため、再構成の段階を"
       "実行できません。"),
    ZH_HANS("本程序无法确定自身路径，因而无法运行重建步骤。"),
    ZH_HANT("本程式無法確定自身路徑，因而無法執行重建步驟。"),
    KO("이 프로그램이 자신의 경로를 알아내지 못해 재구성 단계를 실행할 수 없습니다."),
    DE("Dieses Programm konnte seinen eigenen Pfad nicht ermitteln und kann den "
       "Rekonstruktionsschritt daher nicht ausführen."),
    FR("Ce programme n'a pas pu déterminer son propre chemin, il ne peut donc "
       "pas lancer l'étape de reconstruction."),
    ES("Este programa no pudo averiguar su propia ruta, así que no puede "
       "ejecutar el paso de reconstrucción."),
    PT("Este programa não conseguiu descobrir o próprio caminho, então não pode "
       "executar a etapa de reconstrução."),
    IT("Questo programma non è riuscito a determinare il proprio percorso, "
       "quindi non può eseguire la fase di ricostruzione."),
    NL("Dit programma kon zijn eigen pad niet bepalen en kan de "
       "reconstructiestap daarom niet uitvoeren."),
    RU("Программа не смогла определить собственный путь, поэтому не может "
       "выполнить этап реконструкции."),
    TR("Bu program kendi yolunu belirleyemedi, bu yüzden yeniden oluşturma "
       "adımını çalıştıramıyor."));

SS_MSG(stage_geometry,
    EN("Estimating depth and normals"),
    JA("深度と法線を推定しています"),
    ZH_HANS("正在估计深度与法线"),
    ZH_HANT("正在估計深度與法線"),
    KO("깊이와 법선을 추정하는 중"),
    DE("Tiefe und Normalen werden geschätzt"),
    FR("Estimation de la profondeur et des normales"),
    ES("Estimando profundidad y normales"),
    PT("A estimar profundidade e normais"),
    IT("Stima di profondità e normali"),
    NL("Diepte en normalen worden geschat"),
    RU("Оценка глубины и нормалей"),
    TR("Derinlik ve normaller kestiriliyor"));

SS_MSG(sfm_reusing_model,
    EN("{0} already holds a reconstruction; keeping it and only adding to it "
       "(tick \"Reconstruct again\" to replace it)"),
    JA("{0} にはすでに再構成結果があります。それを残し、上に足すだけにします"
       "（置き換えるには「再構成をやり直す」を有効にしてください）"),
    ZH_HANS("{0} 中已有一份重建结果，将保留它并只在其上追加（要替换请勾选"
            "“重新重建”）"),
    ZH_HANT("{0} 中已有一份重建結果，將保留它並只在其上追加（要取代請勾選"
            "「重新重建」）"),
    KO("{0} 에 이미 재구성 결과가 있어 그대로 두고 위에 더하기만 합니다"
       "(바꾸려면 \"다시 재구성\" 을 켜세요)"),
    DE("{0} enthält bereits eine Rekonstruktion; sie bleibt und es wird nur "
       "ergänzt (\"Neu rekonstruieren\" ersetzt sie)"),
    FR("{0} contient déjà une reconstruction ; elle est conservée et seulement "
       "complétée (cochez « Reconstruire à nouveau » pour la remplacer)"),
    ES("{0} ya contiene una reconstrucción; se conserva y solo se le añade "
       "(marque «Reconstruir de nuevo» para sustituirla)"),
    PT("{0} já contém uma reconstrução; fica e apenas se lhe acrescenta "
       "(marque \"Reconstruir de novo\" para a substituir)"),
    IT("{0} contiene già una ricostruzione; resta e le si aggiunge soltanto "
       "(spunta \"Ricostruisci di nuovo\" per sostituirla)"),
    NL("{0} bevat al een reconstructie; die blijft en er wordt alleen aan "
       "toegevoegd (vink \"Opnieuw reconstrueren\" aan om hem te vervangen)"),
    RU("В {0} уже есть реконструкция; она сохраняется, к ней только добавляется "
       "(чтобы заменить, включите «Реконструировать заново»)"),
    TR("{0} zaten bir yeniden kurma içeriyor; korunur ve yalnızca üzerine eklenir "
       "(değiştirmek için \"Yeniden kur\" seçeneğini işaretleyin)"));

SS_MSG(err_no_geometry_module,
    EN("This build cannot estimate depth and normals (-DSS_BUILD_SAM=OFF); use "
       "`spirula geometry` from a build that has it."),
    JA("このビルドでは深度と法線を推定できません（-DSS_BUILD_SAM=OFF）。"
       "対応したビルドの `spirula geometry` を使ってください。"),
    ZH_HANS("这个构建无法估计深度与法线（-DSS_BUILD_SAM=OFF）；请用带该功能的构建"
            "运行 `spirula geometry`。"),
    ZH_HANT("這個組建無法估計深度與法線（-DSS_BUILD_SAM=OFF）；請用帶該功能的組建"
            "執行 `spirula geometry`。"),
    KO("이 빌드에서는 깊이와 법선을 추정할 수 없습니다(-DSS_BUILD_SAM=OFF). 해당 "
       "기능이 있는 빌드의 `spirula geometry` 를 쓰세요."),
    DE("Diese Fassung kann Tiefe und Normalen nicht schätzen "
       "(-DSS_BUILD_SAM=OFF); `spirula geometry` aus einer Fassung nutzen, die "
       "es kann."),
    FR("Cette version ne peut pas estimer profondeur et normales "
       "(-DSS_BUILD_SAM=OFF) ; utilisez `spirula geometry` d'une version qui le "
       "peut."),
    ES("Esta compilación no puede estimar profundidad ni normales "
       "(-DSS_BUILD_SAM=OFF); use `spirula geometry` de una que sí pueda."),
    PT("Esta compilação não consegue estimar profundidade nem normais "
       "(-DSS_BUILD_SAM=OFF); use `spirula geometry` de uma que consiga."),
    IT("Questa build non può stimare profondità e normali (-DSS_BUILD_SAM=OFF); "
       "usa `spirula geometry` da una build che lo fa."),
    NL("Deze build kan diepte en normalen niet schatten (-DSS_BUILD_SAM=OFF); "
       "gebruik `spirula geometry` uit een build die het wel kan."),
    RU("Эта сборка не умеет оценивать глубину и нормали (-DSS_BUILD_SAM=OFF); "
       "используйте `spirula geometry` из сборки, где это есть."),
    TR("Bu yapı derinlik ve normalleri kestiremez (-DSS_BUILD_SAM=OFF); bunu "
       "yapabilen bir yapıdan `spirula geometry` kullanın."));

SS_MSG(err_spawn_geometry,
    EN("could not start the depth and normal estimation ({0})"),
    JA("深度と法線の推定を起動できませんでした（{0}）"),
    ZH_HANS("无法启动深度与法线估计程序（{0}）"),
    ZH_HANT("無法啟動深度與法線估計程式（{0}）"),
    KO("깊이와 법선 추정을 시작할 수 없습니다({0})"),
    DE("die Schätzung von Tiefe und Normalen konnte nicht gestartet werden ({0})"),
    FR("impossible de lancer l'estimation de profondeur et de normales ({0})"),
    ES("no se pudo iniciar la estimación de profundidad y normales ({0})"),
    PT("não foi possível iniciar a estimativa de profundidade e normais ({0})"),
    IT("non è stato possibile avviare la stima di profondità e normali ({0})"),
    NL("kon het schatten van diepte en normalen niet starten ({0})"),
    RU("не удалось запустить оценку глубины и нормалей ({0})"),
    TR("derinlik ve normal kestirimi başlatılamadı ({0})"));

SS_MSG(err_geometry_failed,
    EN("estimating depth and normals failed (see the log). The reconstruction "
       "itself is finished and can be trained on as it is."),
    JA("深度と法線の推定に失敗しました（ログを見てください）。再構成そのものは"
       "完了しているので、そのまま学習に使えます。"),
    ZH_HANS("深度与法线估计失败（请看日志）。重建本身已经完成，可以直接拿来训练。"),
    ZH_HANT("深度與法線估計失敗（請看記錄）。重建本身已經完成，可以直接拿來訓練。"),
    KO("깊이와 법선 추정에 실패했습니다(로그를 보세요). 재구성 자체는 끝났으므로 "
       "그대로 학습에 쓸 수 있습니다."),
    DE("das Schätzen von Tiefe und Normalen ist fehlgeschlagen (siehe Log). Die "
       "Rekonstruktion selbst ist fertig und kann so trainiert werden."),
    FR("l'estimation de la profondeur et des normales a échoué (voir le journal). "
       "La reconstruction elle-même est terminée et peut servir telle quelle."),
    ES("falló la estimación de profundidad y normales (mire el registro). La "
       "reconstrucción en sí está terminada y sirve tal cual."),
    PT("a estimativa de profundidade e normais falhou (veja o registo). A "
       "reconstrução em si está concluída e serve tal como está."),
    IT("la stima di profondità e normali è fallita (vedi il registro). La "
       "ricostruzione è comunque completa e si può addestrare così com'è."),
    NL("het schatten van diepte en normalen is mislukt (zie het logboek). De "
       "reconstructie zelf is af en kan zo gebruikt worden."),
    RU("оценка глубины и нормалей не удалась (см. журнал). Сама реконструкция "
       "завершена, и на ней можно обучать как есть."),
    TR("derinlik ve normal kestirimi başarısız oldu (günlüğe bakın). Yeniden "
       "kurmanın kendisi tamamlandı ve olduğu gibi eğitilebilir."));

}  // namespace log
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
