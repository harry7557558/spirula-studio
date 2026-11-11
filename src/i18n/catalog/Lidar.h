#pragma once

// What `spirula lidar` says -- its --help and every line a run prints, which
// the dataset screen's Align step reads back through i18n::scan().

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace lidar {

SS_MSG(tagline,
    EN("Align a reconstruction with a laser scan, and add the scan's geometry"),
    JA("再構成をレーザースキャンに位置合わせし、スキャンの形状を加えます"),
    ZH_HANS("把重建与激光扫描对齐，并加入扫描的几何信息"),
    ZH_HANT("把重建與雷射掃描對齊，並加入掃描的幾何資訊"),
    KO("재구성을 레이저 스캔에 정렬하고 스캔의 형상을 더합니다"),
    DE("Eine Rekonstruktion an einem Laserscan ausrichten und die Geometrie des "
       "Scans hinzufügen"),
    FR("Aligner une reconstruction sur un scan laser et y ajouter la géométrie du "
       "scan"),
    ES("Alinear una reconstrucción con un escaneo láser y añadir la geometría del "
       "escaneo"),
    PT("Alinhar uma reconstrução a uma varredura a laser e acrescentar a geometria "
       "da varredura"),
    IT("Allineare una ricostruzione a una scansione laser e aggiungere la geometria "
       "della scansione"),
    NL("Een reconstructie uitlijnen op een laserscan en de geometrie van de scan "
       "toevoegen"),
    RU("Совместить реконструкцию с лазерным сканом и добавить геометрию скана"),
    TR("Bir yeniden oluşturmayı lazer taramasıyla hizala ve taramanın geometrisini "
       "ekle"));

SS_MSG(usage_about,
    EN("Fit a reconstruction to the scan using its images, when available, then its surface.\n- Writes a COLMAP model in scan coordinates with scan points and their image tracks.\n- Renders scan depth and normals for each image.\n- Keeps the original in sparse_unaligned/."),
    JA("データセットフォルダの再構成をスキャンの座標系へ移します。回転・平行移動・スケールは、スキャンの画像が再構成に含まれていればまずその画像に、続いてスキャンの表面に合わせます。\n- 結果は 1 つの COLMAP モデルとして書き戻し、その初期点はスキャン自身の点で、各点にはその点を見ている画像を記録します。\n- 深度マップと法線マップは、すべての画像についてスキャンからレンダリングします。\n- 元の再構成は sparse_unaligned/ に残します。"),
    ZH_HANS("把数据集文件夹中的重建移到扫描的坐标系中——旋转、平移和缩放先拟合到参与了重建的扫描图像，再拟合到扫描的表面——然后写回为一个 COLMAP 模型，其初始点就是扫描自己的点，每个点都列出能看到它的图像。深度图和法线图会为每张图像从扫描渲染。\n原来的重建保留在 sparse_unaligned/ 中。"),
    ZH_HANT("把資料集資料夾中的重建移到掃描的座標系中——旋轉、平移和縮放先擬合到參與了重建的掃描影像，再擬合到掃描的表面——然後寫回為一個 COLMAP 模型，其初始點就是掃描自己的點，每個點都列出能看到它的影像。深度圖和法線圖會為每張影像從掃描渲染。\n原來的重建保留在 sparse_unaligned/ 中。"),
    KO("데이터셋 폴더의 재구성을 스캔의 좌표계로 옮깁니다. 회전, 이동, 스케일은 스캔의 이미지가 재구성에 들어 있으면 먼저 그 이미지에, 이어서 스캔 표면에 맞춥니다.\n- 결과는 하나의 COLMAP 모델로 다시 쓰며, 그 초기 점은 스캔 자신의 점이고 각 점에는 그 점을 보는 이미지가 기록됩니다.\n- 깊이 맵과 법선 맵은 모든 이미지에 대해 스캔에서 렌더링합니다. 원래 재구성은 sparse_unaligned/에 남겨 둡니다."),
    DE("Verschiebt die Rekonstruktion im Datensatzordner in das Bezugssystem des Scans -- Rotation, Translation und Maßstab, angepasst an die Bilder des Scans, soweit sie Teil der Rekonstruktion waren, und dann an seine Oberfläche -- und schreibt sie als ein COLMAP-Modell zurück, dessen Startpunkte die des Scans sind, jeder mit den Bildern, die ihn sehen.\nTiefen- und Normalenkarten werden für jedes Bild aus dem Scan gerendert. Die Ausgangsrekonstruktion bleibt in sparse_unaligned/ erhalten."),
    FR("Déplace la reconstruction du dossier du jeu de données dans le repère du scan -- rotation, translation et échelle, ajustées sur les images du scan lorsqu'elles faisaient partie de la reconstruction, puis sur sa surface -- et la réécrit en un seul modèle COLMAP dont les points de départ sont ceux du scan, chacun listant les images qui le voient.\nDes cartes de profondeur et de normales sont rendues depuis le scan pour chaque image. La reconstruction de départ est conservée dans sparse_unaligned/."),
    ES("Mueve la reconstrucción de la carpeta del conjunto de datos al sistema de referencia del escaneo -- rotación, traslación y escala, ajustadas a las imágenes del escaneo cuando formaban parte de la reconstrucción y después a su superficie -- y la vuelve a escribir como un único modelo COLMAP cuyos puntos iniciales son los del propio escaneo, cada uno con la lista de imágenes que lo ven.\nSe renderizan mapas de profundidad y normales desde el escaneo para cada imagen. La reconstrucción de partida se conserva en sparse_unaligned/."),
    PT("Move a reconstrução da pasta do conjunto de dados para o referencial da varredura -- rotação, translação e escala, ajustadas às imagens da varredura quando faziam parte da reconstrução e depois à sua superfície -- e a grava de volta como um único modelo COLMAP cujos pontos iniciais são os da própria varredura, cada um listando as imagens que o veem.\n- Mapas de profundidade e normais são renderizados a partir da varredura para cada imagem.\n- A reconstrução de partida é mantida em sparse_unaligned/."),
    IT("Sposta la ricostruzione della cartella del set di dati nel sistema di riferimento della scansione -- rotazione, traslazione e scala, adattate alle immagini della scansione quando facevano parte della ricostruzione e poi alla sua superficie -- e la riscrive come un unico modello COLMAP i cui punti iniziali sono quelli della scansione stessa, ciascuno con l'elenco delle immagini che lo vedono.\n- Le mappe di profondità e normali vengono renderizzate dalla scansione per ogni immagine.\n- La ricostruzione di partenza resta in sparse_unaligned/."),
    NL("Verplaatst de reconstructie in de datasetmap naar het assenstelsel van de scan -- rotatie, translatie en schaal, gepast op de beelden van de scan voor zover die deel uitmaakten van de reconstructie en daarna op het oppervlak ervan -- en schrijft haar terug als één COLMAP-model waarvan de beginpunten die van de scan zelf zijn, elk met de beelden die het zien.\nDiepte- en normaalkaarten worden voor elk beeld uit de scan gerenderd. De oorspronkelijke reconstructie blijft bewaard in sparse_unaligned/."),
    RU("Переносит реконструкцию из папки набора данных в систему координат скана -- поворот, сдвиг и масштаб подгоняются по изображениям скана, если они входили в реконструкцию, а затем по его поверхности -- и записывает её обратно как одну модель COLMAP, начальные точки которой -- точки самого скана, и у каждой указаны изображения, которые её видят.\n- Карты глубины и нормалей рендерятся из скана для каждого изображения.\n- Исходная реконструкция сохраняется в sparse_unaligned/."),
    TR("Veri kümesi klasöründeki yeniden oluşturmayı taramanın koordinat sistemine taşır -- dönme, öteleme ve ölçek, taramanın görüntüleri yeniden oluşturmaya dahilse önce onlara, sonra taramanın yüzeyine uydurulur -- ve sonucu, başlangıç noktaları taramanın kendi noktaları olan tek bir COLMAP modeli olarak geri yazar;\n- her nokta onu gören görüntüleri listeler. Derinlik ve normal haritaları her görüntü için taramadan işlenir.\n- Başlangıçtaki yeniden oluşturma sparse_unaligned/ içinde saklanır."));

SS_MSG(usage_extract,
    EN("--extract writes the images an E57 file carries into a dataset's image folder and records their poses, so a reconstruction that includes them can be aligned by them."),
    JA("--extract は E57 ファイルに含まれる画像をデータセットの画像フォルダに書き出し、その姿勢を記録します。\nこれで、それらの画像を含む再構成をその画像によって位置合わせできます。"),
    ZH_HANS("--extract 把 E57 文件中的图像写入数据集的图像文件夹并记录它们的位姿，这样包含这些图像的重建就能借助它们对齐。"),
    ZH_HANT("--extract 把 E57 檔案中的影像寫入資料集的影像資料夾並記錄它們的位姿，這樣包含這些影像的重建就能藉助它們對齊。"),
    KO("--extract는 E57 파일에 담긴 이미지를 데이터셋의 이미지 폴더에 쓰고 그 자세를 기록합니다.\n그러면 이 이미지를 포함한 재구성을 이 이미지로 정렬할 수 있습니다."),
    DE("--extract schreibt E57-Bilder mit Posen in den Bildordner des Datensatzes. Eine Rekonstruktion mit diesen Bildern lässt sich daran ausrichten."),
    FR("--extract écrit les images E57 et leurs poses dans le dossier d’images. Une reconstruction qui les inclut peut être alignée sur elles."),
    ES("--extract guarda fotos E57 y sus poses en la carpeta de imágenes. Una reconstrucción que las incluya puede alinearse con ellas."),
    PT("--extract grava fotos E57 e poses na pasta de imagens. Uma reconstrução que as inclua pode ser alinhada por elas."),
    IT("--extract salva foto E57 e pose nella cartella delle immagini. Una ricostruzione che le include può essere allineata tramite esse."),
    NL("--extract schrijft de beelden die een E57-bestand bevat naar de beeldenmap van een dataset en legt hun poses vast, zodat een reconstructie die ze bevat erop kan worden uitgelijnd."),
    RU("--extract пишет снимки E57 и их позы в папку изображений. Реконструкцию с этими снимками можно совместить по ним."),
    TR("--extract, bir E57 dosyasındaki görüntüleri bir veri kümesinin görüntü klasörüne yazar ve pozlarını kaydeder;\nböylece bu görüntüleri içeren bir yeniden oluşturma onlara göre hizalanabilir."));

SS_MSG(head_options,
    EN("Options:"), JA("オプション:"), ZH_HANS("选项："), ZH_HANT("選項："),
    KO("옵션:"), DE("Optionen:"), FR("Options :"), ES("Opciones:"), PT("Opções:"),
    IT("Opzioni:"), NL("Opties:"), RU("Параметры:"), TR("Seçenekler:"));

SS_MSG(opt_cloud,
    EN("A point cloud in the scan's frame (.e57, .las, .ply); repeat for several"),
    JA("スキャンの座標系にある点群（.e57、.las、.ply）。複数あれば繰り返し指定します"),
    ZH_HANS("位于扫描坐标系中的点云（.e57、.las、.ply）；多个时重复指定"),
    ZH_HANT("位於掃描座標系中的點雲（.e57、.las、.ply）；多個時重複指定"),
    KO("스캔 좌표계의 포인트 클라우드(.e57, .las, .ply). 여러 개면 반복해서 "
       "지정합니다"),
    DE("Eine Punktwolke im Bezugssystem des Scans (.e57, .las, .ply); für mehrere "
       "wiederholen"),
    FR("Un nuage de points dans le repère du scan (.e57, .las, .ply) ; à répéter pour "
       "en donner plusieurs"),
    ES("Una nube de puntos en el sistema de referencia del escaneo (.e57, .las, "
       ".ply); repítalo para varias"),
    PT("Uma nuvem de pontos no referencial da varredura (.e57, .las, .ply); repita "
       "para várias"),
    IT("Una nuvola di punti nel sistema di riferimento della scansione (.e57, .las, "
       ".ply); ripetere per più nuvole"),
    NL("Een puntenwolk in het assenstelsel van de scan (.e57, .las, .ply); herhaal "
       "voor meerdere"),
    RU("Облако точек в системе координат скана (.e57, .las, .ply); для нескольких "
       "повторите"),
    TR("Taramanın koordinat sistemindeki bir nokta bulutu (.e57, .las, .ply); birden "
       "çok için tekrarlayın"));

SS_MSG(opt_mode,
    EN("- auto (default) fits the anchors, then the surface; anchors fits the anchors alone;\n- keep leaves a model that is already in the scan's frame where it is;\n- refine fits the surface from where the model is"),
    JA("- auto（既定）はアンカー画像に合わせてから表面に合わせます。 anchors はアンカー画像だけに合わせます。\n- keep はすでにスキャンの座標系にあるモデルをそのままにします。 refine はモデルの今の位置から表面に合わせます"),
    ZH_HANS("- auto（默认）先拟合锚定图像，再拟合表面； anchors 只拟合锚定图像； keep 让已经处于扫描坐标系中的模型保持原位；\n- refine 从模型当前的位置开始拟合表面"),
    ZH_HANT("- auto（預設）先擬合錨定影像，再擬合表面； anchors 只擬合錨定影像； keep 讓已經位於掃描座標系中的模型保持原位；\n- refine 從模型目前的位置開始擬合表面"),
    KO("- auto(기본값)는 앵커 이미지에 맞춘 뒤 표면에 맞춥니다. anchors는 앵커 이미지에만 맞춥니다.\n- keep은 이미 스캔 좌표계에 있는 모델을 그대로 둡니다. refine은 모델의 현재 위치에서 시작해 표면에 맞춥니다"),
    DE("- auto (Standard) passt an die Ankerbilder an, dann an die Oberfläche; anchors passt nur an die Ankerbilder an;\n- keep lässt ein Modell, das schon im Bezugssystem des Scans liegt, wo es ist;\n- refine passt ab der aktuellen Lage des Modells an die Oberfläche an"),
    FR("- auto (par défaut) ajuste sur les images d'ancrage, puis sur la surface ;\n- anchors ajuste sur les images d'ancrage seules ; keep laisse en place un modèle déjà dans le repère du scan ;\n- refine ajuste sur la surface à partir de la position actuelle du modèle"),
    ES("- auto (por defecto) ajusta a las imágenes ancla y luego a la superficie; anchors ajusta solo a las imágenes ancla;\n- keep deja donde está un modelo que ya está en el sistema de referencia del escaneo;\n- refine ajusta a la superficie desde donde está el modelo"),
    PT("- auto (padrão) ajusta às imagens âncora e depois à superfície; anchors ajusta só às imagens âncora;\n- keep deixa onde está um modelo que já está no referencial da varredura;\n- refine ajusta à superfície a partir de onde o modelo está"),
    IT("- auto (default): immagini di ancoraggio, poi superficie; anchors: solo ancoraggi.\n- keep: lascia il modello già nel sistema della scansione; refine: adatta la superficie dalla posa attuale."),
    NL("- auto (standaard) past op de ankerbeelden en daarna op het oppervlak; anchors past alleen op de ankerbeelden;\n- keep laat een model dat al in het assenstelsel van de scan staat waar het is;\n- refine past op het oppervlak vanaf waar het model staat"),
    RU("- auto (по умолчанию) подгоняет по опорным изображениям, затем по поверхности;\n- anchors -- только по опорным изображениям; keep оставляет на месте модель, которая уже в системе координат скана;\n- refine подгоняет по поверхности от текущего положения модели"),
    TR("- auto (varsayılan) önce çapa görüntülerine, sonra yüzeye uydurur; anchors yalnızca çapa görüntülerine uydurur;\n- keep, zaten taramanın koordinat sisteminde olan bir modeli yerinde bırakır;\n- refine, modelin bulunduğu yerden başlayarak yüzeye uydurur"));

SS_MSG(opt_points,
    EN("Seed points kept from the scan, thinned by voxels (default {0}); all keeps every "
       "one"),
    JA("スキャンから残す初期点の数。ボクセルで間引きます（既定 {0}）。all なら全点を"
       "残します"),
    ZH_HANS("从扫描保留的初始点数，按体素精简（默认 {0}）；all 表示全部保留"),
    ZH_HANT("從掃描保留的初始點數，按體素精簡（預設 {0}）；all 表示全部保留"),
    KO("스캔에서 남길 초기 점 수로, 복셀로 솎아 냅니다(기본값 {0}). all이면 모두 "
       "남깁니다"),
    DE("Aus dem Scan behaltene Startpunkte, nach Voxeln ausgedünnt (Standard {0}); "
       "all behält alle"),
    FR("Points de départ gardés du scan, allégés par voxels (par défaut {0}) ; all "
       "les garde tous"),
    ES("Puntos iniciales que se conservan del escaneo, aligerados por vóxeles (por "
       "defecto {0}); all los conserva todos"),
    PT("Pontos iniciais mantidos da varredura, reduzidos por voxels (padrão {0}); all "
       "mantém todos"),
    IT("Punti iniziali tenuti dalla scansione, sfoltiti per voxel (predefinito {0}); "
       "all li tiene tutti"),
    NL("Beginpunten die uit de scan behouden blijven, uitgedund per voxel (standaard "
       "{0}); all houdt ze allemaal"),
    RU("Начальные точки, оставляемые из скана, прореженные по вокселям (по умолчанию "
       "{0}); all оставляет все"),
    TR("Taramadan tutulan başlangıç noktaları, voksellerle seyreltilir (varsayılan "
       "{0}); all hepsini tutar"));

SS_MSG(opt_track_cap,
    EN("Images listed in a scan point's track, at most (default {0})"),
    JA("1 つのスキャン点に記録する画像の最大数（既定 {0}）"),
    ZH_HANS("每个扫描点最多列出的图像数（默认 {0}）"),
    ZH_HANT("每個掃描點最多列出的影像數（預設 {0}）"),
    KO("스캔 점 하나에 기록할 이미지의 최대 수(기본값 {0})"),
    DE("Höchstzahl der Bilder, die zu einem Scanpunkt aufgeführt werden (Standard {0})"),
    FR("Nombre maximal d'images listées pour un point du scan (par défaut {0})"),
    ES("Máximo de imágenes listadas para un punto del escaneo (por defecto {0})"),
    PT("Máximo de imagens listadas para um ponto da varredura (padrão {0})"),
    IT("Numero massimo di immagini elencate per un punto della scansione (predefinito "
       "{0})"),
    NL("Hoogstens zoveel beelden vermeld bij één scanpunt (standaard {0})"),
    RU("Наибольшее число изображений, указываемых для одной точки скана (по умолчанию "
       "{0})"),
    TR("Bir tarama noktası için listelenen en fazla görüntü sayısı (varsayılan {0})"));

SS_MSG(opt_no_depth,
    EN("Do not render depth and normal maps"),
    JA("深度マップと法線マップをレンダリングしません"),
    ZH_HANS("不渲染深度图和法线图"),
    ZH_HANT("不渲染深度圖和法線圖"),
    KO("깊이 맵과 법선 맵을 렌더링하지 않습니다"),
    DE("Keine Tiefen- und Normalenkarten rendern"),
    FR("Ne pas rendre de cartes de profondeur et de normales"),
    ES("No renderizar mapas de profundidad y normales"),
    PT("Não renderizar mapas de profundidade e normais"),
    IT("Non renderizzare mappe di profondità e normali"),
    NL("Geen diepte- en normaalkaarten renderen"),
    RU("Не рендерить карты глубины и нормалей"),
    TR("Derinlik ve normal haritalarını oluşturma"));

SS_MSG(opt_no_gaps,
    EN("Drop the reconstruction's own points, even where the scan has nothing"),
    JA("スキャンに点がない場所でも、再構成自身の点を使いません"),
    ZH_HANS("丢弃重建自己的点，即使扫描在那里没有点"),
    ZH_HANT("捨棄重建自己的點，即使掃描在那裡沒有點"),
    KO("스캔에 아무것도 없는 곳에서도 재구성 자체의 점을 버립니다"),
    DE("Die eigenen Punkte der Rekonstruktion verwerfen, auch wo der Scan nichts hat"),
    FR("Écarter les points propres de la reconstruction, même là où le scan n'a rien"),
    ES("Descartar los puntos propios de la reconstrucción, incluso donde el escaneo no "
       "tiene nada"),
    PT("Descartar os pontos da própria reconstrução, mesmo onde a varredura não tem "
       "nada"),
    IT("Scartare i punti propri della ricostruzione, anche dove la scansione non ha "
       "nulla"),
    NL("De eigen punten van de reconstructie weglaten, ook waar de scan niets heeft"),
    RU("Отбросить собственные точки реконструкции, даже там, где в скане ничего нет"),
    TR("Taramanın hiçbir şey içermediği yerlerde bile yeniden oluşturmanın kendi "
       "noktalarını at"));

SS_MSG(opt_anchors,
    EN("The anchors file (default: lidar/anchors.json in the dataset)"),
    JA("アンカー画像のファイル（既定: データセット内の lidar/anchors.json）"),
    ZH_HANS("锚定图像文件（默认：数据集中的 lidar/anchors.json）"),
    ZH_HANT("錨定影像檔案（預設：資料集中的 lidar/anchors.json）"),
    KO("앵커 이미지 파일(기본값: 데이터셋 안의 lidar/anchors.json)"),
    DE("Die Datei der Ankerbilder (Standard: lidar/anchors.json im Datensatz)"),
    FR("Le fichier des images d'ancrage (par défaut : lidar/anchors.json dans le jeu "
       "de données)"),
    ES("El archivo de imágenes ancla (por defecto: lidar/anchors.json en el conjunto "
       "de datos)"),
    PT("O arquivo das imagens âncora (padrão: lidar/anchors.json no conjunto de "
       "dados)"),
    IT("Il file delle immagini di ancoraggio (predefinito: lidar/anchors.json nel set "
       "di dati)"),
    NL("Het bestand met ankerbeelden (standaard: lidar/anchors.json in de dataset)"),
    RU("Файл опорных изображений (по умолчанию: lidar/anchors.json в наборе данных)"),
    TR("Çapa görüntüleri dosyası (varsayılan: veri kümesindeki lidar/anchors.json)"));

SS_MSG(opt_image_dir,
    EN("Where the images are: relative to the dataset (default images) or absolute"),
    JA("画像の場所。データセットからの相対パス（既定 images）か絶対パス"),
    ZH_HANS("图像所在位置：相对于数据集的路径（默认 images）或绝对路径"),
    ZH_HANT("影像所在位置：相對於資料集的路徑（預設 images）或絕對路徑"),
    KO("이미지 위치: 데이터셋 기준 상대 경로(기본값 images) 또는 절대 경로"),
    DE("Wo die Bilder liegen: relativ zum Datensatz (Standard images) oder absolut"),
    FR("Emplacement des images : relatif au jeu de données (par défaut images) ou "
       "absolu"),
    ES("Dónde están las imágenes: relativo al conjunto de datos (por defecto images) o "
       "absoluto"),
    PT("Onde estão as imagens: relativo ao conjunto de dados (padrão images) ou "
       "absoluto"),
    IT("Dove sono le immagini: relativo al set di dati (predefinito images) o assoluto"),
    NL("Waar de beelden staan: relatief aan de dataset (standaard images) of absoluut"),
    RU("Где лежат изображения: относительно набора данных (по умолчанию images) или "
       "абсолютный путь"),
    TR("Görüntülerin yeri: veri kümesine göre göreli (varsayılan images) ya da mutlak"));

SS_MSG(opt_mask_dir,
    EN("Where the masks are: relative to the dataset (default masks) or absolute. Depth "
       "and normals are left blank where they take the image out"),
    JA("マスクの場所。データセットからの相対パス（既定 masks）か絶対パス。マスクが"
       "画像を除く部分では、深度と法線を空のままにします"),
    ZH_HANS("蒙版所在位置：相对于数据集的路径（默认 masks）或绝对路径。蒙版去除图像的"
            "地方，深度和法线留空"),
    ZH_HANT("遮罩所在位置：相對於資料集的路徑（預設 masks）或絕對路徑。遮罩去除影像的"
            "地方，深度和法線留空"),
    KO("마스크 위치: 데이터셋 기준 상대 경로(기본값 masks) 또는 절대 경로. 마스크가 "
       "이미지를 빼는 곳은 깊이와 법선을 비워 둡니다"),
    DE("Wo die Masken liegen: relativ zum Datensatz (Standard masks) oder absolut. Wo "
       "sie das Bild ausblenden, bleiben Tiefe und Normalen leer"),
    FR("Emplacement des masques : relatif au jeu de données (par défaut masks) ou "
       "absolu. Là où ils retirent l'image, la profondeur et les normales restent vides"),
    ES("Dónde están las máscaras: relativo al conjunto de datos (por defecto masks) o "
       "absoluto. Donde quitan la imagen, la profundidad y las normales quedan vacías"),
    PT("Onde estão as máscaras: relativo ao conjunto de dados (padrão masks) ou "
       "absoluto. Onde elas retiram a imagem, a profundidade e as normais ficam vazias"),
    IT("Dove sono le maschere: relativo al set di dati (predefinito masks) o assoluto. "
       "Dove tolgono l'immagine, profondità e normali restano vuote"),
    NL("Waar de maskers staan: relatief aan de dataset (standaard masks) of absoluut. "
       "Waar ze het beeld weglaten, blijven diepte en normalen leeg"),
    RU("Где лежат маски: относительно набора данных (по умолчанию masks) или "
       "абсолютный путь. Там, где маска убирает изображение, глубина и нормали остаются "
       "пустыми"),
    TR("Maskelerin yeri: veri kümesine göre göreli (varsayılan masks) ya da mutlak. "
       "Maskenin görüntüyü çıkardığı yerlerde derinlik ve normaller boş kalır"));

SS_MSG(opt_scan_frames,
    EN("Coordinate frames for multiple scans.\n- shared: clouds already registered together.\n- separate: place each cloud through its images in the reconstruction.\n- auto (default): shared when no two files place their scanner in the same position."),
    JA("点群が 1 つの座標系を共有するかどうか。\n`shared`（1 回のレジストレーションで書き出されたもの）、`separate`（それぞれ独自の座標系を持ち、再構成内の画像を通して配置する）、または既定の `auto`（スキャナーの位置が同じになるファイルの組がなければ共有とみなす）"),
    ZH_HANS("这些点云是否共享一个坐标系： `shared`（由同一次配准写出）、`separate`（各有自己的坐标系，通过它在重建中的图像来放置），或默认的 `auto`：\n没有两个文件把扫描仪放在同一处时视为共享"),
    ZH_HANT("這些點雲是否共享一個座標系： `shared`（由同一次對位寫出）、`separate`（各有自己的座標系，透過它在重建中的影像來放置），或預設的 `auto`：\n沒有兩個檔案把掃描儀放在同一處時視為共享"),
    KO("포인트 클라우드들이 하나의 좌표계를 공유하는지:\n`shared`(한 번의 정합으로 내보낸 것), `separate`(각자 좌표계가 있고 재구성 안의 이미지를 통해 배치), 또는 기본값 `auto`: 스캐너를 같은 곳에 두는 두 파일이 없으면 공유로 봄"),
    DE("Ob die Punktwolken ein Bezugssystem teilen:\n`shared` (eine Registrierung hat sie geschrieben), `separate` (jede hat ihr eigenes und wird über ihre Bilder in der Rekonstruktion platziert) oder `auto`, der Standard: geteilt, wenn keine zwei Dateien ihren Scanner an dieselbe Stelle setzen"),
    FR("Si les nuages partagent un repère :\n`shared` (une seule consolidation les a écrits), `separate` (chacun a le sien et est placé par ses images dans la reconstruction), ou `auto`, par défaut : partagé quand deux fichiers ne placent jamais leur scanner au même endroit"),
    ES("Si las nubes comparten un sistema de referencia:\n`shared` (las escribió un solo registro), `separate` (cada una tiene el suyo y se sitúa mediante sus imágenes en la reconstrucción) o `auto`, por defecto: compartido cuando no hay dos archivos que pongan su escáner en el mismo sitio"),
    PT("Se as nuvens compartilham um referencial:\n`shared` (um único registro as gravou), `separate` (cada uma tem o seu e é posicionada pelas suas imagens na reconstrução) ou `auto`, o padrão: compartilhado quando não há dois arquivos que ponham o scanner no mesmo lugar"),
    IT("Se le nuvole condividono un sistema di riferimento:\n`shared` (le ha scritte un'unica registrazione), `separate` (ognuna ha il proprio e viene posizionata tramite le sue immagini nella ricostruzione) o `auto`, il predefinito: condiviso quando nessuna coppia di file mette lo scanner nello stesso punto"),
    NL("Of de puntenwolken één assenstelsel delen:\n`shared` (één registratie schreef ze), `separate` (elk heeft een eigen en wordt geplaatst via zijn beelden in de reconstructie) of `auto`, de standaard: gedeeld als geen twee bestanden hun scanner op dezelfde plek zetten"),
    RU("Общая ли у облаков система координат:\n`shared` (их записала одна регистрация), `separate` (у каждого своя, и оно размещается по своим изображениям в реконструкции) или `auto`, по умолчанию: общая, если никакие два файла не ставят сканер в одно место"),
    TR("Bulutların tek bir koordinat sistemini paylaşıp paylaşmadığı:\n`shared` (tek bir kayıt yazdı), `separate` (her birinin kendi sistemi var ve yeniden oluşturmadaki görüntüleriyle yerleştirilir) ya da varsayılan `auto`: hiçbir iki dosya tarayıcıyı aynı yere koymuyorsa paylaşılır"));

SS_MSG(opt_flip_masks,
    EN("The dataset's masks are white where an image is NOT kept"),
    JA("データセットのマスクは、画像を残さない部分が白です"),
    ZH_HANS("数据集的蒙版在图像不保留的地方为白色"),
    ZH_HANT("資料集的遮罩在影像不保留的地方為白色"),
    KO("데이터셋의 마스크는 이미지를 남기지 않는 곳이 흰색입니다"),
    DE("Die Masken des Datensatzes sind dort weiß, wo ein Bild NICHT behalten wird"),
    FR("Les masques du jeu de données sont blancs là où une image n'est PAS gardée"),
    ES("Las máscaras del conjunto de datos son blancas donde una imagen NO se conserva"),
    PT("As máscaras do conjunto de dados são brancas onde uma imagem NÃO é mantida"),
    IT("Le maschere del set di dati sono bianche dove un'immagine NON viene tenuta"),
    NL("De maskers van de dataset zijn wit waar een beeld NIET behouden blijft"),
    RU("Маски набора данных белые там, где изображение НЕ сохраняется"),
    TR("Veri kümesinin maskeleri, görüntünün TUTULMADIĞI yerlerde beyazdır"));

SS_MSG(opt_render,
    EN("--render-anchors <scan>... <dataset folder>: views of the scan from where the scanner stood, written as images with known poses, for a scan that has no photographs"),
    JA("--render-anchors <scan>...\n- <dataset folder>: スキャナーを据えた位置から見たスキャンのビューを、姿勢が分かっている画像として書き出します。\n- 写真のないスキャン向けです"),
    ZH_HANS("--render-anchors <scan>...\n<dataset folder>：从扫描仪所在位置渲染扫描的视图，作为位姿已知的图像写出，用于没有照片的扫描"),
    ZH_HANT("--render-anchors <scan>...\n<dataset folder>：從掃描儀所在位置渲染掃描的視圖，作為位姿已知的影像寫出，用於沒有照片的掃描"),
    KO("--render-anchors <scan>... <dataset folder>: 스캐너가 서 있던 곳에서 본 스캔의 뷰를 자세가 알려진 이미지로 씁니다.\n사진이 없는 스캔용입니다"),
    DE("--render-anchors <scan>... <dataset folder>: Ansichten des Scans von den Standpunkten des Scanners, als Bilder mit bekannten Posen geschrieben, für einen Scan ohne Fotos"),
    FR("--render-anchors <scan>... <dataset folder> : des vues du scan depuis l'emplacement du scanner, écrites comme images aux poses connues, pour un scan sans photos"),
    ES("--render-anchors <scan>... <dataset folder>: crea vistas desde las posiciones del escáner con poses conocidas, para escaneos sin fotos."),
    PT("--render-anchors <scan>... <dataset folder>: vistas da varredura de onde o scanner estava, gravadas como imagens com poses conhecidas, para uma varredura sem fotografias"),
    IT("--render-anchors <scan>... <dataset folder>: viste della scansione da dove stava lo scanner, scritte come immagini con pose note, per una scansione senza fotografie"),
    NL("--render-anchors <scan>... <dataset folder>: aanzichten van de scan vanaf waar de scanner stond, geschreven als beelden met bekende poses, voor een scan zonder foto's"),
    RU("--render-anchors <scan>... <dataset folder>: виды скана с тех мест, где стоял сканер, записанные как изображения с известными позами, для скана без фотографий"),
    TR("--render-anchors <scan>... <dataset folder>: taramanın, tarayıcının durduğu yerlerden görünümleri;\nfotoğrafı olmayan bir tarama için pozu bilinen görüntüler olarak yazılır"));

SS_MSG(opt_overwrite,
    EN("Align again even when nothing it depends on has changed"),
    JA("依存するものが何も変わっていなくても、もう一度位置合わせします"),
    ZH_HANS("即使所依赖的内容都没有变化，也重新对齐"),
    ZH_HANT("即使所依賴的內容都沒有變化，也重新對齊"),
    KO("의존하는 것이 바뀌지 않았어도 다시 정렬합니다"),
    DE("Erneut ausrichten, auch wenn sich nichts geändert hat, wovon es abhängt"),
    FR("Aligner de nouveau même si rien de ce dont l'alignement dépend n'a changé"),
    ES("Volver a alinear aunque no haya cambiado nada de lo que depende"),
    PT("Alinhar de novo mesmo que nada de que depende tenha mudado"),
    IT("Allineare di nuovo anche se nulla di ciò da cui dipende è cambiato"),
    NL("Opnieuw uitlijnen, ook als niets waarvan het afhangt is veranderd"),
    RU("Совместить заново, даже если ничего из того, от чего это зависит, не "
       "изменилось"),
    TR("Bağlı olduğu hiçbir şey değişmemiş olsa bile yeniden hizala"));

SS_MSG(opt_refine,
    EN("Place the scan's photographs from the scanner's poses, refined on the reconstruction's matches (needs features/ and matches.bin)"),
    JA("スキャンの写真をスキャナーの姿勢から置き、再構成の対応で補正します (features/ と matches.bin が必要)"),
    ZH_HANS("从扫描仪的位姿放置扫描照片，并用重建的匹配修正（需要 features/ 和 matches.bin）"),
    ZH_HANT("從掃描儀的位姿放置掃描照片，並用重建的匹配修正（需要 features/ 和 matches.bin）"),
    KO("스캔 사진을 스캐너의 자세에서 두고 재구성의 매칭으로 보정합니다 (features/와 matches.bin 필요)"),
    DE("Die Fotos des Scans ab den Posen des Scanners setzen, verfeinert an den Zuordnungen der Rekonstruktion (braucht features/ und matches.bin)"),
    FR("Placer les photos du scan depuis les poses du scanner, affinées sur les correspondances de la reconstruction (demande features/ et matches.bin)"),
    ES("Colocar las fotos del escaneo desde las poses del escáner, refinadas con las correspondencias de la reconstrucción (necesita features/ y matches.bin)"),
    PT("Colocar as fotos da varredura a partir das poses do scanner, refinadas nas correspondências da reconstrução (precisa de features/ e matches.bin)"),
    IT("Mettere le foto della scansione dalle pose dello scanner, affinate sulle corrispondenze della ricostruzione (richiede features/ e matches.bin)"),
    NL("De foto's van de scan plaatsen vanaf de poses van de scanner, verfijnd op de overeenkomsten van de reconstructie (vraagt features/ en matches.bin)"),
    RU("Поставить фотографии скана по позам сканера, уточнённым по сопоставлениям реконструкции (нужны features/ и matches.bin)"),
    TR("Taramanın fotoğraflarını tarayıcının pozlarından yerleştir, yeniden yapılandırmanın eşleşmeleriyle iyileştir (features/ ve matches.bin gerekir)"));

SS_MSG(opt_scanner_poses,
    EN("Use no reconstruction: place the scan's photographs where the scanner recorded them"),
    JA("再構成を使わず、スキャンの写真をスキャナーが記録した位置に置きます"),
    ZH_HANS("不使用重建：把扫描中的照片放在扫描仪记录的位置"),
    ZH_HANT("不使用重建：把掃描中的照片放在掃描儀記錄的位置"),
    KO("재구성을 쓰지 않고, 스캔의 사진을 스캐너가 기록한 위치에 둡니다"),
    DE("Keine Rekonstruktion verwenden: die Fotos des Scans dorthin setzen, wo der Scanner "
       "sie aufgezeichnet hat"),
    FR("N'utiliser aucune reconstruction : placer les photos du scan là où le scanner les a "
       "enregistrées"),
    ES("No usar ninguna reconstrucción: colocar las fotos del escaneo donde las registró el "
       "escáner"),
    PT("Não usar reconstrução: colocar as fotos da varredura onde o scanner as registrou"),
    IT("Non usare alcuna ricostruzione: mettere le foto della scansione dove le ha registrate "
       "lo scanner"),
    NL("Geen reconstructie gebruiken: de foto's van de scan neerzetten waar de scanner ze "
       "vastlegde"),
    RU("Не использовать реконструкцию: поставить фотографии скана туда, где их записал "
       "сканер"),
    TR("Yeniden oluşturma kullanma: taramanın fotoğraflarını tarayıcının kaydettiği yere koy"));

SS_MSG(opt_info,
    EN("Describe a point cloud file and stop"),
    JA("点群ファイルの内容を表示して終了します"),
    ZH_HANS("描述点云文件后退出"),
    ZH_HANT("描述點雲檔案後結束"),
    KO("포인트 클라우드 파일을 설명하고 멈춥니다"),
    DE("Eine Punktwolkendatei beschreiben und beenden"),
    FR("Décrire un fichier de nuage de points et s'arrêter"),
    ES("Describir un archivo de nube de puntos y terminar"),
    PT("Descrever um arquivo de nuvem de pontos e parar"),
    IT("Descrivere un file di nuvola di punti e fermarsi"),
    NL("Een puntenwolkbestand beschrijven en stoppen"),
    RU("Описать файл облака точек и завершить работу"),
    TR("Bir nokta bulutu dosyasını tanımla ve dur"));

SS_MSG(err_no_input,
    EN("{0}: name a dataset folder and at least one --cloud"),
    JA("{0}: データセットフォルダと、少なくとも 1 つの --cloud を指定してください"),
    ZH_HANS("{0}：请指定一个数据集文件夹和至少一个 --cloud"),
    ZH_HANT("{0}：請指定一個資料集資料夾和至少一個 --cloud"),
    KO("{0}: 데이터셋 폴더와 --cloud를 하나 이상 지정하세요"),
    DE("{0}: einen Datensatzordner und mindestens ein --cloud angeben"),
    FR("{0} : indiquez un dossier de jeu de données et au moins un --cloud"),
    ES("{0}: indique una carpeta de conjunto de datos y al menos un --cloud"),
    PT("{0}: indique uma pasta de conjunto de dados e pelo menos um --cloud"),
    IT("{0}: indicare una cartella del set di dati e almeno un --cloud"),
    NL("{0}: geef een datasetmap en minstens één --cloud op"),
    RU("{0}: укажите папку набора данных и хотя бы один --cloud"),
    TR("{0}: bir veri kümesi klasörü ve en az bir --cloud belirtin"));

SS_MSG(err_unknown_option,
    EN("Unknown option: {0}"), JA("不明なオプション: {0}"),
    ZH_HANS("未知选项：{0}"), ZH_HANT("未知選項：{0}"), KO("알 수 없는 옵션: {0}"),
    DE("Unbekannte Option: {0}"), FR("Option inconnue : {0}"),
    ES("Opción desconocida: {0}"), PT("Opção desconhecida: {0}"),
    IT("Opzione sconosciuta: {0}"), NL("Onbekende optie: {0}"),
    RU("Неизвестный параметр: {0}"), TR("Bilinmeyen seçenek: {0}"));

SS_MSG(err_bad_value,
    EN("{0}: not a valid value: {1}"),
    JA("{0}: 無効な値です: {1}"),
    ZH_HANS("{0}：无效的值：{1}"),
    ZH_HANT("{0}：無效的值：{1}"),
    KO("{0}: 올바른 값이 아닙니다: {1}"),
    DE("{0}: kein gültiger Wert: {1}"),
    FR("{0} : valeur non valide : {1}"),
    ES("{0}: valor no válido: {1}"),
    PT("{0}: valor inválido: {1}"),
    IT("{0}: valore non valido: {1}"),
    NL("{0}: geen geldige waarde: {1}"),
    RU("{0}: недопустимое значение: {1}"),
    TR("{0}: geçerli bir değer değil: {1}"));

SS_MSG(err_no_model,
    EN("No reconstruction in {0} (sparse/ with cameras.bin and images.bin)"),
    JA("{0} に再構成がありません（cameras.bin と images.bin を含む sparse/）"),
    ZH_HANS("{0} 中没有重建（需要含 cameras.bin 和 images.bin 的 sparse/）"),
    ZH_HANT("{0} 中沒有重建（需要含 cameras.bin 和 images.bin 的 sparse/）"),
    KO("{0}에 재구성이 없습니다(cameras.bin과 images.bin이 든 sparse/)"),
    DE("Keine Rekonstruktion in {0} (sparse/ mit cameras.bin und images.bin)"),
    FR("Aucune reconstruction dans {0} (sparse/ avec cameras.bin et images.bin)"),
    ES("No hay reconstrucción en {0} (sparse/ con cameras.bin e images.bin)"),
    PT("Nenhuma reconstrução em {0} (sparse/ com cameras.bin e images.bin)"),
    IT("Nessuna ricostruzione in {0} (sparse/ con cameras.bin e images.bin)"),
    NL("Geen reconstructie in {0} (sparse/ met cameras.bin en images.bin)"),
    RU("В {0} нет реконструкции (sparse/ с cameras.bin и images.bin)"),
    TR("{0} içinde yeniden oluşturma yok (cameras.bin ve images.bin içeren sparse/)"));

SS_MSG(err_unreadable_model,
    EN("{0} holds a model this cannot read (only COLMAP binary is read); it was left as it is"),
    JA("{0} には読めないモデルがあります（読めるのは COLMAP のバイナリだけです）。そのままにしました"),
    ZH_HANS("{0} 中的模型无法读取（只读取 COLMAP 二进制格式），已保持原样"),
    ZH_HANT("{0} 中的模型無法讀取（只讀取 COLMAP 二進位格式），已保持原樣"),
    KO("{0}에 읽을 수 없는 모델이 있습니다(COLMAP 바이너리만 읽습니다). 그대로 두었습니다"),
    DE("{0} enthält ein Modell, das sich nicht lesen lässt (gelesen wird nur COLMAP-binär); "
       "es bleibt unverändert"),
    FR("{0} contient un modèle illisible (seul le format binaire COLMAP est lu) ; il est "
       "laissé tel quel"),
    ES("{0} contiene un modelo que no se puede leer (solo se lee COLMAP binario); se ha "
       "dejado como estaba"),
    PT("{0} contém um modelo que não pode ser lido (só se lê COLMAP binário); ficou como "
       "estava"),
    IT("{0} contiene un modello che non si può leggere (si legge solo COLMAP binario); è "
       "stato lasciato com'era"),
    NL("{0} bevat een model dat niet te lezen is (alleen COLMAP-binair wordt gelezen); het "
       "is gelaten zoals het was"),
    RU("{0} содержит модель, которую не удаётся прочитать (читается только двоичный "
       "COLMAP); она оставлена как есть"),
    TR("{0} okunamayan bir model içeriyor (yalnızca COLMAP ikili biçimi okunur); olduğu "
       "gibi bırakıldı"));

SS_MSG(err_no_points,
    EN("The point clouds hold no points"),
    JA("点群に点が 1 つもありません"),
    ZH_HANS("点云中没有任何点"),
    ZH_HANT("點雲中沒有任何點"),
    KO("포인트 클라우드에 점이 하나도 없습니다"),
    DE("Die Punktwolken enthalten keine Punkte"),
    FR("Les nuages de points ne contiennent aucun point"),
    ES("Las nubes de puntos no contienen ningún punto"),
    PT("As nuvens de pontos não contêm nenhum ponto"),
    IT("Le nuvole di punti non contengono alcun punto"),
    NL("De puntenwolken bevatten geen punten"),
    RU("В облаках точек нет ни одной точки"),
    TR("Nokta bulutlarında hiç nokta yok"));

SS_MSG(err_none_aligned,
    EN("No scan-photo poses found in the models. Add the scan photos to reconstruction, or use --mode keep if already in the scan frame."),
    JA("モデルにスキャン写真の既知姿勢がありません。写真を再構成に追加するか、既にスキャン座標なら --mode keep を使ってください。"),
    ZH_HANS("模型中没有已知扫描照片位姿。请将扫描照片加入重建；若已在扫描坐标系中，用 --mode keep。"),
    ZH_HANT("模型中沒有已知掃描相片位姿。請將掃描相片加入重建；若已在掃描座標系中，用 --mode keep。"),
    KO("모델에 스캔 사진의 알려진 자세가 없습니다. 사진을 재구성에 추가하거나, 이미 스캔 좌표계라면 --mode keep을 쓰세요."),
    DE("Keine Scanfotoposen im Modell. Scanfotos zur Rekonstruktion hinzufügen; bei Modellen im Scanrahmen --mode keep nutzen."),
    FR("Aucune pose de photo du scan dans les modèles. Ajoutez ces photos, ou utilisez --mode keep si déjà dans le repère du scan."),
    ES("No hay poses de fotos del escaneo en los modelos. Añade esas fotos o usa --mode keep si ya están en el marco del escaneo."),
    PT("Sem poses de fotos da varredura nos modelos. Adicione essas fotos ou use --mode keep se já estiverem no referencial da varredura."),
    IT("Nessuna posa di foto della scansione nei modelli. Aggiungi le foto o usa --mode keep se già nel riferimento della scansione."),
    NL("Geen scanfotoposes in de modellen. Voeg scanfoto’s toe aan de reconstructie, of gebruik --mode keep als het scanframe al klopt."),
    RU("В моделях нет известных поз снимков скана. Добавьте их в реконструкцию или используйте --mode keep, если система скана уже верна."),
    TR("Modellerde bilinen tarama fotoğrafı pozu yok. Fotoğrafları yeniden oluşturmaya ekleyin; tarama çerçevesindeyse --mode keep kullanın."));

SS_MSG(info_cloud,
    EN("{0}: {1}   Points: {2}   Colour: {3}   Scanner positions: {4}"),
    JA("{0}: {1}   点: {2}   色: {3}   スキャナー位置: {4}"),
    ZH_HANS("{0}：{1}   点：{2}   颜色：{3}   扫描仪位置：{4}"),
    ZH_HANT("{0}：{1}   點：{2}   顏色：{3}   掃描儀位置：{4}"),
    KO("{0}: {1}   점: {2}   색: {3}   스캐너 위치: {4}"),
    DE("{0}: {1}   Punkte: {2}   Farbe: {3}   Scannerstandpunkte: {4}"),
    FR("{0} : {1}   Points : {2}   Couleur : {3}   Positions du scanner : {4}"),
    ES("{0}: {1}   Puntos: {2}   Color: {3}   Posiciones del escáner: {4}"),
    PT("{0}: {1}   Pontos: {2}   Cor: {3}   Posições do scanner: {4}"),
    IT("{0}: {1}   Punti: {2}   Colore: {3}   Posizioni dello scanner: {4}"),
    NL("{0}: {1}   Punten: {2}   Kleur: {3}   Scannerposities: {4}"),
    RU("{0}: {1}   Точек: {2}   Цвет: {3}   Позиций сканера: {4}"),
    TR("{0}: {1}   Nokta: {2}   Renk: {3}   Tarayıcı konumu: {4}"));
SS_MSG(word_yes, EN("yes"), JA("あり"), ZH_HANS("有"), ZH_HANT("有"), KO("있음"),
    DE("ja"), FR("oui"), ES("sí"), PT("sim"), IT("sì"), NL("ja"), RU("есть"), TR("var"));
SS_MSG(word_no, EN("no"), JA("なし"), ZH_HANS("无"), ZH_HANT("無"), KO("없음"),
    DE("nein"), FR("non"), ES("no"), PT("não"), IT("no"), NL("nee"), RU("нет"), TR("yok"));

SS_MSG(reading_cloud,
    EN("Reading the scan: {0}"),
    JA("スキャンを読み込み中: {0}"),
    ZH_HANS("正在读取扫描：{0}"),
    ZH_HANT("正在讀取掃描：{0}"),
    KO("스캔 읽는 중: {0}"),
    DE("Scan wird gelesen: {0}"),
    FR("Lecture du scan : {0}"),
    ES("Leyendo el escaneo: {0}"),
    PT("Lendo a varredura: {0}"),
    IT("Lettura della scansione: {0}"),
    NL("Scan lezen: {0}"),
    RU("Чтение скана: {0}"),
    TR("Tarama okunuyor: {0}"));

SS_MSG(cloud_read,
    EN("Points read: {0}   Kept for alignment: {1}   Voxel: {2} m"),
    JA("読み込んだ点: {0}   位置合わせ用に残した点: {1}   ボクセル: {2} m"),
    ZH_HANS("已读取的点：{0}   保留用于对齐：{1}   体素：{2} m"),
    ZH_HANT("已讀取的點：{0}   保留用於對齊：{1}   體素：{2} m"),
    KO("읽은 점: {0}   정렬용으로 남긴 점: {1}   복셀: {2} m"),
    DE("Gelesene Punkte: {0}   Für die Ausrichtung behalten: {1}   Voxel: {2} m"),
    FR("Points lus : {0}   Gardés pour l'alignement : {1}   Voxel : {2} m"),
    ES("Puntos leídos: {0}   Conservados para alinear: {1}   Vóxel: {2} m"),
    PT("Pontos lidos: {0}   Mantidos para o alinhamento: {1}   Voxel: {2} m"),
    IT("Punti letti: {0}   Tenuti per l'allineamento: {1}   Voxel: {2} m"),
    NL("Gelezen punten: {0}   Behouden voor uitlijning: {1}   Voxel: {2} m"),
    RU("Прочитано точек: {0}   Оставлено для совмещения: {1}   Воксель: {2} м"),
    TR("Okunan nokta: {0}   Hizalama için tutulan: {1}   Voksel: {2} m"));

SS_MSG(reused,
    EN("Already aligned with these scans, nothing changed: {0}"),
    JA("これらのスキャンとは位置合わせ済みで、変更はありません: {0}"),
    ZH_HANS("已与这些扫描对齐，没有任何变化：{0}"),
    ZH_HANT("已與這些掃描對齊，沒有任何變化：{0}"),
    KO("이미 이 스캔들과 정렬되어 있고 바뀐 것이 없습니다: {0}"),
    DE("Bereits an diesen Scans ausgerichtet, nichts geändert: {0}"),
    FR("Déjà aligné sur ces scans, rien n'a changé : {0}"),
    ES("Ya alineado con estos escaneos, nada ha cambiado: {0}"),
    PT("Já alinhado com estas varreduras, nada mudou: {0}"),
    IT("Già allineato a queste scansioni, nulla è cambiato: {0}"),
    NL("Al uitgelijnd op deze scans, niets veranderd: {0}"),
    RU("Уже совмещено с этими сканами, ничего не изменилось: {0}"),
    TR("Bu taramalarla zaten hizalı, hiçbir şey değişmedi: {0}"));

SS_MSG(anchors_read,
    EN("Images with a known pose in the scan: {0}"),
    JA("スキャン内での姿勢が分かっている画像: {0}"),
    ZH_HANS("在扫描中位姿已知的图像：{0}"),
    ZH_HANT("在掃描中位姿已知的影像：{0}"),
    KO("스캔 안에서 자세가 알려진 이미지: {0}"),
    DE("Bilder mit bekannter Pose im Scan: {0}"),
    FR("Images dont la pose dans le scan est connue : {0}"),
    ES("Imágenes con pose conocida en el escaneo: {0}"),
    PT("Imagens com pose conhecida na varredura: {0}"),
    IT("Immagini con posa nota nella scansione: {0}"),
    NL("Beelden met een bekende pose in de scan: {0}"),
    RU("Изображений с известной позой в скане: {0}"),
    TR("Taramada pozu bilinen görüntü: {0}"));

SS_MSG(refine_head,
    EN("Refining the photographs from the scanner's poses: {0} of {1} have features, {2} instants"),
    JA("スキャナーの姿勢から写真を補正します: {1} 枚中 {0} 枚に特徴点、瞬間 {2}"),
    ZH_HANS("从扫描仪的位姿修正照片：{1} 张中有 {0} 张有特征点，时刻 {2} 个"),
    ZH_HANT("從掃描儀的位姿修正照片：{1} 張中有 {0} 張有特徵點，時刻 {2} 個"),
    KO("스캐너의 자세에서 사진을 보정합니다: {1}장 중 {0}장에 특징점, 순간 {2}개"),
    DE("Fotos werden ab den Posen des Scanners verfeinert: {0} von {1} mit Merkmalen, {2} Zeitpunkte"),
    FR("Affinage des photos à partir des poses du scanner : {0} sur {1} ont des points, {2} instants"),
    ES("Refinando las fotos desde las poses del escáner: {0} de {1} con puntos, {2} instantes"),
    PT("Refinando as fotos a partir das poses do scanner: {0} de {1} com pontos, {2} instantes"),
    IT("Affinamento delle foto dalle pose dello scanner: {0} di {1} con punti, {2} istanti"),
    NL("Foto's worden verfijnd vanaf de poses van de scanner: {0} van {1} met punten, {2} momenten"),
    RU("Уточнение фотографий от поз сканера: признаки у {0} из {1}, моментов {2}"),
    TR("Fotoğraflar tarayıcının pozlarından iyileştiriliyor: {1} fotoğraftan {0} tanesinde nokta, {2} an"));

SS_MSG(refine_done,
    EN("Refined: points {0}, reprojection {1} px (median), moved {2} m and turned {3} deg from the scanner's poses (median)"),
    JA("補正済み: 点 {0}、再投影 {1} px (中央値)、スキャナーの姿勢から移動 {2} m、回転 {3} 度 (中央値)"),
    ZH_HANS("已修正：点 {0} 个，重投影 {1} px（中位数），相对扫描仪位姿移动 {2} m、旋转 {3} 度（中位数）"),
    ZH_HANT("已修正：點 {0} 個，重投影 {1} px（中位數），相對掃描儀位姿移動 {2} m、旋轉 {3} 度（中位數）"),
    KO("보정됨: 점 {0}개, 재투영 {1} px (중앙값), 스캐너 자세에서 이동 {2} m, 회전 {3}도 (중앙값)"),
    DE("Verfeinert: Punkte {0}, Reprojektion {1} px (Median), gegenüber den Posen des Scanners {2} m verschoben und {3} Grad gedreht (Median)"),
    FR("Affiné : points {0}, reprojection {1} px (médiane), déplacé de {2} m et tourné de {3} degrés depuis les poses du scanner (médiane)"),
    ES("Refinado: puntos {0}, reproyección {1} px (mediana), movido {2} m y girado {3} grados desde las poses del escáner (mediana)"),
    PT("Refinado: pontos {0}, reprojeção {1} px (mediana), movido {2} m e girado {3} graus a partir das poses do scanner (mediana)"),
    IT("Affinato: punti {0}, riproiezione {1} px (mediana), spostato di {2} m e ruotato di {3} gradi dalle pose dello scanner (mediana)"),
    NL("Verfijnd: punten {0}, herprojectie {1} px (mediaan), {2} m verschoven en {3} graden gedraaid ten opzichte van de poses van de scanner (mediaan)"),
    RU("Уточнено: точек {0}, перепроекция {1} px (медиана), сдвиг от поз сканера {2} м и поворот {3} град. (медиана)"),
    TR("İyileştirildi: nokta {0}, yeniden izdüşüm {1} px (ortanca), tarayıcının pozlarından {2} m kayma ve {3} derece dönme (ortanca)"));

SS_MSG(refine_none,
    EN("No photograph of the scans has features in {0}: the scanner's poses are used as they are"),
    JA("{0} にはスキャンの写真の特徴点がありません。スキャナーの姿勢をそのまま使います"),
    ZH_HANS("{0} 中没有扫描照片的特征点：按原样使用扫描仪的位姿"),
    ZH_HANT("{0} 中沒有掃描照片的特徵點：按原樣使用掃描儀的位姿"),
    KO("{0}에 스캔 사진의 특징점이 없습니다. 스캐너의 자세를 그대로 씁니다"),
    DE("Kein Foto der Scans hat Merkmale in {0}: die Posen des Scanners werden unverändert verwendet"),
    FR("Aucune photo des scans n'a de points dans {0} : les poses du scanner sont utilisées telles quelles"),
    ES("Ninguna foto de los escaneos tiene puntos en {0}: se usan las poses del escáner tal cual"),
    PT("Nenhuma foto das varreduras tem pontos em {0}: as poses do scanner são usadas como estão"),
    IT("Nessuna foto delle scansioni ha punti in {0}: le pose dello scanner sono usate così come sono"),
    NL("Geen foto van de scans heeft punten in {0}: de poses van de scanner worden gebruikt zoals ze zijn"),
    RU("Ни у одной фотографии сканов нет признаков в {0}: позы сканера используются как есть"),
    TR("{0} içinde taramaların hiçbir fotoğrafında nokta yok: tarayıcının pozları olduğu gibi kullanılıyor"));

SS_MSG(err_refine_no_matches,
    EN("Refining the photographs needs the reconstruction's features and matches, and {0} has no matches.bin"),
    JA("写真の補正には再構成の特徴点と対応が必要ですが、{0} に matches.bin がありません"),
    ZH_HANS("修正照片需要重建的特征点和匹配，但 {0} 中没有 matches.bin"),
    ZH_HANT("修正照片需要重建的特徵點和匹配，但 {0} 中沒有 matches.bin"),
    KO("사진 보정에는 재구성의 특징점과 매칭이 필요하지만 {0}에 matches.bin이 없습니다"),
    DE("Das Verfeinern der Fotos braucht die Merkmale und Zuordnungen der Rekonstruktion, und {0} hat keine matches.bin"),
    FR("L'affinage des photos demande les points et correspondances de la reconstruction, et {0} n'a pas de matches.bin"),
    ES("Refinar las fotos necesita los puntos y correspondencias de la reconstrucción, y {0} no tiene matches.bin"),
    PT("Refinar as fotos precisa dos pontos e correspondências da reconstrução, e {0} não tem matches.bin"),
    IT("L'affinamento delle foto richiede i punti e le corrispondenze della ricostruzione, e {0} non ha matches.bin"),
    NL("Het verfijnen van de foto's vraagt de punten en overeenkomsten van de reconstructie, en {0} heeft geen matches.bin"),
    RU("Для уточнения фотографий нужны признаки и сопоставления реконструкции, а в {0} нет matches.bin"),
    TR("Fotoğrafları iyileştirmek yeniden yapılandırmanın noktalarını ve eşleşmelerini ister, ama {0} içinde matches.bin yok"));

SS_MSG(err_refine_unbuilt,
    EN("This build has no structure from motion, so it cannot refine the photographs"),
    JA("このビルドには SfM がないため、写真を補正できません"),
    ZH_HANS("此版本不含 SfM，无法修正照片"),
    ZH_HANT("此版本不含 SfM，無法修正照片"),
    KO("이 빌드에는 SfM이 없어 사진을 보정할 수 없습니다"),
    DE("Dieser Build enthält kein SfM und kann die Fotos nicht verfeinern"),
    FR("Cette version n'a pas de SfM et ne peut pas affiner les photos"),
    ES("Esta versión no tiene SfM y no puede refinar las fotos"),
    PT("Esta versão não tem SfM e não pode refinar as fotos"),
    IT("Questa versione non ha SfM e non può affinare le foto"),
    NL("Deze build heeft geen SfM en kan de foto's niet verfijnen"),
    RU("В этой сборке нет SfM, поэтому фотографии уточнить нельзя"),
    TR("Bu sürümde SfM yok, bu yüzden fotoğraflar iyileştirilemez"));

SS_MSG(model_head,
    EN("Model {0}: images {1}, points {2}"),
    JA("モデル {0}: 画像 {1}、点 {2}"),
    ZH_HANS("模型 {0}：图像 {1}，点 {2}"),
    ZH_HANT("模型 {0}：影像 {1}，點 {2}"),
    KO("모델 {0}: 이미지 {1}, 점 {2}"),
    DE("Modell {0}: Bilder {1}, Punkte {2}"),
    FR("Modèle {0} : images {1}, points {2}"),
    ES("Modelo {0}: imágenes {1}, puntos {2}"),
    PT("Modelo {0}: imagens {1}, pontos {2}"),
    IT("Modello {0}: immagini {1}, punti {2}"),
    NL("Model {0}: beelden {1}, punten {2}"),
    RU("Модель {0}: изображений {1}, точек {2}"),
    TR("Model {0}: görüntü {1}, nokta {2}"));

SS_MSG(model_anchors,
    EN("Anchors in it: {0}, agreeing: {1}; their poses differ by {2} deg and {3} m (median)"),
    JA("含まれるアンカー画像: {0}、一致: {1}。姿勢の差は {2} 度、{3} m（中央値）"),
    ZH_HANS("其中的锚定图像：{0}，一致的：{1}；位姿相差 {2} 度和 {3} m（中位数）"),
    ZH_HANT("其中的錨定影像：{0}，一致的：{1}；位姿相差 {2} 度和 {3} m（中位數）"),
    KO("포함된 앵커 이미지: {0}, 일치: {1}. 자세 차이는 {2}도, {3} m(중앙값)"),
    DE("Ankerbilder darin: {0}, übereinstimmend: {1}; ihre Posen weichen um {2} Grad "
       "und {3} m ab (Median)"),
    FR("Images d'ancrage : {0}, concordantes : {1} ; leurs poses diffèrent de {2} "
       "degrés et {3} m (médiane)"),
    ES("Imágenes ancla: {0}, coincidentes: {1}; sus poses difieren en {2} grados y {3} "
       "m (mediana)"),
    PT("Imagens âncora: {0}, concordantes: {1}; as poses diferem em {2} graus e {3} m "
       "(mediana)"),
    IT("Immagini di ancoraggio: {0}, concordi: {1}; le pose differiscono di {2} gradi "
       "e {3} m (mediana)"),
    NL("Ankerbeelden erin: {0}, overeenstemmend: {1}; hun poses verschillen {2} "
       "graden en {3} m (mediaan)"),
    RU("Опорных изображений в ней: {0}, согласующихся: {1}; их позы расходятся на {2} "
       "град. и {3} м (медиана)"),
    TR("İçindeki çapa görüntüsü: {0}, uyuşan: {1}; pozları {2} derece ve {3} m farklı "
       "(medyan)"));

SS_MSG(model_icp,
    EN("Fitted to the scan's surface: {0} m from it (median), {1}% of the points on it"),
    JA("スキャンの表面に合わせました: 表面からの距離 {0} m（中央値）、表面上の点 {1}%"),
    ZH_HANS("已拟合到扫描表面：距表面 {0} m（中位数），{1}% 的点落在表面上"),
    ZH_HANT("已擬合到掃描表面：距表面 {0} m（中位數），{1}% 的點落在表面上"),
    KO("스캔 표면에 맞췄습니다: 표면에서 {0} m(중앙값), 표면 위의 점 {1}%"),
    DE("An die Oberfläche des Scans angepasst: {0} m davon entfernt (Median), {1} % der "
       "Punkte darauf"),
    FR("Ajusté sur la surface du scan : à {0} m d'elle (médiane), {1} % des points "
       "dessus"),
    ES("Ajustado a la superficie del escaneo: a {0} m de ella (mediana), {1} % de los "
       "puntos sobre ella"),
    PT("Ajustado à superfície da varredura: a {0} m dela (mediana), {1}% dos pontos "
       "sobre ela"),
    IT("Adattato alla superficie della scansione: a {0} m da essa (mediana), {1}% dei "
       "punti su di essa"),
    NL("Gepast op het oppervlak van de scan: {0} m ervan af (mediaan), {1}% van de "
       "punten erop"),
    RU("Подогнано по поверхности скана: {0} м от неё (медиана), {1}% точек на ней"),
    TR("Taramanın yüzeyine uyduruldu: ondan {0} m uzakta (medyan), noktaların %{1} "
       "kadarı üzerinde"));

SS_MSG(scale_from_depth,
    EN("The anchors stand in one place, so the scale comes from the scan's depth: {0}   "
       "Points: {1}"),
    JA("アンカー画像が 1 か所に集まっているため、スケールはスキャンの深度から求めます: "
       "{0}   点: {1}"),
    ZH_HANS("锚定图像都在同一处，因此尺度取自扫描的深度：{0}   点：{1}"),
    ZH_HANT("錨定影像都在同一處，因此尺度取自掃描的深度：{0}   點：{1}"),
    KO("앵커 이미지가 한곳에 모여 있어 스케일은 스캔의 깊이에서 구합니다: {0}   점: {1}"),
    DE("Die Ankerbilder stehen an einer Stelle, daher kommt der Maßstab aus der Tiefe "
       "des Scans: {0}   Punkte: {1}"),
    FR("Les images d'ancrage sont toutes au même endroit, l'échelle vient donc de la "
       "profondeur du scan : {0}   Points : {1}"),
    ES("Las imágenes de anclaje están en un mismo sitio, así que la escala sale de la "
       "profundidad del escaneo: {0}   Puntos: {1}"),
    PT("As imagens de ancoragem estão num mesmo lugar, então a escala vem da "
       "profundidade da varredura: {0}   Pontos: {1}"),
    IT("Le immagini di ancoraggio sono tutte nello stesso punto, quindi la scala viene "
       "dalla profondità della scansione: {0}   Punti: {1}"),
    NL("De ankerbeelden staan op één plek, dus de schaal komt uit de diepte van de "
       "scan: {0}   Punten: {1}"),
    RU("Опорные изображения стоят в одном месте, поэтому масштаб берётся из глубины "
       "скана: {0}   Точек: {1}"),
    TR("Çapa görüntüleri tek bir yerde duruyor, bu yüzden ölçek taramanın "
       "derinliğinden gelir: {0}   Nokta: {1}"));

SS_MSG(model_icp_rejected,
    EN("The surface fit moved the model away from its anchors ({0} m); keeping the anchors' fit"),
    JA("表面への当てはめでモデルがアンカー画像から離れたため（{0} m）、アンカー画像"
       "による当てはめを残します"),
    ZH_HANS("表面拟合使模型偏离了它的锚定图像（{0} m）；保留锚定图像的拟合结果"),
    ZH_HANT("表面擬合使模型偏離了它的錨定影像（{0} m）；保留錨定影像的擬合結果"),
    KO("표면 맞춤이 모델을 앵커 이미지에서 멀어지게 했습니다({0} m). 앵커 이미지 "
       "맞춤을 유지합니다"),
    DE("Die Oberflächenanpassung hat das Modell von seinen Ankerbildern weggeschoben "
       "({0} m); die Anpassung an die Ankerbilder bleibt"),
    FR("L'ajustement sur la surface a éloigné le modèle de ses images d'ancrage ({0} "
       "m) ; l'ajustement sur les images d'ancrage est conservé"),
    ES("El ajuste a la superficie alejó el modelo de sus imágenes ancla ({0} m); se "
       "conserva el ajuste a las imágenes ancla"),
    PT("O ajuste à superfície afastou o modelo das suas imagens âncora ({0} m); o "
       "ajuste às imagens âncora é mantido"),
    IT("L'adattamento alla superficie ha allontanato il modello dalle sue immagini di "
       "ancoraggio ({0} m); resta l'adattamento alle immagini di ancoraggio"),
    NL("De oppervlaktepassing duwde het model weg van zijn ankerbeelden ({0} m); de "
       "passing op de ankerbeelden blijft"),
    RU("Подгонка по поверхности увела модель от её опорных изображений ({0} м); "
       "оставлена подгонка по опорным изображениям"),
    TR("Yüzey uydurması modeli çapa görüntülerinden uzaklaştırdı ({0} m); çapa "
       "görüntülerine uydurma korunuyor"));

SS_MSG(model_transform,
    EN("Scale: {0}   Rotation: {1} deg   Translation: {2} {3} {4}"),
    JA("スケール: {0}   回転: {1} 度   平行移動: {2} {3} {4}"),
    ZH_HANS("缩放：{0}   旋转：{1} 度   平移：{2} {3} {4}"),
    ZH_HANT("縮放：{0}   旋轉：{1} 度   平移：{2} {3} {4}"),
    KO("스케일: {0}   회전: {1}도   이동: {2} {3} {4}"),
    DE("Maßstab: {0}   Rotation: {1} Grad   Translation: {2} {3} {4}"),
    FR("Échelle : {0}   Rotation : {1} degrés   Translation : {2} {3} {4}"),
    ES("Escala: {0}   Rotación: {1} grados   Traslación: {2} {3} {4}"),
    PT("Escala: {0}   Rotação: {1} graus   Translação: {2} {3} {4}"),
    IT("Scala: {0}   Rotazione: {1} gradi   Traslazione: {2} {3} {4}"),
    NL("Schaal: {0}   Rotatie: {1} graden   Translatie: {2} {3} {4}"),
    RU("Масштаб: {0}   Поворот: {1} град.   Сдвиг: {2} {3} {4}"),
    TR("Ölçek: {0}   Dönme: {1} derece   Öteleme: {2} {3} {4}"));

SS_MSG(model_kept,
    EN("Model {0} is already in the scan's frame: {1} m from its surface (median)"),
    JA("モデル {0} はすでにスキャンの座標系にあります: 表面からの距離 {1} m（中央値）"),
    ZH_HANS("模型 {0} 已处于扫描坐标系中：距其表面 {1} m（中位数）"),
    ZH_HANT("模型 {0} 已位於掃描座標系中：距其表面 {1} m（中位數）"),
    KO("모델 {0}은(는) 이미 스캔 좌표계에 있습니다: 표면에서 {1} m(중앙값)"),
    DE("Modell {0} liegt bereits im Bezugssystem des Scans: {1} m von seiner "
       "Oberfläche (Median)"),
    FR("Le modèle {0} est déjà dans le repère du scan : à {1} m de sa surface "
       "(médiane)"),
    ES("El modelo {0} ya está en el sistema de referencia del escaneo: a {1} m de su "
       "superficie (mediana)"),
    PT("O modelo {0} já está no referencial da varredura: a {1} m da sua superfície "
       "(mediana)"),
    IT("Il modello {0} è già nel sistema di riferimento della scansione: a {1} m dalla "
       "sua superficie (mediana)"),
    NL("Model {0} staat al in het assenstelsel van de scan: {1} m van het oppervlak "
       "(mediaan)"),
    RU("Модель {0} уже в системе координат скана: {1} м от его поверхности (медиана)"),
    TR("Model {0} zaten taramanın koordinat sisteminde: yüzeyinden {1} m uzakta "
       "(medyan)"));

SS_MSG(model_dropped,
    EN("Model {0} has no image with a known pose in the scan and is left out"),
    JA("モデル {0} にはスキャン内での姿勢が分かっている画像がないため、除外します"),
    ZH_HANS("模型 {0} 没有在扫描中位姿已知的图像，已排除"),
    ZH_HANT("模型 {0} 沒有在掃描中位姿已知的影像，已排除"),
    KO("모델 {0}에는 스캔 안에서 자세가 알려진 이미지가 없어 제외합니다"),
    DE("Modell {0} hat kein Bild mit bekannter Pose im Scan und wird weggelassen"),
    FR("Le modèle {0} n'a aucune image dont la pose dans le scan est connue ; il est "
       "écarté"),
    ES("El modelo {0} no tiene ninguna imagen con pose conocida en el escaneo y se "
       "deja fuera"),
    PT("O modelo {0} não tem nenhuma imagem com pose conhecida na varredura e fica de "
       "fora"),
    IT("Il modello {0} non ha immagini con posa nota nella scansione e viene escluso"),
    NL("Model {0} heeft geen beeld met een bekende pose in de scan en wordt weggelaten"),
    RU("У модели {0} нет изображений с известной позой в скане, она исключена"),
    TR("Model {0}, taramada pozu bilinen hiçbir görüntü içermediği için dışarıda "
       "bırakıldı"));

SS_MSG(frames_agree,
    EN("The scans' files do not put them in one frame, but their images in the "
       "reconstruction do"),
    JA("スキャンのファイルからは 1 つの座標系にあると言えませんが、再構成内の画像は"
       "同じ座標系にあることを示しています"),
    ZH_HANS("扫描文件本身没有表明它们处于同一坐标系，但它们在重建中的图像表明是"),
    ZH_HANT("掃描檔案本身沒有表明它們處於同一座標系，但它們在重建中的影像表明是"),
    KO("스캔 파일만으로는 같은 좌표계에 있다고 할 수 없지만, 재구성 안의 이미지는 "
       "같은 좌표계에 있음을 보여 줍니다"),
    DE("Die Dateien der Scans legen sie nicht in ein Bezugssystem, ihre Bilder in der "
       "Rekonstruktion aber schon"),
    FR("Les fichiers des scans ne les placent pas dans un même repère, mais leurs "
       "images dans la reconstruction, si"),
    ES("Los archivos de los escaneos no los ponen en un mismo sistema de referencia, "
       "pero sus imágenes en la reconstrucción sí"),
    PT("Os arquivos das varreduras não as colocam num mesmo referencial, mas as suas "
       "imagens na reconstrução, sim"),
    IT("I file delle scansioni non le mettono in un unico sistema di riferimento, ma "
       "le loro immagini nella ricostruzione sì"),
    NL("De bestanden van de scans zetten ze niet in één assenstelsel, hun beelden in de "
       "reconstructie wel"),
    RU("Файлы сканов не помещают их в одну систему координат, но их изображения в "
       "реконструкции помещают"),
    TR("Taramaların dosyaları onları tek bir koordinat sistemine koymuyor, ama "
       "yeniden oluşturmadaki görüntüleri koyuyor"));

SS_MSG(frame_reference,
    EN("{0}: the frame the other scans are placed in"),
    JA("{0}: ほかのスキャンを配置する基準の座標系"),
    ZH_HANS("{0}：其他扫描放置到的基准坐标系"),
    ZH_HANT("{0}：其他掃描放置到的基準座標系"),
    KO("{0}: 다른 스캔을 배치하는 기준 좌표계"),
    DE("{0}: das Bezugssystem, in das die anderen Scans gesetzt werden"),
    FR("{0} : le repère dans lequel les autres scans sont placés"),
    ES("{0}: el sistema de referencia en el que se sitúan los demás escaneos"),
    PT("{0}: o referencial em que as outras varreduras são posicionadas"),
    IT("{0}: il sistema di riferimento in cui vengono posizionate le altre scansioni"),
    NL("{0}: het assenstelsel waarin de andere scans worden geplaatst"),
    RU("{0}: система координат, в которую помещаются остальные сканы"),
    TR("{0}: diğer taramaların yerleştirildiği koordinat sistemi"));

SS_MSG(frame_placed,
    EN("{0}: placed through its images in the reconstruction   Images: {1}   "
       "Turned: {2} deg   Moved: {3} m"),
    JA("{0}: 再構成内の画像を通して配置   画像: {1}   回転: {2} 度   移動: {3} m"),
    ZH_HANS("{0}：通过它在重建中的图像放置   图像：{1}   旋转：{2} 度   移动：{3} m"),
    ZH_HANT("{0}：透過它在重建中的影像放置   影像：{1}   旋轉：{2} 度   移動：{3} m"),
    KO("{0}: 재구성 안의 이미지를 통해 배치   이미지: {1}   회전: {2}도   이동: {3} m"),
    DE("{0}: über seine Bilder in der Rekonstruktion platziert   Bilder: {1}   "
       "Gedreht: {2} Grad   Verschoben: {3} m"),
    FR("{0} : placé par ses images dans la reconstruction   Images : {1}   "
       "Rotation : {2} deg   Déplacement : {3} m"),
    ES("{0}: situado mediante sus imágenes en la reconstrucción   Imágenes: {1}   "
       "Giro: {2} grados   Desplazamiento: {3} m"),
    PT("{0}: posicionada pelas suas imagens na reconstrução   Imagens: {1}   "
       "Rotação: {2} graus   Deslocamento: {3} m"),
    IT("{0}: posizionata tramite le sue immagini nella ricostruzione   Immagini: {1}   "
       "Rotazione: {2} gradi   Spostamento: {3} m"),
    NL("{0}: geplaatst via zijn beelden in de reconstructie   Beelden: {1}   "
       "Gedraaid: {2} graden   Verschoven: {3} m"),
    RU("{0}: размещён по своим изображениям в реконструкции   Изображений: {1}   "
       "Поворот: {2} град.   Сдвиг: {3} м"),
    TR("{0}: yeniden oluşturmadaki görüntüleriyle yerleştirildi   Görüntü: {1}   "
       "Dönme: {2} derece   Kayma: {3} m"));

SS_MSG(frame_left_out,
    EN("{0}: in a frame of its own, and the reconstruction placed none of its images; "
       "left out"),
    JA("{0}: 独自の座標系にあり、再構成はその画像を 1 枚も配置できなかったため、除外"
       "します"),
    ZH_HANS("{0}：处于自己的坐标系中，而重建未能放置它的任何图像，已排除"),
    ZH_HANT("{0}：處於自己的座標系中，而重建未能放置它的任何影像，已排除"),
    KO("{0}: 자체 좌표계에 있고 재구성이 그 이미지를 하나도 배치하지 못해 제외합니다"),
    DE("{0}: in einem eigenen Bezugssystem, und die Rekonstruktion hat keines seiner "
       "Bilder platziert; weggelassen"),
    FR("{0} : dans un repère à lui, et la reconstruction n'a placé aucune de ses "
       "images ; écarté"),
    ES("{0}: en un sistema de referencia propio, y la reconstrucción no situó ninguna "
       "de sus imágenes; se deja fuera"),
    PT("{0}: num referencial próprio, e a reconstrução não posicionou nenhuma das suas "
       "imagens; deixada de fora"),
    IT("{0}: in un sistema di riferimento proprio, e la ricostruzione non ha "
       "posizionato nessuna delle sue immagini; esclusa"),
    NL("{0}: in een eigen assenstelsel, en de reconstructie plaatste geen van zijn "
       "beelden; weggelaten"),
    RU("{0}: в собственной системе координат, а реконструкция не разместила ни одного "
       "его изображения; исключён"),
    TR("{0}: kendi koordinat sisteminde ve yeniden oluşturma görüntülerinden hiçbirini "
       "yerleştiremedi; dışarıda bırakıldı"));

SS_MSG(placed_by_scanner,
    EN("Images the reconstruction did not place, put where the scanner recorded them: {0}"),
    JA("再構成で配置できず、スキャナーが記録した位置に置いた画像: {0}"),
    ZH_HANS("重建未能定位、改放在扫描仪记录位置的图像：{0}"),
    ZH_HANT("重建未能定位、改放在掃描儀記錄位置的影像：{0}"),
    KO("재구성이 배치하지 못해 스캐너가 기록한 위치에 둔 이미지: {0}"),
    DE("Bilder, die die Rekonstruktion nicht platziert hat, an die vom Scanner "
       "aufgezeichnete Stelle gesetzt: {0}"),
    FR("Images non placées par la reconstruction, mises là où le scanner les a "
       "enregistrées : {0}"),
    ES("Imágenes que la reconstrucción no situó, colocadas donde las registró el "
       "escáner: {0}"),
    PT("Imagens que a reconstrução não posicionou, colocadas onde o scanner as "
       "registrou: {0}"),
    IT("Immagini non posizionate dalla ricostruzione, messe dove le ha registrate lo "
       "scanner: {0}"),
    NL("Beelden die de reconstructie niet plaatste, neergezet waar de scanner ze "
       "vastlegde: {0}"),
    RU("Изображений, не размещённых реконструкцией и поставленных туда, где их "
       "записал сканер: {0}"),
    TR("Yeniden oluşturmanın yerleştiremediği, tarayıcının kaydettiği yere konan "
       "görüntü: {0}"));

SS_MSG(not_placed,
    EN("Images the reconstruction did not place, left out: {0}. The anchors disagree with "
       "the fit by {1} m, so the scanner's poses would not line up with it"),
    JA("再構成で配置できず、除外した画像: {0}。アンカー画像が当てはめと {1} m "
       "ずれているため、スキャナーの姿勢では合いません"),
    ZH_HANS("重建未能定位、已排除的图像：{0}。锚定图像与拟合相差 {1} m，"
            "扫描仪的位姿与之对不齐"),
    ZH_HANT("重建未能定位、已排除的影像：{0}。錨定影像與擬合相差 {1} m，"
            "掃描儀的位姿與之對不齊"),
    KO("재구성이 배치하지 못해 제외한 이미지: {0}. 앵커 이미지가 맞춤과 {1} m "
       "어긋나 스캐너의 자세로는 맞지 않습니다"),
    DE("Bilder, die die Rekonstruktion nicht platziert hat, weggelassen: {0}. Die "
       "Ankerbilder weichen um {1} m von der Anpassung ab, die Scannerposen passen nicht dazu"),
    FR("Images non placées par la reconstruction, laissées de côté : {0}. Les images "
       "d'ancrage s'écartent de l'ajustement de {1} m ; les poses du scanner n'y "
       "correspondraient pas"),
    ES("Imágenes que la reconstrucción no situó, omitidas: {0}. Las imágenes de anclaje "
       "difieren del ajuste en {1} m, así que las poses del escáner no encajarían"),
    PT("Imagens que a reconstrução não posicionou, deixadas de fora: {0}. As imagens de "
       "ancoragem diferem do ajuste em {1} m, então as poses do scanner não se alinhariam"),
    IT("Immagini non posizionate dalla ricostruzione, escluse: {0}. Le immagini di "
       "ancoraggio si discostano dall'adattamento di {1} m, le pose dello scanner non "
       "combacerebbero"),
    NL("Beelden die de reconstructie niet plaatste, weggelaten: {0}. De ankerbeelden wijken "
       "{1} m af van de passing, dus de scannerposes zouden er niet op aansluiten"),
    RU("Изображения, которые реконструкция не разместила, исключены: {0}. Опорные "
       "изображения расходятся с подгонкой на {1} м, позы сканера с ней не совпадут"),
    TR("Yeniden oluşturmanın yerleştiremediği görüntüler dışarıda bırakıldı: {0}. Çapa "
       "görüntüleri uyumdan {1} m sapıyor, tarayıcının pozları ona oturmaz"));

SS_MSG(images_off_scan,
    EN("Images whose own points mostly miss the scan's surface, left out as misplaced by the "
       "reconstruction: {0}"),
    JA("自身の点の大半がスキャンの表面から外れており、再構成が誤った位置に置いたとして"
       "除外した画像: {0}"),
    ZH_HANS("自身的点大多偏离扫描表面、被视为重建放错位置而排除的图像：{0}"),
    ZH_HANT("自身的點大多偏離掃描表面、被視為重建放錯位置而排除的影像：{0}"),
    KO("자신의 점 대부분이 스캔 표면에서 벗어나 재구성이 잘못 배치한 것으로 보고 "
       "제외한 이미지: {0}"),
    DE("Bilder, deren eigene Punkte meist neben der Oberfläche des Scans liegen, als von "
       "der Rekonstruktion falsch platziert weggelassen: {0}"),
    FR("Images dont les propres points tombent surtout hors de la surface du scan, "
       "écartées comme mal placées par la reconstruction : {0}"),
    ES("Imágenes cuyos propios puntos quedan casi todos fuera de la superficie del "
       "escaneo, omitidas por estar mal situadas por la reconstrucción: {0}"),
    PT("Imagens cujos próprios pontos ficam quase todos fora da superfície da varredura, "
       "deixadas de fora como mal posicionadas pela reconstrução: {0}"),
    IT("Immagini i cui punti cadono per lo più fuori dalla superficie della scansione, "
       "escluse perché mal posizionate dalla ricostruzione: {0}"),
    NL("Beelden waarvan de eigen punten grotendeels naast het oppervlak van de scan "
       "liggen, weggelaten als door de reconstructie verkeerd geplaatst: {0}"),
    RU("Изображений, чьи собственные точки в основном мимо поверхности скана, исключено "
       "как неверно размещённых реконструкцией: {0}"),
    TR("Kendi noktalarının çoğu taramanın yüzeyine düşmeyen, yeniden oluşturmanın yanlış "
       "yere koyduğu için dışarıda bırakılan görüntü: {0}"));

SS_MSG(aligned_summary,
    EN("Aligned models: {0} of {1}   Images: {2}"),
    JA("位置合わせしたモデル: {0} / {1}   画像: {2}"),
    ZH_HANS("已对齐的模型：{0} / {1}   图像：{2}"),
    ZH_HANT("已對齊的模型：{0} / {1}   影像：{2}"),
    KO("정렬한 모델: {0} / {1}   이미지: {2}"),
    DE("Ausgerichtete Modelle: {0} von {1}   Bilder: {2}"),
    FR("Modèles alignés : {0} sur {1}   Images : {2}"),
    ES("Modelos alineados: {0} de {1}   Imágenes: {2}"),
    PT("Modelos alinhados: {0} de {1}   Imagens: {2}"),
    IT("Modelli allineati: {0} di {1}   Immagini: {2}"),
    NL("Uitgelijnde modellen: {0} van {1}   Beelden: {2}"),
    RU("Совмещено моделей: {0} из {1}   Изображений: {2}"),
    TR("Hizalanan model: {0} / {1}   Görüntü: {2}"));

SS_MSG(colorizing,
    EN("The scan has no colour; its points take theirs from the images"),
    JA("スキャンに色がないため、点の色は画像から取ります"),
    ZH_HANS("扫描没有颜色；其点的颜色取自图像"),
    ZH_HANT("掃描沒有顏色；其點的顏色取自影像"),
    KO("스캔에 색이 없어, 점의 색은 이미지에서 가져옵니다"),
    DE("Der Scan hat keine Farbe; seine Punkte erhalten ihre aus den Bildern"),
    FR("Le scan n'a pas de couleur ; ses points prennent la leur dans les images"),
    ES("El escaneo no tiene color; sus puntos toman el suyo de las imágenes"),
    PT("A varredura não tem cor; seus pontos tiram a cor das imagens"),
    IT("La scansione non ha colore; i suoi punti lo prendono dalle immagini"),
    NL("De scan heeft geen kleur; de punten krijgen die uit de beelden"),
    RU("Скан не содержит цвета; точки берут его из изображений"),
    TR("Taramada renk yok; noktaları rengini görüntülerden alır"));

// Read back by the dataset screen: {0} stays before {1} with a separator.
SS_MSG(progress_maps,
    EN("Depth maps: {0} / {1}"),
    JA("深度マップ: {0} / {1}"),
    ZH_HANS("深度图：{0} / {1}"),
    ZH_HANT("深度圖：{0} / {1}"),
    KO("깊이 맵: {0} / {1}"),
    DE("Tiefenkarten: {0} / {1}"),
    FR("Cartes de profondeur : {0} / {1}"),
    ES("Mapas de profundidad: {0} / {1}"),
    PT("Mapas de profundidade: {0} / {1}"),
    IT("Mappe di profondità: {0} / {1}"),
    NL("Dieptekaarten: {0} / {1}"),
    RU("Карты глубины: {0} / {1}"),
    TR("Derinlik haritaları: {0} / {1}"));

SS_MSG(seed_summary,
    EN("Seed points: {0} from the scan (voxel {1} m), {2} of the reconstruction's where the "
       "scan has none"),
    JA("初期点: スキャンから {0}（ボクセル {1} m）、スキャンに点がない場所では再構成"
       "から {2}"),
    ZH_HANS("初始点：来自扫描的 {0}（体素 {1} m），以及扫描没有点之处来自重建的 {2}"),
    ZH_HANT("初始點：來自掃描的 {0}（體素 {1} m），以及掃描沒有點之處來自重建的 {2}"),
    KO("초기 점: 스캔에서 {0}(복셀 {1} m), 스캔에 점이 없는 곳은 재구성에서 {2}"),
    DE("Startpunkte: {0} aus dem Scan (Voxel {1} m), {2} aus der Rekonstruktion, wo "
       "der Scan keine hat"),
    FR("Points de départ : {0} issus du scan (voxel {1} m), {2} de la reconstruction "
       "là où le scan n'en a pas"),
    ES("Puntos iniciales: {0} del escaneo (vóxel {1} m), {2} de la reconstrucción "
       "donde el escaneo no tiene"),
    PT("Pontos iniciais: {0} da varredura (voxel {1} m), {2} da reconstrução onde a "
       "varredura não tem"),
    IT("Punti iniziali: {0} dalla scansione (voxel {1} m), {2} della ricostruzione "
       "dove la scansione non ne ha"),
    NL("Beginpunten: {0} uit de scan (voxel {1} m), {2} uit de reconstructie waar de "
       "scan er geen heeft"),
    RU("Начальных точек: {0} из скана (воксель {1} м), {2} из реконструкции там, где "
       "в скане их нет"),
    TR("Başlangıç noktası: taramadan {0} (voksel {1} m), taramada nokta olmayan "
       "yerlerde yeniden oluşturmadan {2}"));

SS_MSG(tracks_summary,
    EN("Scan points seen by an image: {0}%   Observations: {1}"),
    JA("画像から見えるスキャン点: {0}%   観測: {1}"),
    ZH_HANS("被图像看到的扫描点：{0}%   观测：{1}"),
    ZH_HANT("被影像看到的掃描點：{0}%   觀測：{1}"),
    KO("이미지에 보이는 스캔 점: {0}%   관측: {1}"),
    DE("Von einem Bild gesehene Scanpunkte: {0} %   Beobachtungen: {1}"),
    FR("Points du scan vus par une image : {0} %   Observations : {1}"),
    ES("Puntos del escaneo vistos por una imagen: {0} %   Observaciones: {1}"),
    PT("Pontos da varredura vistos por uma imagem: {0}%   Observações: {1}"),
    IT("Punti della scansione visti da un'immagine: {0}%   Osservazioni: {1}"),
    NL("Scanpunten die een beeld ziet: {0}%   Waarnemingen: {1}"),
    RU("Точек скана, видимых на изображениях: {0}%   Наблюдений: {1}"),
    TR("Bir görüntünün gördüğü tarama noktaları: %{0}   Gözlem: {1}"));

SS_MSG(done,
    EN("Dataset written: {0}   Images: {1}   Seed points: {2}"),
    JA("データセットを書き出しました: {0}   画像: {1}   初期点: {2}"),
    ZH_HANS("数据集已写入：{0}   图像：{1}   初始点：{2}"),
    ZH_HANT("資料集已寫入：{0}   影像：{1}   初始點：{2}"),
    KO("데이터셋을 썼습니다: {0}   이미지: {1}   초기 점: {2}"),
    DE("Datensatz geschrieben: {0}   Bilder: {1}   Startpunkte: {2}"),
    FR("Jeu de données écrit : {0}   Images : {1}   Points de départ : {2}"),
    ES("Conjunto de datos escrito: {0}   Imágenes: {1}   Puntos iniciales: {2}"),
    PT("Conjunto de dados gravado: {0}   Imagens: {1}   Pontos iniciais: {2}"),
    IT("Set di dati scritto: {0}   Immagini: {1}   Punti iniziali: {2}"),
    NL("Dataset geschreven: {0}   Beelden: {1}   Beginpunten: {2}"),
    RU("Набор данных записан: {0}   Изображений: {1}   Начальных точек: {2}"),
    TR("Veri kümesi yazıldı: {0}   Görüntü: {1}   Başlangıç noktası: {2}"));

SS_MSG(rendered_views,
    EN("Views rendered from the scan: {0}"),
    JA("スキャンからレンダリングしたビュー: {0}"),
    ZH_HANS("从扫描渲染的视图：{0}"),
    ZH_HANT("從掃描渲染的視圖：{0}"),
    KO("스캔에서 렌더링한 뷰: {0}"),
    DE("Aus dem Scan gerenderte Ansichten: {0}"),
    FR("Vues rendues depuis le scan : {0}"),
    ES("Vistas renderizadas desde el escaneo: {0}"),
    PT("Vistas renderizadas a partir da varredura: {0}"),
    IT("Viste renderizzate dalla scansione: {0}"),
    NL("Uit de scan gerenderde aanzichten: {0}"),
    RU("Видов, отрендеренных из скана: {0}"),
    TR("Taramadan işlenen görünüm: {0}"));
// ===========================================================================
// The dataset screen
// ===========================================================================

SS_MSG(step_align,
    EN("Align"),         JA("位置合わせ"),    ZH_HANS("对齐"),      ZH_HANT("對齊"),
    KO("정렬"),           DE("Ausrichtung"),  FR("Alignement"),   ES("Alineación"),
    PT("Alinhamento"),   IT("Allineamento"), NL("Uitlijnen"),    RU("Совмещение"),
    TR("Hizalama"));

SS_MSG(stage_align,
    EN("Aligning with the laser scan"),
    JA("レーザースキャンに位置合わせしています"),
    ZH_HANS("正在与激光扫描对齐"),
    ZH_HANT("正在與雷射掃描對齊"),
    KO("레이저 스캔에 정렬하는 중"),
    DE("Wird am Laserscan ausgerichtet"),
    FR("Alignement sur le scan laser"),
    ES("Alineando con el escaneo láser"),
    PT("Alinhando com a varredura a laser"),
    IT("Allineamento alla scansione laser"),
    NL("Uitlijnen op de laserscan"),
    RU("Совмещение с лазерным сканом"),
    TR("Lazer taramasıyla hizalanıyor"));

SS_MSG(err_spawn,
    EN("Could not start the alignment ({0})"),
    JA("位置合わせを起動できませんでした（{0}）"),
    ZH_HANS("无法启动对齐（{0}）"),
    ZH_HANT("無法啟動對齊（{0}）"),
    KO("정렬을 시작하지 못했습니다({0})"),
    DE("Die Ausrichtung konnte nicht gestartet werden ({0})"),
    FR("Impossible de lancer l'alignement ({0})"),
    ES("No se pudo iniciar la alineación ({0})"),
    PT("Não foi possível iniciar o alinhamento ({0})"),
    IT("Non è stato possibile avviare l'allineamento ({0})"),
    NL("De uitlijning kon niet worden gestart ({0})"),
    RU("Не удалось запустить совмещение ({0})"),
    TR("Hizalama başlatılamadı ({0})"));

SS_MSG(err_failed,
    EN("The alignment with the laser scan failed; the log says why"),
    JA("レーザースキャンとの位置合わせに失敗しました。理由はログにあります"),
    ZH_HANS("与激光扫描对齐失败；原因见日志"),
    ZH_HANT("與雷射掃描對齊失敗；原因見日誌"),
    KO("레이저 스캔과의 정렬에 실패했습니다. 이유는 로그에 있습니다"),
    DE("Die Ausrichtung am Laserscan ist fehlgeschlagen; das Protokoll nennt den Grund"),
    FR("L'alignement sur le scan laser a échoué ; le journal dit pourquoi"),
    ES("La alineación con el escaneo láser falló; el registro indica por qué"),
    PT("O alinhamento com a varredura a laser falhou; o registro diz por quê"),
    IT("L'allineamento alla scansione laser non è riuscito; il registro dice perché"),
    NL("De uitlijning op de laserscan is mislukt; het logboek zegt waarom"),
    RU("Совмещение с лазерным сканом не удалось; причина -- в журнале"),
    TR("Lazer taramasıyla hizalama başarısız oldu; nedeni günlükte"));

SS_MSG(sfm_failed_scanner_poses,
    EN("The reconstruction failed; the scans' photographs keep the poses the scanner recorded"),
    JA("再構成に失敗しました。スキャンの写真はスキャナーが記録した姿勢のままにします"),
    ZH_HANS("重建失败；扫描中的照片保留扫描仪记录的位姿"),
    ZH_HANT("重建失敗；掃描中的照片保留掃描儀記錄的位姿"),
    KO("재구성에 실패했습니다. 스캔의 사진은 스캐너가 기록한 자세를 유지합니다"),
    DE("Die Rekonstruktion ist fehlgeschlagen; die Fotos der Scans behalten die vom "
       "Scanner aufgezeichneten Posen"),
    FR("La reconstruction a échoué ; les photos des scans gardent les poses "
       "enregistrées par le scanner"),
    ES("La reconstrucción falló; las fotografías de los escaneos conservan las poses "
       "que registró el escáner"),
    PT("A reconstrução falhou; as fotografias das varreduras mantêm as poses "
       "registradas pelo scanner"),
    IT("La ricostruzione non è riuscita; le fotografie delle scansioni mantengono le "
       "pose registrate dallo scanner"),
    NL("De reconstructie is mislukt; de foto's van de scans houden de poses die de "
       "scanner vastlegde"),
    RU("Реконструкция не удалась; фотографии сканов сохраняют позы, записанные "
       "сканером"),
    TR("Yeniden oluşturma başarısız oldu; taramaların fotoğrafları tarayıcının "
       "kaydettiği pozları korur"));

SS_MSG(add_scan,
    EN("Add LiDAR scan..."),
    JA("LiDAR スキャンを追加..."),
    ZH_HANS("添加 LiDAR 扫描..."),
    ZH_HANT("新增 LiDAR 掃描..."),
    KO("LiDAR 스캔 추가..."),
    DE("LiDAR-Scan hinzufügen..."),
    FR("Ajouter un scan LiDAR..."),
    ES("Añadir escaneo LiDAR..."),
    PT("Adicionar varredura LiDAR..."),
    IT("Aggiungi scansione LiDAR..."),
    NL("LiDAR-scan toevoegen..."),
    RU("Добавить скан LiDAR..."),
    TR("LiDAR taraması ekle..."));

SS_MSG(add_scan_help,
    EN("E57/LAS/PLY scan or XGRIDS PortalCam capture (.xbin): place the model in its frame; use its points, depth and normals for training.\nAn E57 or .xbin with photos can be the only input."),
    JA("E57/LAS/PLYスキャンまたはXGRIDS PortalCamのキャプチャ（.xbin）にモデルの位置・向き・尺度を合わせ、点群・深度・法線を学習に使います。\n写真付きE57や.xbinなら単独で入力できます。"),
    ZH_HANS("E57/LAS/PLY 扫描或 XGRIDS PortalCam 采集（.xbin）：将模型对齐到扫描坐标系，用其点云、深度及法线训练。\n含照片的 E57 或 .xbin 可作为唯一输入。"),
    ZH_HANT("E57/LAS/PLY 掃描或 XGRIDS PortalCam 擷取（.xbin）：將模型對齊到掃描座標系，用其點雲、深度及法線訓練。\n含相片的 E57 或 .xbin 可作為唯一輸入。"),
    KO("E57/LAS/PLY 스캔 또는 XGRIDS PortalCam 캡처(.xbin)에 모델의 위치·회전·크기를 맞추고 점·깊이·법선을 학습에 씁니다.\n사진이 있는 E57이나 .xbin은 단독 입력도 가능합니다."),
    DE("E57/LAS/PLY-Scan oder XGRIDS-PortalCam-Aufnahme (.xbin): Modell im Scanrahmen ausrichten; Punkte, Tiefe und Normalen zum Training nutzen.\nEin E57 oder .xbin mit Fotos genügt als einzige Eingabe."),
    FR("Scan E57/LAS/PLY ou capture XGRIDS PortalCam (.xbin) : aligne le modèle dans son repère ; utilise points, profondeur et normales.\nUn E57 ou .xbin avec photos peut être la seule entrée."),
    ES("Escaneo E57/LAS/PLY o captura XGRIDS PortalCam (.xbin): alinea el modelo y usa sus puntos, profundidad y normales para entrenar.\nUn E57 o .xbin con fotos puede ser la única entrada."),
    PT("Varredura E57/LAS/PLY ou captura XGRIDS PortalCam (.xbin): alinha o modelo e usa pontos, profundidade e normais no treino.\nUm E57 ou .xbin com fotos pode ser a única entrada."),
    IT("Scansione E57/LAS/PLY o acquisizione XGRIDS PortalCam (.xbin): allinea il modello e usa punti, profondità e normali nel training.\nUn E57 o .xbin con foto può essere l’unico input."),
    NL("E57/LAS/PLY-scan of XGRIDS PortalCam-opname (.xbin): lijn het model uit op het scanframe; gebruik punten, diepte en normalen voor training.\nEen E57 of .xbin met foto’s kan de enige invoer zijn."),
    RU("Скан E57/LAS/PLY или съёмка XGRIDS PortalCam (.xbin): совмещает модель с его системой; использует точки, глубину и нормали в обучении.\nE57 или .xbin со снимками может быть единственным входом."),
    TR("E57/LAS/PLY taraması veya XGRIDS PortalCam çekimi (.xbin): modeli tarama çerçevesine hizalar; nokta, derinlik ve normalleri eğitimde kullanır.\nFotoğraflı E57 veya .xbin tek girdi olabilir."));

SS_MSG(pick_scan,
    EN("Choose laser scans"), JA("レーザースキャンを選択"), ZH_HANS("选择激光扫描"),
    ZH_HANT("選擇雷射掃描"), KO("레이저 스캔 선택"), DE("Laserscans wählen"),
    FR("Choisir des scans laser"), ES("Elegir escaneos láser"),
    PT("Escolher varreduras a laser"), IT("Scegli scansioni laser"),
    NL("Kies laserscans"), RU("Выберите лазерные сканы"), TR("Lazer taramalarını seç"));

SS_MSG(scan_row,
    EN("Points: {0}   Photographs: {1}"),
    JA("点: {0}   写真: {1}"),
    ZH_HANS("点：{0}   照片：{1}"),
    ZH_HANT("點：{0}   照片：{1}"),
    KO("점: {0}   사진: {1}"),
    DE("Punkte: {0}   Fotos: {1}"),
    FR("Points : {0}   Photos : {1}"),
    ES("Puntos: {0}   Fotografías: {1}"),
    PT("Pontos: {0}   Fotografias: {1}"),
    IT("Punti: {0}   Fotografie: {1}"),
    NL("Punten: {0}   Foto's: {1}"),
    RU("Точек: {0}   Фотографий: {1}"),
    TR("Nokta: {0}   Fotoğraf: {1}"));

SS_MSG(scan_unreadable,
    EN("{0}: {1}"), JA("{0}: {1}"), ZH_HANS("{0}：{1}"), ZH_HANT("{0}：{1}"),
    KO("{0}: {1}"), DE("{0}: {1}"), FR("{0} : {1}"), ES("{0}: {1}"), PT("{0}: {1}"),
    IT("{0}: {1}"), NL("{0}: {1}"), RU("{0}: {1}"), TR("{0}: {1}"));

SS_MSG(scan_is_splat,
    EN("{0} is a Gaussian splat, a finished reconstruction, not a laser scan. Add the "
       "point cloud the scanner wrote (E57, LAS or PLY)."),
    JA("{0} はガウシアンスプラット（再構成の結果）で、レーザースキャンではありません。"
       "スキャナーが書き出した点群（E57、LAS、PLY）を追加してください。"),
    ZH_HANS("{0} 是高斯泼溅（重建的结果），不是激光扫描。请加入扫描仪写出的点云"
            "（E57、LAS 或 PLY）。"),
    ZH_HANT("{0} 是高斯潑濺（重建的結果），不是雷射掃描。請加入掃描器寫出的點雲"
            "（E57、LAS 或 PLY）。"),
    KO("{0}은(는) 가우시안 스플랫(재구성 결과)이며 레이저 스캔이 아닙니다. 스캐너가 "
       "기록한 포인트 클라우드(E57, LAS 또는 PLY)를 추가하세요."),
    DE("{0} ist ein Gaussian Splat, eine fertige Rekonstruktion, kein Laserscan. Die "
       "Punktwolke hinzufügen, die der Scanner geschrieben hat (E57, LAS oder PLY)."),
    FR("{0} est un splat gaussien, une reconstruction terminée, pas un scan laser. "
       "Ajoutez le nuage de points écrit par le scanner (E57, LAS ou PLY)."),
    ES("{0} es un splat gaussiano, una reconstrucción terminada, no un escaneo láser. "
       "Añada la nube de puntos que escribió el escáner (E57, LAS o PLY)."),
    PT("{0} é um splat gaussiano, uma reconstrução pronta, não uma varredura a laser. "
       "Adicione a nuvem de pontos que o scanner gravou (E57, LAS ou PLY)."),
    IT("{0} è uno splat gaussiano, una ricostruzione finita, non una scansione laser. "
       "Aggiungere la nuvola di punti scritta dallo scanner (E57, LAS o PLY)."),
    NL("{0} is een Gaussian splat, een voltooide reconstructie, geen laserscan. Voeg "
       "de puntenwolk toe die de scanner schreef (E57, LAS of PLY)."),
    RU("{0} — гауссов сплат, готовая реконструкция, а не лазерный скан. Добавьте "
       "облако точек, записанное сканером (E57, LAS или PLY)."),
    TR("{0} bir Gauss splat'i, yani bitmiş bir yeniden oluşturma; lazer taraması "
       "değil. Tarayıcının yazdığı nokta bulutunu ekleyin (E57, LAS veya PLY)."));

SS_MSG(step_scan_photos,
    EN("Scan photos"),            JA("スキャンの写真"),       ZH_HANS("扫描照片"),
    ZH_HANT("掃描照片"),          KO("스캔 사진"),            DE("Scan-Fotos"),
    FR("Photos du scan"),         ES("Fotos del escaneo"),    PT("Fotos da varredura"),
    IT("Foto della scansione"),   NL("Scanfoto's"),           RU("Фото скана"),
    TR("Tarama fotoğrafları"));

SS_MSG(scan_photos_taking,
    EN("Taking the photographs out of {0}..."),
    JA("{0} から写真を取り出しています..."),
    ZH_HANS("正在从 {0} 中取出照片..."),
    ZH_HANT("正在從 {0} 中取出照片..."),
    KO("{0}에서 사진을 꺼내는 중..."),
    DE("Die Fotos werden aus {0} geholt..."),
    FR("Extraction des photos de {0}..."),
    ES("Sacando las fotografías de {0}..."),
    PT("Tirando as fotografias de {0}..."),
    IT("Estrazione delle fotografie da {0}..."),
    NL("De foto's worden uit {0} gehaald..."),
    RU("Фотографии извлекаются из {0}..."),
    TR("Fotoğraflar {0} içinden çıkarılıyor..."));

SS_MSG(scan_photos_place_only,
    EN("Use capture photos only for alignment"),
    JA("キャプチャの写真は位置合わせだけに使う"),
    ZH_HANS("采集的照片只用于对齐"),
    ZH_HANT("擷取的照片只用於對齊"),
    KO("캡처 사진은 정렬에만 사용"),
    DE("Fotos der Aufnahme nur zum Ausrichten verwenden"),
    FR("Utiliser les photos de la capture seulement pour l'alignement"),
    ES("Usar las fotos de la captura solo para alinear"),
    PT("Usar as fotos da captura só para alinhar"),
    IT("Usare le foto dell'acquisizione solo per l'allineamento"),
    NL("Foto's van de opname alleen gebruiken om uit te lijnen"),
    RU("Фото съёмки только для выравнивания"),
    TR("Çekim fotoğraflarını yalnızca hizalama için kullan"));

SS_MSG(scan_photos_place_only_help,
    EN("Capture photos join reconstruction and put your own photos in scan frame. Training then "
       "drops capture photos. It uses only your own photos and scan points."),
    JA("キャプチャの写真は再構成に加わり、あなたの写真をスキャンの座標系に置きます。その後、学習では"
       "キャプチャの写真を外し、あなたの写真とスキャンの点だけを使います。"),
    ZH_HANS("采集的照片参与重建，把您自己的照片放进扫描的坐标系。之后训练去掉采集的照片，只使用您自己的"
            "照片和扫描的点。"),
    ZH_HANT("擷取的照片參與重建，把您自己的照片放進掃描的座標系。之後訓練去掉擷取的照片，只使用您自己的"
            "照片和掃描的點。"),
    KO("캡처 사진은 재구성에 참여해 직접 찍은 사진을 스캔 좌표계에 놓습니다. 그 뒤 학습은 캡처 사진을 "
       "빼고 직접 찍은 사진과 스캔의 점만 씁니다."),
    DE("Die Fotos der Aufnahme bringen Ihre Fotos in das Bezugssystem des Scans. Das Training lässt sie danach weg und nutzt nur Ihre Fotos und die Scanpunkte."),
    FR("Les photos de la capture placent vos photos dans le repère du scan. L'entraînement les retire ensuite et n'utilise que vos photos et les points du scan."),
    ES("Las fotos de la captura colocan sus fotos en el marco del escaneo. Después, el entrenamiento las quita y usa solo sus fotos y los puntos del escaneo."),
    PT("As fotos da captura colocam as suas fotos no referencial da varredura. Depois o treino retira-as e usa só as suas fotos e os pontos da varredura."),
    IT("Le foto dell'acquisizione mettono le tue foto nel sistema della scansione. Poi l'addestramento le toglie e usa solo le tue foto e i punti della scansione."),
    NL("De foto's van de opname plaatsen uw foto's in het assenstelsel van de scan. Daarna laat de training ze weg en gebruikt alleen uw foto's en de scanpunten."),
    RU("Фото съёмки участвуют в реконструкции и ставят ваши собственные фото в систему координат "
       "скана. Затем обучение убирает их и использует только ваши фото и точки скана."),
    TR("Çekim fotoğrafları sizin fotoğraflarınızı tarama sistemine yerleştirir. Sonra eğitim onları çıkarır ve yalnızca sizin fotoğraflarınızla tarama noktalarını kullanır."));

SS_MSG(capture_camera_fps_help,
    EN("Photos per second to take from camera. Camera records {0}. Lower rate gives fewer photos and "
       "faster run. 0 keeps every photo."),
    JA("このカメラから取る毎秒の写真数です。カメラの記録は {0} です。低くすると写真が減り、処理が"
       "速くなります。0 ですべての写真を使います。"),
    ZH_HANS("从该相机每秒取用的照片数。相机记录 {0}。降低后照片更少，运行更快。0 保留全部照片。"),
    ZH_HANT("從該相機每秒取用的照片數。相機記錄 {0}。降低後照片更少，執行更快。0 保留全部照片。"),
    KO("이 카메라에서 초당 가져올 사진 수입니다. 카메라는 {0}로 기록합니다. 낮추면 사진이 줄고 "
       "실행이 빨라집니다. 0은 모든 사진을 씁니다."),
    DE("Fotos pro Sekunde von dieser Kamera. Die Kamera nimmt {0} auf. Ein niedrigerer Wert gibt "
       "weniger Fotos und einen schnelleren Lauf. 0 behält jedes Foto."),
    FR("Photos par seconde prises de cette caméra. La caméra enregistre {0}. Une valeur plus basse "
       "donne moins de photos et un calcul plus rapide. 0 garde chaque photo."),
    ES("Fotos por segundo que se toman de esta cámara. La cámara graba {0}. Un valor más bajo da "
       "menos fotos y una ejecución más rápida. 0 conserva cada foto."),
    PT("Fotos por segundo tiradas desta câmera. A câmera grava {0}. Um valor menor dá menos fotos e "
       "uma execução mais rápida. 0 mantém todas as fotos."),
    IT("Foto al secondo prese da questa fotocamera. La fotocamera registra {0}. Un valore più basso "
       "dà meno foto e un'esecuzione più rapida. 0 tiene ogni foto."),
    NL("Foto's per seconde van deze camera. De camera neemt {0} op. Een lagere waarde geeft minder "
       "foto's en een snellere run. 0 houdt elke foto."),
    RU("Фото в секунду с этой камеры. Камера снимает {0}. Меньшее значение даёт меньше фото и более "
       "быстрый запуск. 0 оставляет каждое фото."),
    TR("Bu kameradan saniyede alınacak fotoğraf sayısı. Kamera {0} kaydeder. Daha düşük bir değer "
       "daha az fotoğraf ve daha hızlı bir çalışma verir. 0 her fotoğrafı tutar."));

SS_MSG(scan_photos_extracting,
    EN("Taking the photographs out of {0} to draw on..."),
    JA("{0} から描画用に写真を取り出しています..."),
    ZH_HANS("正在从 {0} 中取出照片以便绘制..."),
    ZH_HANT("正在從 {0} 中取出照片以便繪製..."),
    KO("그리기 위해 {0}에서 사진을 꺼내는 중..."),
    DE("Die Fotos werden zum Zeichnen aus {0} geholt..."),
    FR("Extraction des photos de {0} pour dessiner dessus..."),
    ES("Sacando las fotografías de {0} para dibujar sobre ellas..."),
    PT("Tirando as fotografias de {0} para desenhar nelas..."),
    IT("Estrazione delle fotografie da {0} per disegnarci sopra..."),
    NL("De foto's worden uit {0} gehaald om op te tekenen..."),
    RU("Фотографии извлекаются из {0}, чтобы рисовать на них..."),
    TR("Üzerine çizmek için fotoğraflar {0} içinden çıkarılıyor..."));

SS_MSG(scan_photos_failed,
    EN("Could not take the photographs out of {0}: {1}"),
    JA("{0} から写真を取り出せませんでした: {1}"),
    ZH_HANS("无法从 {0} 中取出照片：{1}"),
    ZH_HANT("無法從 {0} 中取出照片：{1}"),
    KO("{0}에서 사진을 꺼낼 수 없습니다: {1}"),
    DE("Die Fotos konnten nicht aus {0} geholt werden: {1}"),
    FR("Impossible d'extraire les photos de {0} : {1}"),
    ES("No se pudieron sacar las fotografías de {0}: {1}"),
    PT("Não foi possível tirar as fotografias de {0}: {1}"),
    IT("Impossibile estrarre le fotografie da {0}: {1}"),
    NL("Kon de foto's niet uit {0} halen: {1}"),
    RU("Не удалось извлечь фотографии из {0}: {1}"),
    TR("Fotoğraflar {0} içinden çıkarılamadı: {1}"));

SS_MSG(use_scan_photos,
    EN("Reconstruct the scans' photographs with the rest"),
    JA("スキャンの写真もほかの画像と一緒に再構成する"),
    ZH_HANS("把扫描中的照片与其余图像一起重建"),
    ZH_HANT("把掃描中的照片與其餘影像一起重建"),
    KO("스캔의 사진도 나머지와 함께 재구성"),
    DE("Die Fotos der Scans mit den übrigen rekonstruieren"),
    FR("Reconstruire les photos des scans avec le reste"),
    ES("Reconstruir las fotografías de los escaneos con el resto"),
    PT("Reconstruir as fotografias das varreduras com o restante"),
    IT("Ricostruire le fotografie delle scansioni insieme al resto"),
    NL("De foto's van de scans met de rest reconstrueren"),
    RU("Включить фотографии сканов в реконструкцию"),
    TR("Taramaların fotoğraflarını diğerleriyle birlikte yeniden oluştur"));

SS_MSG(use_scan_photos_help,
    EN("Their poses in the scan are what places the reconstruction.\n- Off, or for a scan without photographs, views rendered from where the scanner stood do that instead.\n- Photographs the reconstruction cannot place keep the scanner's pose."),
    JA("スキャン内でのこれらの写真の姿勢が、再構成の位置を決めます。\n- オフにした場合や写真のないスキャンでは、スキャナーを据えた位置からレンダリングしたビューがその役を担います。\n- 再構成で配置できなかった写真は、スキャナーの姿勢のままにします。"),
    ZH_HANS("这些照片在扫描中的位姿决定重建的位置。 关闭此项，或扫描没有照片时，改由从扫描仪所在位置渲染的视图来完成这件事。\n重建无法定位的照片保留扫描仪的位姿。"),
    ZH_HANT("這些照片在掃描中的位姿決定重建的位置。 關閉此項，或掃描沒有照片時，改由從掃描儀所在位置渲染的視圖來完成這件事。\n重建無法定位的照片保留掃描儀的位姿。"),
    KO("스캔 안에서의 이 사진들의 자세가 재구성의 위치를 정합니다.\n- 끄거나 사진이 없는 스캔이면, 스캐너가 서 있던 곳에서 렌더링한 뷰가 대신 그 일을 합니다.\n- 재구성이 배치하지 못한 사진은 스캐너의 자세를 유지합니다."),
    DE("Ihre Posen im Scan sind es, die die Rekonstruktion platzieren.\n- Ausgeschaltet, oder bei einem Scan ohne Fotos, übernehmen das vom Scannerstandpunkt gerenderte Ansichten.\n- Fotos, die die Rekonstruktion nicht platzieren kann, behalten die Pose des Scanners."),
    FR("Ce sont leurs poses dans le scan qui placent la reconstruction.\n- Désactivé, ou pour un scan sans photos, des vues rendues depuis l'emplacement du scanner s'en chargent.\n- Les photos que la reconstruction ne peut pas placer gardent la pose du scanner."),
    ES("Sitúa el modelo con las poses de las fotos; desactivado o sin fotos, usa vistas desde el escáner.\nLas fotos que no se pueden registrar conservan la pose del escáner."),
    PT("As poses delas na varredura são o que posiciona a reconstrução.\n- Desligado, ou numa varredura sem fotografias, vistas renderizadas de onde o scanner estava fazem isso.\n- As fotografias que a reconstrução não consegue posicionar mantêm a pose do scanner."),
    IT("Sono le loro pose nella scansione a posizionare la ricostruzione.\n- Se disattivato, o per una scansione senza fotografie, lo fanno viste renderizzate da dove stava lo scanner.\n- Le fotografie che la ricostruzione non riesce a posizionare mantengono la posa dello scanner."),
    NL("Plaatst het model via fotoposes; uitgeschakeld of zonder foto’s via aanzichten vanaf de scanner.\nFoto’s die niet geregistreerd kunnen worden behouden de scannerpose."),
    RU("Именно их позы в скане задают положение реконструкции.\n- Если выключено или в скане нет фотографий, это делают виды, отрендеренные с мест, где стоял сканер.\n- Фотографии, которые реконструкция не смогла разместить, сохраняют позу сканера."),
    TR("Yeniden oluşturmayı yerleştiren, bu fotoğrafların taramadaki pozlarıdır.\n- Kapalıysa ya da fotoğrafsız bir taramada, bunu tarayıcının durduğu yerden işlenen görünümler yapar.\n- Yeniden oluşturmanın yerleştiremediği fotoğraflar tarayıcının pozunu korur."));

SS_MSG(keep_scanner_poses,
    EN("Use the scanner's camera poses as they are"),
    JA("スキャナーのカメラ姿勢をそのまま使う"),
    ZH_HANS("直接使用扫描仪记录的相机位姿"),
    ZH_HANT("直接使用掃描儀記錄的相機位姿"),
    KO("스캐너의 카메라 자세를 그대로 사용"),
    DE("Die Kameraposen des Scanners unverändert verwenden"),
    FR("Utiliser telles quelles les poses de caméra du scanner"),
    ES("Usar tal cual las poses de cámara del escáner"),
    PT("Usar as poses de câmera do scanner como estão"),
    IT("Usare così come sono le pose della fotocamera dello scanner"),
    NL("De cameraposes van de scanner gebruiken zoals ze zijn"),
    RU("Использовать позы камер сканера как есть"),
    TR("Tarayıcının kamera pozlarını olduğu gibi kullan"));

SS_MSG(keep_scanner_poses_help,
    EN("With scan photos as the only input, use recorded poses without reconstruction. Untick to reconstruct more accurate camera poses."),
    JA("スキャン写真だけを入力するとき、記録姿勢を使って再構成を省きます。外すと写真からより正確な姿勢を再構成します。"),
    ZH_HANS("仅输入扫描照片时，使用记录位姿，不运行重建。取消勾选可通过重建获得更准确的相机位姿。"),
    ZH_HANT("僅輸入掃描相片時，使用記錄位姿，不執行重建。取消勾選可透過重建獲得更準確的相機位姿。"),
    KO("스캔 사진만 입력할 때 기록된 자세로 재구성을 생략합니다. 해제하면 더 정확한 카메라 자세를 재구성합니다."),
    DE("Bei Scanfotos als einziger Eingabe gespeicherte Posen ohne Rekonstruktion nutzen. Abwählen, um genauere Kameraposen zu rekonstruieren."),
    FR("Avec seulement les photos du scan, utilise les poses enregistrées sans reconstruction. Décochez pour reconstruire des poses plus précises."),
    ES("Si solo hay fotos del escaneo, usa las poses grabadas sin reconstruir. Desmarca para reconstruir poses de cámara más precisas."),
    PT("Com apenas fotos da varredura, usa poses gravadas sem reconstruir. Desmarque para reconstruir poses de câmera mais precisas."),
    IT("Con sole foto della scansione, usa le pose registrate senza ricostruzione. Deseleziona per ricostruire pose più precise."),
    NL("Bij alleen scanfoto’s: gebruik vastgelegde poses zonder reconstructie. Uitvinken om nauwkeurigere cameraposes te reconstrueren."),
    RU("Если вход — только снимки скана, использует записанные позы без реконструкции. Снимите флажок для уточнения поз реконструкцией."),
    TR("Yalnız tarama fotoğrafları varsa kayıtlı pozları kullanır, yeniden oluşturmaz. Daha doğru kamera pozları için işareti kaldırın."));

SS_MSG(scanner_poses_used,
    EN("The scans' photographs are the only images: they keep the poses the scanner "
       "recorded, and no reconstruction runs"),
    JA("画像はスキャンの写真だけです。スキャナーが記録した姿勢のまま使い、再構成は"
       "行いません"),
    ZH_HANS("图像只有扫描中的照片：保留扫描仪记录的位姿，不运行重建"),
    ZH_HANT("影像只有掃描中的照片：保留掃描儀記錄的位姿，不執行重建"),
    KO("이미지가 스캔의 사진뿐입니다. 스캐너가 기록한 자세를 유지하고 재구성은 하지 "
       "않습니다"),
    DE("Die Fotos der Scans sind die einzigen Bilder: Sie behalten die vom Scanner "
       "aufgezeichneten Posen, und keine Rekonstruktion läuft"),
    FR("Les photos des scans sont les seules images : elles gardent les poses "
       "enregistrées par le scanner, et aucune reconstruction n'a lieu"),
    ES("Las fotografías de los escaneos son las únicas imágenes: conservan las poses "
       "que registró el escáner y no se ejecuta ninguna reconstrucción"),
    PT("As fotografias das varreduras são as únicas imagens: mantêm as poses "
       "registradas pelo scanner, e nenhuma reconstrução é executada"),
    IT("Le fotografie delle scansioni sono le uniche immagini: mantengono le pose "
       "registrate dallo scanner e non si esegue alcuna ricostruzione"),
    NL("De foto's van de scans zijn de enige beelden: ze houden de poses die de scanner "
       "vastlegde, en er draait geen reconstructie"),
    RU("Фотографии сканов -- единственные изображения: они сохраняют позы, записанные "
       "сканером, и реконструкция не запускается"),
    TR("Tek görüntüler taramaların fotoğrafları: tarayıcının kaydettiği pozları "
       "korurlar ve yeniden oluşturma çalışmaz"));

SS_MSG(scan_in_frame,
    EN("Cameras already line up with scan"),
    JA("カメラはすでにスキャンと位置が合っている"),
    ZH_HANS("相机已与扫描对齐"),
    ZH_HANT("相機已與掃描對齊"),
    KO("카메라가 이미 스캔과 맞춰져 있음"),
    DE("Die Kameras passen schon zum Scan"),
    FR("Les caméras sont déjà alignées sur le scan"),
    ES("Las cámaras ya están alineadas con el escaneo"),
    PT("As câmeras já estão alinhadas com a varredura"),
    IT("Le fotocamere sono già allineate alla scansione"),
    NL("De camera's staan al gelijk met de scan"),
    RU("Камеры уже совмещены со сканом"),
    TR("Kameralar taramayla zaten hizalı"));

SS_MSG(scan_in_frame_help,
    EN("For an export whose cameras the scanner's software placed, such as an XGRIDS one: nothing is moved, the scan only adds seed points, depth and normals."),
    JA("XGRIDS のエクスポートのように、スキャナーのソフトウェアがカメラを配置したものに使います。\n何も動かさず、スキャンは初期点、深度、法線を加えるだけです。"),
    ZH_HANS("用于相机由扫描仪软件定位的导出数据，例如 XGRIDS 的导出：不移动任何东西，扫描只添加初始点、深度和法线。"),
    ZH_HANT("用於相機由掃描儀軟體定位的匯出資料，例如 XGRIDS 的匯出：不移動任何東西，掃描只加入初始點、深度和法線。"),
    KO("XGRIDS 내보내기처럼 스캐너 소프트웨어가 카메라를 배치한 내보내기용입니다.\n아무것도 옮기지 않고, 스캔은 초기 점, 깊이, 법선만 더합니다."),
    DE("Für vom Scanner platzierte Kameras, etwa XGRIDS: Der Scan fügt nur Startpunkte, Tiefe und Normalen hinzu. Nichts wird verschoben."),
    FR("Pour des caméras placées par le scanner, comme XGRIDS : le scan ajoute points initiaux, profondeur et normales sans rien déplacer."),
    ES("Para una exportación cuyas cámaras situó el software del escáner, como una de XGRIDS: no se mueve nada, el escaneo solo añade puntos iniciales, profundidad y normales."),
    PT("Para uma exportação cujas câmeras o software do scanner posicionou, como uma da XGRIDS: nada é movido, a varredura só acrescenta pontos iniciais, profundidade e normais."),
    IT("Per camere posizionate dal software dello scanner, come XGRIDS: aggiunge punti iniziali, profondità e normali senza spostare nulla."),
    NL("Voor camera’s geplaatst door scannersoftware, zoals XGRIDS: de scan voegt beginpunten, diepte en normalen toe zonder iets te verplaatsen."),
    RU("Для экспорта, камеры которого расставило ПО сканера, например экспорта XGRIDS: ничего не сдвигается, скан лишь добавляет начальные точки, глубину и нормали."),
    TR("XGRIDS gibi kameraları tarayıcı yazılımınca yerleştirilmiş veriler için: tarama nokta, derinlik ve normal ekler; hiçbir şey taşınmaz."));

SS_MSG(scan_needs_builtin,
    EN("Laser scans need the built-in reconstruction"),
    JA("レーザースキャンには内蔵の再構成が必要です"),
    ZH_HANS("激光扫描需要使用内置重建"),
    ZH_HANT("雷射掃描需要使用內建重建"),
    KO("레이저 스캔에는 내장 재구성이 필요합니다"),
    DE("Laserscans brauchen die eingebaute Rekonstruktion"),
    FR("Les scans laser demandent la reconstruction intégrée"),
    ES("Los escaneos láser requieren la reconstrucción integrada"),
    PT("As varreduras a laser exigem a reconstrução integrada"),
    IT("Le scansioni laser richiedono la ricostruzione integrata"),
    NL("Laserscans vereisen de ingebouwde reconstructie"),
    RU("Для лазерных сканов нужна встроенная реконструкция"),
    TR("Lazer taramaları yerleşik yeniden oluşturmayı gerektirir"));

SS_MSG(xgrids_found,
    EN("An XGRIDS export: its fisheye reconstruction and its point cloud were added, and its "
       "masks are read the other way round."),
    JA("XGRIDS のエクスポートです。魚眼の再構成と点群を追加し、マスクは白黒を逆に"
       "して読みます。"),
    ZH_HANS("这是 XGRIDS 导出：已添加它的鱼眼重建和点云，它的蒙版按反相读取。"),
    ZH_HANT("這是 XGRIDS 匯出：已加入它的魚眼重建和點雲，它的遮罩按反相讀取。"),
    KO("XGRIDS 내보내기입니다. 어안 재구성과 포인트 클라우드를 추가했고, 마스크는 "
       "반대로 읽습니다."),
    DE("Ein XGRIDS-Export: Seine Fischaugen-Rekonstruktion und seine Punktwolke wurden "
       "hinzugefügt, und seine Masken werden umgekehrt gelesen."),
    FR("Un export XGRIDS : sa reconstruction fisheye et son nuage de points ont été "
       "ajoutés, et ses masques sont lus à l'inverse."),
    ES("Una exportación de XGRIDS: se añadieron su reconstrucción de ojo de pez y su "
       "nube de puntos, y sus máscaras se leen al revés."),
    PT("Uma exportação da XGRIDS: a reconstrução olho de peixe e a nuvem de pontos "
       "foram adicionadas, e as máscaras são lidas ao contrário."),
    IT("Un'esportazione XGRIDS: sono state aggiunte la sua ricostruzione fisheye e la "
       "sua nuvola di punti, e le sue maschere vengono lette al contrario."),
    NL("Een XGRIDS-export: de fisheye-reconstructie en de puntenwolk zijn toegevoegd, "
       "en de maskers worden omgekeerd gelezen."),
    RU("Экспорт XGRIDS: добавлены его реконструкция «рыбий глаз» и облако точек, а "
       "маски читаются наоборот."),
    TR("Bir XGRIDS dışa aktarımı: balıkgözü yeniden oluşturması ve nokta bulutu "
       "eklendi, maskeleri de ters okunuyor."));

SS_MSG(scan_replaces_geometry,
    EN("Depth and normals come from the laser scan, not from a model"),
    JA("深度と法線は、モデルではなくレーザースキャンから得ます"),
    ZH_HANS("深度与法线来自激光扫描，而不是模型"),
    ZH_HANT("深度與法線來自雷射掃描，而不是模型"),
    KO("깊이와 법선은 모델이 아니라 레이저 스캔에서 얻습니다"),
    DE("Tiefe und Normalen kommen aus dem Laserscan, nicht aus einem Modell"),
    FR("La profondeur et les normales viennent du scan laser, pas d'un modèle"),
    ES("La profundidad y las normales vienen del escaneo láser, no de un modelo"),
    PT("A profundidade e as normais vêm da varredura a laser, não de um modelo"),
    IT("Profondità e normali vengono dalla scansione laser, non da un modello"),
    NL("Diepte en normalen komen uit de laserscan, niet uit een model"),
    RU("Глубина и нормали берутся из лазерного скана, а не из модели"),
    TR("Derinlik ve normaller bir modelden değil, lazer taramasından gelir"));

SS_MSG(extracted,
    EN("Images taken from the scan: {0}   Pinhole: {1}   Panorama: {2}"),
    JA("スキャンから取り出した画像: {0}   ピンホール: {1}   パノラマ: {2}"),
    ZH_HANS("从扫描取出的图像：{0}   针孔：{1}   全景：{2}"),
    ZH_HANT("從掃描取出的影像：{0}   針孔：{1}   全景：{2}"),
    KO("스캔에서 꺼낸 이미지: {0}   핀홀: {1}   파노라마: {2}"),
    DE("Aus dem Scan entnommene Bilder: {0}   Lochkamera: {1}   Panorama: {2}"),
    FR("Images tirées du scan : {0}   Sténopé : {1}   Panorama : {2}"),
    ES("Imágenes extraídas del escaneo: {0}   Estenopeicas: {1}   Panorámicas: {2}"),
    PT("Imagens tiradas da varredura: {0}   Pinhole: {1}   Panorama: {2}"),
    IT("Immagini estratte dalla scansione: {0}   Stenopeiche: {1}   Panorami: {2}"),
    NL("Uit de scan gehaalde beelden: {0}   Pinhole: {1}   Panorama: {2}"),
    RU("Изображений извлечено из скана: {0}   Обскура: {1}   Панорамы: {2}"),
    TR("Taramadan alınan görüntü: {0}   İğne deliği: {1}   Panorama: {2}"));

SS_MSG(extracted_xbin,
    EN("Images taken from the scan: {0}   Pinhole: {1}   Fisheye: {2}"),
    JA("スキャンから取り出した画像: {0}   ピンホール: {1}   魚眼: {2}"),
    ZH_HANS("从扫描取出的图像：{0}   针孔：{1}   鱼眼：{2}"),
    ZH_HANT("從掃描取出的影像：{0}   針孔：{1}   魚眼：{2}"),
    KO("스캔에서 꺼낸 이미지: {0}   핀홀: {1}   어안: {2}"),
    DE("Aus dem Scan entnommene Bilder: {0}   Lochkamera: {1}   Fisheye: {2}"),
    FR("Images tirées du scan : {0}   Sténopé : {1}   Fisheye : {2}"),
    ES("Imágenes extraídas del escaneo: {0}   Estenopeicas: {1}   Ojo de pez: {2}"),
    PT("Imagens tiradas da varredura: {0}   Pinhole: {1}   Olho de peixe: {2}"),
    IT("Immagini estratte dalla scansione: {0}   Stenopeiche: {1}   Fisheye: {2}"),
    NL("Uit de scan gehaalde beelden: {0}   Pinhole: {1}   Fisheye: {2}"),
    RU("Изображений извлечено из скана: {0}   Обскура: {1}   Фишай: {2}"),
    TR("Taramadan alınan görüntü: {0}   İğne deliği: {1}   Balıkgözü: {2}"));

}  // namespace lidar
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
