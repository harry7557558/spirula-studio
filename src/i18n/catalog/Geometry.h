#pragma once

// What `spirula geometry` says: its --help and every line a run prints.
//
// Flag names, the words they accept (`auto`, `png`, `mm`) and the model ids
// are identifiers and stay as they are in every language -- they are what the
// reader types. What is translated is the sentence beside each one.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace geometry {

SS_MSG(tagline,
    EN("Depth and surface normals for a dataset, from a single image each"),
    JA("各画像 1 枚から、データセットの深度と面法線を推定します"),
    ZH_HANS("从每张单幅图像估计数据集的深度与表面法线"),
    ZH_HANT("從每張單幅影像估計資料集的深度與表面法線"),
    KO("이미지 한 장씩으로 데이터셋의 깊이와 표면 법선을 추정합니다"),
    DE("Tiefe und Oberflächennormalen für einen Datensatz, je aus einem Bild"),
    FR("Profondeur et normales de surface d'un jeu de données, image par image"),
    ES("Profundidad y normales de superficie de un conjunto, imagen a imagen"),
    PT("Profundidade e normais de superfície de um conjunto, imagem a imagem"),
    IT("Profondità e normali di superficie di un insieme, immagine per immagine"),
    NL("Diepte en oppervlaktenormalen voor een dataset, per losse afbeelding"),
    RU("Глубина и нормали поверхности для набора, по одному кадру за раз"),
    TR("Bir veri kümesi için derinlik ve yüzey normalleri, tek tek görüntülerden"));

SS_MSG(usage_target,
    EN("Dataset folder: transforms.json, COLMAP sparse/ or Metashape export.\nWrites normals/ and depths/ without changing transforms.json."),
    JA("データセットフォルダ：transforms.json、COLMAP sparse/、Metashape書き出し。\nnormals/ と depths/ に書き込み、transforms.json は変更しません。"),
    ZH_HANS("数据集文件夹：transforms.json、COLMAP sparse/ 或 Metashape 导出。\n写入 normals/ 和 depths/，不改动 transforms.json。"),
    ZH_HANT("資料集資料夾：transforms.json、COLMAP sparse/ 或 Metashape 匯出。\n寫入 normals/ 和 depths/，不改動 transforms.json。"),
    KO("데이터셋 폴더: transforms.json, COLMAP sparse/, Metashape 내보내기.\nnormals/와 depths/에 쓰며 transforms.json은 바꾸지 않습니다."),
    DE("Datensatzordner: transforms.json, COLMAP sparse/ oder Metashape-Export.\nSchreibt normals/ und depths/, ohne transforms.json zu ändern."),
    FR("Dossier du jeu : transforms.json, COLMAP sparse/ ou export Metashape.\nÉcrit normals/ et depths/ sans modifier transforms.json."),
    ES("Carpeta de datos: transforms.json, COLMAP sparse/ o exportación Metashape.\nEscribe normals/ y depths/ sin cambiar transforms.json."),
    PT("Pasta de dados: transforms.json, COLMAP sparse/ ou exportação Metashape.\nGrava normals/ e depths/ sem mudar transforms.json."),
    IT("Cartella dati: transforms.json, COLMAP sparse/ o export Metashape.\nScrive normals/ e depths/ senza cambiare transforms.json."),
    NL("Datamap: transforms.json, COLMAP sparse/ of Metashape-export.\nSchrijft normals/ en depths/ zonder transforms.json te wijzigen."),
    RU("Папка набора: transforms.json, COLMAP sparse/ или экспорт Metashape.\nПишет normals/ и depths/, не меняя transforms.json."),
    TR("Veri klasörü: transforms.json, COLMAP sparse/ veya Metashape dışa aktarımı.\nnormals/ ve depths/ yazar; transforms.json değişmez."));

SS_MSG(head_options,
    EN("Options"), JA("オプション"), ZH_HANS("选项"), ZH_HANT("選項"),
    KO("옵션"), DE("Optionen"), FR("Options"), ES("Opciones"), PT("Opções"),
    IT("Opzioni"), NL("Opties"), RU("Параметры"), TR("Seçenekler"));

SS_MSG(opt_worker_devices,
    EN("Opt-in isolated workers, one per comma-separated device UUID. Default: serial"),
    JA("カンマ区切りのデバイス UUID ごとに分離ワーカーを起動。既定: 直列"),
    ZH_HANS("启用隔离工作进程，每个逗号分隔的设备 UUID 一个。默认：串行"),
    ZH_HANT("啟用隔離工作行程，每個逗號分隔的裝置 UUID 一個。預設：串行"),
    KO("쉼표로 구분한 장치 UUID마다 격리 작업자 사용. 기본값: 순차 처리"),
    DE("Isolierte Worker aktivieren, je einer pro kommagetrennter Geräte-UUID. Standard: seriell"),
    FR("Activer un processus isolé par UUID de périphérique, séparés par des virgules. Défaut : série"),
    ES("Activar un proceso aislado por UUID de dispositivo, separados por comas. Predeterminado: serie"),
    PT("Ativar um processo isolado por UUID de dispositivo, separados por vírgulas. Padrão: série"),
    IT("Attiva un processo isolato per UUID dispositivo, separati da virgole. Predefinito: seriale"),
    NL("Geïsoleerde workers per door komma's gescheiden apparaat-UUID. Standaard: serieel"),
    RU("Изолированный процесс на каждый UUID устройства через запятую. По умолчанию: последовательно"),
    TR("Virgülle ayrılmış her aygıt UUID'si için yalıtılmış işçi. Varsayılan: seri"));

SS_MSG(opt_worker_memory,
    EN("Shared worker task and warp memory budget in MiB. Default: half physical RAM, at least 4096 MiB"),
    JA("ワーカーのタスクとワープ用共有メモリ予算 (MiB)。既定: 物理RAMの半分、最低4096 MiB"),
    ZH_HANS("工作进程任务和变形计划的共享内存预算（MiB）。默认：物理内存的一半，至少 4096 MiB"),
    ZH_HANT("工作行程任務和變形計畫的共享記憶體預算（MiB）。預設：實體記憶體的一半，至少 4096 MiB"),
    KO("작업자 태스크와 워프의 공유 메모리 예산(MiB). 기본값: 물리 RAM의 절반, 최소 4096 MiB"),
    DE("Gemeinsames Speicherbudget für Worker-Aufgaben und Warps in MiB. Standard: halber physischer RAM, mindestens 4096 MiB"),
    FR("Budget mémoire partagé des tâches et déformations en Mio. Défaut : moitié de la RAM physique, au moins 4096 Mio"),
    ES("Presupuesto compartido de tareas y deformaciones en MiB. Predeterminado: mitad de la RAM física, mínimo 4096 MiB"),
    PT("Orçamento de memória partilhada para tarefas e deformações em MiB. Padrão: metade da RAM física, mínimo 4096 MiB"),
    IT("Budget di memoria condiviso per attività e deformazioni in MiB. Predefinito: metà della RAM fisica, minimo 4096 MiB"),
    NL("Gedeeld geheugenbudget voor workertaken en vervormingen in MiB. Standaard: helft van fysiek RAM, minimaal 4096 MiB"),
    RU("Общий бюджет памяти задач и деформаций в МиБ. По умолчанию: половина физической RAM, минимум 4096 МиБ"),
    TR("İşçi görevleri ve eğriltmeler için paylaşılan bellek bütçesi (MiB). Varsayılan: fiziksel RAM'in yarısı, en az 4096 MiB"));

SS_MSG(opt_image_dir,
    EN("The folder holding the images, relative to the dataset or absolute. "
       "Default: images"),
    JA("画像が置かれているフォルダ。データセットからの相対パスまたは絶対パス。"
       "既定: images"),
    ZH_HANS("存放图像的文件夹，相对于数据集或绝对路径。默认：images"),
    ZH_HANT("存放影像的資料夾，相對於資料集或絕對路徑。預設：images"),
    KO("이미지가 있는 폴더. 데이터셋 기준 상대 경로 또는 절대 경로. "
       "기본값: images"),
    DE("Der Ordner mit den Bildern, relativ zum Datensatz oder absolut. "
       "Standard: images"),
    FR("Le dossier contenant les images, relatif au jeu de données ou absolu. "
       "Par défaut : images"),
    ES("La carpeta con las imágenes, relativa al conjunto o absoluta. "
       "Por defecto: images"),
    PT("A pasta com as imagens, relativa ao conjunto ou absoluta. "
       "Predefinição: images"),
    IT("La cartella con le immagini, relativa all'insieme o assoluta. "
       "Predefinito: images"),
    NL("De map met de beelden, relatief aan de dataset of absoluut. "
       "Standaard: images"),
    RU("Папка с изображениями, относительно набора или абсолютная. "
       "По умолчанию: images"),
    TR("Görüntülerin bulunduğu klasör; veri kümesine göre göreli ya da mutlak. "
       "Varsayılan: images"));

SS_MSG(opt_model,
    EN("Which checkpoint to run.\n- A known id is fetched and cached; a path to an .onnx or .pt file is used as it is.\n- Larger is slower and only somewhat better."),
    JA("使うチェックポイントです。\n- 既知の ID は取得してキャッシュし、.onnx や .pt へのパスはそのまま使います。\n- 大きいほど遅く、精度の伸びはわずかです。"),
    ZH_HANS("要运行的权重。\n- 已知 id 会自动下载并缓存；给出 .onnx 或 .pt 路径则直接使用。\n- 越大越慢，精度提升有限。"),
    ZH_HANT("要執行的權重。\n- 已知 id 會自動下載並快取；給出 .onnx 或 .pt 路徑則直接使用。\n- 越大越慢，精度提升有限。"),
    KO("실행할 체크포인트입니다.\n- 알려진 id 는 받아서 캐시하고, .onnx 나 .pt 경로는 그대로 씁니다.\n- 클수록 느리고 정확도 향상은 크지 않습니다."),
    DE("Welche Gewichte laufen sollen.\n- Eine bekannte Kennung wird geholt und zwischengespeichert, ein Pfad auf eine .onnx oder .pt unverändert benutzt.\n- Größer heißt langsamer und nur etwas besser."),
    FR("Quel jeu de poids exécuter.\n- Un identifiant connu est téléchargé puis mis en cache ; un chemin vers un .onnx ou un .pt est utilisé tel quel.\n- Plus gros est plus lent et à peine meilleur."),
    ES("Qué pesos ejecutar.\n- Un identificador conocido se descarga y se guarda en caché; una ruta a un .onnx o .pt se usa tal cual.\n- Más grande es más lento y solo algo mejor."),
    PT("Que pesos executar.\n- Um identificador conhecido é obtido e guardado em cache; um caminho para um .onnx ou .pt é usado tal como está.\n- Maior é mais lento e só um pouco melhor."),
    IT("Quali pesi eseguire.\n- Un identificatore noto viene scaricato e messo in cache; un percorso a un .onnx o .pt è usato così com'è.\n- Più grande è più lento e solo un po' migliore."),
    NL("Welke gewichten draaien.\n- Een bekende id wordt opgehaald en bewaard; een pad naar een .onnx of .pt wordt gebruikt zoals het is.\n- Groter is trager en maar iets beter."),
    RU("Какие веса запускать.\n- Известный идентификатор скачивается и кэшируется, путь к .onnx или .pt берётся как есть.\n- Крупнее — медленнее и лишь немного точнее."),
    TR("Hangi ağırlıkların çalışacağı.\n- Bilinen bir kimlik indirilip önbelleğe alınır; bir .onnx veya .pt yolu olduğu gibi kullanılır.\n- Büyüğü daha yavaştır ve yalnızca biraz daha iyidir."));

SS_MSG(opt_max_size,
    EN("Longest side of the maps written, and of one image the network runs.\nA split sizes its faces to match the written maps (--face-res). 1064 is what the model's own pipeline uses."),
    JA("書き出すマップと、ネットワークが 1 枚で処理する画像の長辺です。\n- 分割時の面は書き出すマップに合わせた大きさになります (--face-res)。\n- 1064 はモデル本来のパイプラインが使う大きさです。"),
    ZH_HANS("写出贴图、以及网络单张处理的图像的长边。 拆分时各面的大小与写出的贴图相匹配 (--face-res)。\n1064 是该模型自身流程使用的大小。"),
    ZH_HANT("寫出貼圖、以及網路單張處理的影像的長邊。 拆分時各面的大小與寫出的貼圖相匹配 (--face-res)。\n1064 是該模型自身流程使用的大小。"),
    KO("기록하는 맵과 신경망이 한 번에 처리하는 이미지의 긴 변입니다.\n분할한 면의 크기는 기록하는 맵에 맞춰집니다 (--face-res). 1064 는 모델 자체 파이프라인이 쓰는 크기입니다."),
    DE("Längste Seite der Karten und Netzwerkeingabe. Teilansichten folgen der Kartengröße (--face-res).\nDie Modellpipeline nutzt 1064."),
    FR("Plus grand côté des cartes et de l’entrée réseau. Les faces suivent les cartes (--face-res).\nLe modèle utilise 1064."),
    ES("Lado mayor de los mapas escritos, y de una imagen que procesa la red.\nUna división dimensiona sus caras según los mapas escritos (--face-res). 1064 es lo que usa la tubería del modelo."),
    PT("Maior lado dos mapas escritos, e de uma imagem que a rede processa.\nUma divisão dimensiona as suas faces pelos mapas escritos (--face-res). 1064 é o que o pipeline do modelo usa."),
    IT("Lato più lungo delle mappe scritte, e di un'immagine che la rete elabora.\nUna divisione dimensiona le sue facce sulle mappe scritte (--face-res). 1064 è ciò che usa la pipeline del modello."),
    NL("Langste zijde van kaarten en netwerkinvoer. Deelbeelden volgen de kaartgrootte (--face-res).\nDe modelpijplijn gebruikt 1064."),
    RU("Наибольшая сторона записываемых карт и одного изображения для сети.\nПри разбиении грани берут размер от записываемых карт (--face-res). 1064 — размер конвейера модели."),
    TR("Haritaların ve ağ girdisinin en uzun kenarı. Bölünmüş yüzler harita boyutunu izler (--face-res).\nModelin işlem hattı 1064 kullanır."));

SS_MSG(opt_num_tokens,
    EN("How many patches MoGe's transformer runs, which is what sets its cost rather than the image size.\nIts own range is 1200 to 3600, and it is capped at what the image holds. Metric3D ignores this."),
    JA("MoGe のトランスフォーマーが処理するパッチ数です。 画像の大きさではなくこれが処理量を決めます。\n本来の範囲は 1200 から 3600 で、画像が持つ数が上限になります。 Metric3D では無視されます。"),
    ZH_HANS("MoGe 的 Transformer 处理多少个图块，决定其开销的是这个而不是图像大小。\n其自身范围是 1200 到 3600，并以图像所含的数量为上限。 Metric3D 会忽略此项。"),
    ZH_HANT("MoGe 的 Transformer 處理多少個圖塊，決定其開銷的是這個而不是影像大小。\n其自身範圍是 1200 到 3600，並以影像所含的數量為上限。 Metric3D 會忽略此項。"),
    KO("MoGe 의 트랜스포머가 처리하는 패치 수입니다. 이미지 크기가 아니라 이 값이 비용을 정합니다.\n자체 범위는 1200 에서 3600 이고 이미지가 가진 수가 상한입니다. Metric3D 는 무시합니다."),
    DE("Wie viele Kacheln MoGes Transformer verarbeitet; das bestimmt die Kosten, nicht die Bildgröße.\nSein eigener Bereich ist 1200 bis 3600, begrenzt durch das, was das Bild hergibt. Metric3D ignoriert dies."),
    FR("Combien de tuiles le transformeur de MoGe traite : c'est cela qui fixe son coût, pas la taille de l'image.\nSa plage propre va de 1200 à 3600, plafonnée par ce que contient l'image. Metric3D l'ignore."),
    ES("Cuántos parches procesa el transformador de MoGe, que es lo que fija su coste y no el tamaño de la imagen.\nSu rango propio va de 1200 a 3600, limitado por lo que la imagen contiene. Metric3D lo ignora."),
    PT("Quantos blocos o transformador do MoGe processa, que é o que define o seu custo e não o tamanho da imagem.\nO seu intervalo é de 1200 a 3600, limitado pelo que a imagem contém. O Metric3D ignora isto."),
    IT("Quante patch elabora il transformer di MoGe: è questo a fissarne il costo, non la dimensione dell'immagine.\nIl suo intervallo va da 1200 a 3600, limitato da quanto contiene l'immagine. Metric3D lo ignora."),
    NL("Hoeveel vlakjes MoGe's transformer verwerkt; dat bepaalt de kosten, niet de beeldgrootte.\nZijn eigen bereik is 1200 tot 3600, begrensd door wat het beeld bevat. Metric3D negeert dit."),
    RU("Сколько фрагментов обрабатывает трансформер MoGe — именно это задаёт стоимость, а не размер изображения.\nСобственный диапазон от 1200 до 3600, с потолком по числу в изображении. Metric3D это игнорирует."),
    TR("MoGe'nin dönüştürücüsünün işlediği yama sayısı; maliyeti görüntü boyutu değil bu belirler.\nKendi aralığı 1200 ile 3600 arasıdır ve görüntünün içerdiğiyle sınırlanır. Metric3D bunu yok sayar."));

SS_MSG(opt_refine_steps,
    EN("MoGe-3's depth refinement passes, each sharpening depth edges a little more. 3 is what it was trained with;\n0 skips refinement. Other models ignore this."),
    JA("MoGe-3 の深度リファインの回数です。 1 回ごとに深度の境界が少しずつ鮮明になります。\n学習時の値は 3 で、0 でリファインを省きます。 他のモデルでは無視されます。"),
    ZH_HANS("MoGe-3 深度细化的次数，每次都会让深度边缘更清晰一些。训练时用的是 3，0 表示跳过细化。其他模型会忽略此项。"),
    ZH_HANT("MoGe-3 深度細化的次數，每次都會讓深度邊緣更清晰一些。訓練時用的是 3，0 表示略過細化。其他模型會忽略此項。"),
    KO("MoGe-3 의 깊이 다듬기 횟수로, 한 번마다 깊이 경계가 조금씩 더 선명해집니다.\n학습 때 값은 3 이고 0 이면 다듬기를 건너뜁니다. 다른 모델은 무시합니다."),
    DE("Die Tiefenverfeinerungen von MoGe-3; jede schärft die Tiefenkanten ein wenig mehr.\nTrainiert wurde mit 3, 0 überspringt die Verfeinerung. Andere Modelle ignorieren dies."),
    FR("Les passes d'affinage de profondeur de MoGe-3, chacune rendant les bords de profondeur un peu plus nets.\nIl a été entraîné avec 3 ; 0 saute l'affinage. Les autres modèles l'ignorent."),
    ES("Las pasadas de refinado de profundidad de MoGe-3; cada una afina un poco más los bordes de profundidad.\nSe entrenó con 3; 0 omite el refinado. Los demás modelos lo ignoran."),
    PT("As passagens de refinamento de profundidade do MoGe-3;\n- cada uma torna as arestas de profundidade um pouco mais nítidas. Foi treinado com 3; 0 salta o refinamento.\n- Os outros modelos ignoram isto."),
    IT("I passaggi di affinamento della profondità di MoGe-3, ognuno rende i bordi di profondità un po' più netti.\nÈ stato addestrato con 3; 0 salta l'affinamento. Gli altri modelli lo ignorano."),
    NL("De diepteverfijningen van MoGe-3; elke maakt de dieptegrenzen iets scherper. Getraind met 3;\n0 slaat het verfijnen over. Andere modellen negeren dit."),
    RU("Число проходов уточнения глубины MoGe-3; каждый делает границы глубины чуть чётче. Модель обучена с 3;\n0 пропускает уточнение. Другие модели это игнорируют."),
    TR("MoGe-3'ün derinlik iyileştirme geçişleri;\n- her biri derinlik kenarlarını biraz daha keskinleştirir. 3 ile eğitildi; 0 iyileştirmeyi atlar.\n- Diğer modeller bunu yok sayar."));

SS_MSG(opt_depth,
    EN("Also write depth maps. Off by default: the normals are what a reconstruction usually wants, and depth doubles both the time on disk and the reading a training run does."),
    JA("深度マップも書き出します。 既定は無効です。\n再構成でふつう効くのは法線で、深度はディスク上の容量も学習時の読み込みも倍にします。"),
    ZH_HANS("同时写出深度图。默认关闭：重建通常真正用得上的是法线，而深度会让磁盘占用和训练时的读取都翻倍。"),
    ZH_HANT("同時寫出深度圖。預設關閉：重建通常真正用得上的是法線，而深度會讓磁碟佔用和訓練時的讀取都翻倍。"),
    KO("깊이 맵도 함께 씁니다. 기본은 꺼짐입니다.\n재구성에서 주로 쓰이는 것은 법선이고, 깊이는 디스크 용량과 학습 때의 읽기를 모두 두 배로 만듭니다."),
    DE("Auch Tiefenkarten schreiben. Standard: aus; meist genügen Normalen. Tiefe verdoppelt Speicherplatz und Leseaufwand beim Training."),
    FR("Écrit aussi la profondeur. Désactivé par défaut ; les normales suffisent souvent. Double le stockage et les lectures d’entraînement."),
    ES("Escribe también profundidad. Por defecto no; suelen bastar las normales. Duplica el espacio y las lecturas durante el entrenamiento."),
    PT("Grava também profundidade. Desligado por padrão; normais costumam bastar. Duplica o espaço e as leituras no treino."),
    IT("Scrive anche la profondità. Disattivo per default; spesso bastano le normali. Raddoppia spazio e letture nel training."),
    NL("Schrijft ook diepte. Standaard uit; normalen zijn meestal genoeg. Verdubbelt schijfruimte en leeswerk tijdens training."),
    RU("Также записывать карты глубины. По умолчанию выключено: реконструкции обычно нужны нормали, а глубина удваивает и место на диске, и чтение во время обучения."),
    TR("Derinlik haritalarını da yaz. Öntanımlı kapalı: bir yeniden oluşturmada işe yarayan genelde normallerdir ve derinlik hem diskteki yeri hem de eğitimdeki okumayı ikiye katlar."));

SS_MSG(opt_normal_format,
    EN("File type for the normal maps. `jpg` is about a fifth the size; the "
       "loss lands on the normal's direction, which the geometry term reads "
       "directly."),
    JA("法線マップの形式です。`jpg` は容量が約 5 分の 1 になりますが、失われる"
       "のは法線の向きそのもので、ジオメトリ項はそれを直接読みます。"),
    ZH_HANS("法线图的文件格式。`jpg` 体积约为五分之一，但损失落在法线方向上，"
            "而几何项正是直接读取它。"),
    ZH_HANT("法線圖的檔案格式。`jpg` 體積約為五分之一，但損失落在法線方向上，"
            "而幾何項正是直接讀取它。"),
    KO("법선 맵의 파일 형식입니다. `jpg` 는 크기가 약 5 분의 1 이지만, 손실이 "
       "법선의 방향에 그대로 실리고 기하 항은 그것을 직접 읽습니다."),
    DE("Dateityp der Normalenkarten. `jpg` ist etwa ein Fünftel so groß; der "
       "Verlust trifft die Richtung der Normale, die der Geometrieterm direkt "
       "liest."),
    FR("Type de fichier des cartes de normales. `jpg` pèse environ un "
       "cinquième ; la perte porte sur la direction de la normale, que le "
       "terme géométrique lit directement."),
    ES("Tipo de archivo de los mapas de normales. `jpg` ocupa una quinta parte; "
       "la pérdida cae sobre la dirección de la normal, que el término "
       "geométrico lee directamente."),
    PT("Tipo de ficheiro dos mapas de normais. `jpg` ocupa cerca de um quinto; "
       "a perda recai sobre a direção da normal, que o termo geométrico lê "
       "diretamente."),
    IT("Tipo di file delle mappe di normali. `jpg` occupa circa un quinto; la "
       "perdita cade sulla direzione della normale, che il termine geometrico "
       "legge direttamente."),
    NL("Bestandstype van de normaalafbeeldingen. `jpg` is ongeveer een vijfde "
       "zo groot; het verlies valt op de richting van de normaal, die de "
       "geometrieterm rechtstreeks leest."),
    RU("Формат файлов карт нормалей. `jpg` примерно впятеро меньше, но потери "
       "приходятся на само направление нормали, которое геометрический член "
       "читает напрямую."),
    TR("Normal haritalarının dosya türü. `jpg` yaklaşık beşte bir yer kaplar; "
       "kayıp doğrudan normalin yönüne biner ve geometri terimi onu doğrudan "
       "okur."));

SS_MSG(opt_jpeg_quality,
    EN("JPEG quality for the normal maps, when the format is `jpg`."),
    JA("形式が `jpg` のときの、法線マップの JPEG 品質です。"),
    ZH_HANS("格式为 `jpg` 时，法线图的 JPEG 质量。"),
    ZH_HANT("格式為 `jpg` 時，法線圖的 JPEG 品質。"),
    KO("형식이 `jpg` 일 때 법선 맵의 JPEG 품질입니다."),
    DE("JPEG-Qualität der Normalenkarten, wenn das Format `jpg` ist."),
    FR("Qualité JPEG des cartes de normales, quand le format est `jpg`."),
    ES("Calidad JPEG de los mapas de normales, cuando el formato es `jpg`."),
    PT("Qualidade JPEG dos mapas de normais, quando o formato é `jpg`."),
    IT("Qualità JPEG delle mappe di normali, quando il formato è `jpg`."),
    NL("JPEG-kwaliteit van de normaalafbeeldingen, als het formaat `jpg` is."),
    RU("Качество JPEG для карт нормалей, когда формат — `jpg`."),
    TR("Biçim `jpg` iken normal haritalarının JPEG kalitesi."));

SS_MSG(opt_ray_depth,
    EN("Store depth along the ray rather than camera Z. auto uses ray depth for split pinhole faces.\nMatches the trainer’s automatic --input-depth-is-ray-depth setting."),
    JA("カメラZ座標ではなく光線沿いの距離を保存。autoはピンホール面への分割時に光線深度を使います。\n学習側の --input-depth-is-ray-depth 自動設定と一致します。"),
    ZH_HANS("存储沿射线的距离而非相机 Z。auto 在分割为针孔视图时使用射线深度。\n与训练器 --input-depth-is-ray-depth 的自动设置一致。"),
    ZH_HANT("儲存沿射線的距離而非相機 Z。auto 在分割為針孔視圖時使用射線深度。\n與訓練器 --input-depth-is-ray-depth 的自動設定一致。"),
    KO("카메라 Z 대신 광선 방향 거리를 저장합니다. auto는 핀홀 뷰로 분할할 때 광선 깊이를 씁니다.\n트레이너의 자동 --input-depth-is-ray-depth 설정과 같습니다."),
    DE("Tiefe entlang des Strahls statt Kamera-Z speichern. auto nutzt sie bei aufgeteilten Lochkamera-Flächen.\nEntspricht der automatischen Trainereinstellung --input-depth-is-ray-depth."),
    FR("Profondeur le long du rayon plutôt que Z caméra. auto l’utilise pour les vues sténopé découpées.\nIdentique au réglage automatique --input-depth-is-ray-depth du moteur."),
    ES("Profundidad sobre el rayo en vez de Z de cámara. auto la usa en vistas estenopeicas divididas.\nCoincide con --input-depth-is-ray-depth automático del entrenador."),
    PT("Profundidade ao longo do raio em vez de Z da câmera. auto a usa em vistas pinhole divididas.\nIgual ao --input-depth-is-ray-depth automático do treinador."),
    IT("Profondità lungo il raggio invece di Z della camera. auto la usa per viste pinhole suddivise.\nCome l’impostazione automatica --input-depth-is-ray-depth del trainer."),
    NL("Diepte langs de straal in plaats van camera-Z. auto gebruikt dit bij gesplitste perspectiefbeelden.\nGelijk aan de automatische traineroptie --input-depth-is-ray-depth."),
    RU("Глубина вдоль луча вместо Z камеры. auto выбирает её для разделённых перспективных видов.\nСовпадает с автоматическим --input-depth-is-ray-depth в обучении."),
    TR("Kamera Z yerine ışın boyunca derinlik. auto bölünmüş iğne deliği yüzlerinde bunu kullanır.\nEğiticinin otomatik --input-depth-is-ray-depth ayarıyla aynıdır."));

SS_MSG(opt_split,
    EN("Whether to split a wide frame into several pinhole faces instead of undistorting it to one.\n`auto` splits a panorama always and a fisheye when one pinhole would keep less than three quarters of the frame."),
    JA("広い画角のフレームを、1 枚に歪み補正する代わりに複数のピンホール面へ分割するかです。\n`auto` はパノラマなら常に分割し、魚眼は 1 枚のピンホールで 4 分の 3 未満しか残らないときに分割します。"),
    ZH_HANS("是否把广角画面拆成多个针孔面，而不是矫正成一张。\n`auto` 对全景总是拆分，对鱼眼则在单张针孔保留不足四分之三画面时拆分。"),
    ZH_HANT("是否把廣角畫面拆成多個針孔面，而不是矯正成一張。\n`auto` 對全景總是拆分，對魚眼則在單張針孔保留不足四分之三畫面時拆分。"),
    KO("넓은 화각의 프레임을 한 장으로 왜곡 보정하는 대신 여러 핀홀 면으로 나눌지입니다.\n`auto` 는 파노라마는 항상 나누고, 어안은 핀홀 한 장이 화면의 4 분의 3 도 남기지 못할 때 나눕니다."),
    DE("Weite Bilder in Lochkamera-Teilansichten zerlegen statt in nur eine entzerren.\nauto: Panoramen immer; Fischauge, wenn eine Ansicht weniger als 75% des Bilds behält."),
    FR("Découper une image à large champ en plusieurs faces sténopé au lieu de la redresser en une seule.\n`auto` découpe toujours un panorama, et un fisheye quand un seul sténopé garderait moins des trois quarts."),
    ES("Si dividir un cuadro de campo amplio en varias caras estenopeicas en vez de enderezarlo en una.\n`auto` divide siempre un panorama, y un ojo de pez cuando un solo estenopo conservaría menos de tres cuartos."),
    PT("Se dividir um quadro de campo largo em várias faces estenopeicas em vez de o endireitar numa só.\n`auto` divide sempre um panorama, e uma olho-de-peixe quando um único estenopo guardaria menos de três quartos."),
    IT("Se dividere un fotogramma ad ampio campo in più facce stenopeiche invece di raddrizzarlo in una sola.\n`auto` divide sempre un panorama, e un fisheye quando un solo stenopeico terrebbe meno di tre quarti."),
    NL("Brede beelden opsplitsen in perspectiefbeelden in plaats van één rechtgetrokken beeld.\nauto: panorama altijd; fisheye als één beeld minder dan 75% behoudt."),
    RU("Разбивать ли широкий кадр на несколько пинхол-граней вместо исправления в одну.\n`auto` всегда разбивает панораму, а «рыбий глаз» — когда одна пинхол-картинка сохранит меньше трёх четвертей кадра."),
    TR("Geniş kareyi tek görünüme düzeltmek yerine iğne deliği yüzlerine böler.\nauto: panoramayı hep; balık gözünü tek yüz görüntünün %75’inden azını koruyorsa böler."));

SS_MSG(opt_face_res,
    EN("How finely a split samples the frame.\n- `output` sizes each face to the map it is written into, and no smaller than the network's least input.\n- `source` sizes it to the frame's own resolution, up to --max-size:\n- several times the work on a high-resolution capture, for a sharper input to the network."),
    JA("分割でフレームをどれだけ細かく取るかです。\n- `output` は各面を書き出すマップに合わせ、ネットワークの最小入力より小さくはしません。\n- `source` は元のフレームの解像度に合わせ、--max-size が上限です。\n- 高解像度の撮影では処理量が数倍になる代わりに、ネットワークへの入力が鮮明になります。"),
    ZH_HANS("拆分时对画面采样的精细程度。\n- `output` 让每个面与写出的贴图相匹配，且不小于网络的最小输入。\n- `source` 让它与原画面的分辨率相同，以 --max-size 为上限：对高分辨率拍摄，开销是数倍，换来更清晰的网络输入。"),
    ZH_HANT("拆分時對畫面取樣的精細程度。\n- `output` 讓每個面與寫出的貼圖相匹配，且不小於網路的最小輸入。\n- `source` 讓它與原畫面的解析度相同，以 --max-size 為上限：對高解析度拍攝，開銷是數倍，換來更清晰的網路輸入。"),
    KO("분할할 때 프레임을 얼마나 촘촘히 샘플링할지입니다.\n- `output` 은 각 면을 기록하는 맵에 맞추되 신경망의 최소 입력보다 작게 하지 않습니다.\n- `source` 는 원본 프레임의 해상도에 맞추며 --max-size 가 상한입니다.\n- 고해상도 촬영에서는 비용이 몇 배가 되는 대신 신경망 입력이 더 선명해집니다."),
    DE("Wie fein eine Zerlegung das Bild abtastet. `output` bemisst jede Fläche nach der Karte, in die sie geschrieben wird, nie kleiner als die kleinste Eingabe des Netzes.\n- `source` nach der Auflösung des Bildes selbst, bis --max-size:\n- bei einer hochauflösenden Aufnahme ein Mehrfaches an Arbeit für eine schärfere Eingabe."),
    FR("Finesse avec laquelle un découpage échantillonne l'image. `output` dimensionne chaque face d'après la carte où elle est écrite, sans descendre sous l'entrée minimale du réseau.\n- `source` d'après la résolution de l'image elle-même, jusqu'à --max-size :\n- plusieurs fois le travail sur une prise haute résolution, pour une entrée plus nette."),
    ES("Con qué finura una división muestrea el cuadro.\n- `output` dimensiona cada cara según el mapa en que se escribe, nunca por debajo de la entrada mínima de la red.\n- `source` según la resolución del propio cuadro, hasta --max-size:\n- varias veces el trabajo en una toma de alta resolución, a cambio de una entrada más nítida."),
    PT("Com que finura uma divisão amostra o quadro.\n- `output` dimensiona cada face pelo mapa onde é escrita, nunca abaixo da entrada mínima da rede.\n- `source` pela resolução do próprio quadro, até --max-size:\n- várias vezes o trabalho numa captura de alta resolução, em troca de uma entrada mais nítida."),
    IT("Quanto finemente una divisione campiona il fotogramma.\n- `output` dimensiona ogni faccia sulla mappa in cui viene scritta, mai sotto l'ingresso minimo della rete.\n- `source` sulla risoluzione del fotogramma stesso, fino a --max-size:\n- più volte il lavoro su una ripresa ad alta risoluzione, per un ingresso più nitido."),
    NL("Hoe fijn een splitsing het beeld bemonstert. `output` bemeet elk vlak naar de kaart waarin het geschreven wordt, nooit kleiner dan de kleinste invoer van het netwerk.\n- `source` naar de resolutie van het beeld zelf, tot --max-size:\n- bij een opname met hoge resolutie een veelvoud van het werk, voor scherpere invoer."),
    RU("Насколько подробно разбиение выбирает кадр.\n- `output` подгоняет каждую грань под карту, в которую она записывается, но не меньше минимального входа сети.\n- `source` — под разрешение самого кадра, до --max-size:\n- на съёмке высокого разрешения в несколько раз больше работы ради более чёткого входа."),
    TR("Bölmenin kareyi ne kadar ince örneklediği.\n- `output` her yüzü yazıldığı haritaya göre boyutlandırır, ağın en küçük girdisinden küçük yapmaz.\n- `source` karenin kendi çözünürlüğüne göre, --max-size sınırına kadar:\n- yüksek çözünürlüklü bir çekimde birkaç kat iş, daha net bir girdi karşılığında."));

SS_MSG(opt_depth_units,
    EN("Units stored in depth maps.\n- relative — scale the scene’s 99.9th percentile to 16 bits. Depth loss ignores overall scale.\n- mm — millimetres. Depths beyond 65.5 m are clipped."),
    JA("深度マップの単位です。\n- relative — シーンの99.9パーセンタイルで割り16ビットを使います。深度損失は対数深度のスケールを無視します。\n- mm — ミリメートル。65.5 mを超える値を切り詰めます。"),
    ZH_HANS("深度图存储单位。\n- relative — 除以场景第 99.9 百分位以用满 16 位。深度损失比较对数深度并忽略尺度。\n- mm — 毫米。超过 65.5 m 的深度会截断。"),
    ZH_HANT("深度圖儲存單位。\n- relative — 除以場景第 99.9 百分位以用滿 16 位。深度損失比較對數深度並忽略尺度。\n- mm — 毫米。超過 65.5 m 的深度會截斷。"),
    KO("깊이 맵 저장 단위입니다.\n- relative — 장면의 99.9백분위수로 나눠 16비트를 채웁니다. 깊이 손실은 로그 깊이의 척도를 무시합니다.\n- mm — 밀리미터. 65.5 m를 넘는 깊이는 잘립니다."),
    DE("Einheit der Tiefenkarten.\n- relative — durch das 99,9. Perzentil teilen für 16 Bit. Tiefen-Loss vergleicht Log-Tiefen ohne Maßstab.\n- mm — Millimeter. Tiefen über 65,5 m werden begrenzt."),
    FR("Unités des cartes de profondeur.\n- relative — divise par le 99,9e percentile pour remplir 16 bits. La perte compare les logs sans échelle.\n- mm — millimètres. Les valeurs dépassant 65,5 m sont écrêtées."),
    ES("Unidades del mapa de profundidad.\n- relative — divide por el percentil 99,9 para llenar 16 bits. La pérdida compara logaritmos sin escala.\n- mm — milímetros. Se recortan profundidades mayores de 65,5 m."),
    PT("Unidades dos mapas de profundidade.\n- relative — divide pelo percentil 99,9 para preencher 16 bits. A perda compara logaritmos sem escala.\n- mm — milímetros. Profundidades acima de 65,5 m são cortadas."),
    IT("Unità delle mappe di profondità.\n- relative — divide per il percentile 99,9 per usare 16 bit. La perdita confronta i logaritmi senza scala.\n- mm — millimetri. Valori oltre 65,5 m sono troncati."),
    NL("Eenheden in dieptekaarten.\n- relative — deelt door het 99,9e percentiel om 16 bits te vullen. Diepteverlies vergelijkt logs zonder schaal.\n- mm — millimeters. Dieptes boven 65,5 m worden afgekapt."),
    RU("Единицы глубины в картах.\n- relative — делит на 99,9-й процентиль для 16 бит. Потеря сравнивает логарифмы глубин без масштаба.\n- mm — миллиметры. Глубина свыше 65,5 м обрезается."),
    TR("Derinlik haritasındaki birim.\n- relative — 16 biti doldurmak için %99,9 yüzdeliğe böler. Kayıp log derinlikleri ölçekten bağımsız kıyaslar.\n- mm — milimetre. 65,5 m üzerindeki değerler kırpılır."));

SS_MSG(opt_image_gamut,
    EN("Colour primaries the dataset's images are in. Frames convert to sRGB "
       "before inference, which is what the model was trained on."),
    JA("データセット画像の色域。推論の前に sRGB へ変換します。モデルはそれで"
       "学習されています。"),
    ZH_HANS("数据集图像的色域。推理前转换为 sRGB，模型即以此训练。"),
    ZH_HANT("資料集影像的色域。推論前轉換為 sRGB，模型即以此訓練。"),
    KO("데이터셋 이미지의 색역. 추론 전에 sRGB 로 변환하며, 모델이 그것으로 "
       "학습되었습니다."),
    DE("Farbprimärvalenzen der Bilder des Datensatzes. Frames werden vor der "
       "Inferenz nach sRGB gewandelt, worauf das Modell trainiert wurde."),
    FR("Primaires de couleur des images du jeu de données. Les images passent "
       "en sRGB avant l'inférence, ce sur quoi le modèle a été entraîné."),
    ES("Primarios de color de las imágenes del conjunto. Los fotogramas pasan "
       "a sRGB antes de la inferencia, que es con lo que se entrenó el modelo."),
    PT("Primárias de cor das imagens do conjunto. Os quadros passam a sRGB "
       "antes da inferência, que é com o que o modelo foi treinado."),
    IT("Primarie di colore delle immagini del set. I fotogrammi passano a sRGB "
       "prima dell'inferenza, su cui il modello è stato addestrato."),
    NL("Kleurprimairen van de beelden in de dataset. Frames gaan naar sRGB voor "
       "de inferentie, waarop het model is getraind."),
    RU("Основные цвета изображений набора. Кадры переводятся в sRGB перед "
       "выводом — на этом обучалась модель."),
    TR("Veri kümesi görüntülerinin renk birincilleri. Kareler çıkarımdan önce "
       "sRGB'ye çevrilir; model bununla eğitildi."));

SS_MSG(opt_image_linear,
    EN("Treat the dataset's images as linear light rather than "
       "display-encoded."),
    JA("データセット画像を表示用エンコードではなくリニア光として扱います。"),
    ZH_HANS("将数据集图像视为线性光而非显示编码。"),
    ZH_HANT("將資料集影像視為線性光而非顯示編碼。"),
    KO("데이터셋 이미지를 디스플레이 인코딩이 아니라 선형 광으로 취급합니다."),
    DE("Bilder des Datensatzes als lineares Licht statt als anzeigecodiert "
       "behandeln."),
    FR("Traiter les images du jeu de données comme de la lumière linéaire "
       "plutôt qu'encodées pour l'affichage."),
    ES("Tratar las imágenes del conjunto como luz lineal en vez de codificadas "
       "para pantalla."),
    PT("Tratar as imagens do conjunto como luz linear em vez de codificadas "
       "para exibição."),
    IT("Trattare le immagini del set come luce lineare anziché codificate per "
       "lo schermo."),
    NL("Beelden van de dataset als lineair licht behandelen in plaats van "
       "weergavegecodeerd."),
    RU("Считать изображения набора линейным светом, а не экранно "
       "закодированными."),
    TR("Veri kümesi görüntülerini ekran kodlu değil, doğrusal ışık olarak ele "
       "al."));

SS_MSG(opt_image_exposure,
    EN("Brighten what the model sees, in linear light, without touching the "
       "files: auto lifts each image darker than a typical photograph, or give "
       "a number of stops."),
    JA("ファイルを変えずに、モデルに渡す画像をリニア光で明るくします。auto は一般的な"
       "写真より暗い画像をそれぞれ持ち上げ、数値なら段数です。"),
    ZH_HANS("在线性光中调亮模型看到的图像，不改动文件：auto 会提亮比普通照片暗的"
            "每张图像，也可给出档数。"),
    ZH_HANT("在線性光中調亮模型看到的影像，不改動檔案：auto 會提亮比一般照片暗的"
            "每張影像，也可給出檔數。"),
    KO("파일은 그대로 두고 모델이 보는 이미지를 선형 광에서 밝게 합니다. auto 는 "
       "일반 사진보다 어두운 이미지를 각각 끌어올리고, 숫자는 스톱 수입니다."),
    DE("Hellt in linearem Licht auf, was das Modell sieht, ohne die Dateien zu "
       "ändern: auto hebt jedes Bild an, das dunkler als ein typisches Foto ist, "
       "oder eine Zahl von Blendenstufen."),
    FR("Éclaircit en lumière linéaire ce que voit le modèle, sans toucher aux "
       "fichiers : auto relève chaque image plus sombre qu'une photo typique, ou "
       "indiquez un nombre de diaphs."),
    ES("Aclara en luz lineal lo que ve el modelo, sin tocar los archivos: auto "
       "levanta cada imagen más oscura que una foto típica, o indique un número "
       "de pasos."),
    PT("Clareia em luz linear o que o modelo vê, sem mexer nos arquivos: auto "
       "ergue cada imagem mais escura que uma foto típica, ou indique um número "
       "de pontos."),
    IT("Schiarisce in luce lineare ciò che vede il modello, senza toccare i "
       "file: auto solleva ogni immagine più scura di una foto tipica, oppure "
       "indica un numero di stop."),
    NL("Maakt in lineair licht lichter wat het model ziet, zonder de bestanden "
       "te wijzigen: auto tilt elk beeld op dat donkerder is dan een gewone "
       "foto, of geef een aantal stops."),
    RU("Осветляет в линейном свете то, что видит модель, не трогая файлы: auto "
       "поднимает каждое изображение темнее обычной фотографии, либо укажите "
       "число ступеней."),
    TR("Modelin gördüğünü dosyalara dokunmadan doğrusal ışıkta aydınlatır: auto, "
       "tipik bir fotoğraftan koyu olan her görüntüyü yükseltir; ya da bir durak "
       "sayısı verin."));

SS_MSG(opt_overwrite,
    EN("Recompute maps that are already on disk. Without it a run continues "
       "where the last one stopped."),
    JA("すでにディスクにあるマップも作り直します。指定しなければ、前回の続きから"
       "実行します。"),
    ZH_HANS("重新计算磁盘上已有的图。不加则从上次中断处继续。"),
    ZH_HANT("重新計算磁碟上已有的圖。不加則從上次中斷處繼續。"),
    KO("이미 디스크에 있는 맵도 다시 계산합니다. 없으면 지난번에 멈춘 곳부터 "
       "이어서 실행합니다."),
    DE("Bereits vorhandene Karten neu berechnen. Ohne das setzt ein Lauf dort "
       "fort, wo der letzte aufgehört hat."),
    FR("Recalculer les cartes déjà présentes sur le disque. Sans cela, une "
       "exécution reprend là où la précédente s'est arrêtée."),
    ES("Recalcular los mapas que ya están en disco. Sin esto, una ejecución "
       "continúa donde se detuvo la anterior."),
    PT("Recalcular os mapas que já estão no disco. Sem isto, uma execução "
       "continua onde a anterior parou."),
    IT("Ricalcolare le mappe già presenti su disco. Senza, un'esecuzione "
       "riprende da dove si è fermata la precedente."),
    NL("Kaarten die al op schijf staan opnieuw berekenen. Zonder dit gaat een "
       "run verder waar de vorige stopte."),
    RU("Пересчитать карты, уже лежащие на диске. Без этого запуск продолжает с "
       "того места, где остановился предыдущий."),
    TR("Diskte zaten bulunan haritaları yeniden hesapla. Bu olmadan bir "
       "çalıştırma, öncekinin bıraktığı yerden sürer."));

SS_MSG(label_common,
    EN("Also:"), JA("その他:"), ZH_HANS("其他:"), ZH_HANT("其他:"), KO("그 밖에:"),
    DE("Außerdem:"), FR("Aussi :"), ES("Además:"), PT("Além disso:"),
    IT("Inoltre:"), NL("Verder:"), RU("Ещё:"), TR("Ayrıca:"));

SS_MSG(label_environment,
    EN("Environment:"), JA("環境変数:"), ZH_HANS("环境变量:"), ZH_HANT("環境變數:"),
    KO("환경 변수:"), DE("Umgebung:"), FR("Environnement :"), ES("Entorno:"),
    PT("Ambiente:"), IT("Ambiente:"), NL("Omgeving:"), RU("Переменные среды:"),
    TR("Ortam:"));

SS_MSG(err_no_dataset,
    EN("Name a dataset folder. `{0} --help` lists what it accepts."),
    JA("データセットのフォルダを指定してください。`{0} --help` に受け付ける形式が"
       "あります。"),
    ZH_HANS("请指定数据集文件夹。`{0} --help` 列出了它接受的格式。"),
    ZH_HANT("請指定資料集資料夾。`{0} --help` 列出了它接受的格式。"),
    KO("데이터셋 폴더를 지정하세요. `{0} --help` 에 받아들이는 형식이 있습니다."),
    DE("Bitte einen Datensatzordner angeben. `{0} --help` zeigt, was er "
       "annimmt."),
    FR("Indiquez un dossier de jeu de données. `{0} --help` liste ce qu'il "
       "accepte."),
    ES("Indica una carpeta de conjunto de datos. `{0} --help` enumera lo que "
       "acepta."),
    PT("Indique uma pasta de conjunto de dados. `{0} --help` lista o que "
       "aceita."),
    IT("Indica una cartella di insieme di dati. `{0} --help` elenca ciò che "
       "accetta."),
    NL("Geef een datasetmap op. `{0} --help` somt op wat hij aanneemt."),
    RU("Укажите папку набора данных. `{0} --help` перечисляет, что она "
       "принимает."),
    TR("Bir veri kümesi klasörü belirtin. `{0} --help` neyi kabul ettiğini "
       "listeler."));

SS_MSG(err_no_images_read,
    EN("Not one image could be read under '{0}'. Pass --image-dir if the "
       "dataset's images are somewhere else."),
    JA("「{0}」の下からは 1 枚も画像を読み取れませんでした。データセットの画像が"
       "別の場所にあるなら --image-dir で指定してください。"),
    ZH_HANS("在「{0}」下一张图像也读不到。如果数据集的图像在别处，"
            "请用 --image-dir 指定。"),
    ZH_HANT("在「{0}」下一張影像也讀不到。如果資料集的影像在別處，"
            "請用 --image-dir 指定。"),
    KO("'{0}' 아래에서 이미지를 한 장도 읽지 못했습니다. 데이터셋의 이미지가 "
       "다른 곳에 있으면 --image-dir 로 지정하세요."),
    DE("Unter '{0}' ließ sich kein einziges Bild lesen. Geben Sie --image-dir "
       "an, wenn die Bilder des Datensatzes woanders liegen."),
    FR("Aucune image n'a pu être lue sous « {0} ». Indiquez --image-dir si les "
       "images du jeu de données sont ailleurs."),
    ES("No se ha podido leer ninguna imagen bajo «{0}». Indica --image-dir si "
       "las imágenes del conjunto están en otro sitio."),
    PT("Não foi possível ler nenhuma imagem em \"{0}\". Informe --image-dir se "
       "as imagens do conjunto estiverem noutro lugar."),
    IT("Non è stato possibile leggere alcuna immagine sotto \"{0}\". Indica "
       "--image-dir se le immagini dell'insieme sono altrove."),
    NL("Onder '{0}' kon geen enkel beeld worden gelezen. Geef --image-dir op "
       "als de beelden van de dataset elders staan."),
    RU("В «{0}» не удалось прочитать ни одного изображения. Укажите "
       "--image-dir, если изображения набора лежат в другом месте."),
    TR("'{0}' altında hiçbir görüntü okunamadı. Veri kümesinin görüntüleri "
       "başka yerdeyse --image-dir ile belirtin."));

SS_MSG(log_dataset,
    EN("{0}: {1} images, {2} cameras"),
    JA("{0}: 画像 {1} 枚、カメラ {2} 台"),
    ZH_HANS("{0}: 图像 {1} 张, 相机 {2} 台"),
    ZH_HANT("{0}: 影像 {1} 張, 相機 {2} 台"),
    KO("{0}: 이미지 {1} 장, 카메라 {2} 대"),
    DE("{0}: Bilder {1}, Kameras {2}"),
    FR("{0} : images {1}, caméras {2}"),
    ES("{0}: imágenes {1}, cámaras {2}"),
    PT("{0}: imagens {1}, câmaras {2}"),
    IT("{0}: immagini {1}, fotocamere {2}"),
    NL("{0}: afbeeldingen {1}, camera's {2}"),
    RU("{0}: изображений {1}, камер {2}"),
    TR("{0}: görüntü {1}, kamera {2}"));

SS_MSG(log_camera_single,
    EN("camera {0}: {1}, undistorted to one {2}x{3} pinhole"),
    JA("カメラ {0}: {1}、1 枚の {2}x{3} ピンホールに歪み補正"),
    ZH_HANS("相机 {0}: {1}, 矫正为一张 {2}x{3} 针孔图"),
    ZH_HANT("相機 {0}: {1}, 矯正為一張 {2}x{3} 針孔圖"),
    KO("카메라 {0}: {1}, 한 장의 {2}x{3} 핀홀로 왜곡 보정"),
    DE("Kamera {0}: {1}, auf eine Lochkamera {2}x{3} entzerrt"),
    FR("caméra {0} : {1}, redressée en un sténopé {2}x{3}"),
    ES("cámara {0}: {1}, enderezada a un estenopo {2}x{3}"),
    PT("câmara {0}: {1}, endireitada para um estenopo {2}x{3}"),
    IT("fotocamera {0}: {1}, raddrizzata in uno stenopeico {2}x{3}"),
    NL("camera {0}: {1}, rechtgetrokken tot één gaatjescamera {2}x{3}"),
    RU("камера {0}: {1}, исправлена в одну пинхол-картинку {2}x{3}"),
    TR("kamera {0}: {1}, tek bir {2}x{3} iğne deliğine düzeltildi"));

SS_MSG(log_camera_split,
    EN("camera {0}: {1}, split into {2} pinhole faces of {3}x{4}"),
    JA("カメラ {0}: {1}、{3}x{4} のピンホール面 {2} 枚に分割"),
    ZH_HANS("相机 {0}: {1}, 拆成 {2} 个 {3}x{4} 的针孔面"),
    ZH_HANT("相機 {0}: {1}, 拆成 {2} 個 {3}x{4} 的針孔面"),
    KO("카메라 {0}: {1}, {3}x{4} 핀홀 면 {2} 개로 분할"),
    DE("Kamera {0}: {1}, in {2} Lochkamera-Flächen zu {3}x{4} zerlegt"),
    FR("caméra {0} : {1}, découpée en {2} faces sténopé de {3}x{4}"),
    ES("cámara {0}: {1}, dividida en {2} caras estenopeicas de {3}x{4}"),
    PT("câmara {0}: {1}, dividida em {2} faces estenopeicas de {3}x{4}"),
    IT("fotocamera {0}: {1}, divisa in {2} facce stenopeiche da {3}x{4}"),
    NL("camera {0}: {1}, gesplitst in {2} gaatjescamera-vlakken van {3}x{4}"),
    RU("камера {0}: {1}, разбита на {2} пинхол-граней {3}x{4}"),
    TR("kamera {0}: {1}, {3}x{4} boyutunda {2} iğne deliği yüzüne bölündü"));

SS_MSG(log_progress,
    EN("{0} / {1} images, {2} ms each, {3} left"),
    JA("{0} / {1} 枚、1 枚あたり {2} ms、残り {3}"),
    ZH_HANS("{0} / {1} 张, 每张 {2} ms, 剩余 {3}"),
    ZH_HANT("{0} / {1} 張, 每張 {2} ms, 剩餘 {3}"),
    KO("{0} / {1} 장, 장당 {2} ms, {3} 남음"),
    DE("{0} / {1} Bilder, {2} ms je Bild, {3} übrig"),
    FR("{0} / {1} images, {2} ms chacune, {3} restant"),
    ES("{0} / {1} imágenes, {2} ms cada una, quedan {3}"),
    PT("{0} / {1} imagens, {2} ms cada, faltam {3}"),
    IT("{0} / {1} immagini, {2} ms ciascuna, {3} rimanenti"),
    NL("{0} / {1} afbeeldingen, {2} ms per stuk, nog {3}"),
    RU("{0} / {1} изображений, по {2} мс, осталось {3}"),
    TR("{0} / {1} görüntü, her biri {2} ms, {3} kaldı"));

SS_MSG(log_done,
    EN("done: {0} written, {1} already there, in {2}"),
    JA("完了: {0} 枚を書き出し、{1} 枚は既存、所要 {2}"),
    ZH_HANS("完成: 写出 {0}, 已存在 {1}, 用时 {2}"),
    ZH_HANT("完成: 寫出 {0}, 已存在 {1}, 用時 {2}"),
    KO("완료: {0} 개 기록, {1} 개는 이미 있음, 소요 {2}"),
    DE("fertig: {0} geschrieben, {1} schon vorhanden, in {2}"),
    FR("terminé : {0} écrites, {1} déjà là, en {2}"),
    ES("listo: {0} escritas, {1} ya estaban, en {2}"),
    PT("concluído: {0} escritas, {1} já existiam, em {2}"),
    IT("fatto: {0} scritte, {1} già presenti, in {2}"),
    NL("klaar: {0} geschreven, {1} stonden er al, in {2}"),
    RU("готово: записано {0}, уже было {1}, за {2}"),
    TR("bitti: {0} yazıldı, {1} zaten vardı, süre {2}"));

SS_MSG(warn_fitted_camera,
    EN("camera {0} has a lens no distortion tier represents exactly;\nthe maps are resampled through the fit and land within a pixel of the images"),
    JA("カメラ {0} のレンズは、どの歪みモデルでも厳密には表せません。\nマップは近似を通して再サンプルされ、画像とは 1 画素以内でずれます"),
    ZH_HANS("相机 {0} 的镜头没有任何畸变档位能精确表示；这些图会经拟合结果重采样，与图像相差在一个像素以内"),
    ZH_HANT("相機 {0} 的鏡頭沒有任何畸變檔位能精確表示；這些圖會經擬合結果重取樣，與影像相差在一個像素以內"),
    KO("카메라 {0} 의 렌즈는 어떤 왜곡 단계로도 정확히 표현되지 않습니다.\n맵은 근사를 거쳐 다시 샘플링되며 이미지와 1 픽셀 이내로 어긋납니다"),
    DE("Kamera {0} hat ein Objektiv, das keine Verzeichnungsstufe exakt abbildet;\ndie Karten werden über die Näherung neu abgetastet und liegen innerhalb eines Pixels zu den Bildern"),
    FR("la caméra {0} a un objectif qu'aucun palier de distorsion ne représente exactement ;\nles cartes sont rééchantillonnées via l'ajustement et tombent à un pixel près des images"),
    ES("la cámara {0} tiene un objetivo que ningún nivel de distorsión representa con exactitud;\nlos mapas se remuestrean a través del ajuste y quedan a menos de un píxel de las imágenes"),
    PT("a câmara {0} tem uma objetiva que nenhum nível de distorção representa exatamente;\nos mapas são reamostrados através do ajuste e ficam a menos de um píxel das imagens"),
    IT("la fotocamera {0} ha un obiettivo che nessun livello di distorsione rappresenta esattamente;\nle mappe sono ricampionate attraverso l'approssimazione e cadono entro un pixel dalle immagini"),
    NL("camera {0} heeft een lens die geen enkele vervormingstrap precies weergeeft;\nde kaarten worden via de benadering herbemonsterd en vallen binnen een pixel van de afbeeldingen"),
    RU("у камеры {0} объектив, который ни один уровень дисторсии не описывает точно;\nкарты пересэмплируются через приближение и ложатся в пределах пикселя от изображений"),
    TR("kamera {0}, hiçbir bozulma kademesinin tam olarak temsil etmediği bir mercek kullanıyor;\nharitalar uyum üzerinden yeniden örneklenir ve görüntülerin bir piksel yakınına düşer"));

SS_MSG(opt_install,
    EN("Install assets for --model <id> without a dataset. Only Marigold v2 needs this.\n- Marigold v2 streams about 45 GB from Hugging Face or ModelScope and keeps 11 GB.\n- --depth and --no-normal choose which networks to build."),
    JA("--model <id> に必要なものを取得して終了します（データセットは不要です）。これが要るのは Marigold v2 だけで、Hugging Face（失敗したときはその ModelScope ミラー）から約 45 GB をストリーミングして組み立て、11 GB を残します。--depth と --no-normal で 2 つのネットワークのどちらを作るか選びます。"),
    ZH_HANS("获取 --model <id> 所需的文件后退出，不需要数据集。只有 Marigold v2 需要这样做：它从 Hugging Face（失败时改用其 ModelScope 镜像）流式读取约 45 GB 来构建，最终保留 11 GB。--depth 和 --no-normal 选择构建两个网络中的哪一个。"),
    ZH_HANT("取得 --model <id> 所需的檔案後結束，不需要資料集。只有 Marigold v2 需要這樣做：它從 Hugging Face（失敗時改用其 ModelScope 鏡像）串流讀取約 45 GB 來建置，最後保留 11 GB。--depth 和 --no-normal 選擇建置兩個網路中的哪一個。"),
    KO("--model <id> 에 필요한 것을 받아 두고 끝냅니다. 데이터셋은 필요 없습니다. 이것이 필요한 모델은 Marigold v2 뿐입니다.\n- Hugging Face(실패하면 그 ModelScope 미러)에서 약 45 GB 를 스트리밍해 만들고 11 GB 를 남깁니다. --depth 와 --no-normal 로 두 신경망 중 무엇을 만들지 고릅니다."),
    DE("Holt, was --model <id> braucht, und endet, ohne Datensatz. Nur Marigold v2 braucht das:\n- Es wird aus rund 45 GB gebaut, die von Hugging Face gestreamt werden (bei Fehlern von seinem ModelScope-Spiegel), und belegt danach 11 GB. --depth und --no-normal wählen, welches seiner zwei Netze gebaut wird."),
    FR("Récupère ce dont --model <id> a besoin puis s'arrête, sans jeu de données. Seul Marigold v2 en a besoin :\n- il est construit à partir d'environ 45 Go lus en flux depuis Hugging Face (son miroir ModelScope en cas d'échec) et en garde 11 Go. --depth et --no-normal choisissent lequel de ses deux réseaux construire."),
    ES("Obtiene lo que necesita --model <id> y termina, sin conjunto de datos. Solo Marigold v2 lo necesita: se construye a partir de unos 45 GB transmitidos desde Hugging Face (desde su réplica de ModelScope si falla) y ocupa 11 GB. --depth y --no-normal eligen cuál de sus dos redes construir."),
    PT("Obtém o que --model <id> precisa e termina, sem conjunto de dados. Só o Marigold v2 precisa disto: é construído a partir de cerca de 45 GB transmitidos do Hugging Face (do seu espelho no ModelScope se falhar) e ocupa 11 GB. --depth e --no-normal escolhem qual das suas duas redes construir."),
    IT("Recupera ciò che serve a --model <id> e termina, senza insieme di dati. Serve solo a Marigold v2: viene costruito da circa 45 GB trasmessi da Hugging Face (dal suo mirror ModelScope in caso di errore) e ne tiene 11 GB. --depth e --no-normal scelgono quale delle sue due reti costruire."),
    NL("Haalt op wat --model <id> nodig heeft en stopt, zonder dataset. Alleen Marigold v2 heeft dit nodig:\n- het wordt gebouwd uit zo'n 45 GB die van Hugging Face worden gestreamd (van zijn ModelScope-spiegel als dat mislukt) en houdt 11 GB over. --depth en --no-normal kiezen welke van zijn twee netwerken wordt gebouwd."),
    RU("Получает то, что нужно для --model <id>, и завершается, без набора данных.\n- Это нужно только Marigold v2: он собирается из примерно 45 ГБ, потоково загружаемых с Hugging Face (при ошибке — с его зеркала на ModelScope), и занимает 11 ГБ. --depth и --no-normal выбирают, какую из двух его сетей собрать."),
    TR("--model <id> için gerekenleri alır ve veri kümesi olmadan çıkar. Buna yalnızca Marigold v2 ihtiyaç duyar:\nHugging Face'ten (başarısız olursa ModelScope yansısından) akıtılan yaklaşık 45 GB'tan oluşturulur ve 11 GB yer kaplar. --depth ve --no-normal iki ağından hangisinin oluşturulacağını seçer."));

SS_MSG(log_install_plan,
    EN("{0}: {1} to stream, {2} to write under {3}, {4} free"),
    JA("{0}: ストリーミング {1}、{3} に書き込み {2}、空き {4}"),
    ZH_HANS("{0}: 需流式读取 {1}, 需写入 {2} 到 {3}, 剩余 {4}"),
    ZH_HANT("{0}: 需串流讀取 {1}, 需寫入 {2} 到 {3}, 剩餘 {4}"),
    KO("{0}: 스트리밍 {1}, {3} 에 기록 {2}, 여유 {4}"),
    DE("{0}: {1} zu streamen, {2} zu schreiben unter {3}, {4} frei"),
    FR("{0} : {1} à lire en flux, {2} à écrire sous {3}, {4} libres"),
    ES("{0}: {1} por transmitir, {2} por escribir en {3}, {4} libres"),
    PT("{0}: {1} a transmitir, {2} a escrever em {3}, {4} livres"),
    IT("{0}: {1} da trasmettere, {2} da scrivere in {3}, {4} liberi"),
    NL("{0}: {1} te streamen, {2} te schrijven onder {3}, {4} vrij"),
    RU("{0}: загрузить {1}, записать {2} в {3}, свободно {4}"),
    TR("{0}: akıtılacak {1}, {3} altına yazılacak {2}, boş {4}"));

SS_MSG(err_install_disk,
    EN("Not enough disk space for {0}: it writes {1} more under {2}, which has {3} free, and keeps {4} spare.\nFree some space, or move the cache with XDG_CACHE_HOME (LOCALAPPDATA on Windows)."),
    JA("{0} を入れるにはディスクの空きが足りません。\n- {2} にさらに {1} を書き込みますが、空きは {3} で、{4} は予備として残します。\n- 空きを増やすか、XDG_CACHE_HOME（Windows では LOCALAPPDATA）でキャッシュの場所を移してください。"),
    ZH_HANS("磁盘空间不足，无法安装 {0}：需要在 {2} 下再写入 {1}，该处剩余 {3}，并需保留 {4} 余量。\n请腾出空间，或用 XDG_CACHE_HOME（Windows 上为 LOCALAPPDATA）把缓存移到别处。"),
    ZH_HANT("磁碟空間不足，無法安裝 {0}：需要在 {2} 下再寫入 {1}，該處剩餘 {3}，並需保留 {4} 餘量。\n請騰出空間，或用 XDG_CACHE_HOME（Windows 上為 LOCALAPPDATA）把快取移到別處。"),
    KO("{0} 을(를) 설치할 디스크 공간이 부족합니다.\n- {2} 에 {1} 을(를) 더 써야 하는데 여유 공간은 {3} 이고, {4} 는 여분으로 남겨 둡니다.\n- 공간을 비우거나 XDG_CACHE_HOME(Windows 에서는 LOCALAPPDATA)으로 캐시 위치를 옮기세요."),
    DE("Zu wenig Speicherplatz für {0}: Es schreibt noch {1} unter {2}, dort sind {3} frei, und {4} bleiben als Reserve.\nSchaffen Sie Platz oder verlegen Sie den Zwischenspeicher mit XDG_CACHE_HOME (unter Windows LOCALAPPDATA)."),
    FR("Pas assez d'espace disque pour {0} : il écrit encore {1} sous {2}, qui a {3} de libre, et garde {4} de réserve.\nLibérez de l'espace ou déplacez le cache avec XDG_CACHE_HOME (LOCALAPPDATA sous Windows)."),
    ES("No hay espacio en disco suficiente para {0}: escribe {1} más en {2}, que tiene {3} libres, y deja {4} de reserva.\nLibera espacio o mueve la caché con XDG_CACHE_HOME (LOCALAPPDATA en Windows)."),
    PT("Não há espaço em disco suficiente para {0}: escreve mais {1} em {2}, que tem {3} livres, e mantém {4} de reserva.\nLiberte espaço ou mude a cache de sítio com XDG_CACHE_HOME (LOCALAPPDATA no Windows)."),
    IT("Spazio su disco insufficiente per {0}: scrive altri {1} in {2}, che ha {3} liberi, e ne tiene {4} di riserva.\nLibera spazio o sposta la cache con XDG_CACHE_HOME (LOCALAPPDATA su Windows)."),
    NL("Te weinig schijfruimte voor {0}: het schrijft nog {1} onder {2}, waar {3} vrij is, en houdt {4} in reserve.\nMaak ruimte vrij of verplaats de cache met XDG_CACHE_HOME (LOCALAPPDATA op Windows)."),
    RU("Недостаточно места на диске для {0}: нужно записать ещё {1} в {2}, где свободно {3}, и оставить {4} в запасе.\nОсвободите место или перенесите кэш через XDG_CACHE_HOME (LOCALAPPDATA в Windows)."),
    TR("{0} için yeterli disk alanı yok: {2} altına {1} daha yazılacak, orada {3} boş ve {4} yedek bırakılıyor.\nYer açın ya da önbelleği XDG_CACHE_HOME (Windows'ta LOCALAPPDATA) ile taşıyın."));

// Read back by the GUI (i18n::scan) for its progress bar: keep the
// separators between the placeholders.
SS_MSG(log_install_progress,
    EN("installing {0}: {1} / {2} GB, {3} MB/s"),
    JA("{0} をインストール中: {1} / {2} GB、{3} MB/s"),
    ZH_HANS("正在安装 {0}: {1} / {2} GB, {3} MB/s"),
    ZH_HANT("正在安裝 {0}: {1} / {2} GB, {3} MB/s"),
    KO("{0} 설치 중: {1} / {2} GB, {3} MB/s"),
    DE("installiere {0}: {1} / {2} GB, {3} MB/s"),
    FR("installation de {0} : {1} / {2} Go, {3} Mo/s"),
    ES("instalando {0}: {1} / {2} GB, {3} MB/s"),
    PT("a instalar {0}: {1} / {2} GB, {3} MB/s"),
    IT("installazione di {0}: {1} / {2} GB, {3} MB/s"),
    NL("{0} installeren: {1} / {2} GB, {3} MB/s"),
    RU("установка {0}: {1} / {2} ГБ, {3} МБ/с"),
    TR("{0} kuruluyor: {1} / {2} GB, {3} MB/sn"));

SS_MSG(log_install_done,
    EN("{0} installed in {1}"),
    JA("{0} のインストール完了、所要 {1}"),
    ZH_HANS("{0} 安装完成, 用时 {1}"),
    ZH_HANT("{0} 安裝完成, 用時 {1}"),
    KO("{0} 설치 완료, 소요 {1}"),
    DE("{0} installiert, in {1}"),
    FR("{0} installé en {1}"),
    ES("{0} instalado en {1}"),
    PT("{0} instalado em {1}"),
    IT("{0} installato in {1}"),
    NL("{0} geïnstalleerd in {1}"),
    RU("{0} установлен за {1}"),
    TR("{0} kuruldu, süre {1}"));

SS_MSG(err_not_installed,
    EN("{0} is not installed. `{1} --install {0}` builds it: {2} to stream, "
       "{3} to write."),
    JA("{0} がインストールされていません。`{1} --install {0}` で組み立てられます"
       "（ストリーミング {2}、書き込み {3}）。"),
    ZH_HANS("{0} 尚未安装。`{1} --install {0}` 可以构建它：需流式读取 {2}，"
            "写入 {3}。"),
    ZH_HANT("{0} 尚未安裝。`{1} --install {0}` 可以建置它：需串流讀取 {2}，"
            "寫入 {3}。"),
    KO("{0} 이(가) 설치되어 있지 않습니다. `{1} --install {0}` 으로 만들 수 있습니다. "
       "스트리밍 {2}, 기록 {3}."),
    DE("{0} ist nicht installiert. `{1} --install {0}` baut es: {2} zu "
       "streamen, {3} zu schreiben."),
    FR("{0} n'est pas installé. `{1} --install {0}` le construit : {2} à lire "
       "en flux, {3} à écrire."),
    ES("{0} no está instalado. `{1} --install {0}` lo construye: {2} por "
       "transmitir, {3} por escribir."),
    PT("{0} não está instalado. `{1} --install {0}` constrói-o: {2} a "
       "transmitir, {3} a escrever."),
    IT("{0} non è installato. `{1} --install {0}` lo costruisce: {2} da "
       "trasmettere, {3} da scrivere."),
    NL("{0} is niet geïnstalleerd. `{1} --install {0}` bouwt het: {2} te "
       "streamen, {3} te schrijven."),
    RU("{0} не установлен. `{1} --install {0}` соберёт его: загрузить {2}, "
       "записать {3}."),
    TR("{0} kurulu değil. `{1} --install {0}` onu oluşturur: akıtılacak {2}, "
       "yazılacak {3}."));

SS_MSG(ask_install,
    EN("Install it now? [y/N] "),
    JA("今すぐインストールしますか? [y/N] "),
    ZH_HANS("现在安装吗？[y/N] "),
    ZH_HANT("現在安裝嗎？[y/N] "),
    KO("지금 설치할까요? [y/N] "),
    DE("Jetzt installieren? [y/N] "),
    FR("L'installer maintenant ? [y/N] "),
    ES("¿Instalarlo ahora? [y/N] "),
    PT("Instalar agora? [y/N] "),
    IT("Installarlo ora? [y/N] "),
    NL("Nu installeren? [y/N] "),
    RU("Установить сейчас? [y/N] "),
    TR("Şimdi kurulsun mu? [y/N] "));

}  // namespace geometry
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
