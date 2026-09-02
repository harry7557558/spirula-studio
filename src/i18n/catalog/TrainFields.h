#pragma once

// What the training flags are called and what they do -- one name and one help
// string per row of config/TrainConfig.h's SS_CONFIG_FIELDS table.
//
// The text lives here rather than in the field table so that it can be
// translated: the GUI's options editor and `spirula train --help` both read
// these, so there is one copy of every sentence and both are localized.
//
// The link to the table is by NAME, resolved at compile time: a consumer
// expands SS_CONFIG_FIELDS and writes `msg::field::member` and
// `msg::field::member##_help`, so a flag added to the table with no entry
// here is a compile error naming the flag, not a blank tooltip nobody
// notices.
//
// Flag NAMES on the command line are not translated -- `--sh-degree` is
// `--sh-degree` in every language. What is translated is the sentence a human
// reads about it, plus the human-readable label the GUI shows in place of the
// bare flag name (the flag name itself is one hover away, and stays
// searchable).
//
// SS_MSG_EN is the staged-rollout marker: English in every slot, greppable,
// counted by tools/check_i18n.sh. Translating one is a matter of turning it
// into SS_MSG with the thirteen tags.

#include "i18n/BeginCatalog.h"

#include <cstddef>
#include <cstring>

namespace spirula {
namespace i18n {
namespace msg {
namespace field {


// ===========================================================================
// Run & Output
// ===========================================================================

SS_MSG(data,
    EN("Dataset folder"), JA("データセットのフォルダ"),
    ZH_HANS("数据集文件夹"), ZH_HANT("資料集資料夾"), KO("데이터셋 폴더"),
    DE("Datensatzordner"), FR("Dossier du jeu de données"),
    ES("Carpeta del conjunto de datos"), PT("Pasta do conjunto de dados"),
    IT("Cartella del set di dati"), NL("Datasetmap"),
    RU("Папка набора данных"), TR("Veri kümesi klasörü"));
SS_MSG(data_help,
    EN("Folder holding the dataset to train on. COLMAP, Nerfstudio and Metashape "
       "layouts are all recognized."),
    JA("学習に使うデータセットのフォルダです。COLMAP、Nerfstudio、Metashape の"
       "いずれの構成も認識します。"),
    ZH_HANS("用于训练的数据集所在文件夹。COLMAP、Nerfstudio 和 Metashape 三种"
            "目录结构都能识别。"),
    ZH_HANT("用於訓練的資料集所在資料夾。COLMAP、Nerfstudio 和 Metashape 三種"
            "目錄結構都能辨識。"),
    KO("학습에 사용할 데이터셋이 들어 있는 폴더입니다. COLMAP, Nerfstudio, Metashape "
       "구조를 모두 인식합니다."),
    DE("Ordner mit dem Datensatz, auf dem trainiert wird. COLMAP-, Nerfstudio- "
       "und Metashape-Strukturen werden alle erkannt."),
    FR("Dossier contenant le jeu de données à entraîner. Les structures COLMAP, "
       "Nerfstudio et Metashape sont toutes reconnues."),
    ES("Carpeta que contiene el conjunto de datos con el que entrenar. Se reconocen "
       "las estructuras COLMAP, Nerfstudio y Metashape."),
    PT("Pasta que contém o conjunto de dados para treinar. As estruturas COLMAP, "
       "Nerfstudio e Metashape são todas reconhecidas."),
    IT("Cartella che contiene il set di dati su cui addestrare. Le strutture "
       "COLMAP, Nerfstudio e Metashape sono tutte riconosciute."),
    NL("Map met de dataset waarop wordt getraind. De structuren van COLMAP, Nerfstudio "
       "en Metashape worden alle herkend."),
    RU("Папка с набором данных для обучения. Распознаются структуры COLMAP, Nerfstudio "
       "и Metashape."),
    TR("Üzerinde eğitim yapılacak veri kümesinin bulunduğu klasör. COLMAP, Nerfstudio "
       "ve Metashape düzenlerinin hepsi tanınır."));

SS_MSG(resume,
    EN("Resume a previous run"), JA("前回の学習を再開"),
    ZH_HANS("继续之前的训练"), ZH_HANT("繼續之前的訓練"),
    KO("이전 학습 이어하기"), DE("Vorherigen Lauf fortsetzen"),
    FR("Reprendre un entraînement"), ES("Reanudar una ejecución anterior"),
    PT("Retomar uma execução anterior"),
    IT("Riprendere un'esecuzione precedente"), NL("Eerdere run hervatten"),
    RU("Продолжить прошлый запуск"), TR("Önceki çalıştırmayı sürdür"));
SS_MSG(resume_help,
    EN("Continue a previous run instead of starting from scratch. Point this "
       "at a run's output folder to pick up its newest checkpoint, or at one "
       "specific step-*.ckpt folder. The model and dataset settings come from "
       "that run, while run length, save cadence and viewer settings still come "
       "from the command line. Only works if the earlier run was saved with save_full_checkpoint "
       "turned on."),
    JA("最初からではなく、前回の学習の続きから始めます。実行の出力フォルダを指"
       "定するといちばん新しいチェックポイントを、特定の step-*.ckpt フォルダ"
       "を指定するとそれを読み込みます。モデルとデータセットの設定はその実行か"
       "ら引き継がれ、学習の長さ、保存の間隔、ビューアの設定はコマンドラインの"
       "指定が使われます。前回の実行が save_full_checkpoint を有効にして保存さ"
       "れている場合にだけ使えます。"),
    ZH_HANS("从上次训练继续，而不是从头开始。指向某次运行的输出文件夹会取用其"
            "中最新的检查点，指向具体的 step-*.ckpt 文件夹则读取该检查点。模型"
            "和数据集设置沿用那次运行，而训练长度、保存间隔和查看器设置仍来自"
            "命令行。只有当先前的运行开启了 save_full_checkpoint 时才可用。"),
    ZH_HANT("從上次訓練繼續，而不是從頭開始。指向某次執行的輸出資料夾會取用其"
            "中最新的檢查點，指向具體的 step-*.ckpt 資料夾則讀取該檢查點。模型"
            "和資料集設定沿用那次執行，而訓練長度、儲存間隔和檢視器設定仍來自"
            "命令列。只有當先前的執行開啟了 save_full_checkpoint 時才可用。"),
    KO("처음부터가 아니라 이전 학습에 이어서 시작합니다. 어떤 실행의 출력 폴더"
       "를 가리키면 그중 가장 최근 체크포인트를, 특정 step-*.ckpt 폴더를 가리"
       "키면 그것을 불러옵니다. 모델과 데이터셋 설정은 그 실행에서 가져오고, "
       "학습 길이·저장 주기·뷰어 설정은 여전히 명령줄에서 옵니다. 이전 실행이"
       " save_full_checkpoint를 켠 채 저장된 경우에만 동작합니다."),
    DE("Einen früheren Lauf fortsetzen, statt von vorn zu beginnen. Auf den Ausgabeordner "
       "eines Laufs zeigen lassen, um dessen neuesten Checkpoint aufzugreifen, "
       "oder auf einen bestimmten step-*.ckpt-Ordner. Modell- und Datensatzeinstellungen "
       "stammen aus jenem Lauf, während Lauflänge, Speicherintervall und Viewer-Einstellungen "
       "weiterhin von der Kommandozeile kommen. Funktioniert nur, wenn der frühere "
       "Lauf mit eingeschaltetem save_full_checkpoint gespeichert wurde."),
    FR("Reprendre un entraînement précédent au lieu de repartir de zéro. Pointez "
       "sur le dossier de sortie d'une exécution pour reprendre sa dernière sauvegarde, "
       "ou sur un dossier step-*.ckpt précis. Les réglages du modèle et du jeu "
       "de données viennent de cette exécution, tandis que la durée, la cadence "
       "de sauvegarde et les réglages de la visionneuse viennent toujours de "
       "la ligne de commande. Ne fonctionne que si l'exécution précédente a été "
       "enregistrée avec save_full_checkpoint activé."),
    ES("Reanudar una ejecución anterior en vez de empezar de cero. Apúntelo a "
       "la carpeta de salida de una ejecución para tomar su punto de control "
       "más reciente, o a una carpeta step-*.ckpt concreta. Los ajustes de modelo "
       "y conjunto de datos vienen de aquella ejecución, mientras que la duración, "
       "la frecuencia de guardado y los ajustes del visor siguen viniendo de "
       "la línea de órdenes. Solo funciona si la ejecución anterior se guardó "
       "con save_full_checkpoint activado."),
    PT("Retomar uma execução anterior em vez de começar do zero. Aponte para "
       "a pasta de saída de uma execução para pegar o checkpoint mais recente, "
       "ou para uma pasta step-*.ckpt específica. Os ajustes de modelo e conjunto "
       "de dados vêm daquela execução, enquanto a duração, a frequência de gravação "
       "e os ajustes do visualizador continuam vindo da linha de comando. Só "
       "funciona se a execução anterior foi salva com save_full_checkpoint ligado."),
    IT("Riprendere un'esecuzione precedente invece di ricominciare da zero. Puntare "
       "alla cartella di output di un'esecuzione per riprenderne il checkpoint "
       "più recente, oppure a una specifica cartella step-*.ckpt. Le impostazioni "
       "di modello e set di dati vengono da quell'esecuzione, mentre durata, "
       "cadenza di salvataggio e impostazioni del visualizzatore vengono ancora "
       "dalla riga di comando. Funziona solo se l'esecuzione precedente è stata "
       "salvata con save_full_checkpoint attivo."),
    NL("Een eerdere run hervatten in plaats van opnieuw te beginnen. Wijs naar "
       "de uitvoermap van een run om het nieuwste checkpoint op te pakken, of "
       "naar één specifieke step-*.ckpt-map. De model- en datasetinstellingen "
       "komen uit die run, terwijl runlengte, opslagfrequentie en viewerinstellingen "
       "nog steeds van de opdrachtregel komen. Werkt alleen als de eerdere run "
       "met save_full_checkpoint aan is opgeslagen."),
    RU("Продолжить прошлый запуск, а не начинать заново. Укажите папку вывода "
       "запуска, чтобы взять его самую свежую контрольную точку, или конкретную "
       "папку step-*.ckpt. Настройки модели и набора данных берутся из того запуска, "
       "а длительность, частота сохранения и настройки просмотрщика по-прежнему "
       "берутся из командной строки. Работает, только если прошлый запуск сохранялся "
       "с включённым save_full_checkpoint."),
    TR("Sıfırdan başlamak yerine önceki bir çalıştırmayı sürdürür. Bir çalıştırmanın "
       "çıktı klasörünü gösterirseniz en yeni denetim noktası, belirli bir step-*.ckpt "
       "klasörünü gösterirseniz o alınır. Model ve veri kümesi ayarları o çalıştırmadan "
       "gelir; çalıştırma uzunluğu, kaydetme sıklığı ve görüntüleyici ayarları "
       "yine komut satırından gelir. Yalnızca önceki çalıştırma save_full_checkpoint "
       "açıkken kaydedildiyse çalışır."));

SS_MSG(output_dir_prefix,
    EN("Output folder"), JA("出力フォルダ"), ZH_HANS("输出文件夹"),
    ZH_HANT("輸出資料夾"), KO("출력 폴더"), DE("Ausgabeordner"),
    FR("Dossier de sortie"), ES("Carpeta de salida"), PT("Pasta de saída"),
    IT("Cartella di output"), NL("Uitvoermap"), RU("Папка вывода"),
    TR("Çıktı klasörü"));
SS_MSG(output_dir_prefix_help,
    EN("Folder that each run's output directory is created inside."),
    JA("各実行の出力フォルダがこの中に作られます。"),
    ZH_HANS("每次运行的输出目录都创建在这个文件夹里。"),
    ZH_HANT("每次執行的輸出目錄都建立在這個資料夾裡。"),
    KO("각 실행의 출력 폴더가 이 안에 만들어집니다."),
    DE("Ordner, in dem das Ausgabeverzeichnis jedes Laufs angelegt wird."),
    FR("Dossier dans lequel est créé le répertoire de sortie de chaque exécution."),
    ES("Carpeta dentro de la cual se crea el directorio de salida de cada ejecución."),
    PT("Pasta dentro da qual é criado o diretório de saída de cada execução."),
    IT("Cartella all'interno della quale viene creata la directory di output "
       "di ogni esecuzione."),
    NL("Map waarin de uitvoermap van elke run wordt aangemaakt."),
    RU("Папка, внутри которой создаётся каталог вывода каждого запуска."),
    TR("Her çalıştırmanın çıktı dizininin içinde oluşturulduğu klasör."));

SS_MSG(output_dir_name,
    EN("Run name"), JA("実行名"), ZH_HANS("运行名称"), ZH_HANT("執行名稱"),
    KO("실행 이름"), DE("Laufname"), FR("Nom de l'exécution"),
    ES("Nombre de la ejecución"), PT("Nome da execução"),
    IT("Nome dell'esecuzione"), NL("Runnaam"), RU("Имя запуска"),
    TR("Çalıştırma adı"));
SS_MSG(output_dir_name_help,
    EN("Name of this run's folder inside the output prefix. Leave empty to get "
       "a name built from the dataset name and the current time."),
    JA("出力フォルダの中に作られる、この実行のフォルダ名です。空にすると、デー"
       "タセット名と現在時刻から名前が作られます。"),
    ZH_HANS("本次运行在输出文件夹中的目录名。留空则根据数据集名称和当前时间自"
            "动生成。"),
    ZH_HANT("本次執行在輸出資料夾中的目錄名。留空則依資料集名稱和目前時間自動"
            "產生。"),
    KO("출력 폴더 안에 만들어지는 이번 실행의 폴더 이름입니다. 비워 두면 데이"
       "터셋 이름과 현재 시각으로 이름을 만듭니다."),
    DE("Name des Ordners dieses Laufs im Ausgabeordner. Leer lassen, um einen "
       "Namen aus Datensatzname und aktueller Zeit zu erhalten."),
    FR("Nom du dossier de cette exécution dans le dossier de sortie. Laisser "
       "vide pour un nom construit à partir du nom du jeu de données et de l'heure."),
    ES("Nombre de la carpeta de esta ejecución dentro de la carpeta de salida. "
       "Déjelo vacío para obtener un nombre formado por el nombre del conjunto "
       "de datos y la hora actual."),
    PT("Nome da pasta desta execução dentro da pasta de saída. Deixe vazio para "
       "obter um nome formado pelo nome do conjunto de dados e a hora atual."),
    IT("Nome della cartella di questa esecuzione dentro la cartella di output. "
       "Lasciare vuoto per un nome costruito dal nome del set di dati e dall'ora "
       "attuale."),
    NL("Naam van de map van deze run in de uitvoermap. Laat leeg voor een naam "
       "die uit de datasetnaam en de huidige tijd wordt opgebouwd."),
    RU("Имя папки этого запуска внутри папки вывода. Оставьте пустым, чтобы имя "
       "составилось из названия набора данных и текущего времени."),
    TR("Bu çalıştırmanın çıktı klasörü içindeki klasör adı. Veri kümesi adı ve "
       "o anki saatten oluşan bir ad için boş bırakın."));

SS_MSG(num_iterations,
    EN("Training steps"), JA("学習ステップ数"), ZH_HANS("训练步数"),
    ZH_HANT("訓練步數"), KO("학습 스텝 수"), DE("Trainingsschritte"),
    FR("Étapes d'entraînement"), ES("Pasos de entrenamiento"),
    PT("Passos de treinamento"), IT("Passi di addestramento"),
    NL("Trainingsstappen"), RU("Шаги обучения"), TR("Eğitim adımı sayısı"));
SS_MSG(num_iterations_help,
    EN("How long to train, in steps. More steps often give better visual results "
       "with diminishing returns, and a proportionally longer wait."),
    JA("学習の長さをステップ数で指定します。ステップを増やすと見た目はよくなり"
       "ますが、効果は次第に小さくなり、待ち時間はその分長くなります。"),
    ZH_HANS("训练时长，以步数计。步数越多画面通常越好，但收益递减，等待时间也"
            "成比例增加。"),
    ZH_HANT("訓練長度，以步數計。步數越多畫面通常越好，但效益遞減，等待時間也"
            "成比例增加。"),
    KO("학습 길이를 스텝 수로 지정합니다. 스텝이 많을수록 결과가 좋아지지만 효"
       "과는 점점 줄고 기다리는 시간은 그만큼 늘어납니다."),
    DE("Wie lange trainiert wird, in Schritten. Mehr Schritte sehen meist besser "
       "aus, mit abnehmendem Nutzen und entsprechend längerer Wartezeit."),
    FR("Durée de l'entraînement, en étapes. Plus d'étapes donne souvent un meilleur "
       "rendu, avec des gains décroissants et une attente proportionnellement "
       "plus longue."),
    ES("Duración del entrenamiento, en pasos. Más pasos suelen dar mejor resultado "
       "visual, con rendimientos decrecientes y una espera proporcionalmente "
       "más larga."),
    PT("Duração do treinamento, em passos. Mais passos costumam dar melhor resultado "
       "visual, com ganhos decrescentes e uma espera proporcionalmente maior."),
    IT("Durata dell'addestramento, in passi. Più passi danno spesso un risultato "
       "visivo migliore, con rendimenti decrescenti e un'attesa proporzionalmente "
       "più lunga."),
    NL("Hoe lang er wordt getraind, in stappen. Meer stappen geven meestal een "
       "beter beeld, met afnemende opbrengst en een evenredig langere wachttijd."),
    RU("Длительность обучения в шагах. Больше шагов обычно даёт лучший результат, "
       "но отдача убывает, а ждать приходится пропорционально дольше."),
    TR("Eğitimin adım cinsinden uzunluğu. Daha çok adım genelde daha iyi görünür; "
       "kazanç azalarak sürer ve bekleme süresi orantılı olarak uzar."));

SS_MSG(steps_per_save,
    EN("Checkpoint interval"), JA("チェックポイントの間隔"),
    ZH_HANS("检查点间隔"), ZH_HANT("檢查點間隔"), KO("체크포인트 간격"),
    DE("Checkpoint-Intervall"), FR("Intervalle de sauvegarde"),
    ES("Intervalo de puntos de control"), PT("Intervalo de checkpoints"),
    IT("Intervallo dei checkpoint"), NL("Checkpoint-interval"),
    RU("Интервал контрольных точек"), TR("Denetim noktası aralığı"));
SS_MSG(steps_per_save_help,
    EN("How often a checkpoint is written, in steps. Use -1 to save only when "
       "training finishes, or 0 to never save. Frequent saves cost disk space "
       "and a little time."),
    JA("チェックポイントを書き出す間隔をステップ数で指定します。-1 なら学習の"
       "終わりにだけ保存し、0 なら保存しません。頻繁に保存するとディスク容量と"
       "少しの時間を使います。"),
    ZH_HANS("多少步写一次检查点。-1 表示只在训练结束时保存，0 表示从不保存。频"
            "繁保存会占用磁盘空间和少量时间。"),
    ZH_HANT("多少步寫一次檢查點。-1 表示只在訓練結束時儲存，0 表示從不儲存。頻"
            "繁儲存會佔用磁碟空間和少量時間。"),
    KO("체크포인트를 쓰는 간격을 스텝 수로 지정합니다. -1이면 학습이 끝날 때만"
       " 저장하고 0이면 저장하지 않습니다. 자주 저장하면 디스크 공간과 약간의"
       " 시간이 듭니다."),
    DE("Wie oft ein Checkpoint geschrieben wird, in Schritten. -1 speichert nur "
       "am Ende des Trainings, 0 gar nicht. Häufiges Speichern kostet Plattenplatz "
       "und ein wenig Zeit."),
    FR("À quelle fréquence une sauvegarde est écrite, en étapes. -1 n'enregistre "
       "qu'à la fin de l'entraînement, 0 jamais. Des sauvegardes fréquentes coûtent "
       "de l'espace disque et un peu de temps."),
    ES("Cada cuántos pasos se escribe un punto de control. -1 guarda solo al "
       "terminar el entrenamiento y 0 no guarda nunca. Guardar a menudo cuesta "
       "espacio en disco y algo de tiempo."),
    PT("De quantos em quantos passos um checkpoint é gravado. -1 salva apenas "
       "ao terminar o treinamento e 0 nunca salva. Gravar com frequência custa "
       "espaço em disco e um pouco de tempo."),
    IT("Ogni quanti passi viene scritto un checkpoint. -1 salva solo alla fine "
       "dell'addestramento, 0 non salva mai. Salvare spesso costa spazio su disco "
       "e un po' di tempo."),
    NL("Om de hoeveel stappen een checkpoint wordt geschreven. -1 slaat alleen "
       "aan het eind van de training op, 0 nooit. Vaak opslaan kost schijfruimte "
       "en een beetje tijd."),
    RU("Как часто пишется контрольная точка, в шагах. -1 сохраняет только в конце "
       "обучения, 0 — никогда. Частые сохранения стоят места на диске и немного "
       "времени."),
    TR("Bir denetim noktasının kaç adımda bir yazılacağı. -1 yalnızca eğitim "
       "bitince kaydeder, 0 hiç kaydetmez. Sık kaydetmek disk alanına ve biraz "
       "zamana mal olur."));

SS_MSG(save_only_latest_checkpoint,
    EN("Keep only the newest checkpoint"),
    JA("最新のチェックポイントだけ残す"), ZH_HANS("只保留最新的检查点"),
    ZH_HANT("只保留最新的檢查點"), KO("가장 최근 체크포인트만 보관"),
    DE("Nur den neuesten Checkpoint behalten"),
    FR("Ne garder que la dernière sauvegarde"),
    ES("Conservar solo el punto de control más reciente"),
    PT("Manter apenas o checkpoint mais recente"),
    IT("Conservare solo il checkpoint più recente"),
    NL("Alleen het nieuwste checkpoint bewaren"),
    RU("Хранить только последнюю контрольную точку"),
    TR("Yalnızca en yeni denetim noktasını sakla"));
SS_MSG(save_only_latest_checkpoint_help,
    EN("Keep only the newest checkpoint and delete older ones as training goes. "
       "Turn off to keep the whole history, which uses considerably more disk "
       "space."),
    JA("いちばん新しいチェックポイントだけを残し、古いものは学習の進行に合わせ"
       "て削除します。オフにすると履歴をすべて残しますが、ディスク容量をかなり"
       "多く使います。"),
    ZH_HANS("只保留最新的检查点，随着训练推进删除较旧的。关闭后会保留全部历史，"
            "但会占用多得多的磁盘空间。"),
    ZH_HANT("只保留最新的檢查點，隨著訓練推進刪除較舊的。關閉後會保留全部歷史，"
            "但會佔用多得多的磁碟空間。"),
    KO("가장 최근 체크포인트만 남기고 학습이 진행되는 동안 오래된 것은 지웁니"
       "다. 끄면 전체 이력을 남기지만 디스크를 훨씬 많이 씁니다."),
    DE("Nur den neuesten Checkpoint behalten und ältere im Verlauf des Trainings "
       "löschen. Abgeschaltet bleibt die ganze Historie erhalten, was erheblich "
       "mehr Plattenplatz braucht."),
    FR("Ne garder que la sauvegarde la plus récente et supprimer les plus anciennes "
       "au fil de l'entraînement. Décoché, tout l'historique est conservé, ce "
       "qui occupe bien plus d'espace disque."),
    ES("Conservar solo el punto de control más reciente y borrar los anteriores "
       "según avanza el entrenamiento. Sin marcar se guarda todo el historial, "
       "lo que ocupa bastante más disco."),
    PT("Manter apenas o checkpoint mais recente e apagar os antigos conforme "
       "o treinamento avança. Desmarcado, todo o histórico é mantido, o que ocupa "
       "bem mais disco."),
    IT("Conservare solo il checkpoint più recente ed eliminare i precedenti man "
       "mano che l'addestramento procede. Deselezionato mantiene tutta la storia, "
       "il che occupa molto più disco."),
    NL("Alleen het nieuwste checkpoint bewaren en oudere tijdens de training "
       "verwijderen. Uitgevinkt blijft de hele geschiedenis staan, wat aanzienlijk "
       "meer schijfruimte kost."),
    RU("Хранить только самую свежую контрольную точку и удалять старые по ходу "
       "обучения. Без флажка сохраняется вся история, что занимает заметно больше "
       "места на диске."),
    TR("Yalnızca en yeni denetim noktasını tutar ve eğitim ilerledikçe eskileri "
       "siler. Kapatılırsa tüm geçmiş saklanır ve bu belirgin biçimde daha çok "
       "disk yeri kaplar."));

SS_MSG(save_full_checkpoint,
    EN("Save resumable checkpoints"), JA("再開できるチェックポイントを保存"),
    ZH_HANS("保存可继续训练的检查点"), ZH_HANT("儲存可繼續訓練的檢查點"),
    KO("이어할 수 있는 체크포인트 저장"),
    DE("Fortsetzbare Checkpoints speichern"),
    FR("Enregistrer des sauvegardes reprenables"),
    ES("Guardar puntos de control reanudables"),
    PT("Salvar checkpoints retomáveis"),
    IT("Salvare checkpoint riprendibili"),
    NL("Hervatbare checkpoints opslaan"),
    RU("Сохранять точки для продолжения"),
    TR("Sürdürülebilir denetim noktaları kaydet"));
SS_MSG(save_full_checkpoint_help,
    EN("Also store everything needed to resume training later, not just the finished "
       "splats. Checkpoints get much larger because they carry every splat slot "
       "and the optimizer state. Leave off if you only want the exported splat "
       "file."),
    JA("仕上がったスプラットだけでなく、後で学習を再開するのに必要なものもすべ"
       "て保存します。すべてのスプラット枠とオプティマイザの状態を持つので、チ"
       "ェックポイントはかなり大きくなります。書き出したスプラットファイルだけ"
       "が欲しいならオフのままにしてください。"),
    ZH_HANS("除了训练好的泼溅，还保存以后继续训练所需的一切。检查点会大得多，"
            "因为它包含每个泼溅槽位和优化器状态。如果只想要导出的泼溅文件，就"
            "保持关闭。"),
    ZH_HANT("除了訓練好的潑濺，還儲存以後繼續訓練所需的一切。檢查點會大得多，"
            "因為它包含每個潑濺槽位和最佳化器狀態。如果只想要匯出的潑濺檔案，"
            "就保持關閉。"),
    KO("완성된 스플랫뿐 아니라 나중에 학습을 이어가는 데 필요한 것도 모두 저장"
       "합니다. 모든 스플랫 자리와 옵티마이저 상태를 담기 때문에 체크포인트가"
       " 훨씬 커집니다. 내보낸 스플랫 파일만 필요하면 꺼 두십시오."),
    DE("Zusätzlich alles speichern, was zum späteren Fortsetzen nötig ist, nicht "
       "nur die fertigen Splats. Checkpoints werden viel größer, weil sie jeden "
       "Splat-Platz und den Optimiererzustand mitführen. Ausgeschaltet lassen, "
       "wenn nur die exportierte Splat-Datei gebraucht wird."),
    FR("Enregistrer aussi tout ce qu'il faut pour reprendre l'entraînement plus "
       "tard, pas seulement les splats finis. Les sauvegardes deviennent bien "
       "plus grosses car elles portent chaque emplacement de splat et l'état "
       "de l'optimiseur. À laisser décoché si seul le fichier de splats exporté "
       "vous intéresse."),
    ES("Guardar además todo lo necesario para reanudar el entrenamiento más tarde, "
       "no solo los splats terminados. Los puntos de control se vuelven mucho "
       "mayores porque llevan cada hueco de splat y el estado del optimizador. "
       "Déjelo sin marcar si solo quiere el archivo de splats exportado."),
    PT("Guardar também tudo o que é preciso para retomar o treinamento depois, "
       "não só os splats prontos. Os checkpoints ficam bem maiores porque carregam "
       "cada posição de splat e o estado do otimizador. Deixe desmarcado se quiser "
       "apenas o arquivo de splats exportado."),
    IT("Salvare anche tutto ciò che serve per riprendere l'addestramento più "
       "tardi, non solo gli splat finiti. I checkpoint diventano molto più grandi "
       "perché portano ogni posto splat e lo stato dell'ottimizzatore. Lasciare "
       "deselezionato se serve solo il file di splat esportato."),
    NL("Ook alles opslaan wat nodig is om later verder te trainen, niet alleen "
       "de afgeronde splats. Checkpoints worden veel groter omdat ze elke splatplek "
       "en de optimizerstatus meedragen. Laat uit als je alleen het geëxporteerde "
       "splatbestand wilt."),
    RU("Сохранять не только готовые сплаты, но и всё, что нужно, чтобы позже "
       "продолжить обучение. Контрольные точки становятся заметно больше, поскольку "
       "несут каждую ячейку сплата и состояние оптимизатора. Оставьте выключенным, "
       "если нужен только выгруженный файл сплатов."),
    TR("Yalnızca bitmiş splat'ları değil, sonradan eğitimi sürdürmek için gereken "
       "her şeyi de kaydeder. Denetim noktaları çok daha büyür, çünkü her splat "
       "yuvasını ve iyileştirici durumunu taşırlar. Yalnızca dışa aktarılmış "
       "splat dosyasını istiyorsanız kapalı bırakın."));

SS_MSG(save_eval_images,
    EN("Save evaluation images"), JA("評価用画像を保存"),
    ZH_HANS("保存评估图像"), ZH_HANT("儲存評估影像"), KO("평가 이미지 저장"),
    DE("Auswertungsbilder speichern"),
    FR("Enregistrer les images d'évaluation"),
    ES("Guardar imágenes de evaluación"), PT("Salvar imagens de avaliação"),
    IT("Salvare le immagini di valutazione"), NL("Evaluatiebeelden opslaan"),
    RU("Сохранять изображения оценки"),
    TR("Değerlendirme görüntülerini kaydet"));
SS_MSG(save_eval_images_help,
    EN("Write rendered and reference images for the held-out views when training "
       "finishes. Useful for judging quality, at the cost of a little disk space."),
    JA("学習が終わったとき、評価用に取り分けた視点のレンダリング画像と参照画像"
       "を書き出します。品質を確かめるのに便利ですが、少しディスクを使います。"),
    ZH_HANS("训练结束时，为留出的评估视角写出渲染图和参考图。便于判断质量，代"
            "价是占用少量磁盘空间。"),
    ZH_HANT("訓練結束時，為留出的評估視角寫出算圖與參考影像。便於判斷品質，代"
            "價是佔用少量磁碟空間。"),
    KO("학습이 끝나면 평가용으로 떼어 둔 시점의 렌더 이미지와 원본 이미지를 저"
       "장합니다. 품질을 판단하기 좋지만 디스크를 조금 씁니다."),
    DE("Am Ende des Trainings gerenderte und Referenzbilder für die zurückgehaltenen "
       "Ansichten schreiben. Nützlich, um die Qualität zu beurteilen, kostet "
       "etwas Plattenplatz."),
    FR("Écrire, à la fin de l'entraînement, les images rendues et de référence "
       "pour les vues mises de côté. Utile pour juger de la qualité, au prix "
       "d'un peu d'espace disque."),
    ES("Escribir, al terminar el entrenamiento, las imágenes renderizadas y de "
       "referencia de las vistas apartadas. Útil para juzgar la calidad, a costa "
       "de algo de espacio en disco."),
    PT("Gravar, ao terminar o treinamento, as imagens renderizadas e de referência "
       "das vistas reservadas. Útil para julgar a qualidade, ao custo de um pouco "
       "de espaço em disco."),
    IT("Scrivere, alla fine dell'addestramento, le immagini renderizzate e di "
       "riferimento per le viste messe da parte. Utile per giudicare la qualità, "
       "al costo di un po' di spazio su disco."),
    NL("Aan het eind van de training gerenderde en referentiebeelden voor de "
       "opzijgezette weergaven wegschrijven. Handig om de kwaliteit te beoordelen, "
       "ten koste van wat schijfruimte."),
    RU("По окончании обучения записывать отрисованные и эталонные изображения "
       "для отложенных видов. Удобно для оценки качества ценой небольшого места "
       "на диске."),
    TR("Eğitim bitince, ayrılmış görünümler için çizilen ve kaynak görüntüleri "
       "yazar. Kaliteyi değerlendirmek için işe yarar, karşılığında biraz disk "
       "yeri harcar."));

SS_MSG(viewer_port,
    EN("Web viewer port"), JA("Web ビューアのポート"),
    ZH_HANS("网页查看器端口"), ZH_HANT("網頁檢視器連接埠"),
    KO("웹 뷰어 포트"), DE("Port des Web-Viewers"),
    FR("Port de la visionneuse web"), ES("Puerto del visor web"),
    PT("Porta do visualizador web"), IT("Porta del visualizzatore web"),
    NL("Poort van de webviewer"), RU("Порт веб-просмотрщика"),
    TR("Web görüntüleyici bağlantı noktası"));
SS_MSG(viewer_port_help,
    EN("Network port the built-in web viewer listens on. Change it if that port "
       "is already taken."),
    JA("内蔵の Web ビューアが待ち受けるネットワークポートです。そのポートがす"
       "でに使われているときは変更してください。"),
    ZH_HANS("内置网页查看器监听的网络端口。如果该端口已被占用，请改成别的。"),
    ZH_HANT("內建網頁檢視器監聽的網路連接埠。如果該連接埠已被占用，請改成別的。"),
    KO("내장 웹 뷰어가 사용하는 네트워크 포트입니다. 그 포트가 이미 쓰이고 있"
       "으면 바꾸십시오."),
    DE("Netzwerkport, auf dem der eingebaute Web-Viewer lauscht. Ändern, wenn "
       "dieser Port schon belegt ist."),
    FR("Port réseau sur lequel écoute la visionneuse web intégrée. À changer "
       "si ce port est déjà pris."),
    ES("Puerto de red en el que escucha el visor web integrado. Cámbielo si ese "
       "puerto ya está ocupado."),
    PT("Porta de rede em que o visualizador web embutido escuta. Mude-a se essa "
       "porta já estiver ocupada."),
    IT("Porta di rete su cui ascolta il visualizzatore web integrato. Da cambiare "
       "se quella porta è già occupata."),
    NL("Netwerkpoort waarop de ingebouwde webviewer luistert. Verander die als "
       "de poort al bezet is."),
    RU("Сетевой порт, который слушает встроенный веб-просмотрщик. Измените, если "
       "порт уже занят."),
    TR("Yerleşik web görüntüleyicinin dinlediği ağ bağlantı noktası. O bağlantı "
       "noktası doluysa değiştirin."));

SS_MSG(disable_viewer,
    EN("Disable the web viewer"), JA("Web ビューアを使わない"),
    ZH_HANS("不启用网页查看器"), ZH_HANT("不啟用網頁檢視器"),
    KO("웹 뷰어 사용 안 함"), DE("Web-Viewer abschalten"),
    FR("Désactiver la visionneuse web"), ES("Desactivar el visor web"),
    PT("Desativar o visualizador web"),
    IT("Disattivare il visualizzatore web"), NL("Webviewer uitschakelen"),
    RU("Отключить веб-просмотрщик"), TR("Web görüntüleyiciyi kapat"));
SS_MSG(disable_viewer_help,
    EN("Do not start the web viewer. Frees a little memory and avoids port conflicts "
       "when several trainings run at once."),
    JA("Web ビューアを起動しません。メモリが少し空き、学習を同時に何本も走らせ"
       "るときのポートの衝突も避けられます。"),
    ZH_HANS("不启动网页查看器。可以省下一点内存，并避免同时跑多个训练时的端口"
            "冲突。"),
    ZH_HANT("不啟動網頁檢視器。可以省下一點記憶體，並避免同時跑多個訓練時的連"
            "接埠衝突。"),
    KO("웹 뷰어를 시작하지 않습니다. 메모리를 조금 아끼고, 학습을 여러 개 동시"
       "에 돌릴 때 포트 충돌도 피할 수 있습니다."),
    DE("Den Web-Viewer nicht starten. Spart etwas Speicher und vermeidet Portkonflikte, "
       "wenn mehrere Trainings gleichzeitig laufen."),
    FR("Ne pas lancer la visionneuse web. Libère un peu de mémoire et évite les "
       "conflits de port quand plusieurs entraînements tournent en même temps."),
    ES("No iniciar el visor web. Libera algo de memoria y evita conflictos de "
       "puerto cuando hay varios entrenamientos a la vez."),
    PT("Não iniciar o visualizador web. Libera um pouco de memória e evita conflitos "
       "de porta quando vários treinamentos rodam ao mesmo tempo."),
    IT("Non avviare il visualizzatore web. Libera un po' di memoria ed evita "
       "conflitti di porta quando più addestramenti girano insieme."),
    NL("De webviewer niet starten. Scheelt wat geheugen en voorkomt poortconflicten "
       "als er meerdere trainingen tegelijk lopen."),
    RU("Не запускать веб-просмотрщик. Освобождает немного памяти и снимает конфликты "
       "портов, когда одновременно идут несколько обучений."),
    TR("Web görüntüleyiciyi başlatmaz. Biraz bellek kazandırır ve aynı anda birden "
       "çok eğitim koşarken bağlantı noktası çakışmalarını önler."));

SS_MSG(keep_viewer_alive,
    EN("Keep the viewer open at the end"),
    JA("終了後もビューアを開いたままにする"),
    ZH_HANS("训练结束后保持查看器打开"), ZH_HANT("訓練結束後保持檢視器開啟"),
    KO("학습이 끝나도 뷰어 유지"), DE("Viewer am Ende offen lassen"),
    FR("Garder la visionneuse ouverte à la fin"),
    ES("Mantener el visor abierto al terminar"),
    PT("Manter o visualizador aberto no fim"),
    IT("Tenere il visualizzatore aperto alla fine"),
    NL("Viewer aan het eind open houden"),
    RU("Оставлять просмотрщик открытым в конце"),
    TR("Bitince görüntüleyiciyi açık tut"));
SS_MSG(keep_viewer_alive_help,
    EN("Keep the program running after training finishes so the result stays "
       "open in the viewer. Press Ctrl-C to exit. Has no effect when the viewer "
       "is disabled."),
    JA("学習が終わってもプログラムを終了させず、結果をビューアで開いたままにし"
       "ます。終了するには Ctrl-C を押してください。ビューアを無効にしていると"
       "きは効果がありません。"),
    ZH_HANS("训练结束后不退出程序，让结果继续在查看器中打开。按 Ctrl-C 退出。"
            "查看器被禁用时该项无效。"),
    ZH_HANT("訓練結束後不結束程式，讓結果繼續在檢視器中開啟。按 Ctrl-C 結束。"
            "檢視器被停用時該項無效。"),
    KO("학습이 끝나도 프로그램을 종료하지 않고 결과를 뷰어에 열어 둡니다. 끝내"
       "려면 Ctrl-C를 누르십시오. 뷰어를 껐을 때는 효과가 없습니다."),
    DE("Das Programm nach dem Training weiterlaufen lassen, damit das Ergebnis "
       "im Viewer offen bleibt. Zum Beenden Strg-C drücken. Ohne Wirkung, wenn "
       "der Viewer abgeschaltet ist."),
    FR("Garder le programme en fonctionnement après l'entraînement pour que le "
       "résultat reste ouvert dans la visionneuse. Appuyez sur Ctrl-C pour quitter. "
       "Sans effet si la visionneuse est désactivée."),
    ES("Mantener el programa en marcha al terminar el entrenamiento para que "
       "el resultado siga abierto en el visor. Pulse Ctrl-C para salir. Sin efecto "
       "si el visor está desactivado."),
    PT("Manter o programa em execução depois do treinamento para que o resultado "
       "continue aberto no visualizador. Pressione Ctrl-C para sair. Sem efeito "
       "se o visualizador estiver desativado."),
    IT("Tenere il programma in esecuzione dopo l'addestramento perché il risultato "
       "resti aperto nel visualizzatore. Premere Ctrl-C per uscire. Senza effetto "
       "se il visualizzatore è disattivato."),
    NL("Het programma na de training laten doorlopen zodat het resultaat in de "
       "viewer open blijft. Druk op Ctrl-C om af te sluiten. Zonder effect als "
       "de viewer uit staat."),
    RU("Не завершать программу после обучения, чтобы результат оставался открытым "
       "в просмотрщике. Для выхода нажмите Ctrl-C. Без действия, когда просмотрщик "
       "отключён."),
    TR("Eğitim bitince programı kapatmaz; sonuç görüntüleyicide açık kalır. Çıkmak "
       "için Ctrl-C'ye basın. Görüntüleyici kapalıyken etkisi yoktur."));


// ===========================================================================
// Dataset
// ===========================================================================

SS_MSG(data_format,
    EN("Dataset format"), JA("データセットの形式"), ZH_HANS("数据集格式"),
    ZH_HANT("資料集格式"), KO("데이터셋 형식"), DE("Datensatzformat"),
    FR("Format du jeu de données"), ES("Formato del conjunto de datos"),
    PT("Formato do conjunto de dados"), IT("Formato del set di dati"),
    NL("Datasetformaat"), RU("Формат набора данных"),
    TR("Veri kümesi biçimi"));
SS_MSG(data_format_help,
    EN("Which dataset layout to read. Leave empty to detect it from the folder "
       "contents."),
    JA("読み込むデータセットの構成です。空にするとフォルダの中身から自動で判定"
       "します。"),
    ZH_HANS("要读取的数据集目录结构。留空则根据文件夹内容自动判断。"),
    ZH_HANT("要讀取的資料集目錄結構。留空則依資料夾內容自動判斷。"),
    KO("읽어들일 데이터셋 구조입니다. 비워 두면 폴더 내용을 보고 자동으로 판별"
       "합니다."),
    DE("Welche Datensatzstruktur gelesen wird. Leer lassen, um sie am Ordnerinhalt "
       "zu erkennen."),
    FR("Structure de jeu de données à lire. Laisser vide pour la détecter d'après "
       "le contenu du dossier."),
    ES("Estructura del conjunto de datos que se va a leer. Déjelo vacío para "
       "detectarla por el contenido de la carpeta."),
    PT("Estrutura do conjunto de dados a ler. Deixe vazio para detectá-la pelo "
       "conteúdo da pasta."),
    IT("Struttura del set di dati da leggere. Lasciare vuoto per rilevarla dal "
       "contenuto della cartella."),
    NL("Welke datasetstructuur wordt gelezen. Laat leeg om die aan de inhoud "
       "van de map te herkennen."),
    RU("Какую структуру набора данных читать. Оставьте пустым, чтобы определить "
       "её по содержимому папки."),
    TR("Okunacak veri kümesi düzeni. Klasör içeriğinden algılanması için boş "
       "bırakın."));

SS_MSG(image_dir,
    EN("Image subfolder"), JA("画像のサブフォルダ"), ZH_HANS("图像子文件夹"),
    ZH_HANT("影像子資料夾"), KO("이미지 하위 폴더"),
    DE("Bilder-Unterordner"), FR("Sous-dossier des images"),
    ES("Subcarpeta de imágenes"), PT("Subpasta de imagens"),
    IT("Sottocartella delle immagini"), NL("Submap met beelden"),
    RU("Подпапка изображений"), TR("Görüntü alt klasörü"));
SS_MSG(image_dir_help,
    EN("Subfolder holding the training images, for COLMAP and Metashape datasets."),
    JA("学習画像が入っているサブフォルダです。COLMAP と Metashape のデータセッ"
       "トで使います。"),
    ZH_HANS("存放训练图像的子文件夹，用于 COLMAP 和 Metashape 数据集。"),
    ZH_HANT("存放訓練影像的子資料夾，用於 COLMAP 和 Metashape 資料集。"),
    KO("학습 이미지가 들어 있는 하위 폴더입니다. COLMAP과 Metashape 데이터셋에"
       "서 씁니다."),
    DE("Unterordner mit den Trainingsbildern, bei COLMAP- und Metashape-Datensätzen."),
    FR("Sous-dossier contenant les images d'entraînement, pour les jeux de données "
       "COLMAP et Metashape."),
    ES("Subcarpeta con las imágenes de entrenamiento, para conjuntos de datos "
       "COLMAP y Metashape."),
    PT("Subpasta com as imagens de treinamento, para conjuntos de dados COLMAP "
       "e Metashape."),
    IT("Sottocartella con le immagini di addestramento, per i set di dati COLMAP "
       "e Metashape."),
    NL("Submap met de trainingsbeelden, voor COLMAP- en Metashape-datasets."),
    RU("Подпапка с обучающими изображениями — для наборов COLMAP и Metashape."),
    TR("Eğitim görüntülerini içeren alt klasör; COLMAP ve Metashape veri kümeleri "
       "için."));

SS_MSG(mask_dir,
    EN("Mask subfolder"), JA("マスクのサブフォルダ"),
    ZH_HANS("蒙版子文件夹"), ZH_HANT("遮罩子資料夾"), KO("마스크 하위 폴더"),
    DE("Masken-Unterordner"), FR("Sous-dossier des masques"),
    ES("Subcarpeta de máscaras"), PT("Subpasta de máscaras"),
    IT("Sottocartella delle maschere"), NL("Submap met maskers"),
    RU("Подпапка масок"), TR("Maske alt klasörü"));
SS_MSG(mask_dir_help,
    EN("Subfolder holding the image masks, for COLMAP and Metashape datasets. "
       "What a mask means is set by apply_loss_for_mask."),
    JA("画像マスクが入っているサブフォルダです。COLMAP と Metashape のデータセ"
       "ットで使います。マスクの意味は apply_loss_for_mask で決まります。"),
    ZH_HANS("存放图像蒙版的子文件夹，用于 COLMAP 和 Metashape 数据集。蒙版的含"
            "义由 apply_loss_for_mask 决定。"),
    ZH_HANT("存放影像遮罩的子資料夾，用於 COLMAP 和 Metashape 資料集。遮罩的含"
            "意由 apply_loss_for_mask 決定。"),
    KO("이미지 마스크가 들어 있는 하위 폴더입니다. COLMAP과 Metashape 데이터셋"
       "에서 씁니다. 마스크의 의미는 apply_loss_for_mask가 정합니다."),
    DE("Unterordner mit den Bildmasken, bei COLMAP- und Metashape-Datensätzen. "
       "Was eine Maske bedeutet, legt apply_loss_for_mask fest."),
    FR("Sous-dossier contenant les masques d'image, pour les jeux de données "
       "COLMAP et Metashape. Ce que signifie un masque est fixé par apply_loss_for_mask."),
    ES("Subcarpeta con las máscaras de imagen, para conjuntos de datos COLMAP "
       "y Metashape. Lo que significa una máscara lo fija apply_loss_for_mask."),
    PT("Subpasta com as máscaras de imagem, para conjuntos de dados COLMAP e "
       "Metashape. O que uma máscara significa é definido por apply_loss_for_mask."),
    IT("Sottocartella con le maschere delle immagini, per i set di dati COLMAP "
       "e Metashape. Che cosa significhi una maschera lo stabilisce apply_loss_for_mask."),
    NL("Submap met de beeldmaskers, voor COLMAP- en Metashape-datasets. Wat een "
       "masker betekent, bepaalt apply_loss_for_mask."),
    RU("Подпапка с масками изображений — для наборов COLMAP и Metashape. Что "
       "означает маска, задаёт apply_loss_for_mask."),
    TR("Görüntü maskelerini içeren alt klasör; COLMAP ve Metashape veri kümeleri "
       "için. Maskenin ne anlama geldiğini apply_loss_for_mask belirler."));

SS_MSG(load_masks,
    EN("Use dataset masks"), JA("データセットのマスクを使う"),
    ZH_HANS("使用数据集的蒙版"), ZH_HANT("使用資料集的遮罩"),
    KO("데이터셋의 마스크 사용"), DE("Masken des Datensatzes nutzen"),
    FR("Utiliser les masques du jeu"),
    ES("Usar las máscaras del conjunto"),
    PT("Usar as máscaras do conjunto"),
    IT("Usare le maschere del set"),
    NL("Maskers van de dataset gebruiken"),
    RU("Использовать маски набора"),
    TR("Veri kümesinin maskelerini kullan"));
SS_MSG(load_masks_help,
    EN("Use the dataset's masks when they exist. What they then mean is set by "
       "apply_loss_for_mask; turn off to train as if the dataset had none."),
    JA("データセットにマスクがあれば使います。その意味は apply_loss_for_mask で"
       "決まります。オフにするとマスクがないものとして学習します。"),
    ZH_HANS("数据集里有蒙版时就使用它们。它们的含义由 apply_loss_for_mask 决定；"
            "关掉就当作数据集没有蒙版来训练。"),
    ZH_HANT("資料集裡有遮罩時就使用它們。它們的含意由 apply_loss_for_mask 決定；"
            "關掉就當作資料集沒有遮罩來訓練。"),
    KO("데이터셋에 마스크가 있으면 사용합니다. 그 의미는 apply_loss_for_mask가 "
       "정하며, 끄면 마스크가 없는 데이터셋처럼 학습합니다."),
    DE("Die Masken des Datensatzes verwenden, sofern vorhanden. Was sie bedeuten, "
       "legt apply_loss_for_mask fest; abschalten trainiert wie ohne Masken."),
    FR("Utiliser les masques du jeu de données lorsqu'ils existent. Ce qu'ils "
       "signifient est fixé par apply_loss_for_mask ; décocher entraîne comme "
       "si le jeu n'en avait pas."),
    ES("Usar las máscaras del conjunto de datos cuando existan. Lo que significan "
       "lo fija apply_loss_for_mask; desactive para entrenar como si no hubiera."),
    PT("Usar as máscaras do conjunto de dados quando existirem. O que elas "
       "significam é definido por apply_loss_for_mask; desligue para treinar "
       "como se não houvesse nenhuma."),
    IT("Usare le maschere del set di dati quando ci sono. Che cosa significhino "
       "lo stabilisce apply_loss_for_mask; disattivare addestra come se non ce "
       "ne fossero."),
    NL("De maskers van de dataset gebruiken als die er zijn. Wat ze betekenen, "
       "bepaalt apply_loss_for_mask; zet uit om te trainen alsof er geen zijn."),
    RU("Использовать маски набора, если они есть. Что они означают, задаёт "
       "apply_loss_for_mask; выключите, чтобы обучать как без масок."),
    TR("Veri kümesinde maskeler varsa onları kullanır. Ne anlama geldiklerini "
       "apply_loss_for_mask belirler; maskesiz eğitmek için kapatın."));

SS_MSG(apply_loss_for_mask,
    EN("Train masked pixels as empty"), JA("マスク部分を空として学習"),
    ZH_HANS("把被遮住的像素当作空白训练"),
    ZH_HANT("把被遮住的像素當作空白訓練"),
    KO("가려진 픽셀을 빈 곳으로 학습"),
    DE("Maskierte Pixel als leer trainieren"),
    FR("Entraîner les pixels masqués comme vides"),
    ES("Entrenar los píxeles enmascarados como vacíos"),
    PT("Treinar os pixels mascarados como vazios"),
    IT("Addestrare i pixel mascherati come vuoti"),
    NL("Gemaskeerde pixels als leeg trainen"),
    RU("Обучать закрытые маской пиксели как пустоту"),
    TR("Maskelenen pikselleri boş olarak eğit"));
SS_MSG(apply_loss_for_mask_help,
    EN("Whether masked-out pixels are ignored or trained as empty space. Off "
       "ignores them, which is how you hide distractions such as people, cars, "
       "or the black area outside a fisheye circle. On trains them as empty, "
       "which removes the background and leaves just the subject."),
    JA("マスクされた画素を無視するか、空として学習するかを決めます。オフなら無"
       "視され、通行人や車、魚眼の円外の黒い部分といった邪魔物を隠すのに使えま"
       "す。オンなら空として学習され、背景が取り除かれて被写体だけが残ります。"),
    ZH_HANS("被遮住的像素是忽略还是按空白训练。关闭时忽略它们，可用来隐藏行人、"
            "汽车、鱼眼圆外的黑边等干扰物。开启时按空白训练，会去掉背景，只留"
            "下主体。"),
    ZH_HANT("被遮住的像素是忽略還是按空白訓練。關閉時忽略它們，可用來隱藏行人、"
            "汽車、魚眼圓外的黑邊等干擾物。開啟時按空白訓練，會去掉背景，只留"
            "下主體。"),
    KO("가려진 픽셀을 무시할지, 빈 공간으로 학습할지 정합니다. 끄면 무시하므로"
       " 사람, 자동차, 어안 원 바깥의 검은 영역 같은 방해물을 가리는 데 쓸 수"
       " 있습니다. 켜면 빈 곳으로 학습해 배경이 사라지고 피사체만 남습니다."),
    DE("Ob maskierte Pixel ignoriert oder als leerer Raum trainiert werden. Aus "
       "ignoriert sie, womit sich Störendes wie Passanten, Autos oder der schwarze "
       "Bereich außerhalb des Fischaugenkreises ausblenden lässt. An trainiert "
       "sie als leer, was den Hintergrund entfernt und nur das Motiv übrig lässt."),
    FR("Les pixels masqués sont-ils ignorés ou entraînés comme du vide. Décoché, "
       "ils sont ignorés, ce qui permet de cacher les gêneurs : passants, voitures, "
       "ou la zone noire hors du cercle fisheye. Coché, ils sont entraînés comme "
       "vides, ce qui supprime l'arrière-plan et ne laisse que le sujet."),
    ES("Si los píxeles enmascarados se ignoran o se entrenan como espacio vacío. "
       "Desactivado los ignora, que es como se ocultan elementos molestos: transeúntes, "
       "coches o la zona negra fuera del círculo de ojo de pez. Activado los "
       "entrena como vacíos, lo que elimina el fondo y deja solo el sujeto."),
    PT("Se os pixels mascarados são ignorados ou treinados como espaço vazio. "
       "Desligado os ignora, que é como se escondem elementos indesejados: pessoas, "
       "carros ou a área preta fora do círculo olho de peixe. Ligado os treina "
       "como vazios, o que remove o fundo e deixa só o sujeito."),
    IT("Se i pixel mascherati vengono ignorati o addestrati come spazio vuoto. "
       "Disattivato li ignora, ed è così che si nascondono gli elementi di disturbo: "
       "passanti, automobili o l'area nera fuori dal cerchio fisheye. Attivato "
       "li addestra come vuoti, il che rimuove lo sfondo e lascia solo il soggetto."),
    NL("Of gemaskeerde pixels worden genegeerd of als lege ruimte getraind. Uit "
       "negeert ze, waarmee je stoorelementen verbergt: voorbijgangers, auto's "
       "of het zwarte gebied buiten de fisheye-cirkel. Aan traint ze als leeg, "
       "waardoor de achtergrond verdwijnt en alleen het onderwerp overblijft."),
    RU("Игнорировать закрытые маской пиксели или обучать их как пустоту. Выключено "
       "— игнорирует; так скрывают помехи: прохожих, машины, чёрную область вне "
       "круга фишая. Включено — обучает как пустоту, что убирает фон и оставляет "
       "только объект."),
    TR("Maskelenen piksellerin yok sayılması mı yoksa boş alan olarak eğitilmesi "
       "mi. Kapalıyken yok sayılır; geçen insanlar, arabalar ya da balıkgözü "
       "dairesinin dışındaki siyah alan gibi istenmeyenler böyle gizlenir. Açıkken "
       "boş olarak eğitilir; arka plan kalkar ve yalnızca özne kalır."));

SS_MSG(flip_mask,
    EN("Flip masks"), JA("マスクを反転"),
    ZH_HANS("反转蒙版"), ZH_HANT("反轉遮罩"),
    KO("마스크 반전"), DE("Masken umkehren"),
    FR("Inverser les masques"), ES("Invertir las máscaras"),
    PT("Inverter as máscaras"),
    IT("Invertire le maschere"),
    NL("Maskers omkeren"), RU("Инвертировать маски"),
    TR("Maskeleri ters çevir"));
SS_MSG(flip_mask_help,
    EN("Swap what a mask keeps for what it hides. Masks here are white where the "
       "image is kept; turn this on for masks that instead paint the region to "
       "remove. Applied before the mask edge adjustment."),
    JA("マスクが残す領域と隠す領域を入れ替えます。ここでのマスクは残す部分が白"
       "です。取り除く領域を塗ったマスクではこれを有効にしてください。マスク境"
       "界の調整より前に適用されます。"),
    ZH_HANS("交换蒙版保留与隐藏的区域。这里的蒙版以白色表示保留的部分；若蒙版画"
            "的是要去掉的区域，请打开此项。它在蒙版边缘调整之前生效。"),
    ZH_HANT("交換遮罩保留與隱藏的區域。這裡的遮罩以白色表示保留的部分；若遮罩畫"
            "的是要去掉的區域，請開啟此項。它在遮罩邊緣調整之前生效。"),
    KO("마스크가 남기는 영역과 가리는 영역을 맞바꿉니다. 여기서 마스크는 남길 "
       "부분이 흰색입니다. 지울 영역을 칠한 마스크라면 이 항목을 켜십시오. 마스크 "
       "가장자리 조정보다 먼저 적용됩니다."),
    DE("Vertauscht, was eine Maske behält, mit dem, was sie verbirgt. Masken sind "
       "hier weiß, wo das Bild behalten wird; für Masken, die stattdessen den zu "
       "entfernenden Bereich zeichnen, einschalten. Wirkt vor der "
       "Maskenrand-Anpassung."),
    FR("Échange ce qu'un masque garde et ce qu'il cache. Ici les masques sont "
       "blancs là où l'image est gardée ; activez ceci pour des masques qui "
       "peignent au contraire la zone à retirer. Appliqué avant l'ajustement du "
       "bord du masque."),
    ES("Intercambia lo que una máscara conserva con lo que oculta. Aquí las "
       "máscaras son blancas donde se conserva la imagen; actívalo para máscaras "
       "que en cambio pintan la zona a quitar. Se aplica antes del ajuste del "
       "borde de la máscara."),
    PT("Troca o que uma máscara mantém pelo que ela esconde. Aqui as máscaras são "
       "brancas onde a imagem é mantida; ative isto para máscaras que pintam antes "
       "a área a remover. Aplicado antes do ajuste da borda da máscara."),
    IT("Scambia ciò che una maschera conserva con ciò che nasconde. Qui le "
       "maschere sono bianche dove l'immagine viene conservata; attivalo per "
       "maschere che dipingono invece l'area da togliere. Si applica prima della "
       "regolazione del bordo della maschera."),
    NL("Verwisselt wat een masker behoudt met wat het verbergt. Maskers zijn hier "
       "wit waar het beeld behouden blijft; zet dit aan voor maskers die juist het "
       "te verwijderen gebied inkleuren. Werkt vóór de aanpassing van de maskerrand."),
    RU("Меняет местами то, что маска сохраняет, и то, что она скрывает. Здесь "
       "маски белые там, где изображение сохраняется; включите это для масок, "
       "которые вместо этого закрашивают удаляемую область. Действует до правки "
       "края маски."),
    TR("Bir maskenin koruduğu ile gizlediğini yer değiştirir. Buradaki maskeler "
       "görüntünün korunduğu yerde beyazdır; bunun yerine kaldırılacak alanı "
       "boyayan maskeler için açın. Maske kenarı ayarından önce uygulanır."));

SS_MSG(mask_boundary_offset,
    EN("Mask edge adjustment"), JA("マスク境界の調整"),
    ZH_HANS("蒙版边缘调整"), ZH_HANT("遮罩邊緣調整"),
    KO("마스크 가장자리 조정"), DE("Maskenrand-Anpassung"),
    FR("Ajustement du bord du masque"), ES("Ajuste del borde de la máscara"),
    PT("Ajuste da borda da máscara"),
    IT("Regolazione del bordo della maschera"),
    NL("Aanpassing van de maskerrand"), RU("Правка края маски"),
    TR("Maske kenarı ayarı"));
SS_MSG(mask_boundary_offset_help,
    EN("Grow or shrink masks by this fraction of the image size. Negative values "
       "pull the mask edge inward, trimming halos and bad pixels along the boundary; "
       "positive values push it outward."),
    JA("マスクを画像サイズのこの割合だけ広げたり縮めたりします。負の値はマスク"
       "の縁を内側に寄せ、境界沿いのにじみや不良画素を削ります。正の値は外側へ"
       "広げます。"),
    ZH_HANS("按图像尺寸的这一比例扩大或收缩蒙版。负值把蒙版边缘往里收，去掉边"
            "界处的光晕和坏像素；正值则往外扩。"),
    ZH_HANT("按影像尺寸的這一比例擴大或收縮遮罩。負值把遮罩邊緣往內收，去掉邊"
            "界處的光暈和壞像素；正值則往外擴。"),
    KO("마스크를 이미지 크기의 이 비율만큼 넓히거나 좁힙니다. 음수는 마스크 가"
       "장자리를 안쪽으로 당겨 경계의 번짐과 불량 픽셀을 깎아내고, 양수는 바깥"
       "으로 밉니다."),
    DE("Masken um diesen Anteil der Bildgröße vergrößern oder verkleinern. Negative "
       "Werte ziehen den Maskenrand nach innen und schneiden Halos und schlechte "
       "Pixel am Rand weg; positive schieben ihn nach außen."),
    FR("Élargir ou rétrécir les masques de cette fraction de la taille de l'image. "
       "Les valeurs négatives tirent le bord du masque vers l'intérieur, en rognant "
       "les halos et les mauvais pixels du contour ; les positives le poussent "
       "vers l'extérieur."),
    ES("Ampliar o encoger las máscaras en esta fracción del tamaño de la imagen. "
       "Los valores negativos meten el borde hacia dentro y recortan halos y "
       "píxeles malos del contorno; los positivos lo empujan hacia fuera."),
    PT("Ampliar ou encolher as máscaras nesta fração do tamanho da imagem. Valores "
       "negativos puxam a borda da máscara para dentro, aparando halos e pixels "
       "ruins do contorno; positivos a empurram para fora."),
    IT("Allargare o restringere le maschere di questa frazione della dimensione "
       "dell'immagine. I valori negativi tirano il bordo della maschera verso "
       "l'interno, tagliando aloni e pixel cattivi lungo il contorno; quelli "
       "positivi lo spingono verso l'esterno."),
    NL("Maskers vergroten of verkleinen met dit deel van de beeldgrootte. Negatieve "
       "waarden trekken de maskerrand naar binnen en snijden halo's en slechte "
       "pixels langs de rand weg; positieve duwen hem naar buiten."),
    RU("Расширять или сужать маски на эту долю размера изображения. Отрицательные "
       "значения втягивают край маски внутрь, срезая ореолы и плохие пиксели "
       "по границе; положительные выталкивают его наружу."),
    TR("Maskeleri görüntü boyutunun bu kesri kadar büyütür ya da küçültür. Negatif "
       "değerler maske kenarını içeri çeker ve sınırdaki halkalarla bozuk pikselleri "
       "budar; pozitif değerler dışarı iter."));

SS_MSG(depth_dir,
    EN("Depth map subfolder"), JA("深度マップのサブフォルダ"),
    ZH_HANS("深度图子文件夹"), ZH_HANT("深度圖子資料夾"),
    KO("깊이 맵 하위 폴더"), DE("Tiefenkarten-Unterordner"),
    FR("Sous-dossier des cartes de profondeur"),
    ES("Subcarpeta de mapas de profundidad"),
    PT("Subpasta de mapas de profundidade"),
    IT("Sottocartella delle mappe di profondità"),
    NL("Submap met dieptekaarten"), RU("Подпапка карт глубины"),
    TR("Derinlik haritası alt klasörü"));
SS_MSG(depth_dir_help,
    EN("Subfolder holding depth maps, for COLMAP and Metashape datasets. Only "
       "read when load_depths is on."),
    JA("深度マップが入っているサブフォルダです。COLMAP と Metashape のデータセ"
       "ットで使います。load_depths がオンのときだけ読み込まれます。"),
    ZH_HANS("存放深度图的子文件夹，用于 COLMAP 和 Metashape 数据集。只有 load_depths "
            "打开时才会读取。"),
    ZH_HANT("存放深度圖的子資料夾，用於 COLMAP 和 Metashape 資料集。只有 load_depths "
            "開啟時才會讀取。"),
    KO("깊이 맵이 들어 있는 하위 폴더입니다. COLMAP과 Metashape 데이터셋에서 "
       "씁니다. load_depths가 켜져 있을 때만 읽습니다."),
    DE("Unterordner mit den Tiefenkarten, bei COLMAP- und Metashape-Datensätzen. "
       "Wird nur gelesen, wenn load_depths an ist."),
    FR("Sous-dossier contenant les cartes de profondeur, pour les jeux de données "
       "COLMAP et Metashape. Lu seulement si load_depths est activé."),
    ES("Subcarpeta con los mapas de profundidad, para conjuntos de datos COLMAP "
       "y Metashape. Solo se lee si load_depths está activado."),
    PT("Subpasta com os mapas de profundidade, para conjuntos de dados COLMAP "
       "e Metashape. Só é lida quando load_depths está ligado."),
    IT("Sottocartella con le mappe di profondità, per i set di dati COLMAP e "
       "Metashape. Viene letta solo se load_depths è attivo."),
    NL("Submap met de dieptekaarten, voor COLMAP- en Metashape-datasets. Wordt "
       "alleen gelezen als load_depths aan staat."),
    RU("Подпапка с картами глубины — для наборов COLMAP и Metashape. Читается, "
       "только когда включён load_depths."),
    TR("Derinlik haritalarını içeren alt klasör; COLMAP ve Metashape veri kümeleri "
       "için. Yalnızca load_depths açıkken okunur."));

SS_MSG(normal_dir,
    EN("Normal map subfolder"), JA("法線マップのサブフォルダ"),
    ZH_HANS("法线图子文件夹"), ZH_HANT("法線圖子資料夾"),
    KO("노멀 맵 하위 폴더"), DE("Normalenkarten-Unterordner"),
    FR("Sous-dossier des cartes de normales"),
    ES("Subcarpeta de mapas de normales"),
    PT("Subpasta de mapas de normais"),
    IT("Sottocartella delle mappe di normali"), NL("Submap met normal maps"),
    RU("Подпапка карт нормалей"), TR("Normal haritası alt klasörü"));
SS_MSG(normal_dir_help,
    EN("Subfolder holding normal maps, for COLMAP and Metashape datasets. Only "
       "read when load_normals is on."),
    JA("法線マップが入っているサブフォルダです。COLMAP と Metashape のデータセ"
       "ットで使います。load_normals がオンのときだけ読み込まれます。"),
    ZH_HANS("存放法线图的子文件夹，用于 COLMAP 和 Metashape 数据集。只有 load_normals "
            "打开时才会读取。"),
    ZH_HANT("存放法線圖的子資料夾，用於 COLMAP 和 Metashape 資料集。只有 load_normals "
            "開啟時才會讀取。"),
    KO("노멀 맵이 들어 있는 하위 폴더입니다. COLMAP과 Metashape 데이터셋에서 "
       "씁니다. load_normals가 켜져 있을 때만 읽습니다."),
    DE("Unterordner mit den Normalenkarten, bei COLMAP- und Metashape-Datensätzen. "
       "Wird nur gelesen, wenn load_normals an ist."),
    FR("Sous-dossier contenant les cartes de normales, pour les jeux de données "
       "COLMAP et Metashape. Lu seulement si load_normals est activé."),
    ES("Subcarpeta con los mapas de normales, para conjuntos de datos COLMAP "
       "y Metashape. Solo se lee si load_normals está activado."),
    PT("Subpasta com os mapas de normais, para conjuntos de dados COLMAP e Metashape. "
       "Só é lida quando load_normals está ligado."),
    IT("Sottocartella con le mappe di normali, per i set di dati COLMAP e Metashape. "
       "Viene letta solo se load_normals è attivo."),
    NL("Submap met de normal maps, voor COLMAP- en Metashape-datasets. Wordt "
       "alleen gelezen als load_normals aan staat."),
    RU("Подпапка с картами нормалей — для наборов COLMAP и Metashape. Читается, "
       "только когда включён load_normals."),
    TR("Normal haritalarını içeren alt klasör; COLMAP ve Metashape veri kümeleri "
       "için. Yalnızca load_normals açıkken okunur."));

SS_MSG(load_depths,
    EN("Use dataset depth maps"), JA("データセットの深度マップを使う"),
    ZH_HANS("使用数据集的深度图"), ZH_HANT("使用資料集的深度圖"),
    KO("데이터셋의 깊이 맵 사용"), DE("Tiefenkarten des Datensatzes nutzen"),
    FR("Utiliser les cartes de profondeur du jeu"),
    ES("Usar los mapas de profundidad del conjunto"),
    PT("Usar os mapas de profundidade do conjunto"),
    IT("Usare le mappe di profondità del set"),
    NL("Dieptekaarten van de dataset gebruiken"),
    RU("Использовать карты глубины набора"),
    TR("Veri kümesinin derinlik haritalarını kullan"));
SS_MSG(load_depths_help,
    EN("Use the dataset's depth maps when they exist. They drive depth supervision; "
       "turn off to ignore them."),
    JA("データセットに深度マップがあれば使います。深度による誘導に使われます。"
       "無視したいときはオフにしてください。"),
    ZH_HANS("数据集里有深度图时就使用它们。它们用于深度监督；不想用就关掉。"),
    ZH_HANT("資料集裡有深度圖時就使用它們。它們用於深度監督；不想用就關掉。"),
    KO("데이터셋에 깊이 맵이 있으면 사용합니다. 깊이 감독에 쓰이며, 무시하려면"
       " 끄십시오."),
    DE("Die Tiefenkarten des Datensatzes verwenden, sofern vorhanden. Sie steuern "
       "die Tiefenführung; zum Ignorieren abschalten."),
    FR("Utiliser les cartes de profondeur du jeu de données lorsqu'elles existent. "
       "Elles alimentent le guidage par la profondeur ; décocher pour les ignorer."),
    ES("Usar los mapas de profundidad del conjunto de datos cuando existan. Alimentan "
       "la guía por profundidad; desactive para ignorarlos."),
    PT("Usar os mapas de profundidade do conjunto de dados quando existirem. "
       "Eles alimentam a orientação por profundidade; desligue para ignorá-los."),
    IT("Usare le mappe di profondità del set di dati quando ci sono. Alimentano "
       "la guida dalla profondità; disattivare per ignorarle."),
    NL("De dieptekaarten van de dataset gebruiken als die er zijn. Ze voeden "
       "de dieptesturing; zet uit om ze te negeren."),
    RU("Использовать карты глубины набора, если они есть. Они питают направление "
       "по глубине; выключите, чтобы их игнорировать."),
    TR("Veri kümesinde derinlik haritaları varsa onları kullanır. Derinlik rehberliğini "
       "beslerler; yok saymak için kapatın."));

SS_MSG(load_normals,
    EN("Use dataset normal maps"), JA("データセットの法線マップを使う"),
    ZH_HANS("使用数据集的法线图"), ZH_HANT("使用資料集的法線圖"),
    KO("데이터셋의 노멀 맵 사용"),
    DE("Normalenkarten des Datensatzes nutzen"),
    FR("Utiliser les cartes de normales du jeu"),
    ES("Usar los mapas de normales del conjunto"),
    PT("Usar os mapas de normais do conjunto"),
    IT("Usare le mappe di normali del set"),
    NL("Normal maps van de dataset gebruiken"),
    RU("Использовать карты нормалей набора"),
    TR("Veri kümesinin normal haritalarını kullan"));
SS_MSG(load_normals_help,
    EN("Use the dataset's normal maps when they exist. They drive normal supervision; "
       "turn off to ignore them."),
    JA("データセットに法線マップがあれば使います。法線による誘導に使われます。"
       "無視したいときはオフにしてください。"),
    ZH_HANS("数据集里有法线图时就使用它们。它们用于法线监督；不想用就关掉。"),
    ZH_HANT("資料集裡有法線圖時就使用它們。它們用於法線監督；不想用就關掉。"),
    KO("데이터셋에 노멀 맵이 있으면 사용합니다. 노멀 감독에 쓰이며, 무시하려면"
       " 끄십시오."),
    DE("Die Normalenkarten des Datensatzes verwenden, sofern vorhanden. Sie steuern "
       "die Normalenführung; zum Ignorieren abschalten."),
    FR("Utiliser les cartes de normales du jeu de données lorsqu'elles existent. "
       "Elles alimentent le guidage par les normales ; décocher pour les ignorer."),
    ES("Usar los mapas de normales del conjunto de datos cuando existan. Alimentan "
       "la guía por normales; desactive para ignorarlos."),
    PT("Usar os mapas de normais do conjunto de dados quando existirem. Eles "
       "alimentam a orientação por normais; desligue para ignorá-los."),
    IT("Usare le mappe di normali del set di dati quando ci sono. Alimentano "
       "la guida dalle normali; disattivare per ignorarle."),
    NL("De normal maps van de dataset gebruiken als die er zijn. Ze voeden de "
       "normalensturing; zet uit om ze te negeren."),
    RU("Использовать карты нормалей набора, если они есть. Они питают направление "
       "по нормалям; выключите, чтобы их игнорировать."),
    TR("Veri kümesinde normal haritaları varsa onları kullanır. Normal rehberliğini "
       "beslerler; yok saymak için kapatın."));

SS_MSG(depth_unit_scale_factor,
    EN("Depth unit scale"), JA("深度の単位スケール"),
    ZH_HANS("深度单位比例"), ZH_HANT("深度單位比例"), KO("깊이 단위 배율"),
    DE("Maßstab der Tiefenwerte"), FR("Échelle des unités de profondeur"),
    ES("Escala de las unidades de profundidad"),
    PT("Escala das unidades de profundidade"),
    IT("Scala delle unità di profondità"),
    NL("Schaal van de diepte-eenheden"), RU("Масштаб единиц глубины"),
    TR("Derinlik birimi ölçeği"));
SS_MSG(depth_unit_scale_factor_help,
    EN("Multiplier that turns the stored depth values into scene units. The default "
       "reads them as millimeters."),
    JA("保存されている深度値をシーンの単位に直す倍率です。既定ではミリメートル"
       "として読みます。"),
    ZH_HANS("把存储的深度值换算成场景单位的倍数。默认按毫米读取。"),
    ZH_HANT("把儲存的深度值換算成場景單位的倍數。預設按毫米讀取。"),
    KO("저장된 깊이 값을 장면 단위로 바꾸는 배수입니다. 기본값은 밀리미터로 읽"
       "습니다."),
    DE("Faktor, der die gespeicherten Tiefenwerte in Szeneneinheiten umrechnet. "
       "Der Standard liest sie als Millimeter."),
    FR("Facteur qui convertit les valeurs de profondeur enregistrées en unités "
       "de scène. Par défaut, elles sont lues en millimètres."),
    ES("Factor que convierte los valores de profundidad guardados en unidades "
       "de escena. Por defecto se leen como milímetros."),
    PT("Fator que converte os valores de profundidade guardados em unidades de "
       "cena. Por padrão são lidos como milímetros."),
    IT("Fattore che converte i valori di profondità salvati in unità di scena. "
       "Per impostazione predefinita vengono letti come millimetri."),
    NL("Factor die de opgeslagen dieptewaarden omrekent naar scène-eenheden. "
       "Standaard worden ze als millimeters gelezen."),
    RU("Множитель, переводящий сохранённые значения глубины в единицы сцены. "
       "По умолчанию они читаются как миллиметры."),
    TR("Saklanan derinlik değerlerini sahne birimine çeviren çarpan. Varsayılan "
       "onları milimetre olarak okur."));

SS_MSG(colmap_recon_dir,
    EN("COLMAP reconstruction folder"), JA("COLMAP の再構成フォルダ"),
    ZH_HANS("COLMAP 重建文件夹"), ZH_HANT("COLMAP 重建資料夾"),
    KO("COLMAP 재구성 폴더"), DE("COLMAP-Rekonstruktionsordner"),
    FR("Dossier de reconstruction COLMAP"),
    ES("Carpeta de reconstrucción de COLMAP"),
    PT("Pasta de reconstrução do COLMAP"),
    IT("Cartella di ricostruzione COLMAP"), NL("COLMAP-reconstructiemap"),
    RU("Папка реконструкции COLMAP"), TR("COLMAP yeniden oluşturma klasörü"));
SS_MSG(colmap_recon_dir_help,
    EN("Which COLMAP reconstruction to read, relative to the dataset folder, "
       "such as sparse/0. Leave empty to pick automatically: the reconstruction "
       "with the most registered images wins."),
    JA("読み込む COLMAP の再構成を、データセットフォルダからの相対パスで指定し"
       "ます（例：sparse/0）。空にすると自動で選ばれ、登録画像がいちばん多い再"
       "構成が使われます。"),
    ZH_HANS("要读取的 COLMAP 重建结果，相对数据集文件夹给出，例如 sparse/0。留"
            "空则自动选择：注册图像最多的那个重建胜出。"),
    ZH_HANT("要讀取的 COLMAP 重建結果，相對資料集資料夾給出，例如 sparse/0。留"
            "空則自動選擇：註冊影像最多的那個重建勝出。"),
    KO("읽어들일 COLMAP 재구성을 데이터셋 폴더 기준 상대 경로로 지정합니다(예"
       ": sparse/0). 비워 두면 자동으로 고르며, 등록된 이미지가 가장 많은 재구"
       "성이 선택됩니다."),
    DE("Welche COLMAP-Rekonstruktion gelesen wird, relativ zum Datensatzordner, "
       "etwa sparse/0. Leer lassen für die automatische Wahl: Es gewinnt die "
       "Rekonstruktion mit den meisten registrierten Bildern."),
    FR("Reconstruction COLMAP à lire, relative au dossier du jeu de données, "
       "par exemple sparse/0. Laisser vide pour un choix automatique : celle "
       "qui contient le plus d'images enregistrées l'emporte."),
    ES("Reconstrucción de COLMAP que se leerá, relativa a la carpeta del conjunto "
       "de datos, por ejemplo sparse/0. Déjelo vacío para elegirla automáticamente: "
       "gana la que tenga más imágenes registradas."),
    PT("Reconstrução do COLMAP a ler, relativa à pasta do conjunto de dados, "
       "por exemplo sparse/0. Deixe vazio para escolher automaticamente: vence "
       "a que tiver mais imagens registradas."),
    IT("Ricostruzione COLMAP da leggere, relativa alla cartella del set di dati, "
       "ad esempio sparse/0. Lasciare vuoto per la scelta automatica: vince quella "
       "con più immagini registrate."),
    NL("Welke COLMAP-reconstructie wordt gelezen, relatief aan de datasetmap, "
       "bijvoorbeeld sparse/0. Laat leeg voor een automatische keuze: die met "
       "de meeste geregistreerde beelden wint."),
    RU("Какую реконструкцию COLMAP читать — путь относительно папки набора, например "
       "sparse/0. Оставьте пустым для автоматического выбора: побеждает реконструкция "
       "с наибольшим числом зарегистрированных снимков."),
    TR("Okunacak COLMAP yeniden oluşturması; veri kümesi klasörüne göre, örneğin "
       "sparse/0. Otomatik seçim için boş bırakın: en çok kayıtlı görüntüye sahip "
       "olan kazanır."));

SS_MSG(metashape_xml,
    EN("Metashape camera export"), JA("Metashape のカメラ書き出し"),
    ZH_HANS("Metashape 相机导出文件"), ZH_HANT("Metashape 相機匯出檔"),
    KO("Metashape 카메라 내보내기"), DE("Metashape-Kameraexport"),
    FR("Export de caméras Metashape"),
    ES("Exportación de cámaras de Metashape"),
    PT("Exportação de câmeras do Metashape"),
    IT("Esportazione delle camere Metashape"), NL("Metashape-camera-export"),
    RU("Экспорт камер Metashape"), TR("Metashape kamera dışa aktarımı"));
SS_MSG(metashape_xml_help,
    EN("Metashape camera export to read. Leave empty to find it automatically "
       "inside the dataset folder."),
    JA("読み込む Metashape のカメラ書き出しファイルです。空にするとデータセッ"
       "トフォルダの中から自動で探します。"),
    ZH_HANS("要读取的 Metashape 相机导出文件。留空则在数据集文件夹里自动查找。"),
    ZH_HANT("要讀取的 Metashape 相機匯出檔。留空則在資料集資料夾裡自動尋找。"),
    KO("읽어들일 Metashape 카메라 내보내기 파일입니다. 비워 두면 데이터셋 폴더"
       " 안에서 자동으로 찾습니다."),
    DE("Metashape-Kameraexport, der gelesen wird. Leer lassen, um ihn im Datensatzordner "
       "automatisch zu finden."),
    FR("Export de caméras Metashape à lire. Laisser vide pour le trouver automatiquement "
       "dans le dossier du jeu de données."),
    ES("Exportación de cámaras de Metashape que se leerá. Déjelo vacío para encontrarla "
       "automáticamente dentro de la carpeta del conjunto de datos."),
    PT("Exportação de câmeras do Metashape a ler. Deixe vazio para encontrá-la "
       "automaticamente dentro da pasta do conjunto de dados."),
    IT("Esportazione delle camere Metashape da leggere. Lasciare vuoto per trovarla "
       "automaticamente nella cartella del set di dati."),
    NL("Metashape-camera-export die wordt gelezen. Laat leeg om die automatisch "
       "in de datasetmap te vinden."),
    RU("Экспорт камер Metashape, который будет прочитан. Оставьте пустым, чтобы "
       "найти его автоматически в папке набора данных."),
    TR("Okunacak Metashape kamera dışa aktarımı. Veri kümesi klasöründe kendiliğinden "
       "bulunması için boş bırakın."));

SS_MSG(metashape_ply,
    EN("Metashape point cloud"), JA("Metashape の点群"),
    ZH_HANS("Metashape 点云"), ZH_HANT("Metashape 點雲"),
    KO("Metashape 포인트 클라우드"), DE("Metashape-Punktwolke"),
    FR("Nuage de points Metashape"), ES("Nube de puntos de Metashape"),
    PT("Nuvem de pontos do Metashape"), IT("Nuvola di punti Metashape"),
    NL("Metashape-puntenwolk"), RU("Облако точек Metashape"),
    TR("Metashape nokta bulutu"));
SS_MSG(metashape_ply_help,
    EN("Metashape point cloud export used to seed the splats. Leave empty to "
       "find it automatically."),
    JA("スプラットの初期化に使う Metashape の点群書き出しファイルです。空にす"
       "ると自動で探します。"),
    ZH_HANS("用于初始化泼溅的 Metashape 点云导出文件。留空则自动查找。"),
    ZH_HANT("用於初始化潑濺的 Metashape 點雲匯出檔。留空則自動尋找。"),
    KO("스플랫의 초기화에 쓰는 Metashape 포인트 클라우드 파일입니다. 비워 두면"
       " 자동으로 찾습니다."),
    DE("Metashape-Punktwolkenexport, mit dem die Splats initialisiert werden. "
       "Leer lassen, um ihn automatisch zu finden."),
    FR("Export de nuage de points Metashape servant à amorcer les splats. Laisser "
       "vide pour le trouver automatiquement."),
    ES("Exportación de nube de puntos de Metashape usada para sembrar los splats. "
       "Déjelo vacío para encontrarla automáticamente."),
    PT("Exportação de nuvem de pontos do Metashape usada para semear os splats. "
       "Deixe vazio para encontrá-la automaticamente."),
    IT("Esportazione della nuvola di punti Metashape usata per inizializzare "
       "gli splat. Lasciare vuoto per trovarla automaticamente."),
    NL("Metashape-puntenwolkexport waarmee de splats worden opgestart. Laat leeg "
       "om die automatisch te vinden."),
    RU("Экспорт облака точек Metashape, из которого засеваются сплаты. Оставьте "
       "пустым для автоматического поиска."),
    TR("Splat'ları tohumlamak için kullanılan Metashape nokta bulutu dışa aktarımı. "
       "Kendiliğinden bulunması için boş bırakın."));

SS_MSG(metashape_psx,
    EN("Metashape project file"), JA("Metashape のプロジェクトファイル"),
    ZH_HANS("Metashape 工程文件"), ZH_HANT("Metashape 專案檔"),
    KO("Metashape 프로젝트 파일"), DE("Metashape-Projektdatei"),
    FR("Fichier de projet Metashape"),
    ES("Archivo de proyecto de Metashape"),
    PT("Arquivo de projeto do Metashape"), IT("File di progetto Metashape"),
    NL("Metashape-projectbestand"), RU("Файл проекта Metashape"),
    TR("Metashape proje dosyası"));
SS_MSG(metashape_psx_help,
    EN("Metashape project file, used to resolve ambiguity when several images "
       "in the project share a file name."),
    JA("Metashape のプロジェクトファイルです。プロジェクト内で複数の画像が同じ"
       "ファイル名を持つとき、どれを指すのかを解決するために使います。"),
    ZH_HANS("Metashape 工程文件。当工程里有多张图像同名时，用它来判断到底指哪"
            "一张。"),
    ZH_HANT("Metashape 專案檔。當專案裡有多張影像同名時，用它來判斷到底指哪一"
            "張。"),
    KO("Metashape 프로젝트 파일입니다. 프로젝트 안에서 여러 이미지가 같은 파일"
       " 이름을 쓸 때 어느 것인지 가리기 위해 사용합니다."),
    DE("Metashape-Projektdatei, mit der Mehrdeutigkeiten aufgelöst werden, wenn "
       "im Projekt mehrere Bilder denselben Dateinamen tragen."),
    FR("Fichier de projet Metashape, utilisé pour lever l'ambiguïté quand plusieurs "
       "images du projet portent le même nom de fichier."),
    ES("Archivo de proyecto de Metashape, usado para resolver la ambigüedad cuando "
       "varias imágenes del proyecto comparten nombre de archivo."),
    PT("Arquivo de projeto do Metashape, usado para resolver a ambiguidade quando "
       "várias imagens do projeto têm o mesmo nome de arquivo."),
    IT("File di progetto Metashape, usato per sciogliere l'ambiguità quando più "
       "immagini del progetto hanno lo stesso nome di file."),
    NL("Metashape-projectbestand, gebruikt om onduidelijkheid op te lossen als "
       "meerdere beelden in het project dezelfde bestandsnaam hebben."),
    RU("Файл проекта Metashape — нужен, чтобы разрешить неоднозначность, когда "
       "несколько изображений проекта имеют одинаковое имя файла."),
    TR("Metashape proje dosyası; projede birden çok görüntü aynı dosya adını "
       "taşıdığında hangisinin kastedildiğini çözmek için kullanılır."));

SS_MSG(train_resolution_divisor,
    EN("Training resolution divisor"), JA("学習解像度の分母"),
    ZH_HANS("训练分辨率缩小倍数"), ZH_HANT("訓練解析度縮小倍數"),
    KO("학습 해상도 축소 배수"),
    DE("Teiler der Trainingsauflösung"),
    FR("Diviseur de la résolution d'entraînement"),
    ES("Divisor de la resolución de entrenamiento"),
    PT("Divisor da resolução de treino"),
    IT("Divisore della risoluzione di addestramento"),
    NL("Deler van de trainingsresolutie"),
    RU("Делитель разрешения обучения"),
    TR("Eğitim çözünürlüğü böleni"));
SS_MSG(train_resolution_divisor_help,
    EN("Train on smaller images than the ones on disk: 2 halves each side, 4 "
       "quarters it. 0 or 1 trains at the images' own resolution. The cameras "
       "always follow the image files, so a downscaled image folder needs no "
       "factor here."),
    JA("ディスク上の画像より小さくして学習します。2 なら各辺が半分、4 なら 4 "
       "分の 1 です。0 か 1 なら画像そのものの解像度で学習します。カメラは常に"
       "画像ファイルに合わせるので、縮小済みの画像フォルダにこの値は要りませ"
       "ん。"),
    ZH_HANS("以比磁盘上更小的图像训练：2 表示每边减半，4 表示为四分之一。0 或"
            " 1 表示按图像本身的分辨率训练。相机始终跟随图像文件，因此已缩小的"
            "图像目录无需在此填写倍数。"),
    ZH_HANT("以比磁碟上更小的影像訓練：2 表示每邊減半，4 表示為四分之一。0 或"
            " 1 表示依影像本身的解析度訓練。相機一律跟隨影像檔案，因此已縮小的"
            "影像目錄無需在此填寫倍數。"),
    KO("디스크에 있는 이미지보다 작게 학습합니다. 2는 각 변을 절반으로, 4는 4"
       "분의 1로 줄입니다. 0 또는 1이면 이미지 자체 해상도로 학습합니다. 카메"
       "라는 항상 이미지 파일을 따르므로, 이미 축소된 이미지 폴더에는 이 값이 "
       "필요 없습니다."),
    DE("Mit kleineren Bildern trainieren, als auf der Festplatte liegen: 2 "
       "halbiert jede Seite, 4 viertelt sie. 0 oder 1 trainiert in der "
       "Auflösung der Bilder selbst. Die Kameras richten sich immer nach den "
       "Bilddateien, ein bereits verkleinerter Bildordner braucht hier also "
       "keinen Faktor."),
    FR("Entraîner sur des images plus petites que celles du disque : 2 divise "
       "chaque côté par deux, 4 par quatre. 0 ou 1 entraîne à la résolution "
       "des images elles-mêmes. Les caméras suivent toujours les fichiers "
       "image, un dossier d'images déjà réduit n'a donc pas besoin de facteur "
       "ici."),
    ES("Entrenar con imágenes más pequeñas que las del disco: 2 reduce cada "
       "lado a la mitad, 4 a la cuarta parte. 0 o 1 entrena a la resolución "
       "propia de las imágenes. Las cámaras siguen siempre a los archivos de "
       "imagen, así que una carpeta ya reducida no necesita factor aquí."),
    PT("Treinar com imagens menores do que as do disco: 2 reduz cada lado à "
       "metade, 4 a um quarto. 0 ou 1 treina na resolução das próprias "
       "imagens. As câmeras seguem sempre os arquivos de imagem, portanto uma "
       "pasta já reduzida não precisa de fator aqui."),
    IT("Addestrare su immagini più piccole di quelle su disco: 2 dimezza ogni "
       "lato, 4 lo riduce a un quarto. 0 o 1 addestra alla risoluzione delle "
       "immagini stesse. Le camere seguono sempre i file immagine, quindi una "
       "cartella già ridotta non richiede alcun fattore qui."),
    NL("Trainen op kleinere beelden dan die op schijf: 2 halveert elke zijde, "
       "4 brengt ze op een kwart. 0 of 1 traint op de eigen resolutie van de "
       "beelden. De camera's volgen altijd de beeldbestanden, dus een al "
       "verkleinde beeldmap heeft hier geen factor nodig."),
    RU("Обучаться на изображениях меньше тех, что лежат на диске: 2 уменьшает "
       "каждую сторону вдвое, 4 — вчетверо. 0 или 1 обучает в собственном "
       "разрешении изображений. Камеры всегда следуют за файлами изображений, "
       "поэтому уже уменьшенной папке коэффициент здесь не нужен."),
    TR("Diskteki görüntülerden daha küçüğüyle eğitir: 2 her kenarı yarıya, 4 "
       "dörtte bire indirir. 0 veya 1, görüntülerin kendi çözünürlüğünde "
       "eğitir. Kameralar her zaman görüntü dosyalarını izler, bu yüzden "
       "önceden küçültülmüş bir görüntü klasörü burada çarpan istemez."));

SS_MSG(downscale_rounding_mode,
    EN("Downscale rounding"), JA("縮小時の丸め方"),
    ZH_HANS("缩小时的取整方式"), ZH_HANT("縮小時的取整方式"),
    KO("축소 시 반올림 방식"), DE("Rundung beim Verkleinern"),
    FR("Arrondi lors de la réduction"), ES("Redondeo al reducir"),
    PT("Arredondamento ao reduzir"), IT("Arrotondamento nella riduzione"),
    NL("Afronding bij verkleinen"), RU("Округление при уменьшении"),
    TR("Küçültmede yuvarlama"));
SS_MSG(downscale_rounding_mode_help,
    EN("How each side is rounded when the size is divided by "
       "train_resolution_divisor: `floor` never rounds a side up, `round` "
       "takes the nearest, `ceil` always rounds up."),
    JA("train_resolution_divisor で割ったとき、各辺をどう丸めるかです。`floor` "
       "は切り捨て、`round` は四捨五入、`ceil` は切り上げです。"),
    ZH_HANS("尺寸除以 train_resolution_divisor 后每条边如何取整：`floor` 向下取"
            "整，`round` 取最接近的整数，`ceil` 向上取整。"),
    ZH_HANT("尺寸除以 train_resolution_divisor 後每條邊如何取整：`floor` 向下取"
            "整，`round` 取最接近的整數，`ceil` 向上取整。"),
    KO("train_resolution_divisor로 나눌 때 각 변을 어떻게 반올림할지입니다. "
       "`floor`는 내림, `round`는 가장 가까운 값, `ceil`은 올림입니다."),
    DE("Wie jede Seite gerundet wird, wenn die Größe durch "
       "train_resolution_divisor geteilt wird: `floor` rundet nie auf, `round` "
       "nimmt den nächsten Wert, `ceil` rundet immer auf."),
    FR("Comment chaque côté est arrondi après division par "
       "train_resolution_divisor : `floor` n'arrondit jamais vers le haut, "
       "`round` prend le plus proche, `ceil` arrondit toujours vers le haut."),
    ES("Cómo se redondea cada lado al dividir el tamaño por "
       "train_resolution_divisor: `floor` nunca redondea hacia arriba, `round` "
       "toma el más cercano, `ceil` siempre redondea hacia arriba."),
    PT("Como cada lado é arredondado ao dividir o tamanho por "
       "train_resolution_divisor: `floor` nunca arredonda para cima, `round` "
       "usa o mais próximo, `ceil` arredonda sempre para cima."),
    IT("Come viene arrotondato ogni lato dividendo la dimensione per "
       "train_resolution_divisor: `floor` non arrotonda mai per eccesso, "
       "`round` prende il più vicino, `ceil` arrotonda sempre per eccesso."),
    NL("Hoe elke zijde wordt afgerond bij deling van de grootte door "
       "train_resolution_divisor: `floor` rondt nooit naar boven af, `round` "
       "neemt de dichtstbijzijnde, `ceil` rondt altijd naar boven af."),
    RU("Как округляется каждая сторона при делении размера на "
       "train_resolution_divisor: `floor` никогда не округляет вверх, `round` "
       "берёт ближайшее, `ceil` всегда округляет вверх."),
    TR("Boyut train_resolution_divisor'e bölünürken her kenarın nasıl "
       "yuvarlanacağı: `floor` asla yukarı yuvarlamaz, `round` en yakını "
       "alır, `ceil` her zaman yukarı yuvarlar."));

SS_MSG(eval_mode,
    EN("Evaluation split"), JA("評価用の分け方"), ZH_HANS("评估集划分方式"),
    ZH_HANT("評估集劃分方式"), KO("평가용 분할 방식"),
    DE("Aufteilung für die Auswertung"), FR("Répartition pour l'évaluation"),
    ES("División para la evaluación"), PT("Divisão para a avaliação"),
    IT("Suddivisione per la valutazione"), NL("Verdeling voor de evaluatie"),
    RU("Разделение для оценки"), TR("Değerlendirme ayrımı"));
SS_MSG(eval_mode_help,
    EN("How images are split between training and held-out evaluation. `all` "
       "trains on every image and reports numbers on views it has already seen. "
       "`interval` holds out every Nth image, `fraction` holds out a share of "
       "them, and `filename` looks for train or eval in the file names. Holding "
       "images out gives honest quality numbers at the cost of a few training "
       "views."),
    JA("画像を学習用と評価用にどう分けるかです。`all` はすべての画像で学習し、"
       "すでに見た視点で数値を出します。`interval` は N 枚ごとに 1 枚を取り分"
       "け、`fraction` は一定の割合を取り分け、`filename` はファイル名に train "
       "や eval が含まれるかを見ます。画像を取り分けると、学習用の視点が数枚減"
       "る代わりに正直な品質の数値が得られます。"),
    ZH_HANS("图像如何在训练和留出的评估之间划分。`all` 用全部图像训练，并在已"
            "经见过的视角上报告数值；`interval` 每隔 N 张留出一张；`fraction` "
            "留出一定比例；`filename` 则查看文件名中是否含有 train 或 eval。留"
            "出图像会牺牲几个训练视角，但能得到诚实的质量数值。"),
    ZH_HANT("影像如何在訓練和留出的評估之間劃分。`all` 用全部影像訓練，並在已"
            "經見過的視角上報告數值；`interval` 每隔 N 張留出一張；`fraction` "
            "留出一定比例；`filename` 則查看檔名中是否含有 train 或 eval。留出"
            "影像會犧牲幾個訓練視角，但能得到誠實的品質數值。"),
    KO("이미지를 학습용과 평가용으로 어떻게 나눌지입니다. `all`은 모든 이미지"
       "를 학습에 쓰고 이미 본 시점에서 수치를 보고합니다. `interval`은 N장마"
       "다 한 장을, `fraction`은 일정 비율을 떼어 두고, `filename`은 파일 이름"
       "에 train이나 eval이 있는지 봅니다. 이미지를 떼어 두면 학습 시점 몇 개"
       "를 잃는 대신 정직한 품질 수치를 얻습니다."),
    DE("Wie Bilder zwischen Training und zurückgehaltener Auswertung aufgeteilt "
       "werden. `all` trainiert auf jedem Bild und meldet Zahlen zu bereits gesehenen "
       "Ansichten. `interval` hält jedes N-te Bild zurück, `fraction` einen Anteil "
       "davon, und `filename` sucht train oder eval in den Dateinamen. Zurückgehaltene "
       "Bilder liefern ehrliche Qualitätszahlen zum Preis einiger Trainingsansichten."),
    FR("Comment les images sont réparties entre entraînement et évaluation mise "
       "de côté. `all` entraîne sur toutes les images et rapporte des chiffres "
       "sur des vues déjà vues. `interval` met de côté une image sur N, `fraction` "
       "une part d'entre elles, et `filename` cherche train ou eval dans les "
       "noms de fichiers. Mettre des images de côté donne des chiffres honnêtes "
       "au prix de quelques vues d'entraînement."),
    ES("Cómo se reparten las imágenes entre entrenamiento y evaluación apartada. "
       "`all` entrena con todas las imágenes e informa de cifras sobre vistas "
       "ya vistas. `interval` aparta una de cada N, `fraction` una proporción "
       "de ellas, y `filename` busca train o eval en los nombres de archivo. "
       "Apartar imágenes da cifras de calidad honestas a costa de unas pocas "
       "vistas de entrenamiento."),
    PT("Como as imagens são repartidas entre treinamento e avaliação reservada. "
       "`all` treina com todas as imagens e informa números sobre vistas já vistas. "
       "`interval` reserva uma a cada N, `fraction` uma proporção delas, e `filename` "
       "procura train ou eval nos nomes dos arquivos. Reservar imagens dá números "
       "de qualidade honestos ao custo de algumas vistas de treinamento."),
    IT("Come le immagini vengono divise tra addestramento e valutazione messa "
       "da parte. `all` addestra su tutte le immagini e riporta numeri su viste "
       "già viste. `interval` mette da parte una immagine ogni N, `fraction` "
       "una quota di esse, e `filename` cerca train o eval nei nomi dei file. "
       "Mettere da parte immagini dà numeri di qualità onesti al costo di qualche "
       "vista di addestramento."),
    NL("Hoe beelden worden verdeeld tussen training en apart gehouden evaluatie. "
       "`all` traint op elk beeld en rapporteert cijfers over al geziene weergaven. "
       "`interval` houdt elk N-de beeld apart, `fraction` een aandeel ervan, "
       "en `filename` zoekt train of eval in de bestandsnamen. Beelden apart "
       "houden geeft eerlijke kwaliteitscijfers ten koste van een paar trainingsbeelden."),
    RU("Как изображения делятся между обучением и отложенной оценкой. `all` обучается "
       "на всех снимках и сообщает цифры по уже виденным видам. `interval` откладывает "
       "каждый N-й снимок, `fraction` — их долю, а `filename` ищет train или "
       "eval в именах файлов. Отложенные снимки дают честные цифры качества ценой "
       "нескольких обучающих видов."),
    TR("Görüntülerin eğitim ile ayrılmış değerlendirme arasında nasıl bölüneceği. "
       "`all` her görüntüyle eğitir ve zaten görülmüş görünümler üzerinden sayı "
       "bildirir. `interval` her N görüntüde birini, `fraction` belirli bir oranını "
       "ayırır, `filename` ise dosya adlarında train ya da eval arar. Görüntü "
       "ayırmak, birkaç eğitim görünümü karşılığında dürüst kalite sayıları verir."));

SS_MSG(eval_interval,
    EN("Evaluation interval"), JA("評価画像の間隔"), ZH_HANS("评估图像间隔"),
    ZH_HANT("評估影像間隔"), KO("평가 이미지 간격"),
    DE("Abstand der Auswertungsbilder"),
    FR("Intervalle des images d'évaluation"),
    ES("Intervalo de las imágenes de evaluación"),
    PT("Intervalo das imagens de avaliação"),
    IT("Intervallo delle immagini di valutazione"),
    NL("Interval van de evaluatiebeelden"),
    RU("Интервал изображений оценки"), TR("Değerlendirme görüntüsü aralığı"));
SS_MSG(eval_interval_help,
    EN("Hold out every Nth image when eval_mode is `interval`. Larger values "
       "keep more images for training."),
    JA("eval_mode が `interval` のとき、N 枚ごとに 1 枚を評価用に取り分けます。"
       "値を大きくするほど学習に使える画像が増えます。"),
    ZH_HANS("当 eval_mode 为 `interval` 时，每隔 N 张留出一张作评估。数值越大，"
            "留给训练的图像越多。"),
    ZH_HANT("當 eval_mode 為 `interval` 時，每隔 N 張留出一張作評估。數值越大，"
            "留給訓練的影像越多。"),
    KO("eval_mode가 `interval`일 때 N장마다 한 장을 평가용으로 뗍니다. 값이 클"
       "수록 학습에 쓰는 이미지가 많아집니다."),
    DE("Bei eval_mode `interval` jedes N-te Bild zurückhalten. Größere Werte "
       "lassen mehr Bilder für das Training übrig."),
    FR("Avec eval_mode `interval`, mettre de côté une image sur N. Des valeurs "
       "plus grandes laissent plus d'images pour l'entraînement."),
    ES("Con eval_mode `interval`, apartar una de cada N imágenes. Valores mayores "
       "dejan más imágenes para el entrenamiento."),
    PT("Com eval_mode `interval`, reservar uma a cada N imagens. Valores maiores "
       "deixam mais imagens para o treinamento."),
    IT("Con eval_mode `interval`, mettere da parte una immagine ogni N. Valori "
       "più grandi lasciano più immagini per l'addestramento."),
    NL("Bij eval_mode `interval` elk N-de beeld apart houden. Grotere waarden "
       "laten meer beelden over voor de training."),
    RU("При eval_mode `interval` откладывать каждый N-й снимок. Большие значения "
       "оставляют больше изображений для обучения."),
    TR("eval_mode `interval` iken her N görüntüde birini ayırır. Büyük değerler "
       "eğitime daha çok görüntü bırakır."));

SS_MSG(train_split_fraction,
    EN("Training share"), JA("学習に使う割合"), ZH_HANS("训练集比例"),
    ZH_HANT("訓練集比例"), KO("학습에 쓰는 비율"),
    DE("Anteil für das Training"), FR("Part utilisée pour l'entraînement"),
    ES("Proporción para el entrenamiento"),
    PT("Proporção para o treinamento"), IT("Quota per l'addestramento"),
    NL("Aandeel voor de training"), RU("Доля для обучения"),
    TR("Eğitim için ayrılan oran"));
SS_MSG(train_split_fraction_help,
    EN("Share of the images used for training when eval_mode is `fraction`; the "
       "rest are held out."),
    JA("eval_mode が `fraction` のとき、学習に使う画像の割合です。残りは評価用"
       "に取り分けられます。"),
    ZH_HANS("当 eval_mode 为 `fraction` 时，用于训练的图像比例；其余留作评估。"),
    ZH_HANT("當 eval_mode 為 `fraction` 時，用於訓練的影像比例；其餘留作評估。"),
    KO("eval_mode가 `fraction`일 때 학습에 쓰는 이미지 비율입니다. 나머지는 평"
       "가용으로 뗍니다."),
    DE("Bei eval_mode `fraction` der Anteil der Bilder, der zum Training dient; "
       "der Rest wird zurückgehalten."),
    FR("Avec eval_mode `fraction`, la part des images servant à l'entraînement "
       "; le reste est mis de côté."),
    ES("Con eval_mode `fraction`, la proporción de imágenes usada para entrenar; "
       "el resto se aparta."),
    PT("Com eval_mode `fraction`, a proporção de imagens usada para treinar; "
       "o resto é reservado."),
    IT("Con eval_mode `fraction`, la quota di immagini usata per addestrare; "
       "il resto viene messo da parte."),
    NL("Bij eval_mode `fraction` het aandeel beelden dat voor de training wordt "
       "gebruikt; de rest wordt apart gehouden."),
    RU("При eval_mode `fraction` — доля изображений, идущих на обучение; остальные "
       "откладываются."),
    TR("eval_mode `fraction` iken eğitim için kullanılan görüntü oranı; kalanı "
       "ayrılır."));

SS_MSG(validation_fraction,
    EN("Validation share"), JA("検証に使う割合"), ZH_HANS("验证集比例"),
    ZH_HANT("驗證集比例"), KO("검증에 쓰는 비율"),
    DE("Anteil für die Validierung"), FR("Part utilisée pour la validation"),
    ES("Proporción para la validación"), PT("Proporção para a validação"),
    IT("Quota per la validazione"), NL("Aandeel voor de validatie"),
    RU("Доля для проверки"), TR("Doğrulama için ayrılan oran"));
SS_MSG(validation_fraction_help,
    EN("Share of the training images set aside to watch for overfitting. Training "
       "can then stop before quality starts to drop. Early stopping is not active "
       "yet: the images are held out but the run always goes to the end."),
    JA("過学習を見張るために取り分けておく学習画像の割合です。品質が落ち始める"
       "前に学習を止められます。早期終了はまだ有効ではありません。画像は取り分"
       "けられますが、実行は常に最後まで進みます。"),
    ZH_HANS("为监视过拟合而从训练图像中留出的比例。这样可以在质量开始下降前停"
            "止训练。早停目前尚未启用：图像会被留出，但训练始终会跑到最后。"),
    ZH_HANT("為監視過擬合而從訓練影像中留出的比例。這樣可以在品質開始下降前停"
            "止訓練。早停目前尚未啟用：影像會被留出，但訓練始終會跑到最後。"),
    KO("과적합을 감시하려고 학습 이미지에서 떼어 두는 비율입니다. 품질이 떨어"
       "지기 시작하기 전에 학습을 멈출 수 있습니다. 조기 종료는 아직 동작하지"
       " 않습니다. 이미지는 떼어 두지만 실행은 언제나 끝까지 갑니다."),
    DE("Anteil der Trainingsbilder, der zurückgelegt wird, um Überanpassung zu "
       "beobachten. Das Training kann dann enden, bevor die Qualität abzufallen "
       "beginnt. Frühes Stoppen ist noch nicht aktiv: Die Bilder werden zurückgehalten, "
       "aber der Lauf geht immer bis zum Ende."),
    FR("Part des images d'entraînement mise de côté pour surveiller le surapprentissage. "
       "L'entraînement peut alors s'arrêter avant que la qualité ne baisse. L'arrêt "
       "anticipé n'est pas encore actif : les images sont mises de côté, mais "
       "l'exécution va toujours jusqu'au bout."),
    ES("Proporción de las imágenes de entrenamiento apartada para vigilar el "
       "sobreajuste. El entrenamiento puede así detenerse antes de que la calidad "
       "empiece a caer. La parada temprana todavía no está activa: las imágenes "
       "se apartan, pero la ejecución siempre llega al final."),
    PT("Proporção das imagens de treinamento reservada para vigiar o sobreajuste. "
       "O treinamento pode então parar antes de a qualidade começar a cair. A "
       "parada antecipada ainda não está ativa: as imagens são reservadas, mas "
       "a execução vai sempre até o fim."),
    IT("Quota delle immagini di addestramento messa da parte per sorvegliare "
       "il sovradattamento. L'addestramento può così fermarsi prima che la qualità "
       "cominci a calare. L'arresto anticipato non è ancora attivo: le immagini "
       "vengono messe da parte, ma l'esecuzione arriva sempre alla fine."),
    NL("Aandeel van de trainingsbeelden dat apart wordt gezet om overfitting "
       "in de gaten te houden. De training kan dan stoppen voordat de kwaliteit "
       "begint te dalen. Vroegtijdig stoppen werkt nog niet: de beelden worden "
       "apart gezet, maar de run loopt altijd tot het eind."),
    RU("Доля обучающих изображений, отложенная, чтобы следить за переобучением. "
       "Тогда обучение можно остановить до того, как качество начнёт падать. "
       "Ранняя остановка пока не работает: снимки откладываются, но запуск всегда "
       "идёт до конца."),
    TR("Aşırı öğrenmeyi izlemek için eğitim görüntülerinden ayrılan oran. Böylece "
       "kalite düşmeye başlamadan eğitim durdurulabilir. Erken durdurma henüz "
       "etkin değil: görüntüler ayrılır ama çalıştırma hep sonuna kadar gider."));

SS_MSG(warp_to_pinhole,
    EN("Split fisheye into perspective views"),
    JA("魚眼を通常の透視画像に分割"), ZH_HANS("把鱼眼图拆成普通透视图"),
    ZH_HANT("把魚眼影像拆成一般透視影像"),
    KO("어안 이미지를 일반 원근 뷰로 분할"),
    DE("Fisheye in Perspektivansichten teilen"),
    FR("Découper le fisheye en vues perspectives"),
    ES("Dividir el ojo de pez en vistas en perspectiva"),
    PT("Dividir o olho de peixe em vistas em perspectiva"),
    IT("Dividere il fisheye in viste prospettiche"),
    NL("Fisheye opsplitsen in perspectiefbeelden"),
    RU("Разбивать фишай на перспективные виды"),
    TR("Balıkgözünü perspektif görünümlere böl"));
SS_MSG(warp_to_pinhole_help,
    EN("Split each fisheye image into five ordinary perspective views before "
       "training. Often gives better quality and wider compatibility for fisheye "
       "and 360 captures, at the cost of more images to process."),
    JA("学習の前に、魚眼画像を 5 枚の通常の透視画像に分割します。魚眼や 360 度"
       "の撮影では品質と互換性が上がることが多いですが、処理する画像は増えます。"),
    ZH_HANS("训练前把每张鱼眼图拆成五张普通透视图。对鱼眼和 360 度素材通常能提"
            "升质量和兼容性，代价是要处理的图像变多。"),
    ZH_HANT("訓練前把每張魚眼影像拆成五張一般透視影像。對魚眼和 360 度素材通常"
            "能提升品質和相容性，代價是要處理的影像變多。"),
    KO("학습 전에 어안 이미지를 다섯 장의 일반 원근 뷰로 나눕니다. 어안과 360도"
       " 촬영에서는 품질과 호환성이 좋아지는 경우가 많지만, 처리할 이미지가 늘"
       "어납니다."),
    DE("Jedes Fisheye-Bild vor dem Training in fünf gewöhnliche Perspektivansichten "
       "teilen. Bringt bei Fisheye- und 360-Grad-Aufnahmen oft bessere Qualität "
       "und breitere Verträglichkeit, kostet aber mehr zu verarbeitende Bilder."),
    FR("Découper chaque image fisheye en cinq vues perspectives ordinaires avant "
       "l'entraînement. Donne souvent une meilleure qualité et une plus large "
       "compatibilité pour les prises fisheye et 360, au prix de plus d'images "
       "à traiter."),
    ES("Dividir cada imagen de ojo de pez en cinco vistas en perspectiva ordinarias "
       "antes de entrenar. Suele dar mejor calidad y más compatibilidad en capturas "
       "de ojo de pez y 360, a costa de más imágenes que procesar."),
    PT("Dividir cada imagem olho de peixe em cinco vistas em perspectiva comuns "
       "antes de treinar. Costuma dar melhor qualidade e mais compatibilidade "
       "em capturas olho de peixe e 360, ao custo de mais imagens a processar."),
    IT("Dividere ogni immagine fisheye in cinque viste prospettiche ordinarie "
       "prima dell'addestramento. Spesso dà qualità migliore e maggiore compatibilità "
       "per riprese fisheye e 360, al costo di più immagini da elaborare."),
    NL("Elk fisheyebeeld vóór de training opsplitsen in vijf gewone perspectiefbeelden. "
       "Geeft bij fisheye- en 360-opnamen vaak betere kwaliteit en bredere compatibiliteit, "
       "ten koste van meer te verwerken beelden."),
    RU("Перед обучением разбивать каждый фишай-снимок на пять обычных перспективных "
       "видов. Для фишая и 360-съёмки это часто даёт лучшее качество и совместимость "
       "ценой большего числа изображений."),
    TR("Eğitimden önce her balıkgözü görüntüsünü beş sıradan perspektif görünüme "
       "böler. Balıkgözü ve 360 derece çekimlerde çoğu kez daha iyi kalite ve "
       "daha geniş uyumluluk verir; karşılığında işlenecek görüntü sayısı artar."));

SS_MSG(warp_spherical_to_pinhole,
    EN("Split panoramas into cube faces"), JA("パノラマをキューブ面に分割"),
    ZH_HANS("把全景图拆成立方体面"), ZH_HANT("把全景影像拆成立方體面"),
    KO("파노라마를 큐브 면으로 분할"),
    DE("Panoramen in Würfelflächen teilen"),
    FR("Découper les panoramas en faces de cube"),
    ES("Dividir las panorámicas en caras de cubo"),
    PT("Dividir os panoramas em faces de cubo"),
    IT("Dividere i panorami in facce di cubo"),
    NL("Panorama's opsplitsen in kubusvlakken"),
    RU("Разбивать панорамы на грани куба"),
    TR("Panoramaları küp yüzlerine böl"));
SS_MSG(warp_spherical_to_pinhole_help,
    EN("Split each 360 panorama into six cube faces before training. Turn this "
       "off to train directly on the panorama, which keeps the original "
       "pixels."),
    JA("学習の前に、360 度パノラマを 6 つのキューブ面に分割します。オフにする"
       "とパノラマのまま学習するので、元の画素をそのまま保てます。"),
    ZH_HANS("训练前把每张 360 度全景图拆成六个立方体面。关闭则直接在全景图上训"
            "练，保留原始像素。"),
    ZH_HANT("訓練前把每張 360 度全景影像拆成六個立方體面。關閉則直接在全景影像"
            "上訓練，保留原始像素。"),
    KO("학습 전에 360도 파노라마를 여섯 개의 큐브 면으로 나눕니다. 끄면 파노라"
       "마 그대로 학습해 원본 픽셀을 지킵니다."),
    DE("Jedes 360-Grad-Panorama vor dem Training in sechs Würfelflächen "
       "teilen. Abgeschaltet wird direkt auf dem Panorama trainiert, was die "
       "Originalpixel bewahrt."),
    FR("Découper chaque panorama 360 en six faces de cube avant "
       "l'entraînement. Décoché, l'entraînement se fait directement sur le "
       "panorama, ce qui conserve les pixels d'origine."),
    ES("Dividir cada panorámica 360 en seis caras de cubo antes de entrenar. "
       "Sin marcar se entrena directamente sobre la panorámica, lo que "
       "conserva los píxeles originales."),
    PT("Dividir cada panorama 360 em seis faces de cubo antes de treinar. "
       "Desmarcado, treina-se diretamente no panorama, o que preserva os "
       "pixels originais."),
    IT("Dividere ogni panorama 360 in sei facce di cubo prima "
       "dell'addestramento. Disattivato si addestra direttamente sul "
       "panorama, il che conserva i pixel originali."),
    NL("Elk 360-panorama vóór de training opsplitsen in zes kubusvlakken. "
       "Uitgevinkt wordt er rechtstreeks op het panorama getraind, wat de "
       "originele pixels behoudt."),
    RU("Перед обучением разбивать каждую 360-панораму на шесть граней куба. "
       "Выключено — обучение идёт прямо по панораме, и исходные пиксели "
       "сохраняются."),
    TR("Eğitimden önce her 360 derece panoramayı altı küp yüzüne böler. "
       "Kapatılırsa doğrudan panorama üzerinde eğitilir ve özgün pikseller "
       "korunur."));

SS_MSG(warp_face_fit,
    EN("Face size when splitting"), JA("分割時の面サイズ"),
    ZH_HANS("拆分后各面的尺寸"), ZH_HANT("拆分後各面的尺寸"),
    KO("분할 시 면 크기"), DE("Flächengröße beim Aufteilen"),
    FR("Taille des faces au découpage"), ES("Tamaño de las caras al dividir"),
    PT("Tamanho das faces ao dividir"),
    IT("Dimensione delle facce nella suddivisione"),
    NL("Vlakgrootte bij het opsplitsen"),
    RU("Размер граней при разбиении"),
    TR("Bölmede yüz boyutu"));
SS_MSG(warp_face_fit_help,
    EN("Whether the pinhole faces a wide lens is split into all share one "
       "size. `uniform` renders them in a single pass, which the fused "
       "optimizer needs, and is the faster choice. `per-face` crops each face "
       "to the part its lens fills (never under half a frame), drawing 15-40% "
       "fewer pixels for less VRAM, but costs one pass per size and turns the "
       "fused optimizer off."),
    JA("広角レンズを分割したピンホール面をすべて同じサイズにするかどうか。"
       "`uniform` は 1 回のパスで描画でき、融合オプティマイザはこれを必要とし、"
       "速度も最も速くなります。`per-face` は各面をレンズが写る範囲（最小でも"
       "面の半分）に切り詰め、画素数が 15〜40% 減って VRAM を節約しますが、"
       "サイズごとに 1 パスかかり融合オプティマイザは無効になります。"),
    ZH_HANS("拆分广角镜头得到的针孔面是否统一为同一尺寸。`uniform` 只需一遍渲"
            "染，融合优化器需要这样，速度也最快。`per-face` 把每个面裁到镜头实"
            "际覆盖的部分（不小于半个面），像素少 15-40%，更省显存，但每种尺寸"
            "要多一遍渲染，且会关闭融合优化器。"),
    ZH_HANT("拆分廣角鏡頭得到的針孔面是否統一為同一尺寸。`uniform` 只需一遍算"
            "圖，融合最佳化器需要如此，速度也最快。`per-face` 把每個面裁到鏡頭實"
            "際涵蓋的部分（不小於半個面），像素少 15-40%，更省顯示記憶體，但每種"
            "尺寸要多一遍算圖，且會關閉融合最佳化器。"),
    KO("광각 렌즈를 분할한 핀홀 면을 모두 같은 크기로 둘지 여부입니다. "
       "`uniform` 은 한 번의 패스로 그리며 통합 옵티마이저가 이를 필요로 하고 "
       "속도도 가장 빠릅니다. `per-face` 는 각 면을 렌즈가 담기는 부분(최소 면의 "
       "절반)까지 잘라 픽셀이 15-40% 줄고 VRAM 을 아끼지만, 크기마다 패스가 "
       "하나씩 늘고 통합 옵티마이저가 꺼집니다."),
    DE("Ob die Lochkamera-Flächen, in die ein Weitwinkel zerlegt wird, alle "
       "dieselbe Größe haben. `uniform` rendert sie in einem Durchgang, was "
       "der fusionierte Optimierer braucht, und ist die schnellere Wahl. "
       "`per-face` beschneidet jede Fläche auf den vom Objektiv gefüllten Teil "
       "(nie unter eine halbe Fläche) und zeichnet 15-40% weniger Pixel bei "
       "weniger VRAM, kostet aber einen Durchgang je Größe und schaltet den "
       "fusionierten Optimierer ab."),
    FR("Si les faces sténopé issues du découpage d'un objectif large ont "
       "toutes la même taille. `uniform` les rend en une seule passe, ce dont "
       "l'optimiseur fusionné a besoin, et c'est le choix le plus rapide. "
       "`per-face` recadre chaque face sur la partie que remplit l'objectif "
       "(jamais moins d'une demi-face), soit 15 à 40% de pixels en moins et "
       "moins de VRAM, mais coûte une passe par taille et désactive "
       "l'optimiseur fusionné."),
    ES("Si las caras estenopeicas en que se divide un objetivo ancho comparten "
       "un mismo tamaño. `uniform` las renderiza en una sola pasada, que es lo "
       "que necesita el optimizador fusionado, y es la opción más rápida. "
       "`per-face` recorta cada cara a la parte que llena su objetivo (nunca "
       "menos de media cara) y dibuja entre un 15% y un 40% menos de píxeles "
       "con menos VRAM, pero cuesta una pasada por tamaño y desactiva el "
       "optimizador fusionado."),
    PT("Se as faces estenopeicas em que uma lente larga é dividida têm todas o "
       "mesmo tamanho. `uniform` desenha-as numa única passagem, que é o que o "
       "otimizador fundido precisa, e é a opção mais rápida. `per-face` recorta "
       "cada face à parte que a lente preenche (nunca menos de meia face) e "
       "desenha 15 a 40% menos pixels com menos VRAM, mas custa uma passagem "
       "por tamanho e desliga o otimizador fundido."),
    IT("Se le facce stenopeiche in cui viene divisa un'ottica ampia hanno tutte "
       "la stessa dimensione. `uniform` le disegna in un solo passaggio, ciò "
       "che serve all'ottimizzatore fuso, ed è la scelta più veloce. `per-face` "
       "ritaglia ogni faccia alla parte coperta dall'obiettivo (mai meno di "
       "mezza faccia) e disegna il 15-40% di pixel in meno con meno VRAM, ma "
       "costa un passaggio per dimensione e disattiva l'ottimizzatore fuso."),
    NL("Of de pinhole-vlakken waarin een groothoeklens wordt opgesplitst "
       "allemaal dezelfde grootte hebben. `uniform` rendert ze in één doorgang, "
       "wat de gefuseerde optimizer nodig heeft, en is de snelste keuze. "
       "`per-face` snijdt elk vlak bij tot het deel dat de lens vult (nooit "
       "minder dan een half vlak) en tekent 15-40% minder pixels met minder "
       "VRAM, maar kost een doorgang per grootte en schakelt de gefuseerde "
       "optimizer uit."),
    RU("Должны ли все пинхол-грани, на которые разбивается широкоугольный "
       "объектив, иметь один размер. `uniform` отрисовывает их за один проход, "
       "что нужно объединённому оптимизатору, и работает быстрее всего. "
       "`per-face` обрезает каждую грань до части, которую заполняет объектив "
       "(не меньше половины грани), рисуя на 15-40% меньше пикселей и экономя "
       "видеопамять, но требует прохода на каждый размер и отключает "
       "объединённый оптимизатор."),
    TR("Geniş bir merceğin bölündüğü iğne deliği yüzlerinin hepsinin aynı "
       "boyutta olup olmayacağı. `uniform` hepsini tek geçişte çizer; birleşik "
       "eniyileyici bunu gerektirir ve en hızlı seçenektir. `per-face` her yüzü "
       "merceğin doldurduğu kısma (en az yarım yüz) kırpar, %15-40 daha az "
       "piksel çizip VRAM'den tasarruf eder, ancak boyut başına bir geçişe mal "
       "olur ve birleşik eniyileyiciyi kapatır."));

SS_MSG(warp_back_face,
    EN("Rear face when splitting"), JA("分割時に後方の面も使う"),
    ZH_HANS("拆分时使用朝后的面"), ZH_HANT("拆分時使用朝後的面"),
    KO("분할 시 뒤쪽 면 사용"), DE("Rückseitige Fläche beim Aufteilen"),
    FR("Face arrière au découpage"), ES("Cara trasera al dividir"),
    PT("Face traseira ao dividir"),
    IT("Faccia posteriore nella suddivisione"),
    NL("Achtervlak bij het opsplitsen"),
    RU("Задняя грань при разбиении"), TR("Bölmede arka yüz"));
SS_MSG(warp_back_face_help,
    EN("Whether a lens seen past 135 degrees also gets a sixth pinhole face "
       "pointing backwards. On a real fisheye that direction holds little but "
       "the lens folded over itself, which is usually masked out anyway, and "
       "the face slows every step down and takes one more appearance slot per "
       "image (about 20% more bilateral grid memory). Turn it on for a lens "
       "that genuinely sees behind itself. A panorama always uses all six."),
    JA("135 度を超えて写るレンズに、後ろ向きのピンホール面をもう 1 枚加えるか"
       "どうか。実際の魚眼ではその方向に写るのはレンズの折り返し像がほとんど"
       "で、たいていマスクで除かれます。それでも 1 ステップごとの処理は遅くな"
       "り、画像ごとの外観スロットを 1 つ余計に使います（バイラテラルグリッド"
       "のメモリが約 20% 増）。本当に後方まで写るレンズのときだけ有効にしてく"
       "ださい。パノラマは常に 6 面すべてを使います。"),
    ZH_HANS("视场超过 135 度的镜头是否再加一张朝后的针孔面。真实鱼眼在这个方向"
            "上几乎只有镜头折返的像，通常本来就会被遮罩掉，却会拖慢每一步，并"
            "让每张图像多占一个外观槽（双边网格显存约多 20%）。只有镜头确实能"
            "看到身后时才打开。全景图始终使用全部六个面。"),
    ZH_HANT("視場超過 135 度的鏡頭是否再加一張朝後的針孔面。真實魚眼在這個方向"
            "上幾乎只有鏡頭折返的影像，通常本來就會被遮罩掉，卻會拖慢每一步，"
            "並讓每張影像多佔一個外觀槽（雙邊網格顯示記憶體約多 20%）。只有鏡"
            "頭確實能看到身後時才開啟。全景影像一律使用全部六個面。"),
    KO("135도를 넘겨 보이는 렌즈에 뒤를 향하는 핀홀 면을 하나 더 둘지 여부입니"
       "다. 실제 어안에서 그 방향에는 렌즈가 접혀 생긴 상뿐이라 보통 마스크로 "
       "지워지지만, 면이 늘면 스텝마다 느려지고 이미지당 외관 슬롯을 하나 더 "
       "씁니다(양방향 그리드 메모리 약 20% 증가). 정말로 뒤까지 보는 렌즈에서"
       "만 켜세요. 파노라마는 항상 여섯 면을 모두 씁니다."),
    DE("Ob ein über 135 Grad hinaus sehendes Objektiv zusätzlich eine nach "
       "hinten gerichtete Lochkamera-Fläche bekommt. Bei einem echten Fisheye "
       "steht dort fast nur das umgeklappte Bild des Objektivs, das ohnehin "
       "meist maskiert ist; die Fläche bremst jeden Schritt und kostet je Bild "
       "einen weiteren Erscheinungs-Slot (rund 20% mehr Speicher für das "
       "bilaterale Gitter). Nur einschalten, wenn das Objektiv wirklich nach "
       "hinten sieht. Ein Panorama nutzt immer alle sechs."),
    FR("Si un objectif vu au-delà de 135 degrés reçoit en plus une face "
       "sténopé tournée vers l'arrière. Sur un vrai fisheye, cette direction "
       "ne contient guère que l'image repliée de l'objectif, le plus souvent "
       "masquée ; la face ralentit chaque étape et coûte un emplacement "
       "d'apparence de plus par image (environ 20% de mémoire en plus pour la "
       "grille bilatérale). À activer seulement pour un objectif qui voit "
       "vraiment derrière lui. Un panorama utilise toujours les six."),
    ES("Si un objetivo que ve más allá de 135 grados recibe además una cara "
       "estenopeica orientada hacia atrás. En un ojo de pez real esa dirección "
       "solo contiene la imagen plegada del objetivo, casi siempre enmascarada; "
       "la cara ralentiza cada paso y ocupa una ranura de apariencia más por "
       "imagen (alrededor de un 20% más de memoria para la rejilla bilateral). "
       "Actívala solo con un objetivo que vea de verdad hacia atrás. Una "
       "panorámica siempre usa las seis."),
    PT("Se uma lente vista para além de 135 graus recebe também uma face "
       "estenopeica virada para trás. Numa olho de peixe real essa direção "
       "contém pouco mais do que a imagem dobrada da lente, quase sempre "
       "mascarada; a face atrasa cada passo e ocupa mais um espaço de aparência "
       "por imagem (cerca de 20% mais memória para a grelha bilateral). Ligue "
       "apenas com uma lente que veja mesmo para trás. Um panorama usa sempre "
       "as seis."),
    IT("Se un obiettivo che vede oltre i 135 gradi riceve anche una faccia "
       "stenopeica rivolta all'indietro. In un fisheye reale quella direzione "
       "contiene quasi solo l'immagine ripiegata dell'obiettivo, di norma "
       "mascherata; la faccia rallenta ogni passo e occupa uno slot di aspetto "
       "in più per immagine (circa il 20% di memoria in più per la griglia "
       "bilaterale). Attivala solo con un obiettivo che veda davvero dietro di "
       "sé. Un panorama usa sempre tutte e sei."),
    NL("Of een lens die verder dan 135 graden kijkt er een naar achteren "
       "gericht pinhole-vlak bij krijgt. Bij een echte fisheye staat in die "
       "richting bijna alleen het omgevouwen beeld van de lens, dat meestal "
       "toch gemaskeerd is; het vlak vertraagt elke stap en kost per beeld een "
       "extra uiterlijk-slot (ongeveer 20% meer geheugen voor het bilaterale "
       "raster). Zet dit alleen aan voor een lens die echt achter zich kijkt. "
       "Een panorama gebruikt altijd alle zes."),
    RU("Получает ли объектив, видящий дальше 135 градусов, дополнительную "
       "пинхол-грань, направленную назад. У настоящего фишая в этом "
       "направлении почти всегда лишь завёрнутое изображение самого объектива, "
       "которое обычно и так замаскировано; грань замедляет каждый шаг и "
       "занимает ещё один слот внешнего вида на изображение (примерно на 20% "
       "больше памяти под билатеральную сетку). Включайте только для "
       "объектива, который действительно видит назад. Панорама всегда "
       "использует все шесть."),
    TR("135 dereceden geniş gören bir merceğe ayrıca arkaya bakan bir iğne "
       "deliği yüzü eklenip eklenmeyeceği. Gerçek bir balıkgözünde o yönde "
       "neredeyse yalnızca merceğin katlanmış görüntüsü bulunur ve zaten "
       "çoğunlukla maskelenir; bu yüz her adımı yavaşlatır ve görüntü başına "
       "bir görünüm yuvası daha harcar (bilateral ızgara belleğinde yaklaşık "
       "%20 artış). Yalnızca gerçekten arkasını gören bir mercek için açın. "
       "Panorama her zaman altı yüzü de kullanır."));

SS_MSG(deblur_training_images,
    EN("Deblur training images"), JA("学習画像のぶれを補正"),
    ZH_HANS("对训练图像去模糊"), ZH_HANT("對訓練影像去模糊"),
    KO("학습 이미지 흐림 보정"),
    DE("Unschärfe der Trainingsbilder entfernen"),
    FR("Corriger le flou des images d'entraînement"),
    ES("Corregir el desenfoque de las imágenes"),
    PT("Corrigir o desfoque das imagens"),
    IT("Correggere la sfocatura delle immagini"),
    NL("Onscherpte uit de trainingsbeelden halen"),
    RU("Убирать смаз с обучающих изображений"),
    TR("Eğitim görüntülerindeki bulanıklığı gider"));
SS_MSG(deblur_training_images_help,
    EN("Sharpen blurry photos with a learned deblurring model before training. "
       "Not supported yet."),
    JA("学習の前に、ぶれた写真を学習済みのモデルでくっきりさせます。まだ対応し"
       "ていません。"),
    ZH_HANS("训练前用一个学习到的去模糊模型让模糊照片变清晰。尚未支持。"),
    ZH_HANT("訓練前用一個學習到的去模糊模型讓模糊照片變清晰。尚未支援。"),
    KO("학습 전에 흐린 사진을 학습된 디블러 모델로 선명하게 만듭니다. 아직 지"
       "원하지 않습니다."),
    DE("Verwackelte Fotos vor dem Training mit einem gelernten Entschärfungsmodell "
       "schärfen. Noch nicht unterstützt."),
    FR("Rendre nettes les photos floues avec un modèle de défloutage appris, "
       "avant l'entraînement. Pas encore pris en charge."),
    ES("Enfocar las fotos borrosas con un modelo de desenfoque aprendido antes "
       "de entrenar. Todavía no está admitido."),
    PT("Deixar nítidas as fotos borradas com um modelo de remoção de desfoque "
       "aprendido, antes de treinar. Ainda não é suportado."),
    IT("Rendere nitide le foto sfocate con un modello di deblurring appreso, "
       "prima dell'addestramento. Non ancora supportato."),
    NL("Wazige foto's vóór de training verscherpen met een geleerd ontwazingsmodel. "
       "Nog niet ondersteund."),
    RU("Перед обучением делать смазанные снимки резче обученной моделью устранения "
       "смаза. Пока не поддерживается."),
    TR("Eğitimden önce bulanık fotoğrafları öğrenilmiş bir netleştirme modeliyle "
       "keskinleştirir. Henüz desteklenmiyor."));


// ===========================================================================
// Scene Placement
// ===========================================================================

SS_MSG(orientation_method,
    EN("Upright method"), JA("上向きの決め方"), ZH_HANS("摆正方式"),
    ZH_HANT("擺正方式"), KO("수직 정렬 방식"), DE("Aufrichtungsverfahren"),
    FR("Méthode de redressement"), ES("Método de enderezado"),
    PT("Método de nivelamento"), IT("Metodo di raddrizzamento"),
    NL("Methode voor rechtzetten"), RU("Способ выравнивания по вертикали"),
    TR("Dikleştirme yöntemi"));
SS_MSG(orientation_method_help,
    EN("How the scene is rotated to stand upright. This only affects how the "
       "result is framed for viewing, not the splats themselves. Anything other "
       "than `up` is approximated for now."),
    JA("シーンをどう回して上向きに立たせるかです。結果を見るときの見え方だけに"
       "関わり、スプラットそのものは変わりません。`up` 以外は今のところ近似で"
       "す。"),
    ZH_HANS("如何旋转场景使其摆正。它只影响观看时的取景方式，不改变泼溅本身。"
            "目前 `up` 以外的选项都是近似实现。"),
    ZH_HANT("如何旋轉場景使其擺正。它只影響觀看時的取景方式，不改變潑濺本身。"
            "目前 `up` 以外的選項都是近似實作。"),
    KO("장면을 어떻게 돌려 똑바로 세울지입니다. 결과를 볼 때의 화면 구성에만 "
       "영향을 주고 스플랫 자체는 바뀌지 않습니다. `up` 이외는 아직 근사 구현"
       "입니다."),
    DE("Wie die Szene gedreht wird, damit sie aufrecht steht. Betrifft nur, wie "
       "das Ergebnis zur Ansicht gerahmt wird, nicht die Splats selbst. Alles "
       "außer `up` ist derzeit eine Näherung."),
    FR("Comment la scène est tournée pour se tenir droite. N'agit que sur le "
       "cadrage du résultat à l'affichage, pas sur les splats eux-mêmes. Tout "
       "ce qui n'est pas `up` reste approximatif pour l'instant."),
    ES("Cómo se gira la escena para que quede derecha. Solo afecta al encuadre "
       "del resultado al verlo, no a los splats en sí. Todo lo que no sea `up` "
       "es por ahora una aproximación."),
    PT("Como a cena é girada para ficar em pé. Só afeta o enquadramento do resultado "
       "ao visualizar, não os splats em si. Tudo o que não for `up` é por enquanto "
       "aproximado."),
    IT("Come la scena viene ruotata per stare dritta. Riguarda solo l'inquadratura "
       "del risultato in visualizzazione, non gli splat stessi. Tutto ciò che "
       "non è `up` è per ora approssimato."),
    NL("Hoe de scène wordt gedraaid om rechtop te staan. Raakt alleen de kadrering "
       "van het resultaat bij het bekijken, niet de splats zelf. Alles behalve "
       "`up` is voorlopig een benadering."),
    RU("Как сцена поворачивается, чтобы стоять вертикально. Влияет только на "
       "кадрирование результата при просмотре, а не на сами сплаты. Всё, кроме "
       "`up`, пока приближение."),
    TR("Sahnenin dik durması için nasıl döndürüleceği. Yalnızca sonucun görüntülenirken "
       "nasıl çerçevelendiğini etkiler, splat'ların kendisini değil. `up` dışındaki "
       "her şey şimdilik yaklaşıktır."));

SS_MSG(center_method,
    EN("Centering method"), JA("中心の決め方"), ZH_HANS("居中方式"),
    ZH_HANT("置中方式"), KO("중심 정하는 방식"), DE("Zentrierverfahren"),
    FR("Méthode de centrage"), ES("Método de centrado"),
    PT("Método de centralização"), IT("Metodo di centratura"),
    NL("Centreermethode"), RU("Способ центрирования"), TR("Ortalama yöntemi"));
SS_MSG(center_method_help,
    EN("How the scene's origin is chosen. Like the orientation setting, this "
       "affects framing rather than the splats themselves. Anything other than "
       "`poses` is approximated for now."),
    JA("シーンの原点をどう決めるかです。向きの設定と同じく、スプラットそのもの"
       "ではなく見え方に関わります。`poses` 以外は今のところ近似です。"),
    ZH_HANS("如何确定场景的原点。与朝向设置一样，它影响的是取景而不是泼溅本身。"
            "目前 `poses` 以外的选项都是近似实现。"),
    ZH_HANT("如何確定場景的原點。與朝向設定一樣，它影響的是取景而不是潑濺本身。"
            "目前 `poses` 以外的選項都是近似實作。"),
    KO("장면의 원점을 어떻게 정할지입니다. 방향 설정과 마찬가지로 스플랫 자체"
       "가 아니라 화면 구성에 영향을 줍니다. `poses` 이외는 아직 근사 구현입니"
       "다."),
    DE("Wie der Ursprung der Szene gewählt wird. Wie bei der Ausrichtung betrifft "
       "das die Rahmung, nicht die Splats selbst. Alles außer `poses` ist derzeit "
       "eine Näherung."),
    FR("Comment l'origine de la scène est choisie. Comme pour l'orientation, "
       "cela concerne le cadrage et non les splats eux-mêmes. Tout ce qui n'est "
       "pas `poses` reste approximatif pour l'instant."),
    ES("Cómo se elige el origen de la escena. Igual que con la orientación, afecta "
       "al encuadre y no a los splats en sí. Todo lo que no sea `poses` es por "
       "ahora una aproximación."),
    PT("Como a origem da cena é escolhida. Tal como na orientação, isso afeta "
       "o enquadramento e não os splats em si. Tudo o que não for `poses` é por "
       "enquanto aproximado."),
    IT("Come viene scelta l'origine della scena. Come per l'orientamento, riguarda "
       "l'inquadratura e non gli splat stessi. Tutto ciò che non è `poses` è "
       "per ora approssimato."),
    NL("Hoe de oorsprong van de scène wordt gekozen. Net als bij de oriëntatie "
       "raakt dit de kadrering en niet de splats zelf. Alles behalve `poses` "
       "is voorlopig een benadering."),
    RU("Как выбирается начало координат сцены. Как и с ориентацией, это влияет "
       "на кадрирование, а не на сами сплаты. Всё, кроме `poses`, пока приближение."),
    TR("Sahnenin başlangıç noktasının nasıl seçileceği. Yönelim ayarında olduğu "
       "gibi, bu splat'ları değil çerçevelemeyi etkiler. `poses` dışındaki her "
       "şey şimdilik yaklaşıktır."));

SS_MSG(auto_scale_poses,
    EN("Normalize scene scale"), JA("シーンの大きさを正規化"),
    ZH_HANS("归一化场景尺度"), ZH_HANT("正規化場景尺度"),
    KO("장면 크기 정규화"), DE("Szenengröße normalisieren"),
    FR("Normaliser l'échelle de la scène"),
    ES("Normalizar la escala de la escena"),
    PT("Normalizar a escala da cena"),
    IT("Normalizzare la scala della scena"),
    NL("Schaal van de scène normaliseren"),
    RU("Нормализовать масштаб сцены"), TR("Sahne ölçeğini normalleştir"));
SS_MSG(auto_scale_poses_help,
    EN("Normalize the scene so the cameras fit in a unit-sized box. This keeps "
       "learning rates and regularizers meaningful across scenes of very different "
       "physical size; turn it off only if the dataset is already scaled the "
       "way you want."),
    JA("カメラが単位の大きさの箱に収まるようにシーンを正規化します。物理的な大"
       "きさが大きく違うシーンでも学習率や正則化の意味が保たれます。データセッ"
       "トがすでに望みどおりの尺度になっている場合にだけオフにしてください。"),
    ZH_HANS("把场景归一化，使相机正好装进一个单位大小的盒子里。这样在物理尺寸"
            "相差很大的场景之间，学习率和正则项仍然有意义。只有当数据集本来就"
            "是你想要的尺度时才关闭它。"),
    ZH_HANT("把場景正規化，使相機正好裝進一個單位大小的盒子裡。這樣在物理尺寸"
            "相差很大的場景之間，學習率和正則項仍然有意義。只有當資料集本來就"
            "是你想要的尺度時才關閉它。"),
    KO("카메라가 단위 크기 상자에 들어가도록 장면을 정규화합니다. 물리적 크기"
       "가 크게 다른 장면 사이에서도 학습률과 정규화 항의 의미가 유지됩니다. "
       "데이터셋이 이미 원하는 크기라면 그때만 끄십시오."),
    DE("Die Szene so normalisieren, dass die Kameras in einen einheitsgroßen "
       "Kasten passen. Das hält Lernraten und Regularisierer über Szenen sehr "
       "unterschiedlicher physischer Größe hinweg sinnvoll; nur abschalten, wenn "
       "der Datensatz schon so skaliert ist, wie Sie es wollen."),
    FR("Normaliser la scène pour que les caméras tiennent dans une boîte de taille "
       "unitaire. Cela garde un sens aux taux d'apprentissage et aux régularisateurs "
       "sur des scènes de tailles physiques très différentes ; à désactiver seulement "
       "si le jeu de données est déjà à l'échelle voulue."),
    ES("Normalizar la escena para que las cámaras quepan en una caja de tamaño "
       "unidad. Así las tasas de aprendizaje y los regularizadores siguen teniendo "
       "sentido en escenas de tamaños físicos muy distintos; desactívelo solo "
       "si el conjunto ya está a la escala que quiere."),
    PT("Normalizar a cena para que as câmeras caibam numa caixa de tamanho unitário. "
       "Assim as taxas de aprendizado e os regularizadores continuam fazendo "
       "sentido em cenas de tamanhos físicos muito diferentes; desligue apenas "
       "se o conjunto já estiver na escala desejada."),
    IT("Normalizzare la scena perché le camere stiano in una scatola di dimensione "
       "unitaria. Così tassi di apprendimento e regolarizzatori restano sensati "
       "su scene di dimensioni fisiche molto diverse; disattivare solo se il "
       "set di dati è già alla scala voluta."),
    NL("De scène normaliseren zodat de camera's in een doos van eenheidsgrootte "
       "passen. Zo blijven leersnelheden en regularisatoren zinvol bij scènes "
       "van zeer verschillende fysieke omvang; zet dit alleen uit als de dataset "
       "al op de gewenste schaal staat."),
    RU("Нормализовать сцену так, чтобы камеры помещались в единичный куб. Это "
       "сохраняет осмысленность скоростей обучения и регуляризаторов для сцен "
       "очень разного физического размера; выключайте, только если набор уже "
       "в нужном масштабе."),
    TR("Kameraların birim boyutlu bir kutuya sığması için sahneyi normalleştirir. "
       "Böylece fiziksel boyutları çok farklı sahnelerde öğrenme oranları ve "
       "düzenleyiciler anlamlı kalır; yalnızca veri kümesi zaten istediğiniz "
       "ölçekteyse kapatın."));

SS_MSG(outlier_threshold,
    EN("Camera outlier threshold"), JA("外れカメラのしきい値"),
    ZH_HANS("离群相机阈值"), ZH_HANT("離群相機閾值"),
    KO("이상치 카메라 임계값"), DE("Schwelle für Ausreißerkameras"),
    FR("Seuil des caméras aberrantes"), ES("Umbral de cámaras atípicas"),
    PT("Limiar de câmeras discrepantes"), IT("Soglia delle camere anomale"),
    NL("Drempel voor uitschietercamera's"), RU("Порог выбросов среди камер"),
    TR("Aykırı kamera eşiği"));
SS_MSG(outlier_threshold_help,
    EN("Discard cameras that sit far outside the rest of the capture. Lowering "
       "this rejects more, which helps when a few badly estimated poses stretch "
       "the scene and throw its scale off. Leave at infinity to keep every camera."),
    JA("撮影範囲から大きく外れたカメラを取り除きます。値を下げるほど多く除外さ"
       "れ、推定を誤ったカメラがシーンを引き伸ばして大きさを狂わせている場合に"
       "有効です。無限大のままにすると、すべてのカメラを残します。"),
    ZH_HANS("剔除明显偏离拍摄范围的相机。数值越小剔除得越多，适合个别位姿估计"
            "错误把场景拉扯变形、尺度失准的情况。保持为无穷大则保留全部相机。"),
    ZH_HANT("剔除明顯偏離拍攝範圍的相機。數值越小剔除得越多，適合個別位姿估計"
            "錯誤把場景拉扯變形、尺度失準的情況。保持為無限大則保留全部相機。"),
    KO("촬영 범위에서 크게 벗어난 카메라를 버립니다. 값을 낮출수록 더 많이 걸"
       "러내며, 잘못 추정된 몇 개의 포즈가 장면을 늘려 크기를 망칠 때 도움이 "
       "됩니다. 무한대로 두면 모든 카메라를 유지합니다."),
    DE("Kameras verwerfen, die weit außerhalb der übrigen Aufnahme liegen. Ein "
       "kleinerer Wert verwirft mehr, was hilft, wenn einige falsch geschätzte "
       "Posen die Szene dehnen und ihren Maßstab verfälschen. Auf unendlich lassen, "
       "um jede Kamera zu behalten."),
    FR("Écarter les caméras situées bien à l'extérieur du reste de la prise. "
       "Une valeur plus basse en rejette davantage, ce qui aide quand quelques "
       "poses mal estimées étirent la scène et faussent son échelle. Laisser "
       "à l'infini pour garder toutes les caméras."),
    ES("Descartar las cámaras que quedan muy fuera del resto de la captura. Bajarlo "
       "rechaza más, lo que ayuda cuando unas pocas poses mal estimadas estiran "
       "la escena y falsean su escala. Déjelo en infinito para conservar todas "
       "las cámaras."),
    PT("Descartar as câmeras que ficam bem fora do resto da captura. Baixar o "
       "valor rejeita mais, o que ajuda quando algumas poses mal estimadas esticam "
       "a cena e falseiam sua escala. Deixe em infinito para manter todas as "
       "câmeras."),
    IT("Scartare le camere che si trovano molto fuori dal resto della ripresa. "
       "Un valore più basso ne scarta di più, il che aiuta quando poche pose "
       "stimate male allungano la scena e ne falsano la scala. Lasciare a infinito "
       "per tenerle tutte."),
    NL("Camera's weggooien die ver buiten de rest van de opname liggen. Lager "
       "verwerpt er meer, wat helpt als een paar slecht geschatte poses de scène "
       "oprekken en de schaal vertekenen. Op oneindig laten om elke camera te "
       "houden."),
    RU("Отбрасывать камеры, оказавшиеся далеко за пределами остальной съёмки. "
       "Меньшее значение отбрасывает больше — это помогает, когда несколько плохо "
       "оценённых поз растягивают сцену и сбивают её масштаб. Оставьте бесконечность, "
       "чтобы сохранить все камеры."),
    TR("Çekimin geri kalanının çok dışında kalan kameraları eler. Değeri düşürmek "
       "daha çoğunu eler; birkaç yanlış kestirilmiş duruş sahneyi gerip ölçeğini "
       "bozduğunda işe yarar. Tüm kameraları tutmak için sonsuzda bırakın."));

SS_MSG(relative_scale,
    EN("Scene scale multiplier"), JA("シーンの倍率"),
    ZH_HANS("场景缩放倍数"), ZH_HANT("場景縮放倍數"), KO("장면 배율"),
    DE("Skalierungsfaktor der Szene"), FR("Facteur d'échelle de la scène"),
    ES("Factor de escala de la escena"), PT("Fator de escala da cena"),
    IT("Fattore di scala della scena"), NL("Schaalfactor van de scène"),
    RU("Множитель масштаба сцены"), TR("Sahne ölçek çarpanı"));
SS_MSG(relative_scale_help,
    EN("Multiply the whole scene by this factor before training. Raise it when "
       "a large capture comes out too small for detail to resolve. Leave unset "
       "to let the optimizer cope with scene scale on its own."),
    JA("学習の前にシーン全体をこの倍率で拡大します。大きな撮影が小さく出て細部"
       "が解像しないときに上げてください。未設定にすると、シーンの大きさへの対"
       "処はオプティマイザに任されます。"),
    ZH_HANS("训练前把整个场景乘以这个倍数。当大范围的拍摄出来太小、细节无法分"
            "辨时可以调高。不设置则把场景尺度的问题交给优化器自己应对。"),
    ZH_HANT("訓練前把整個場景乘以這個倍數。當大範圍的拍攝出來太小、細節無法分"
            "辨時可以調高。不設定則把場景尺度的問題交給最佳化器自己應對。"),
    KO("학습 전에 장면 전체를 이 배수만큼 키웁니다. 넓은 촬영이 너무 작게 나와"
       " 디테일이 분해되지 않을 때 올리십시오. 설정하지 않으면 장면 크기 문제"
       "는 옵티마이저가 알아서 처리합니다."),
    DE("Die ganze Szene vor dem Training mit diesem Faktor multiplizieren. Erhöhen, "
       "wenn eine große Aufnahme zu klein herauskommt, um Detail aufzulösen. "
       "Nicht gesetzt überlässt es dem Optimierer, mit der Szenengröße zurechtzukommen."),
    FR("Multiplier toute la scène par ce facteur avant l'entraînement. À augmenter "
       "quand une grande prise ressort trop petite pour que le détail se résolve. "
       "Non défini, on laisse l'optimiseur composer seul avec l'échelle de la "
       "scène."),
    ES("Multiplicar toda la escena por este factor antes de entrenar. Súbalo "
       "cuando una captura grande sale demasiado pequeña para resolver el detalle. "
       "Sin definir, se deja que el optimizador se las arregle con la escala "
       "de la escena."),
    PT("Multiplicar toda a cena por este fator antes de treinar. Aumente quando "
       "uma captura grande sai pequena demais para resolver o detalhe. Sem definir, "
       "deixa-se o otimizador lidar sozinho com a escala da cena."),
    IT("Moltiplicare l'intera scena per questo fattore prima dell'addestramento. "
       "Da alzare quando una ripresa ampia esce troppo piccola perché il dettaglio "
       "si risolva. Se non impostato, si lascia che sia l'ottimizzatore a gestire "
       "la scala della scena."),
    NL("De hele scène vóór de training met deze factor vermenigvuldigen. Verhoog "
       "dit als een grote opname te klein uitvalt om detail op te lossen. Niet "
       "ingesteld laat je de optimizer zelf met de schaal van de scène omgaan."),
    RU("Умножить всю сцену на этот коэффициент перед обучением. Повышайте, когда "
       "крупная съёмка выходит слишком мелкой и детали не разрешаются. Если не "
       "задано, разбираться с масштабом сцены оставляют оптимизатору."),
    TR("Eğitimden önce tüm sahneyi bu çarpanla büyütür. Geniş bir çekim ayrıntı "
       "çözünmeyecek kadar küçük çıktığında yükseltin. Ayarlanmazsa sahne ölçeğiyle "
       "başa çıkmak iyileştiriciye bırakılır."));

SS_MSG(train_frame,
    EN("Training coordinate frame"), JA("学習に使う座標系"),
    ZH_HANS("训练坐标系"), ZH_HANT("訓練座標系"), KO("학습 좌표계"),
    DE("Koordinatensystem für das Training"),
    FR("Repère utilisé pour l'entraînement"),
    ES("Sistema de coordenadas del entrenamiento"),
    PT("Sistema de coordenadas do treinamento"),
    IT("Sistema di coordinate dell'addestramento"),
    NL("Coördinatenstelsel voor de training"),
    RU("Система координат обучения"), TR("Eğitim koordinat sistemi"));
SS_MSG(train_frame_help,
    EN("Coordinate frame the splats are trained in. Only `points`, the dataset's "
       "own frame, is supported."),
    JA("スプラットを学習する座標系です。対応しているのは、データセット自身の座"
       "標系である `points` だけです。"),
    ZH_HANS("训练泼溅所用的坐标系。目前只支持 `points`，即数据集自身的坐标系。"),
    ZH_HANT("訓練潑濺所用的座標系。目前只支援 `points`，即資料集自身的座標系。"),
    KO("스플랫을 학습하는 좌표계입니다. 데이터셋 자체의 좌표계인 `points`만 지"
       "원합니다."),
    DE("Koordinatensystem, in dem die Splats trainiert werden. Unterstützt wird "
       "nur `points`, das eigene System des Datensatzes."),
    FR("Repère dans lequel les splats sont entraînés. Seul `points`, le repère "
       "propre au jeu de données, est pris en charge."),
    ES("Sistema de coordenadas en el que se entrenan los splats. Solo se admite "
       "`points`, el propio sistema del conjunto de datos."),
    PT("Sistema de coordenadas em que os splats são treinados. Só há suporte "
       "para `points`, o próprio sistema do conjunto de dados."),
    IT("Sistema di coordinate in cui gli splat vengono addestrati. È supportato "
       "solo `points`, il sistema proprio del set di dati."),
    NL("Coördinatenstelsel waarin de splats worden getraind. Alleen `points`, "
       "het eigen stelsel van de dataset, wordt ondersteund."),
    RU("Система координат, в которой обучаются сплаты. Поддерживается только "
       "`points` — собственная система набора данных."),
    TR("Splat'ların eğitildiği koordinat sistemi. Yalnızca veri kümesinin kendi "
       "sistemi olan `points` desteklenir."));


// ===========================================================================
// Splat Model
// ===========================================================================

SS_MSG(primitive,
    EN("Splat shape"), JA("スプラットの形"), ZH_HANS("泼溅形状"),
    ZH_HANT("潑濺形狀"), KO("스플랫 모양"), DE("Splat-Form"),
    FR("Forme des splats"), ES("Forma de los splats"),
    PT("Forma dos splats"), IT("Forma degli splat"), NL("Splatvorm"),
    RU("Форма сплатов"), TR("Splat biçimi"));
SS_MSG(primitive_help,
    EN("Shape used for each splat. `3dgs` is the standard Gaussian most compatible "
       "with mainstream viewers, `mip` reduces aliasing when views differ a lot "
       "in distance or resolution, and `3dgut` handles wide-angle and distorted "
       "lenses more accurately and gives cleaner geometry for meshing."),
    JA("各スプラットに使う形です。`3dgs` は標準的なガウシアンで、一般的なビュ"
       "ーアとの互換性がいちばん高い形です。`mip` は距離や解像度が大きく違う視"
       "点でのジャギーを抑えます。`3dgut` は広角や歪みの大きいレンズをより正確"
       "に扱い、メッシュ化に向いたきれいなジオメトリになります。"),
    ZH_HANS("每个泼溅使用的形状。`3dgs` 是标准高斯，与主流查看器兼容性最好；`mip` "
            "在视角距离或分辨率差异很大时减少锯齿；`3dgut` 更准确地处理广角和"
            "畸变镜头，生成更适合网格化的干净几何。"),
    ZH_HANT("每個潑濺使用的形狀。`3dgs` 是標準高斯，與主流檢視器相容性最好；`mip` "
            "在視角距離或解析度差異很大時減少鋸齒；`3dgut` 更準確地處理廣角和"
            "變形鏡頭，產生更適合網格化的乾淨幾何。"),
    KO("각 스플랫에 쓰는 모양입니다. `3dgs`는 표준 가우시안으로 일반 뷰어와의"
       " 호환성이 가장 좋고, `mip`은 거리나 해상도가 크게 다른 시점에서 계단 "
       "현상을 줄이며, `3dgut`은 광각·왜곡 렌즈를 더 정확히 다루고 메시로 만들"
       "기 좋은 깔끔한 지오메트리를 냅니다."),
    DE("Form, die für jeden Splat verwendet wird. `3dgs` ist die Standard-Gaußfunktion "
       "und mit verbreiteten Betrachtern am besten verträglich, `mip` verringert "
       "Aliasing, wenn sich Ansichten in Abstand oder Auflösung stark unterscheiden, "
       "und `3dgut` behandelt Weitwinkel und verzeichnete Objektive genauer und "
       "liefert sauberere Geometrie für die Netzerzeugung."),
    FR("Forme utilisée pour chaque splat. `3dgs` est la gaussienne standard, "
       "la plus compatible avec les visionneuses courantes, `mip` réduit l'aliasing "
       "quand les vues diffèrent beaucoup en distance ou en résolution, et `3dgut` "
       "traite plus fidèlement les grands angles et les objectifs distordus et "
       "donne une géométrie plus propre pour le maillage."),
    ES("Forma que se usa para cada splat. `3dgs` es la gaussiana estándar, la "
       "más compatible con los visores habituales; `mip` reduce el aliasing cuando "
       "las vistas difieren mucho en distancia o resolución; y `3dgut` trata "
       "con más exactitud los grandes angulares y los objetivos distorsionados "
       "y da una geometría más limpia para el mallado."),
    PT("Forma usada para cada splat. `3dgs` é a gaussiana padrão, a mais compatível "
       "com visualizadores comuns; `mip` reduz o serrilhado quando as vistas "
       "diferem muito em distância ou resolução; e `3dgut` trata com mais precisão "
       "grandes angulares e lentes distorcidas e dá uma geometria mais limpa "
       "para a malha."),
    IT("Forma usata per ogni splat. `3dgs` è la gaussiana standard, la più compatibile "
       "con i visualizzatori diffusi; `mip` riduce l'aliasing quando le viste "
       "differiscono molto per distanza o risoluzione; e `3dgut` gestisce con "
       "più precisione grandangoli e obiettivi distorti e dà una geometria più "
       "pulita per la mesh."),
    NL("Vorm die voor elke splat wordt gebruikt. `3dgs` is de standaard-gaussiaan "
       "en werkt het best in gangbare viewers, `mip` vermindert aliasing als "
       "beelden sterk verschillen in afstand of resolutie, en `3dgut` behandelt "
       "groothoek- en vervormde lenzen nauwkeuriger en geeft schonere geometrie "
       "voor mesh-generatie."),
    RU("Форма каждого сплата. `3dgs` — стандартная гауссиана, наиболее совместимая "
       "с распространёнными просмотрщиками; `mip` уменьшает ступенчатость, когда "
       "виды сильно различаются по расстоянию или разрешению; `3dgut` точнее "
       "работает с широкоугольной и искажающей оптикой и даёт более чистую геометрию "
       "под меш."),
    TR("Her splat için kullanılan biçim. `3dgs` standart Gauss'tur ve yaygın "
       "görüntüleyicilerle en uyumlusudur; `mip`, görünümler uzaklık ya da çözünürlük "
       "olarak çok farklıyken tırtıklanmayı azaltır; `3dgut` ise geniş açılı "
       "ve bozuk mercekleri daha doğru işler ve ağ çıkarmak için daha temiz geometri "
       "verir."));

SS_MSG(sh_degree,
    EN("Color detail (SH)"), JA("色の細かさ（SH）"),
    ZH_HANS("颜色细节（SH）"), ZH_HANT("顏色細節（SH）"),
    KO("색 디테일(SH)"), DE("Farbdetail (SH)"),
    FR("Détail des couleurs (SH)"), ES("Detalle de color (SH)"),
    PT("Detalhe de cor (SH)"), IT("Dettaglio del colore (SH)"),
    NL("Kleurdetail (SH)"), RU("Детальность цвета (SH)"),
    TR("Renk ayrıntısı (SH)"));
SS_MSG(sh_degree_help,
    EN("How much the color of a splat may change with viewing angle. Higher shows "
       "sharper reflections and shading as the camera moves, at the cost of memory "
       "and file size; 0 gives flat, view-independent color. Values of 4 or higher "
       "have limited support in mainstream viewers."),
    JA("見る角度によってスプラットの色がどれだけ変わってよいかです。高いほどカ"
       "メラを動かしたときの反射や陰影がはっきりしますが、メモリとファイルサイ"
       "ズが増えます。0 なら視点によらない平坦な色になります。4 以上は一般的な"
       "ビューアでの対応が限られます。"),
    ZH_HANS("泼溅的颜色可以随视角变化多少。数值越高，移动相机时反射和明暗越清"
            "晰，代价是内存和文件体积；0 表示与视角无关的平坦颜色。4 及以上在"
            "主流查看器中支持有限。"),
    ZH_HANT("潑濺的顏色可以隨視角變化多少。數值越高，移動相機時反射和明暗越清"
            "晰，代價是記憶體和檔案大小；0 表示與視角無關的平坦顏色。4 以上在"
            "主流檢視器中支援有限。"),
    KO("보는 각도에 따라 스플랫의 색이 얼마나 달라질 수 있는지입니다. 값이 클"
       "수록 카메라를 움직일 때 반사와 음영이 또렷해지지만 메모리와 파일 크기"
       "가 늘어납니다. 0이면 시점과 무관한 평평한 색이 됩니다. 4 이상은 일반 "
       "뷰어에서 지원이 제한적입니다."),
    DE("Wie stark sich die Farbe eines Splats mit dem Blickwinkel ändern darf. "
       "Höher zeigt schärfere Reflexe und Schattierungen, wenn sich die Kamera "
       "bewegt, kostet aber Speicher und Dateigröße; 0 ergibt flache, blickunabhängige "
       "Farbe. Werte ab 4 werden von verbreiteten Betrachtern nur eingeschränkt "
       "unterstützt."),
    FR("Dans quelle mesure la couleur d'un splat peut changer avec l'angle de "
       "vue. Plus haut donne des reflets et un ombrage plus nets quand la caméra "
       "bouge, au prix de la mémoire et de la taille du fichier ; 0 donne une "
       "couleur plate, indépendante de la vue. Les valeurs de 4 ou plus sont "
       "mal prises en charge par les visionneuses courantes."),
    ES("Cuánto puede cambiar el color de un splat con el ángulo de visión. Más "
       "alto muestra reflejos y sombreado más nítidos al mover la cámara, a costa "
       "de memoria y tamaño de archivo; 0 da un color plano, independiente de "
       "la vista. Los valores de 4 o más tienen soporte limitado en los visores "
       "habituales."),
    PT("Quanto a cor de um splat pode mudar com o ângulo de visão. Mais alto "
       "mostra reflexos e sombreado mais nítidos ao mover a câmera, ao custo "
       "de memória e tamanho de arquivo; 0 dá uma cor chapada, independente da "
       "vista. Valores de 4 ou mais têm suporte limitado nos visualizadores comuns."),
    IT("Quanto il colore di uno splat può cambiare con l'angolo di vista. Più "
       "alto mostra riflessi e ombreggiature più nitidi quando la camera si muove, "
       "al costo di memoria e dimensione del file; 0 dà un colore piatto, indipendente "
       "dalla vista. I valori da 4 in su hanno supporto limitato nei visualizzatori "
       "diffusi."),
    NL("Hoeveel de kleur van een splat met de kijkhoek mag veranderen. Hoger "
       "toont scherpere reflecties en schaduwen als de camera beweegt, ten koste "
       "van geheugen en bestandsgrootte; 0 geeft vlakke, kijkrichtingonafhankelijke "
       "kleur. Waarden van 4 en hoger worden in gangbare viewers beperkt ondersteund."),
    RU("Насколько цвет сплата может меняться с углом обзора. Больше — резче отражения "
       "и затенение при движении камеры, ценой памяти и размера файла; 0 даёт "
       "плоский цвет, не зависящий от вида. Значения 4 и выше слабо поддерживаются "
       "распространёнными просмотрщиками."),
    TR("Bir splat'ın renginin bakış açısıyla ne kadar değişebileceği. Yüksek "
       "değerler kamera hareket ederken yansıma ve gölgelemeyi keskinleştirir, "
       "karşılığında bellek ve dosya boyutu artar; 0 bakıştan bağımsız düz renk "
       "verir. 4 ve üstü yaygın görüntüleyicilerde sınırlı desteklenir."));

SS_MSG(sh_degree_warmup_every,
    EN("Color detail warm-up interval"), JA("色の細かさを上げる間隔"),
    ZH_HANS("颜色细节提升间隔"), ZH_HANT("顏色細節提升間隔"),
    KO("색 디테일 상승 간격"), DE("Intervall der Farbdetail-Steigerung"),
    FR("Intervalle de montée en détail des couleurs"),
    ES("Intervalo de aumento del detalle de color"),
    PT("Intervalo de aumento do detalhe de cor"),
    IT("Intervallo di aumento del dettaglio del colore"),
    NL("Interval voor toename van kleurdetail"),
    RU("Интервал роста детальности цвета"),
    TR("Renk ayrıntısı artış aralığı"));
SS_MSG(sh_degree_warmup_every_help,
    EN("How many steps between each step up in view-dependent color detail. Introducing "
       "it gradually keeps early training stable and stops reflections from being "
       "baked in too early. Small values reach full detail almost immediately."),
    JA("視点依存の色の細かさを 1 段上げるまでのステップ数です。少しずつ導入す"
       "ると学習の序盤が安定し、反射が早く焼き付いてしまうのを防げます。小さく"
       "すると、ほぼすぐに最大の細かさに達します。"),
    ZH_HANS("视角相关颜色细节每提升一级所间隔的步数。逐步引入可以让训练初期更"
            "稳定，也能避免反射过早被固化。数值很小时几乎立刻就达到最高细节。"),
    ZH_HANT("視角相關顏色細節每提升一級所間隔的步數。逐步引入可以讓訓練初期更"
            "穩定，也能避免反射過早被固化。數值很小時幾乎立刻就達到最高細節。"),
    KO("시점 의존 색 디테일을 한 단계 올리기까지의 스텝 수입니다. 조금씩 들이"
       "면 학습 초반이 안정되고 반사가 너무 일찍 굳는 것을 막습니다. 값이 작으"
       "면 거의 곧바로 최대 디테일에 이릅니다."),
    DE("Wie viele Schritte zwischen den Stufen des blickabhängigen Farbdetails "
       "liegen. Ein allmähliches Einführen hält das frühe Training stabil und "
       "verhindert, dass Reflexe zu früh eingebrannt werden. Kleine Werte erreichen "
       "fast sofort das volle Detail."),
    FR("Combien d'étapes séparent chaque palier de détail de la couleur dépendante "
       "de la vue. Une introduction progressive stabilise le début de l'entraînement "
       "et évite de figer trop tôt les reflets. De petites valeurs atteignent "
       "le détail complet presque immédiatement."),
    ES("Cuántos pasos median entre cada escalón de detalle del color dependiente "
       "de la vista. Introducirlo poco a poco estabiliza el inicio del entrenamiento "
       "y evita fijar los reflejos demasiado pronto. Valores pequeños alcanzan "
       "el detalle completo casi de inmediato."),
    PT("Quantos passos medeiam cada degrau de detalhe da cor dependente da vista. "
       "Introduzi-lo aos poucos estabiliza o início do treinamento e evita fixar "
       "os reflexos cedo demais. Valores pequenos atingem o detalhe completo "
       "quase de imediato."),
    IT("Quanti passi separano ogni gradino di dettaglio del colore dipendente "
       "dalla vista. Introdurlo per gradi stabilizza l'inizio dell'addestramento "
       "ed evita di fissare troppo presto i riflessi. Valori piccoli raggiungono "
       "il dettaglio pieno quasi subito."),
    NL("Hoeveel stappen er zitten tussen elke trap in het kijkrichtingafhankelijke "
       "kleurdetail. Geleidelijk invoeren houdt het begin van de training stabiel "
       "en voorkomt dat reflecties te vroeg worden vastgelegd. Kleine waarden "
       "bereiken vrijwel meteen het volle detail."),
    RU("Сколько шагов между ступенями роста детальности цвета, зависящего от "
       "вида. Постепенное введение делает начало обучения устойчивее и не даёт "
       "бликам впечататься слишком рано. Малые значения выводят на полную детальность "
       "почти сразу."),
    TR("Bakışa bağlı renk ayrıntısının her bir kademesi arasında kaç adım olduğu. "
       "Kademeli devreye almak eğitimin başını kararlı tutar ve yansımaların "
       "çok erken işlenmesini engeller. Küçük değerler neredeyse hemen tam ayrıntıya "
       "ulaşır."));

SS_MSG(background_mode,
    EN("Background"), JA("背景"), ZH_HANS("背景"), ZH_HANT("背景"),
    KO("배경"), DE("Hintergrund"), FR("Arrière-plan"), ES("Fondo"),
    PT("Fundo"), IT("Sfondo"), NL("Achtergrond"), RU("Фон"), TR("Arka plan"));
SS_MSG(background_mode_help,
    EN("What fills pixels no splat covers. `black` is the usual choice, "
       "`noise` discourages background transparency, and `sh` learns a skybox "
       "so distant background is represented instead of ignored."),
    JA("スプラットが覆っていない画素を何で埋めるかです。`black` が通常の選択で"
       "す。`noise` は背景が透けるのを抑えます。`sh` はスカイボックスを学習し、"
       "遠景を無視せずに表現します。"),
    ZH_HANS("没有泼溅覆盖的像素用什么填充。`black` 是常规选择；`noise` 可以抑"
            "制背景透明；`sh` 会学习一个天空盒，让远景被表示出来而不是被忽略。"),
    ZH_HANT("沒有潑濺覆蓋的像素用什麼填滿。`black` 是常規選擇；`noise` 可以抑"
            "制背景透明；`sh` 會學習一個天空盒，讓遠景被表示出來而不是被忽略。"),
    KO("스플랫이 덮지 않은 픽셀을 무엇으로 채울지입니다. `black`이 보통 선택이"
       "고, `noise`는 배경이 비치는 것을 억제하며, `sh`는 스카이박스를 학습해 "
       "먼 배경을 무시하지 않고 표현합니다."),
    DE("Womit Pixel gefüllt werden, die kein Splat bedeckt. `black` ist die "
       "übliche Wahl, `noise` hält den Hintergrund davon ab, durchsichtig zu "
       "werden, und `sh` lernt eine Skybox, sodass ferner Hintergrund "
       "dargestellt statt ignoriert wird."),
    FR("Ce qui remplit les pixels qu'aucun splat ne couvre. `black` est le "
       "choix habituel, `noise` décourage la transparence de l'arrière-plan, "
       "et `sh` apprend un skybox pour que l'arrière-plan lointain soit "
       "représenté au lieu d'être ignoré."),
    ES("Con qué se rellenan los píxeles que ningún splat cubre. `black` es la "
       "opción habitual, `noise` desalienta la transparencia del fondo, y "
       "`sh` aprende un skybox para que el fondo lejano se represente en vez "
       "de ignorarse."),
    PT("Com o que são preenchidos os pixels que nenhum splat cobre. `black` é "
       "a escolha habitual, `noise` desencoraja a transparência do fundo, e "
       "`sh` aprende um skybox para que o fundo distante seja representado em "
       "vez de ignorado."),
    IT("Con che cosa vengono riempiti i pixel che nessuno splat copre. "
       "`black` è la scelta abituale, `noise` scoraggia la trasparenza dello "
       "sfondo, e `sh` impara uno skybox così lo sfondo lontano viene "
       "rappresentato invece che ignorato."),
    NL("Waarmee pixels worden gevuld die geen enkele splat bedekt. `black` is "
       "de gebruikelijke keuze, `noise` ontmoedigt doorzichtigheid van de "
       "achtergrond, en `sh` leert een skybox zodat verre achtergrond wordt "
       "weergegeven in plaats van genegeerd."),
    RU("Чем заполняются пиксели, не покрытые ни одним сплатом. `black` — "
       "обычный выбор, `noise` не даёт фону становиться прозрачным, а `sh` "
       "обучает скайбокс, чтобы дальний фон был представлен, а не "
       "проигнорирован."),
    TR("Hiçbir splat'ın kaplamadığı pikselleri neyin dolduracağı. `black` "
       "olağan seçimdir, `noise` arka planın saydamlaşmasını caydırır, `sh` "
       "ise bir gökyüzü kutusu öğrenerek uzak arka planın yok sayılmak yerine "
       "temsil edilmesini sağlar."));

SS_MSG(background_sh_degree,
    EN("Skybox detail"), JA("スカイボックスの細かさ"), ZH_HANS("天空盒细节"),
    ZH_HANT("天空盒細節"), KO("스카이박스 디테일"), DE("Detail der Skybox"),
    FR("Détail du skybox"), ES("Detalle del skybox"),
    PT("Detalhe do skybox"), IT("Dettaglio dello skybox"),
    NL("Detail van de skybox"), RU("Детальность скайбокса"),
    TR("Gökyüzü kutusu ayrıntısı"));
SS_MSG(background_sh_degree_help,
    EN("How detailed the learned skybox may be. Higher captures finer sky and "
       "distant scenery. Only used with the `sh` background."),
    JA("学習するスカイボックスをどこまで細かくするかです。高いほど空や遠景を細"
       "かく捉えます。背景が `sh` のときだけ使われます。"),
    ZH_HANS("学习到的天空盒可以有多细。数值越高，天空和远景越细腻。仅在背景为"
            " `sh` 时使用。"),
    ZH_HANT("學習到的天空盒可以有多細。數值越高，天空和遠景越細緻。僅在背景為"
            " `sh` 時使用。"),
    KO("학습한 스카이박스가 얼마나 세밀할 수 있는지입니다. 값이 클수록 하늘과"
       " 먼 풍경을 더 곱게 담습니다. 배경이 `sh`일 때만 쓰입니다."),
    DE("Wie fein die gelernte Skybox sein darf. Höher erfasst Himmel und ferne "
       "Kulisse feiner. Wird nur mit dem Hintergrund `sh` verwendet."),
    FR("Niveau de détail du skybox appris. Plus haut capte un ciel et des lointains "
       "plus fins. Utilisé uniquement avec l'arrière-plan `sh`."),
    ES("Cuánto detalle puede tener el skybox aprendido. Más alto capta un cielo "
       "y un paisaje lejano más finos. Solo se usa con el fondo `sh`."),
    PT("Quanto detalhe o skybox aprendido pode ter. Mais alto capta um céu e "
       "um cenário distante mais finos. Só é usado com o fundo `sh`."),
    IT("Quanto può essere dettagliato lo skybox appreso. Più alto cattura cielo "
       "e paesaggio lontano più fini. Usato solo con lo sfondo `sh`."),
    NL("Hoe gedetailleerd de geleerde skybox mag zijn. Hoger legt fijnere lucht "
       "en verre omgeving vast. Wordt alleen bij achtergrond `sh` gebruikt."),
    RU("Насколько подробным может быть обученный скайбокс. Больше — тоньше передаётся "
       "небо и дальний план. Используется только с фоном `sh`."),
    TR("Öğrenilen gökyüzü kutusunun ne kadar ayrıntılı olabileceği. Yüksek değerler "
       "gökyüzünü ve uzak manzarayı daha ince yakalar. Yalnızca `sh` arka planıyla "
       "kullanılır."));

SS_MSG(background_noise_warmup,
    EN("Background noise warm-up"), JA("背景ノイズの立ち上がり"),
    ZH_HANS("背景噪声预热"), ZH_HANT("背景雜訊預熱"),
    KO("배경 노이즈 워밍업"), DE("Anlauf des Hintergrundrauschens"),
    FR("Montée du bruit de fond"), ES("Arranque del ruido de fondo"),
    PT("Aquecimento do ruído de fundo"), IT("Avvio del rumore di fondo"),
    NL("Opbouw van de achtergrondruis"), RU("Разгон фонового шума"),
    TR("Arka plan gürültüsü ısınması"));
SS_MSG(background_noise_warmup_help,
    EN("How many steps the background noise takes to reach full strength. Only "
       "used with the `noise` background."),
    JA("背景ノイズが最大の強さになるまでのステップ数です。背景が `noise` のと"
       "きだけ使われます。"),
    ZH_HANS("背景噪声达到最大强度所需的步数。仅在背景为 `noise` 时使用。"),
    ZH_HANT("背景雜訊達到最大強度所需的步數。僅在背景為 `noise` 時使用。"),
    KO("배경 노이즈가 최대 세기에 이르기까지의 스텝 수입니다. 배경이 `noise`일"
       " 때만 쓰입니다."),
    DE("Wie viele Schritte das Hintergrundrauschen braucht, um volle Stärke zu "
       "erreichen. Wird nur mit dem Hintergrund `noise` verwendet."),
    FR("Combien d'étapes le bruit de fond met à atteindre sa pleine force. Utilisé "
       "uniquement avec l'arrière-plan `noise`."),
    ES("Cuántos pasos tarda el ruido de fondo en alcanzar toda su fuerza. Solo "
       "se usa con el fondo `noise`."),
    PT("Quantos passos o ruído de fundo leva para atingir força total. Só é usado "
       "com o fundo `noise`."),
    IT("Quanti passi impiega il rumore di fondo a raggiungere la piena forza. "
       "Usato solo con lo sfondo `noise`."),
    NL("Hoeveel stappen de achtergrondruis nodig heeft om op volle sterkte te "
       "komen. Wordt alleen bij achtergrond `noise` gebruikt."),
    RU("За сколько шагов фоновый шум набирает полную силу. Используется только "
       "с фоном `noise`."),
    TR("Arka plan gürültüsünün tam güce ulaşması için gereken adım sayısı. Yalnızca "
       "`noise` arka planıyla kullanılır."));

SS_MSG(background_noise_pre_warmup,
    EN("Initial background noise"), JA("最初の背景ノイズの強さ"),
    ZH_HANS("初始背景噪声强度"), ZH_HANT("初始背景雜訊強度"),
    KO("초기 배경 노이즈 세기"), DE("Anfängliches Hintergrundrauschen"),
    FR("Bruit de fond initial"), ES("Ruido de fondo inicial"),
    PT("Ruído de fundo inicial"), IT("Rumore di fondo iniziale"),
    NL("Aanvankelijke achtergrondruis"), RU("Начальный фоновый шум"),
    TR("Başlangıçtaki arka plan gürültüsü"));
SS_MSG(background_noise_pre_warmup_help,
    EN("How strong the background noise is at the very start, from 0 to 1. Higher "
       "values keep splats from being washed away in the first steps."),
    JA("いちばん最初の背景ノイズの強さを 0 から 1 で指定します。値を上げると、"
       "序盤の数ステップでスプラットが洗い流されるのを防げます。"),
    ZH_HANS("最开始时背景噪声的强度，取值 0 到 1。调高可以避免最初几步里泼溅被"
            "冲刷掉。"),
    ZH_HANT("最開始時背景雜訊的強度，取值 0 到 1。調高可以避免最初幾步裡潑濺被"
            "沖刷掉。"),
    KO("맨 처음 배경 노이즈의 세기를 0에서 1 사이로 지정합니다. 값을 올리면 초"
       "반 몇 스텝에서 스플랫이 씻겨 나가는 것을 막습니다."),
    DE("Wie stark das Hintergrundrauschen ganz zu Beginn ist, von 0 bis 1. Höhere "
       "Werte bewahren die Splats davor, in den ersten Schritten weggespült zu "
       "werden."),
    FR("Force du bruit de fond tout au début, de 0 à 1. Des valeurs plus hautes "
       "empêchent les splats d'être emportés dans les premières étapes."),
    ES("Intensidad del ruido de fondo al principio, de 0 a 1. Valores más altos "
       "evitan que los splats se laven en los primeros pasos."),
    PT("Intensidade do ruído de fundo logo no início, de 0 a 1. Valores mais "
       "altos evitam que os splats sejam levados nos primeiros passos."),
    IT("Intensità del rumore di fondo proprio all'inizio, da 0 a 1. Valori più "
       "alti evitano che gli splat vengano spazzati via nei primi passi."),
    NL("Hoe sterk de achtergrondruis helemaal aan het begin is, van 0 tot 1. "
       "Hogere waarden voorkomen dat splats in de eerste stappen worden weggespoeld."),
    RU("Насколько силён фоновый шум в самом начале, от 0 до 1. Более высокие "
       "значения не дают сплатам смыться на первых шагах."),
    TR("Arka plan gürültüsünün en başta ne kadar güçlü olduğu; 0 ile 1 arası. "
       "Yüksek değerler splat'ların ilk adımlarda silinip gitmesini önler."));

SS_MSG(scale_init,
    EN("Initial splat size"), JA("スプラットの初期サイズ"),
    ZH_HANS("泼溅初始大小"), ZH_HANT("潑濺初始大小"), KO("스플랫 초기 크기"),
    DE("Anfangsgröße der Splats"), FR("Taille initiale des splats"),
    ES("Tamaño inicial de los splats"), PT("Tamanho inicial dos splats"),
    IT("Dimensione iniziale degli splat"), NL("Beginformaat van de splats"),
    RU("Начальный размер сплатов"), TR("Splat başlangıç boyutu"));
SS_MSG(scale_init_help,
    EN("How big each splat starts out. Leave unset to derive it from the point "
       "cloud; larger fills space faster but starts blurrier."),
    JA("各スプラットの初期サイズです。未設定なら点群から決まります。大きいほど"
       "空間を早く埋めますが、最初はぼやけます。"),
    ZH_HANS("每个泼溅的初始大小。不设置则由点云推算。越大填满空间越快，但一开"
            "始更模糊。"),
    ZH_HANT("每個潑濺的初始大小。不設定則由點雲推算。越大填滿空間越快，但一開"
            "始更模糊。"),
    KO("각 스플랫의 초기 크기입니다. 설정하지 않으면 포인트 클라우드에서 정합"
       "니다. 클수록 공간을 빨리 채우지만 처음에는 더 흐릿합니다."),
    DE("Anfangsgröße jedes Splats. Nicht gesetzt wird sie aus der Punktwolke "
       "abgeleitet; größer füllt den Raum schneller, startet aber unschärfer."),
    FR("Taille initiale de chaque splat. Non défini, elle est déduite du nuage "
       "de points ; plus grand remplit l'espace plus vite mais démarre plus flou."),
    ES("Tamaño inicial de cada splat. Sin definir, se deduce de la nube de puntos; "
       "más grande llena el espacio antes pero empieza más borroso."),
    PT("Tamanho inicial de cada splat. Sem definir, é deduzido da nuvem de pontos; "
       "maior preenche o espaço mais depressa mas começa mais borrado."),
    IT("Dimensione iniziale di ogni splat. Se non impostata, viene dedotta dalla "
       "nuvola di punti; più grande riempie lo spazio prima ma parte più sfocato."),
    NL("Beginformaat van elke splat. Niet ingesteld wordt het uit de puntenwolk "
       "afgeleid; groter vult de ruimte sneller maar begint waziger."),
    RU("Начальный размер каждого сплата. Если не задан, выводится из облака точек; "
       "больше — пространство заполняется быстрее, но старт более размытый."),
    TR("Her splat'ın başlangıç boyutu. Ayarlanmazsa nokta bulutundan türetilir; "
       "büyük olması boşluğu daha çabuk doldurur ama başlangıç daha bulanıktır."));

SS_MSG(opacity_init,
    EN("Initial splat opacity"), JA("スプラットの初期不透明度"),
    ZH_HANS("泼溅初始不透明度"), ZH_HANT("潑濺初始不透明度"),
    KO("스플랫 초기 불투명도"), DE("Anfangsdeckkraft der Splats"),
    FR("Opacité initiale des splats"), ES("Opacidad inicial de los splats"),
    PT("Opacidade inicial dos splats"), IT("Opacità iniziale degli splat"),
    NL("Begindekking van de splats"), RU("Начальная непрозрачность сплатов"),
    TR("Splat başlangıç saydamsızlığı"));
SS_MSG(opacity_init_help,
    EN("How solid each splat starts out. Leave unset to choose automatically; "
       "lower makes early training more forgiving, higher locks geometry in sooner."),
    JA("各スプラットの初期の濃さです。未設定なら自動で選ばれます。低いほど学習"
       "の序盤が寛容になり、高いほど早くジオメトリが固まります。"),
    ZH_HANS("每个泼溅的初始实心程度。不设置则自动选择。数值越低，训练初期越宽"
            "容；越高则几何更早定形。"),
    ZH_HANT("每個潑濺的初始實心程度。不設定則自動選擇。數值越低，訓練初期越寬"
            "容；越高則幾何更早定形。"),
    KO("각 스플랫이 처음에 얼마나 진한지입니다. 설정하지 않으면 자동으로 고릅"
       "니다. 낮으면 학습 초반이 너그러워지고, 높으면 지오메트리가 더 일찍 굳"
       "습니다."),
    DE("Wie fest jeder Splat anfangs ist. Nicht gesetzt wird automatisch gewählt; "
       "niedriger macht das frühe Training nachsichtiger, höher legt die Geometrie "
       "früher fest."),
    FR("À quel point chaque splat est plein au départ. Non défini, la valeur "
       "est choisie automatiquement ; plus bas rend le début de l'entraînement "
       "plus indulgent, plus haut fige la géométrie plus tôt."),
    ES("Cuán sólido empieza cada splat. Sin definir, se elige automáticamente; "
       "más bajo hace el inicio del entrenamiento más indulgente, más alto fija "
       "la geometría antes."),
    PT("Quão sólido cada splat começa. Sem definir, é escolhido automaticamente; "
       "mais baixo torna o início do treinamento mais tolerante, mais alto fixa "
       "a geometria mais cedo."),
    IT("Quanto è pieno ogni splat all'inizio. Se non impostato, viene scelto "
       "automaticamente; più basso rende l'inizio dell'addestramento più indulgente, "
       "più alto fissa prima la geometria."),
    NL("Hoe massief elke splat begint. Niet ingesteld wordt het automatisch gekozen; "
       "lager maakt het begin van de training vergevingsgezinder, hoger legt "
       "de geometrie eerder vast."),
    RU("Насколько плотным начинается каждый сплат. Если не задано, выбирается "
       "автоматически; меньше — начало обучения снисходительнее, больше — геометрия "
       "закрепляется раньше."),
    TR("Her splat'ın başlangıçta ne kadar dolu olduğu. Ayarlanmazsa kendiliğinden "
       "seçilir; düşük değerler eğitimin başını daha bağışlayıcı yapar, yüksek "
       "değerler geometriyi daha erken sabitler."));

SS_MSG(suppress_initial_scales,
    EN("Shrink splats in sparse areas"),
    JA("点が疎な場所ではスプラットを小さく"), ZH_HANS("在点稀疏处缩小泼溅"),
    ZH_HANT("在點稀疏處縮小潑濺"), KO("점이 드문 곳의 스플랫 축소"),
    DE("Splats in dünn besetzten Bereichen verkleinern"),
    FR("Réduire les splats dans les zones clairsemées"),
    ES("Reducir los splats donde hay pocos puntos"),
    PT("Reduzir os splats onde há poucos pontos"),
    IT("Ridurre gli splat dove i punti sono radi"),
    NL("Splats verkleinen waar weinig punten zijn"),
    RU("Уменьшать сплаты в разреженных местах"),
    TR("Nokta seyrek olan yerlerde splat'ları küçült"));
SS_MSG(suppress_initial_scales_help,
    EN("Start splats small where the point cloud is sparse. Keeps them from blooming "
       "into large floaters over empty space."),
    JA("点群が疎な場所では、スプラットを小さく始めます。何もない空間で大きな浮"
       "遊物にふくらむのを防ぎます。"),
    ZH_HANS("在点云稀疏的地方让泼溅从更小的尺寸开始。可以防止它们在空处膨胀成"
            "大团漂浮物。"),
    ZH_HANT("在點雲稀疏的地方讓潑濺從更小的尺寸開始。可以防止它們在空處膨脹成"
            "大團漂浮物。"),
    KO("포인트 클라우드가 성긴 곳에서는 스플랫을 작게 시작합니다. 빈 공간에서"
       " 큰 부유물로 부풀어 오르는 것을 막습니다."),
    DE("Splats dort klein starten lassen, wo die Punktwolke dünn ist. Bewahrt "
       "sie davor, über leerem Raum zu großen Schwebeteilen aufzublühen."),
    FR("Faire démarrer les splats petits là où le nuage de points est clairsemé. "
       "Les empêche de gonfler en gros flotteurs au-dessus du vide."),
    ES("Hacer que los splats empiecen pequeños donde la nube de puntos es escasa. "
       "Evita que se hinchen hasta convertirse en grandes restos flotantes sobre "
       "el vacío."),
    PT("Fazer os splats começarem pequenos onde a nuvem de pontos é esparsa. "
       "Evita que inchem até virarem grandes resíduos flutuantes sobre o vazio."),
    IT("Far partire gli splat piccoli dove la nuvola di punti è rada. Evita che "
       "si gonfino in grossi frammenti fluttuanti sopra il vuoto."),
    NL("Splats klein laten beginnen waar de puntenwolk dun is. Voorkomt dat ze "
       "boven lege ruimte uitdijen tot grote zwevers."),
    RU("Начинать со сплатов поменьше там, где облако точек разрежено. Не даёт "
       "им раздуваться в крупные «летающие» артефакты над пустотой."),
    TR("Nokta bulutunun seyrek olduğu yerlerde splat'ları küçük başlatır. Boşluğun "
       "üstünde büyük uçuşan artıklara şişmelerini önler."));

SS_MSG(use_camera_optimizer,
    EN("Refine camera poses"), JA("カメラ位置を微調整"),
    ZH_HANS("微调相机位姿"), ZH_HANT("微調相機位姿"),
    KO("카메라 포즈 미세 조정"), DE("Kameraposen nachjustieren"),
    FR("Affiner les poses des caméras"),
    ES("Afinar las poses de las cámaras"),
    PT("Ajustar as poses das câmeras"), IT("Affinare le pose delle camere"),
    NL("Cameraposes bijstellen"), RU("Уточнять положения камер"),
    TR("Kamera duruşlarını ince ayarla"));
SS_MSG(use_camera_optimizer_help,
    EN("Let training nudge the camera poses to absorb small pose errors. Not "
       "supported yet."),
    JA("小さな姿勢の誤差を吸収できるよう、学習中にカメラの位置をわずかに動かし"
       "ます。まだ対応していません。"),
    ZH_HANS("让训练过程微调相机位姿，以吸收细小的位姿误差。尚未支持。"),
    ZH_HANT("讓訓練過程微調相機位姿，以吸收細小的位姿誤差。尚未支援。"),
    KO("작은 포즈 오차를 흡수하도록 학습이 카메라 위치를 조금씩 움직이게 합니"
       "다. 아직 지원하지 않습니다."),
    DE("Das Training die Kameraposen leicht nachschieben lassen, um kleine Posenfehler "
       "aufzufangen. Noch nicht unterstützt."),
    FR("Laisser l'entraînement ajuster légèrement les poses des caméras pour "
       "absorber de petites erreurs de pose. Pas encore pris en charge."),
    ES("Dejar que el entrenamiento retoque un poco las poses de las cámaras para "
       "absorber pequeños errores de pose. Todavía no está admitido."),
    PT("Deixar o treinamento ajustar levemente as poses das câmeras para absorver "
       "pequenos erros de pose. Ainda não é suportado."),
    IT("Lasciare che l'addestramento sposti leggermente le pose delle camere "
       "per assorbire piccoli errori di posa. Non ancora supportato."),
    NL("De training de cameraposes licht laten bijstellen om kleine posefouten "
       "op te vangen. Nog niet ondersteund."),
    RU("Позволить обучению слегка подвинуть положения камер, чтобы поглотить "
       "мелкие ошибки поз. Пока не поддерживается."),
    TR("Küçük duruş hatalarını soğurmak için eğitimin kamera duruşlarını hafifçe "
       "kaydırmasına izin verir. Henüz desteklenmiyor."));


// ===========================================================================
// Detail & Splat Count
// ===========================================================================

SS_MSG(quality,
    EN("Detail level"), JA("精細さ"), ZH_HANS("细节水平"), ZH_HANT("細節水準"),
    KO("디테일 수준"),
    DE("Detailgrad"), FR("Niveau de détail"), ES("Nivel de detalle"),
    PT("Nível de detalhe"), IT("Livello di dettaglio"), NL("Detailniveau"),
    RU("Уровень детализации"), TR("Ayrıntı düzeyi"));
SS_MSG(quality_help,
    EN("Overall detail level, setting the splat budget and how long training "
       "runs. Use a higher level for larger datasets, or for datasets with "
       "more detail in them."),
    JA("全体の精細さです。スプラット数の上限と学習の長さをまとめて決めます。"
       "データセットが大きいときや、細部の多いデータセットでは高い段階を"
       "使ってください。"),
    ZH_HANS("整体细节水平，同时决定泼溅数量上限和训练时长。数据集较大、或者"
            "细节较多时，请选更高的档位。"),
    ZH_HANT("整體細節水準，同時決定潑濺數量上限和訓練長度。資料集較大、或者"
            "細節較多時，請選更高的檔位。"),
    KO("전체적인 디테일 수준으로, 스플랫 예산과 학습 길이를 함께 정합니다. "
       "데이터셋이 크거나 세부가 많다면 더 높은 단계를 쓰세요."),
    DE("Gesamtdetailgrad; legt das Splat-Budget und die Trainingsdauer fest. "
       "Für größere Datensätze oder solche mit mehr Details eine höhere Stufe "
       "wählen."),
    FR("Niveau de détail global : fixe le budget de splats et la durée de "
       "l'entraînement. Prenez un niveau plus élevé pour les grands jeux de "
       "données, ou pour ceux qui contiennent plus de détails."),
    ES("Nivel de detalle general: fija el presupuesto de splats y la duración "
       "del entrenamiento. Use un nivel más alto con conjuntos de datos "
       "grandes, o con más detalle dentro."),
    PT("Nível de detalhe geral: define o orçamento de splats e a duração do "
       "treinamento. Use um nível mais alto para conjuntos de dados maiores, "
       "ou com mais detalhe dentro."),
    IT("Livello di dettaglio complessivo: fissa il budget di splat e la "
       "durata dell'addestramento. Usi un livello più alto per set di dati "
       "grandi, o con più dettaglio dentro."),
    NL("Algemeen detailniveau: bepaalt het splatbudget en hoe lang er wordt "
       "getraind. Neem een hoger niveau bij grotere datasets, of datasets met "
       "meer detail erin."),
    RU("Общий уровень детализации: задаёт бюджет сплатов и длительность "
       "обучения. Для больших наборов данных или наборов с большим "
       "количеством деталей берите уровень выше."),
    TR("Genel ayrıntı düzeyi; splat bütçesini ve eğitimin ne kadar süreceğini "
       "birlikte belirler. Daha büyük veri kümelerinde ya da içinde daha çok "
       "ayrıntı olanlarda daha yüksek bir düzey kullanın."));

SS_MSG(cap_max,
    EN("Maximum splats"), JA("スプラット数の上限"), ZH_HANS("泼溅数量上限"),
    ZH_HANT("潑濺數量上限"), KO("최대 스플랫 수"),
    DE("Höchstzahl der Splats"), FR("Nombre maximal de splats"),
    ES("Número máximo de splats"), PT("Número máximo de splats"),
    IT("Numero massimo di splat"), NL("Maximum aantal splats"),
    RU("Максимум сплатов"), TR("En fazla splat sayısı"));
SS_MSG(cap_max_help,
    EN("Largest number of splats the scene may grow to. This is the main quality "
       "dial: raising it captures more detail and produces a bigger file that "
       "renders more slowly. Worth tuning per scene."),
    JA("シーンが増やせるスプラット数の上限です。品質を決める主なつまみで、上げ"
       "るほど細部を捉えられますが、ファイルは大きく、表示は重くなります。シー"
       "ンごとに調整する価値があります。"),
    ZH_HANS("场景最多可以增长到的泼溅数量。这是主要的质量旋钮：调高能捕捉更多"
            "细节，但文件更大、渲染更慢。值得逐个场景调整。"),
    ZH_HANT("場景最多可以增長到的潑濺數量。這是主要的品質旋鈕：調高能捕捉更多"
            "細節，但檔案更大、算圖更慢。值得逐個場景調整。"),
    KO("장면이 늘어날 수 있는 최대 스플랫 수입니다. 품질을 좌우하는 주된 조절"
       "값으로, 올리면 더 많은 디테일을 담지만 파일이 커지고 렌더링이 느려집니"
       "다. 장면마다 조정할 가치가 있습니다."),
    DE("Größte Zahl von Splats, auf die die Szene wachsen darf. Das ist der Hauptregler "
       "für die Qualität: höher erfasst mehr Details und ergibt eine größere "
       "Datei, die langsamer rendert. Lohnt sich, pro Szene einzustellen."),
    FR("Nombre maximal de splats que la scène peut atteindre. C'est le principal "
       "réglage de qualité : l'augmenter capte plus de détails et produit un "
       "fichier plus gros, plus lent à afficher. Il vaut la peine de l'ajuster "
       "par scène."),
    ES("Número máximo de splats hasta el que puede crecer la escena. Es el mando "
       "principal de calidad: subirlo capta más detalle y produce un archivo "
       "mayor que se muestra más despacio. Vale la pena ajustarlo por escena."),
    PT("Número máximo de splats a que a cena pode chegar. É o principal controle "
       "de qualidade: aumentá-lo capta mais detalhe e gera um arquivo maior, "
       "que renderiza mais devagar. Vale ajustar por cena."),
    IT("Numero massimo di splat a cui la scena può crescere. È la manopola principale "
       "della qualità: alzarlo cattura più dettaglio e produce un file più grande, "
       "più lento da mostrare. Vale la pena regolarlo per ogni scena."),
    NL("Grootste aantal splats waartoe de scène mag groeien. Dit is de belangrijkste "
       "kwaliteitsknop: hoger legt meer detail vast en geeft een groter bestand "
       "dat trager rendert. De moeite waard om per scène af te stemmen."),
    RU("Наибольшее число сплатов, до которого может вырасти сцена. Это главный "
       "регулятор качества: выше — больше деталей, но крупнее файл и медленнее "
       "отрисовка. Стоит подбирать под каждую сцену."),
    TR("Sahnenin çıkabileceği en büyük splat sayısı. Kalitenin ana ayarıdır: "
       "yükseltmek daha çok ayrıntı yakalar, dosyayı büyütür ve görüntülemeyi "
       "yavaşlatır. Sahne başına ayarlamaya değer."));

SS_MSG(distraction_robustness,
    EN("Distraction robustness"), JA("写り込みへの強さ"),
    ZH_HANS("干扰物鲁棒性"), ZH_HANT("干擾物穩健度"), KO("방해물 견고성"),
    DE("Robustheit gegen Störobjekte"), FR("Robustesse aux intrus"),
    ES("Robustez ante elementos molestos"),
    PT("Robustez a elementos indesejados"),
    IT("Robustezza agli elementi di disturbo"),
    NL("Bestandheid tegen stoorelementen"), RU("Устойчивость к помехам"),
    TR("İstenmeyen nesnelere dayanıklılık"));
SS_MSG(distraction_robustness_help,
    EN("Ignore people, cars and anything else that moves between photos. "
       "Splats are no longer spent on them, at the cost of some sensitivity "
       "to real detail."),
    JA("写真ごとに動く人や車などを無視します。そうしたものにスプラットを使わな"
       "くなりますが、本物の細部への感度は少し下がります。"),
    ZH_HANS("忽略在照片之间移动的行人、车辆等。不再为它们分配泼溅，代价是对真"
            "实细节的敏感度略有下降。"),
    ZH_HANT("忽略在照片之間移動的行人、車輛等。不再為它們分配潑濺，代價是對真"
            "實細節的敏感度略有下降。"),
    KO("사진마다 움직이는 사람, 자동차 등을 무시합니다. 그런 것에 스플랫을 쓰"
       "지 않게 되지만, 실제 디테일에 대한 민감도는 조금 떨어집니다."),
    DE("Personen, Autos und alles andere ignorieren, was sich zwischen den "
       "Fotos bewegt. Auf sie werden keine Splats mehr verwendet, um den "
       "Preis etwas geringerer Empfindlichkeit für echte Details."),
    FR("Ignorer les passants, les voitures et tout ce qui bouge d'une photo à "
       "l'autre. Plus aucun splat n'y est consacré, au prix d'une sensibilité "
       "un peu moindre aux vrais détails."),
    ES("Ignorar personas, coches y cualquier otra cosa que se mueva entre "
       "fotos. Ya no se gastan splats en ellos, a costa de algo de "
       "sensibilidad al detalle real."),
    PT("Ignorar pessoas, carros e qualquer outra coisa que se mova entre as "
       "fotos. Deixam de ser gastos splats com eles, ao custo de alguma "
       "sensibilidade ao detalhe real."),
    IT("Ignorare persone, automobili e tutto ciò che si muove da una foto "
       "all'altra. Non vengono più spesi splat su di loro, al costo di un po' "
       "di sensibilità al dettaglio reale."),
    NL("Mensen, auto's en al het andere dat tussen de foto's beweegt negeren. "
       "Daar gaan geen splats meer heen, ten koste van iets minder gevoel "
       "voor echt detail."),
    RU("Игнорировать людей, машины и всё остальное, что меняется от снимка к "
       "снимку. Сплаты на них больше не тратятся ценой некоторой потери "
       "чувствительности к настоящим деталям."),
    TR("Fotoğraflar arasında yer değiştiren insanları, arabaları ve "
       "benzerlerini yok sayar. Onlara artık splat harcanmaz; karşılığında "
       "gerçek ayrıntıya duyarlılık biraz azalır."));

SS_MSG(min_init_fraction,
    EN("Minimum starting splats"), JA("開始時のスプラット数の下限"),
    ZH_HANS("起始泼溅数下限"), ZH_HANT("起始潑濺數下限"),
    KO("시작 스플랫 수 하한"), DE("Mindestzahl der Start-Splats"),
    FR("Nombre minimal de splats de départ"),
    ES("Número mínimo de splats iniciales"),
    PT("Número mínimo de splats iniciais"),
    IT("Numero minimo di splat iniziali"), NL("Minimum aantal beginsplats"),
    RU("Минимум начальных сплатов"), TR("En az başlangıç splat sayısı"));
SS_MSG(min_init_fraction_help,
    EN("Smallest starting splat count, as a share of cap_max. Raise it when the "
       "initial point cloud is sparse, such as synthetic scenes, so training "
       "has enough to work with."),
    JA("開始時のスプラット数の下限を、cap_max に対する割合で指定します。合成シ"
       "ーンなど初期点群が疎なときに上げると、学習が始めやすくなります。"),
    ZH_HANS("起始泼溅数量的下限，按 cap_max 的比例给出。初始点云稀疏时（例如合"
            "成场景）调高它，训练才有足够的东西可用。"),
    ZH_HANT("起始潑濺數量的下限，按 cap_max 的比例給出。初始點雲稀疏時（例如合"
            "成場景）調高它，訓練才有足夠的東西可用。"),
    KO("시작 스플랫 수의 하한을 cap_max에 대한 비율로 지정합니다. 합성 장면처"
       "럼 초기 포인트 클라우드가 성길 때 올리면 학습이 다룰 재료가 충분해집니"
       "다."),
    DE("Kleinste Anfangszahl an Splats, als Anteil von cap_max. Erhöhen, wenn "
       "die Startpunktwolke dünn ist, etwa bei synthetischen Szenen, damit das "
       "Training genug Material hat."),
    FR("Nombre minimal de splats au départ, en proportion de cap_max. À augmenter "
       "quand le nuage de points initial est clairsemé, par exemple pour les "
       "scènes de synthèse, afin que l'entraînement ait de quoi travailler."),
    ES("Número mínimo de splats al comenzar, como proporción de cap_max. Súbalo "
       "cuando la nube de puntos inicial sea escasa, como en escenas sintéticas, "
       "para que el entrenamiento tenga material suficiente."),
    PT("Número mínimo de splats no início, como proporção de cap_max. Aumente "
       "quando a nuvem de pontos inicial for esparsa, como em cenas sintéticas, "
       "para o treinamento ter material suficiente."),
    IT("Numero minimo di splat all'avvio, come quota di cap_max. Da alzare quando "
       "la nuvola di punti iniziale è rada, per esempio nelle scene sintetiche, "
       "così l'addestramento ha materiale a sufficienza."),
    NL("Kleinste aantal splats bij de start, als aandeel van cap_max. Verhoog "
       "dit als de beginpuntenwolk dun is, bijvoorbeeld bij synthetische scènes, "
       "zodat de training genoeg heeft om mee te werken."),
    RU("Наименьшее стартовое число сплатов, как доля от cap_max. Повышайте, когда "
       "исходное облако точек разрежено — например, в синтетических сценах, — "
       "чтобы обучению было с чем работать."),
    TR("Başlangıçtaki en az splat sayısı; cap_max'e oran olarak verilir. Sentetik "
       "sahneler gibi başlangıç nokta bulutunun seyrek olduğu durumlarda yükseltin "
       "ki eğitimin çalışacak yeterli malzemesi olsun."));

SS_MSG(growth_factor,
    EN("Growth per round"), JA("1 回ごとの増加率"), ZH_HANS("每轮增长倍数"),
    ZH_HANT("每輪增長倍數"), KO("회차당 증가 배수"),
    DE("Wachstum pro Runde"), FR("Croissance par cycle"),
    ES("Crecimiento por ronda"), PT("Crescimento por rodada"),
    IT("Crescita per ciclo"), NL("Groei per ronde"), RU("Прирост за раунд"),
    TR("Tur başına büyüme"));
SS_MSG(growth_factor_help,
    EN("How fast the splat count grows at each round, as a multiplier. Higher "
       "reaches cap_max sooner; lower grows gradually, which tends to place splats "
       "more carefully."),
    JA("1 回の増加ラウンドでスプラット数がどれだけ増えるかを倍率で指定します。"
       "高いほど早く cap_max に届き、低いほど少しずつ増えるので、スプラットの"
       "置き方が丁寧になりがちです。"),
    ZH_HANS("每一轮泼溅数量增长的倍数。数值越高越快达到 cap_max；越低则逐步增"
            "长，泼溅的摆放往往更讲究。"),
    ZH_HANT("每一輪潑濺數量增長的倍數。數值越高越快達到 cap_max；越低則逐步增"
            "長，潑濺的擺放往往更講究。"),
    KO("한 회차마다 스플랫 수가 얼마나 늘어나는지를 배수로 지정합니다. 크면 cap_max에"
       " 빨리 닿고, 작으면 조금씩 늘어나 스플랫 배치가 더 신중해지는 편입니다"
       "."),
    DE("Wie stark die Splat-Zahl in jeder Runde wächst, als Multiplikator. Höher "
       "erreicht cap_max eher; niedriger wächst allmählich, was Splats meist "
       "sorgfältiger platziert."),
    FR("De combien le nombre de splats croît à chaque cycle, en multiplicateur. "
       "Plus haut atteint cap_max plus tôt ; plus bas croît graduellement, ce "
       "qui place généralement les splats avec plus de soin."),
    ES("Cuánto crece el número de splats en cada ronda, como multiplicador. Más "
       "alto llega antes a cap_max; más bajo crece poco a poco, lo que suele "
       "colocar los splats con más cuidado."),
    PT("Quanto o número de splats cresce a cada rodada, como multiplicador. Mais "
       "alto chega antes a cap_max; mais baixo cresce aos poucos, o que costuma "
       "posicionar os splats com mais cuidado."),
    IT("Di quanto cresce il numero di splat a ogni ciclo, come moltiplicatore. "
       "Più alto raggiunge prima cap_max; più basso cresce gradualmente, il che "
       "di solito colloca gli splat con più cura."),
    NL("Hoeveel het aantal splats per ronde groeit, als vermenigvuldiger. Hoger "
       "bereikt cap_max eerder; lager groeit geleidelijk, wat splats meestal "
       "zorgvuldiger plaatst."),
    RU("Насколько растёт число сплатов за каждый раунд, множителем. Больше — "
       "быстрее достигается cap_max; меньше — рост постепенный, и сплаты обычно "
       "расставляются аккуратнее."),
    TR("Splat sayısının her turda ne kadar büyüyeceği, çarpan olarak. Yüksek "
       "değerler cap_max'e daha çabuk ulaşır; düşük değerler yavaş büyür ve splat'ları "
       "genelde daha özenli yerleştirir."));

SS_MSG(min_opacity,
    EN("Recycle below opacity"), JA("再利用する不透明度のしきい値"),
    ZH_HANS("低于此不透明度即回收"), ZH_HANT("低於此不透明度即回收"),
    KO("이 불투명도 미만이면 재활용"),
    DE("Deckkraft, unter der recycelt wird"),
    FR("Opacité en dessous de laquelle recycler"),
    ES("Opacidad por debajo de la cual reciclar"),
    PT("Opacidade abaixo da qual reciclar"),
    IT("Opacità sotto cui riciclare"),
    NL("Dekking waaronder wordt hergebruikt"),
    RU("Непрозрачность, ниже которой сплат перерабатывается"),
    TR("Altında geri dönüştürülecek saydamsızlık"));
SS_MSG(min_opacity_help,
    EN("Splats fainter than this get recycled into places that need them. Raising "
       "it prunes harder and keeps the splat budget on visible surfaces."),
    JA("これより薄いスプラットは、必要な場所へ回されます。上げるほど強く刈り込"
       "まれ、スプラットの予算が見える面に集まります。"),
    ZH_HANS("比这更淡的泼溅会被回收到需要的位置。数值越高修剪越狠，泼溅预算更"
            "集中在可见表面上。"),
    ZH_HANT("比這更淡的潑濺會被回收到需要的位置。數值越高修剪越狠，潑濺預算更"
            "集中在可見表面上。"),
    KO("이보다 옅은 스플랫은 필요한 곳으로 재배치됩니다. 값을 올릴수록 더 세게"
       " 걸러내어 스플랫 예산이 보이는 면에 모입니다."),
    DE("Splats, die blasser als dieser Wert sind, werden dorthin recycelt, wo "
       "sie gebraucht werden. Erhöhen beschneidet härter und hält das Splat-Budget "
       "auf sichtbaren Flächen."),
    FR("Les splats plus pâles que cette valeur sont recyclés là où ils manquent. "
       "L'augmenter élague plus fort et concentre le budget de splats sur les "
       "surfaces visibles."),
    ES("Los splats más tenues que este valor se reciclan hacia donde hacen falta. "
       "Subirlo poda con más fuerza y concentra el presupuesto de splats en las "
       "superficies visibles."),
    PT("Os splats mais fracos que este valor são reciclados para onde fazem falta. "
       "Aumentá-lo poda com mais força e concentra o orçamento de splats nas "
       "superfícies visíveis."),
    IT("Gli splat più deboli di questo valore vengono riciclati dove servono. "
       "Alzarlo pota più forte e concentra il budget di splat sulle superfici "
       "visibili."),
    NL("Splats die vager zijn dan deze waarde worden hergebruikt waar ze nodig "
       "zijn. Verhogen snoeit harder en houdt het splatbudget op zichtbare oppervlakken."),
    RU("Сплаты бледнее этого значения перерабатываются туда, где нужны. Повышение "
       "обрезает жёстче и удерживает бюджет сплатов на видимых поверхностях."),
    TR("Bundan daha soluk splat'lar gereken yerlere geri dönüştürülür. Yükseltmek "
       "daha sert budar ve splat bütçesini görünen yüzeylerde tutar."));

SS_MSG(refine_every,
    EN("Refinement interval"), JA("スプラット追加の間隔"),
    ZH_HANS("细化间隔"), ZH_HANT("細化間隔"), KO("정제 간격"),
    DE("Verfeinerungsintervall"), FR("Intervalle de raffinement"),
    ES("Intervalo de refinamiento"), PT("Intervalo de refinamento"),
    IT("Intervallo di raffinamento"), NL("Verfijningsinterval"),
    RU("Интервал уточнения"), TR("İyileştirme aralığı"));
SS_MSG(refine_every_help,
    EN("How many steps between rounds of adding and relocating splats. Smaller "
       "reacts to missing detail sooner; larger is calmer and slightly cheaper."),
    JA("スプラットを増やしたり移したりするラウンドの間隔をステップ数で指定しま"
       "す。短いほど足りない細部に早く反応し、長いほど落ち着いていて、わずかに"
       "軽くなります。"),
    ZH_HANS("两轮新增和迁移泼溅之间相隔多少步。间隔越短，对缺失细节反应越快；"
            "越长则更平稳，也略微省一点开销。"),
    ZH_HANT("兩輪新增和遷移潑濺之間相隔多少步。間隔越短，對缺失細節反應越快；"
            "越長則更平穩，也略微省一點開銷。"),
    KO("스플랫을 늘리고 옮기는 회차 사이의 간격을 스텝 수로 지정합니다. 짧으면"
       " 부족한 디테일에 더 빨리 반응하고, 길면 더 차분하며 비용도 약간 적습니"
       "다."),
    DE("Wie viele Schritte zwischen den Runden liegen, in denen Splats hinzukommen "
       "und umziehen. Kleiner reagiert früher auf fehlendes Detail; größer ist "
       "ruhiger und etwas billiger."),
    FR("Combien d'étapes séparent les cycles d'ajout et de déplacement de splats. "
       "Plus petit réagit plus tôt au détail manquant ; plus grand est plus calme "
       "et légèrement moins coûteux."),
    ES("Cuántos pasos median entre las rondas de añadir y reubicar splats. Menor "
       "reacciona antes al detalle que falta; mayor es más tranquilo y algo más "
       "barato."),
    PT("Quantos passos separam as rodadas de acrescentar e realocar splats. Menor "
       "reage mais cedo ao detalhe que falta; maior é mais calmo e um pouco mais "
       "barato."),
    IT("Quanti passi separano i cicli di aggiunta e spostamento degli splat. "
       "Più piccolo reagisce prima al dettaglio mancante; più grande è più calmo "
       "e leggermente più economico."),
    NL("Hoeveel stappen er zitten tussen de rondes waarin splats worden toegevoegd "
       "en verplaatst. Kleiner reageert eerder op ontbrekend detail; groter is "
       "rustiger en iets goedkoper."),
    RU("Сколько шагов между раундами добавления и перемещения сплатов. Меньше "
       "— быстрее реакция на нехватку деталей; больше — спокойнее и чуть дешевле."),
    TR("Splat ekleme ve taşıma turları arasında kaç adım olduğu. Küçük değerler "
       "eksik ayrıntıya daha erken tepki verir; büyük değerler daha sakin ve "
       "biraz daha ucuzdur."));

SS_MSG(refine_start_iter,
    EN("Refinement start"), JA("スプラット追加の開始"),
    ZH_HANS("细化起始步"), ZH_HANT("細化起始步"), KO("정제 시작 스텝"),
    DE("Beginn der Verfeinerung"), FR("Début du raffinement"),
    ES("Inicio del refinamiento"), PT("Início do refinamento"),
    IT("Inizio del raffinamento"), NL("Start van de verfijning"),
    RU("Начало уточнения"), TR("İyileştirmenin başlangıcı"));
SS_MSG(refine_start_iter_help,
    EN("Step at which splats first start being added. Waiting a little lets the "
       "initial splats settle before the count starts growing."),
    JA("スプラットが追加され始めるステップです。少し待つことで、数が増え始める"
       "前に最初のスプラットが落ち着きます。"),
    ZH_HANS("开始新增泼溅的步数。稍等一会儿，可以让最初的泼溅先稳定下来，再让"
            "数量增长。"),
    ZH_HANT("開始新增潑濺的步數。稍等一會兒，可以讓最初的潑濺先穩定下來，再讓"
            "數量增長。"),
    KO("스플랫이 처음 추가되기 시작하는 스텝입니다. 조금 기다리면 수가 늘기 전"
       "에 초기 스플랫이 자리를 잡습니다."),
    DE("Schritt, ab dem erstmals Splats hinzukommen. Ein wenig zu warten lässt "
       "die anfänglichen Splats sich setzen, bevor die Zahl zu wachsen beginnt."),
    FR("Étape à laquelle les splats commencent à être ajoutés. Attendre un peu "
       "laisse les splats initiaux se poser avant que le nombre ne se mette à "
       "croître."),
    ES("Paso en el que empiezan a añadirse splats. Esperar un poco deja que los "
       "splats iniciales se asienten antes de que el número empiece a crecer."),
    PT("Passo em que os splats começam a ser acrescentados. Esperar um pouco "
       "deixa os splats iniciais assentarem antes de o número começar a crescer."),
    IT("Passo in cui gli splat iniziano a essere aggiunti. Aspettare un poco "
       "lascia assestare gli splat iniziali prima che il numero cominci a crescere."),
    NL("Stap waarop splats voor het eerst worden toegevoegd. Even wachten laat "
       "de eerste splats bezinken voordat het aantal begint te groeien."),
    RU("Шаг, с которого начинают добавляться сплаты. Небольшая пауза даёт первым "
       "сплатам устояться, прежде чем их число начнёт расти."),
    TR("Splat eklemenin başladığı adım. Biraz beklemek, sayı artmaya başlamadan "
       "önce ilk splat'ların oturmasını sağlar."));

SS_MSG(refine_stop_num_iter,
    EN("Stop refining before the end"),
    JA("終了何ステップ前に追加をやめるか"), ZH_HANS("结束前多少步停止细化"),
    ZH_HANT("結束前多少步停止細化"), KO("종료 몇 스텝 전에 정제 중단"),
    DE("Verfeinerung so viele Schritte vor Schluss beenden"),
    FR("Arrêt du raffinement avant la fin"),
    ES("Detener el refinamiento antes del final"),
    PT("Parar o refinamento antes do fim"),
    IT("Fermare il raffinamento prima della fine"),
    NL("Verfijning zoveel stappen voor het eind stoppen"),
    RU("Прекратить уточнение за столько шагов до конца"),
    TR("Bitişten kaç adım önce iyileştirmeyi durdur"));
SS_MSG(refine_stop_num_iter_help,
    EN("Stop adding splats this many steps before the end. The remaining steps "
       "polish what already exists instead of introducing new splats that never "
       "get refined."),
    JA("終わりのこのステップ数だけ手前で、スプラットの追加をやめます。残りのス"
       "テップは、二度と磨かれない新しいスプラットを入れる代わりに、すでにある"
       "ものを仕上げるのに使われます。"),
    ZH_HANS("在结束前这么多步停止新增泼溅。剩下的步数用来打磨已有的泼溅，而不"
            "是引入永远得不到细化的新泼溅。"),
    ZH_HANT("在結束前這麼多步停止新增潑濺。剩下的步數用來打磨已有的潑濺，而不"
            "是引入永遠得不到細化的新潑濺。"),
    KO("끝나기 이만큼의 스텝 앞에서 스플랫 추가를 멈춥니다. 남은 스텝은 다듬어"
       "지지도 못할 새 스플랫을 넣는 대신 이미 있는 것을 마무리하는 데 쓰입니"
       "다."),
    DE("So viele Schritte vor Schluss keine Splats mehr hinzufügen. Die verbleibenden "
       "Schritte polieren das Vorhandene, statt neue Splats einzuführen, die "
       "nie verfeinert werden."),
    FR("Cesser d'ajouter des splats ce nombre d'étapes avant la fin. Les étapes "
       "restantes peaufinent ce qui existe déjà au lieu d'introduire des splats "
       "qui ne seront jamais affinés."),
    ES("Dejar de añadir splats este número de pasos antes del final. Los pasos "
       "restantes pulen lo que ya existe en vez de introducir splats que nunca "
       "llegarían a refinarse."),
    PT("Parar de acrescentar splats este número de passos antes do fim. Os passos "
       "restantes polem o que já existe em vez de introduzir splats que nunca "
       "seriam refinados."),
    IT("Smettere di aggiungere splat questo numero di passi prima della fine. "
       "I passi restanti rifiniscono ciò che esiste già invece di introdurre "
       "splat che non verrebbero mai raffinati."),
    NL("Zoveel stappen voor het eind stoppen met splats toevoegen. De resterende "
       "stappen polijsten wat er al is in plaats van nieuwe splats toe te voegen "
       "die nooit verfijnd worden."),
    RU("Прекращать добавление сплатов за столько шагов до конца. Оставшиеся шаги "
       "доводят уже существующее, а не вводят сплаты, которые никто не успеет "
       "уточнить."),
    TR("Bitişten bu kadar adım önce splat eklemeyi bırakır. Kalan adımlar, hiç "
       "iyileştirilmeyecek yeni splat'lar eklemek yerine var olanı cilalar."));

SS_MSG(refine_stop_iter,
    EN("Earliest refinement stop"), JA("追加をやめる最も早いステップ"),
    ZH_HANS("最早停止细化的步数"), ZH_HANT("最早停止細化的步數"),
    KO("정제를 멈출 수 있는 최소 스텝"),
    DE("Frühester Stopp der Verfeinerung"),
    FR("Arrêt du raffinement au plus tôt"),
    ES("Parada más temprana del refinamiento"),
    PT("Parada mais cedo do refinamento"),
    IT("Arresto più precoce del raffinamento"),
    NL("Vroegste stop van de verfijning"),
    RU("Самая ранняя остановка уточнения"),
    TR("İyileştirmenin en erken durma adımı"));
SS_MSG(refine_stop_iter_help,
    EN("Earliest step at which splat growth may stop. Growth ends at whichever "
       "comes later, this step or refine_stop_num_iter before the end, so short "
       "runs still get to add splats at all."),
    JA("スプラットの増加を止められるいちばん早いステップです。増加が終わるのは、"
       "このステップか、終了 refine_stop_num_iter 手前のどちらか遅いほうなので、"
       "短い実行でもスプラットを増やす機会が残ります。"),
    ZH_HANS("泼溅增长最早可以停止的步数。增长会在这一步和“结束前 refine_stop_num_iter "
            "步”两者中较晚的那个时刻结束，因此短训练也仍有机会新增泼溅。"),
    ZH_HANT("潑濺增長最早可以停止的步數。增長會在這一步和「結束前 refine_stop_num_iter "
            "步」兩者中較晚的那個時刻結束，因此短訓練也仍有機會新增潑濺。"),
    KO("스플랫 증가를 멈출 수 있는 가장 이른 스텝입니다. 증가는 이 스텝과 '끝"
       "나기 refine_stop_num_iter 스텝 전' 중 더 늦은 쪽에서 끝나므로, 짧은 실"
       "행에서도 스플랫을 늘릴 기회가 남습니다."),
    DE("Frühester Schritt, an dem das Splat-Wachstum enden darf. Es endet zum "
       "späteren der beiden Zeitpunkte: diesem Schritt oder refine_stop_num_iter "
       "vor Schluss, sodass auch kurze Läufe überhaupt Splats hinzufügen können."),
    FR("Étape la plus précoce à laquelle la croissance des splats peut s'arrêter. "
       "Elle s'arrête au plus tardif des deux : cette étape, ou refine_stop_num_iter "
       "avant la fin, si bien que les courtes exécutions ont quand même le temps "
       "d'ajouter des splats."),
    ES("Paso más temprano en el que puede detenerse el crecimiento de splats. "
       "Termina en el más tardío de los dos: este paso o refine_stop_num_iter "
       "antes del final, de modo que las ejecuciones cortas aún llegan a añadir "
       "splats."),
    PT("Passo mais cedo em que o crescimento de splats pode parar. Ele termina "
       "no mais tardio dos dois: este passo ou refine_stop_num_iter antes do "
       "fim, de modo que execuções curtas ainda chegam a acrescentar splats."),
    IT("Passo più precoce in cui la crescita degli splat può fermarsi. Finisce "
       "al più tardo tra i due: questo passo oppure refine_stop_num_iter prima "
       "della fine, così anche le esecuzioni brevi fanno in tempo ad aggiungere "
       "splat."),
    NL("Vroegste stap waarop de splatgroei mag stoppen. Ze eindigt op het latere "
       "van de twee: deze stap of refine_stop_num_iter voor het eind, zodat korte "
       "runs toch splats kunnen toevoegen."),
    RU("Самый ранний шаг, на котором рост сплатов может прекратиться. Рост заканчивается "
       "на более позднем из двух: этом шаге или refine_stop_num_iter до конца, "
       "так что даже короткие запуски успевают добавить сплаты."),
    TR("Splat büyümesinin durabileceği en erken adım. Büyüme, bu adım ile bitişten "
       "refine_stop_num_iter adım önce olan andan geç olanında sona erer; böylece "
       "kısa çalıştırmalar da splat ekleyebilir."));

SS_MSG(noise_lr,
    EN("Position jitter"), JA("位置のゆらぎ"), ZH_HANS("位置抖动"),
    ZH_HANT("位置抖動"), KO("위치 흔들림"), DE("Positionsrauschen"),
    FR("Agitation des positions"), ES("Sacudida de las posiciones"),
    PT("Tremor das posições"), IT("Perturbazione delle posizioni"),
    NL("Positieruis"), RU("Дрожание позиций"), TR("Konum titreşimi"));
SS_MSG(noise_lr_help,
    EN("How much random jitter is applied to splat positions early in training. "
       "Jitter helps splats escape bad spots and spread into unfilled areas; "
       "too much of it blurs detail."),
    JA("学習の序盤に、スプラットの位置へどれだけランダムなゆらぎを加えるかです。"
       "ゆらぎはスプラットが悪い場所から抜け出して未充填の場所へ広がるのを助け"
       "ますが、多すぎると細部がぼやけます。"),
    ZH_HANS("训练初期给泼溅位置施加多少随机抖动。抖动能帮助泼溅逃离不好的位置、"
            "扩散到未填充的区域，但过多会让细节发糊。"),
    ZH_HANT("訓練初期給潑濺位置施加多少隨機抖動。抖動能幫助潑濺逃離不好的位置、"
            "擴散到未填充的區域，但過多會讓細節發糊。"),
    KO("학습 초반에 스플랫 위치에 얼마나 무작위 흔들림을 줄지입니다. 흔들림은"
       " 스플랫이 나쁜 자리에서 빠져나와 빈 곳으로 퍼지도록 돕지만, 지나치면 "
       "디테일이 뭉개집니다."),
    DE("Wie viel zufälliges Zittern früh im Training auf die Splat-Positionen "
       "gelegt wird. Zittern hilft Splats, schlechte Stellen zu verlassen und "
       "sich in ungefüllte Bereiche auszubreiten; zu viel davon verwischt Detail."),
    FR("Quelle agitation aléatoire est appliquée aux positions des splats en "
       "début d'entraînement. L'agitation aide les splats à quitter les mauvais "
       "endroits et à gagner les zones non remplies ; trop en floute le détail."),
    ES("Cuánta sacudida aleatoria se aplica a las posiciones de los splats al "
       "principio del entrenamiento. La sacudida ayuda a los splats a salir de "
       "malos sitios y extenderse a zonas sin rellenar; demasiada emborrona el "
       "detalle."),
    PT("Quanto tremor aleatório é aplicado às posições dos splats no início do "
       "treinamento. O tremor ajuda os splats a sair de lugares ruins e se espalhar "
       "por áreas não preenchidas; em excesso borra o detalhe."),
    IT("Quanta perturbazione casuale viene applicata alle posizioni degli splat "
       "all'inizio dell'addestramento. La perturbazione aiuta gli splat a uscire "
       "dai punti sbagliati e a diffondersi nelle zone non riempite; troppa sfoca "
       "il dettaglio."),
    NL("Hoeveel willekeurige trilling er vroeg in de training op de splatposities "
       "wordt gezet. Trilling helpt splats slechte plekken te verlaten en zich "
       "over ongevulde gebieden te verspreiden; te veel maakt detail wazig."),
    RU("Сколько случайного дрожания добавляется к позициям сплатов в начале обучения. "
       "Дрожание помогает сплатам выбираться из неудачных мест и расходиться "
       "в незаполненные области; избыток размывает детали."),
    TR("Eğitimin başında splat konumlarına ne kadar rastgele titreşim uygulandığı. "
       "Titreşim splat'ların kötü noktalardan kurtulup dolmamış alanlara yayılmasına "
       "yardım eder; fazlası ayrıntıyı bulanıklaştırır."));

SS_MSG(noise_lr_final,
    EN("Final position jitter"), JA("最後の位置のゆらぎ"),
    ZH_HANS("最终位置抖动"), ZH_HANT("最終位置抖動"), KO("최종 위치 흔들림"),
    DE("Positionsrauschen am Ende"), FR("Agitation finale des positions"),
    ES("Sacudida final de las posiciones"), PT("Tremor final das posições"),
    IT("Perturbazione finale delle posizioni"),
    NL("Positieruis aan het eind"), RU("Итоговое дрожание позиций"),
    TR("Son konum titreşimi"));
SS_MSG(noise_lr_final_help,
    EN("How much position jitter is left at the end of training. Lower lets detail "
       "settle and sharpen over the final steps."),
    JA("学習の終わりに位置のゆらぎをどれだけ残すかです。下げるほど最後の数ステ"
       "ップで細部が落ち着き、くっきりします。"),
    ZH_HANS("训练结束时还保留多少位置抖动。数值越低，最后几步里细节越能稳定下"
            "来、变得清晰。"),
    ZH_HANT("訓練結束時還保留多少位置抖動。數值越低，最後幾步裡細節越能穩定下"
            "來、變得清晰。"),
    KO("학습이 끝날 때 위치 흔들림을 얼마나 남길지입니다. 낮출수록 마지막 몇 "
       "스텝에서 디테일이 자리 잡고 또렷해집니다."),
    DE("Wie viel Positionszittern am Ende des Trainings übrig bleibt. Niedriger "
       "lässt Detail über die letzten Schritte zur Ruhe kommen und schärfer werden."),
    FR("Quelle agitation de position subsiste à la fin de l'entraînement. Plus "
       "bas laisse le détail se poser et se préciser sur les dernières étapes."),
    ES("Cuánta sacudida de posición queda al final del entrenamiento. Más bajo "
       "deja que el detalle se asiente y se afile en los últimos pasos."),
    PT("Quanto tremor de posição resta no fim do treinamento. Mais baixo deixa "
       "o detalhe assentar e ganhar nitidez nos últimos passos."),
    IT("Quanta perturbazione di posizione resta alla fine dell'addestramento. "
       "Più basso lascia che il dettaglio si assesti e si affini negli ultimi "
       "passi."),
    NL("Hoeveel positietrilling er aan het eind van de training overblijft. Lager "
       "laat detail in de laatste stappen bezinken en scherper worden."),
    RU("Сколько дрожания позиций остаётся к концу обучения. Меньше — детали успевают "
       "устояться и стать резче на последних шагах."),
    TR("Eğitimin sonunda ne kadar konum titreşimi kaldığı. Düşük değerler son "
       "adımlarda ayrıntının oturup keskinleşmesini sağlar."));

SS_MSG(use_revised_densification,
    EN("Revised densification rule"), JA("改良版の増やし方を使う"),
    ZH_HANS("使用改进的加密规则"), ZH_HANT("使用改良的加密規則"),
    KO("개선된 밀집화 규칙 사용"), DE("Überarbeitete Verdichtungsregel"),
    FR("Règle de densification révisée"),
    ES("Regla de densificación revisada"),
    PT("Regra de densificação revisada"),
    IT("Regola di densificazione rivista"), NL("Herziene verdichtingsregel"),
    RU("Уточнённое правило уплотнения"),
    TR("Gözden geçirilmiş yoğunlaştırma kuralı"));
SS_MSG(use_revised_densification_help,
    EN("Use the improved rule for deciding where new splats go. It usually recovers "
       "missing detail faster; turn off to match the original method."),
    JA("新しいスプラットの置き場所を決める、改良された規則を使います。ふつうは"
       "足りない細部を早く取り戻せます。オリジナルの方法に合わせたいときはオフ"
       "にしてください。"),
    ZH_HANS("采用改进的规则来决定新泼溅放在哪里。通常能更快补回缺失的细节；想"
            "复现原始方法就关闭它。"),
    ZH_HANT("採用改良的規則來決定新潑濺放在哪裡。通常能更快補回缺失的細節；想"
            "重現原始方法就關閉它。"),
    KO("새 스플랫을 어디에 놓을지 정하는 개선된 규칙을 씁니다. 보통 부족한 디"
       "테일을 더 빨리 되찾습니다. 원래 방식과 맞추려면 끄십시오."),
    DE("Die verbesserte Regel dafür verwenden, wohin neue Splats kommen. Sie "
       "holt fehlendes Detail meist schneller zurück; abschalten, um dem ursprünglichen "
       "Verfahren zu entsprechen."),
    FR("Utiliser la règle améliorée qui décide où vont les nouveaux splats. Elle "
       "récupère en général plus vite le détail manquant ; décocher pour retrouver "
       "la méthode d'origine."),
    ES("Usar la regla mejorada que decide dónde van los splats nuevos. Suele "
       "recuperar antes el detalle que falta; desactívela para igualar el método "
       "original."),
    PT("Usar a regra melhorada que decide para onde vão os splats novos. Costuma "
       "recuperar mais depressa o detalhe que falta; desligue para igualar o "
       "método original."),
    IT("Usare la regola migliorata che decide dove vanno i nuovi splat. Di solito "
       "recupera più in fretta il dettaglio mancante; disattivare per riprodurre "
       "il metodo originale."),
    NL("De verbeterde regel gebruiken die bepaalt waar nieuwe splats komen. Die "
       "haalt ontbrekend detail meestal sneller terug; zet uit om de oorspronkelijke "
       "methode te evenaren."),
    RU("Использовать улучшенное правило выбора мест для новых сплатов. Обычно "
       "оно быстрее возвращает недостающие детали; выключите, чтобы совпасть "
       "с исходным методом."),
    TR("Yeni splat'ların nereye gideceğine karar veren geliştirilmiş kuralı kullanır. "
       "Eksik ayrıntıyı genelde daha çabuk geri getirir; özgün yöntemle eşleşmek "
       "için kapatın."));

SS_MSG(densify_score_mode,
    EN("Detail score mode"), JA("詳細スコアの取り方"),
    ZH_HANS("细节评分方式"), ZH_HANT("細節評分方式"), KO("디테일 점수 방식"),
    DE("Verfahren für den Detailwert"), FR("Mode de score de détail"),
    ES("Modo de puntuación de detalle"), PT("Modo de pontuação de detalhe"),
    IT("Modalità del punteggio di dettaglio"),
    NL("Modus voor de detailscore"),
    RU("Способ подсчёта потребности в детализации"), TR("Ayrıntı puanı kipi"));
SS_MSG(densify_score_mode_help,
    EN("How a splat's need-more-detail score builds up over time. `mean` is the "
       "balanced default, `max` reacts to a single bad view, `median` ignores "
       "the occasional odd view and helps when people or cars move through the "
       "scene, and `geom` sits between mean and median."),
    JA("スプラットの「もっと細かくすべき」度合いを、時間をかけてどう積み上げる"
       "かです。`mean` はバランスのとれた既定値、`max` は一枚でも悪い視点があ"
       "れば反応し、`median` はときどき混じる異常な視点を無視するので人や車が"
       "通るシーンに向き、`geom` は mean と median の中間です。"),
    ZH_HANS("泼溅的“需要更多细节”评分如何随时间累积。`mean` 是均衡的默认值；`max` "
            "只要有一个视角变差就会反应；`median` 会忽略偶尔的异常视角，适合有"
            "行人或车辆经过的场景；`geom` 介于 mean 和 median 之间。"),
    ZH_HANT("潑濺的「需要更多細節」評分如何隨時間累積。`mean` 是均衡的預設值；"
            "`max` 只要有一個視角變差就會反應；`median` 會忽略偶爾的異常視角，"
            "適合有行人或車輛經過的場景；`geom` 介於 mean 和 median 之間。"),
    KO("스플랫의 \"더 세밀해야 한다\" 점수를 시간에 따라 어떻게 쌓을지입니다. "
       "`mean`은 균형 잡힌 기본값, `max`는 한 장이라도 나쁜 시점이 있으면 반응"
       "하고, `median`은 가끔 섞이는 이상한 시점을 무시해 사람이나 차가 지나가"
       "는 장면에 알맞으며, `geom`은 mean과 median의 중간입니다."),
    DE("Wie sich der Wert „hier fehlt Detail“ eines Splats über die Zeit aufbaut. "
       "`mean` ist der ausgewogene Standard, `max` reagiert schon auf eine einzige "
       "schlechte Ansicht, `median` ignoriert gelegentliche Ausreißer und hilft, "
       "wenn Personen oder Autos durch die Szene laufen, und `geom` liegt zwischen "
       "mean und median."),
    FR("Comment le score « il manque du détail » d'un splat s'accumule au fil "
       "du temps. `mean` est le défaut équilibré, `max` réagit à une seule mauvaise "
       "vue, `median` ignore la vue aberrante occasionnelle et aide quand des "
       "passants ou des voitures traversent la scène, et `geom` se situe entre "
       "mean et median."),
    ES("Cómo se acumula con el tiempo la puntuación de «aquí falta detalle» de "
       "un splat. `mean` es el valor equilibrado por defecto, `max` reacciona "
       "a una sola vista mala, `median` ignora la vista rara ocasional y ayuda "
       "cuando pasan personas o coches por la escena, y `geom` queda entre mean "
       "y median."),
    PT("Como a pontuação de «falta detalhe aqui» de um splat se acumula ao longo "
       "do tempo. `mean` é o padrão equilibrado, `max` reage a uma única vista "
       "ruim, `median` ignora a vista estranha ocasional e ajuda quando pessoas "
       "ou carros passam pela cena, e `geom` fica entre mean e median."),
    IT("Come il punteggio «qui manca dettaglio» di uno splat si accumula nel "
       "tempo. `mean` è il valore predefinito equilibrato, `max` reagisce a una "
       "sola vista cattiva, `median` ignora la vista anomala occasionale e aiuta "
       "quando persone o automobili attraversano la scena, e `geom` sta tra mean "
       "e median."),
    NL("Hoe de score «hier ontbreekt detail» van een splat zich in de tijd opbouwt. "
       "`mean` is de evenwichtige standaard, `max` reageert op één enkel slecht "
       "beeld, `median` negeert het incidentele vreemde beeld en helpt als mensen "
       "of auto's door de scène lopen, en `geom` zit tussen mean en median in."),
    RU("Как со временем накапливается оценка «здесь не хватает деталей». `mean` "
       "— сбалансированное значение по умолчанию, `max` реагирует на один плохой "
       "вид, `median` пропускает случайные странные виды и помогает, когда по "
       "сцене идут люди или машины, а `geom` находится между mean и median."),
    TR("Bir splat'ın «burada ayrıntı eksik» puanının zamanla nasıl biriktiği. "
       "`mean` dengeli varsayılandır, `max` tek bir kötü görünüme bile tepki "
       "verir, `median` ara sıra çıkan tuhaf görünümleri yok sayar ve sahneden "
       "insan ya da araba geçtiğinde işe yarar, `geom` ise mean ile median arasındadır."));

SS_MSG(densify_score_blend_world_grad,
    EN("Size-based scoring share"), JA("大きさによる評価の割合"),
    ZH_HANS("按体积评分的比重"), ZH_HANT("按體積評分的比重"),
    KO("크기 기반 점수 비중"), DE("Anteil der größenbasierten Bewertung"),
    FR("Part du score fondé sur la taille"),
    ES("Peso de la puntuación por tamaño"),
    PT("Peso da pontuação por tamanho"),
    IT("Peso del punteggio basato sulla dimensione"),
    NL("Aandeel van de scoring op grootte"), RU("Доля оценки по размеру"),
    TR("Boyuta dayalı puanlamanın payı"));
SS_MSG(densify_score_blend_world_grad_help,
    EN("Balance between adding splats where the image looks wrong and where splats "
       "are physically large. Raise toward 1 to spend more splats on big distant "
       "structures that image-based scoring tends to starve; 0 uses image error "
       "alone."),
    JA("画像が合っていない場所にスプラットを足すか、物理的に大きなスプラットに"
       "足すかの釣り合いです。1 に近づけるほど、画像基準の評価では見落とされが"
       "ちな遠くの大きな構造にスプラットを回します。0 なら画像の誤差だけで決め"
       "ます。"),
    ZH_HANS("在“图像不匹配的地方”和“泼溅本身很大的地方”之间取得平衡。越接近 1，"
            "越多泼溅会分给基于图像的评分容易饿着的远处大结构；0 则只看图像误"
            "差。"),
    ZH_HANT("在「影像不吻合的地方」和「潑濺本身很大的地方」之間取得平衡。越接"
            "近 1，越多潑濺會分給基於影像的評分容易餓著的遠處大結構；0 則只看"
            "影像誤差。"),
    KO("이미지가 어긋난 곳에 스플랫을 더할지, 물리적으로 큰 스플랫에 더할지의"
       " 균형입니다. 1에 가까울수록 이미지 기반 점수가 놓치기 쉬운 멀고 큰 구"
       "조에 스플랫을 더 씁니다. 0이면 이미지 오차만 봅니다."),
    DE("Das Gleichgewicht zwischen „dort ergänzen, wo das Bild falsch aussieht“ "
       "und „dort, wo Splats physisch groß sind“. Richtung 1 fließen mehr Splats "
       "in große entfernte Strukturen, die bildbasierte Bewertung gern aushungert; "
       "0 nutzt allein den Bildfehler."),
    FR("L'équilibre entre ajouter des splats là où l'image est fausse et là où "
       "les splats sont physiquement grands. Vers 1, davantage de splats vont "
       "aux grandes structures lointaines que le score fondé sur l'image a tendance "
       "à affamer ; 0 n'utilise que l'erreur d'image."),
    ES("El equilibrio entre añadir splats donde la imagen falla y donde los splats "
       "son físicamente grandes. Hacia 1 se dedican más splats a las estructuras "
       "grandes y lejanas que la puntuación basada en imagen suele desatender; "
       "0 usa solo el error de imagen."),
    PT("O equilíbrio entre acrescentar splats onde a imagem está errada e onde "
       "os splats são fisicamente grandes. Rumo a 1, mais splats vão para as "
       "grandes estruturas distantes que a pontuação baseada em imagem tende "
       "a deixar de lado; 0 usa apenas o erro de imagem."),
    IT("L'equilibrio tra aggiungere splat dove l'immagine è sbagliata e dove "
       "gli splat sono fisicamente grandi. Verso 1 vanno più splat alle grandi "
       "strutture lontane che il punteggio basato sull'immagine tende a trascurare; "
       "0 usa solo l'errore d'immagine."),
    NL("De balans tussen splats toevoegen waar het beeld niet klopt en waar splats "
       "fysiek groot zijn. Richting 1 gaan meer splats naar grote structuren "
       "in de verte die op beeld gebaseerde scoring vaak overslaat; 0 gebruikt "
       "alleen de beeldfout."),
    RU("Баланс между «добавлять сплаты там, где картинка не сходится» и «там, "
       "где сплаты физически велики». Ближе к 1 больше сплатов уходит на крупные "
       "далёкие структуры, которые оценка по изображению обычно обделяет; 0 использует "
       "только ошибку изображения."),
    TR("Splat'ları görüntünün yanlış göründüğü yere mi yoksa splat'ların fiziksel "
       "olarak büyük olduğu yere mi eklemek gerektiği arasındaki denge. 1'e doğru "
       "gidildikçe, görüntüye dayalı puanlamanın aç bıraktığı büyük uzak yapılara "
       "daha çok splat ayrılır; 0 yalnızca görüntü hatasını kullanır."));

SS_MSG(densify_loss_map_mode,
    EN("Detail error measure"), JA("誤差の測り方"), ZH_HANS("误差度量方式"),
    ZH_HANT("誤差度量方式"), KO("오차 측정 방식"),
    DE("Fehlermaß für neue Splats"),
    FR("Mesure d'erreur pour la densification"),
    ES("Medida de error para la densificación"),
    PT("Medida de erro para a densificação"),
    IT("Misura dell'errore per la densificazione"),
    NL("Foutmaat voor de verdichting"), RU("Мера ошибки для уплотнения"),
    TR("Yoğunlaştırma için hata ölçüsü"));
SS_MSG(densify_loss_map_mode_help,
    EN("What kind of error decides where new splats are added. `ssim_structure` "
       "targets mismatched patterns and edges while ignoring brightness differences. "
       "`ssim_full`, `ssim_cs` and `loss_full` fold in progressively more of "
       "the raw color error. `edge_aware` chases edges in the reference photos "
       "whether or not they are already reconstructed well. `robust_edge_aware` "
       "does the same but ignores the worst-matching pixels, so moving people "
       "and cars do not attract splats. `none` spreads new splats evenly. Any mode with `_nms` first thins the "
       "error down to its ridge lines, so splats land on the sharpest edge rather than spreading across the blur around it."),
    JA("新しいスプラットをどこに足すかを、どの種類の誤差で決めるかです。`ssim_structure` "
       "は明るさの違いを無視して、模様やエッジの食い違いを狙います。`ssim_full`、"
       "`ssim_cs`、`loss_full` の順に、生の色の誤差をより多く取り込みます。`edge_aware` "
       "は、すでにうまく再現できているかどうかに関わらず、元写真のエッジを追い"
       "ます。`robust_edge_aware` は同じですが、いちばん合っていない画素を無視"
       "するので、動く人や車にスプラットが集まりません。`none` は新しいスプラ"
       "ットを均等に散らします。`_nms` の付いたモードは、まず誤差を稜線だけに"
       "細らせるので、ぼやけた広がりではなく、いちばん鋭いところにスプラットが乗ります。"),
    ZH_HANS("用哪种误差决定在哪里新增泼溅。`ssim_structure` 忽略明暗差异，专门"
            "盯住图案和边缘的不一致。`ssim_full`、`ssim_cs`、`loss_full` 依次"
            "纳入更多原始颜色误差。`edge_aware` 追踪参考照片中的边缘，无论那里"
            "是否已经重建得很好。`robust_edge_aware` 与之相同，但忽略最不匹配"
            "的像素，因此移动的行人和车辆不会吸引泼溅。`none` 则把新泼溅均匀铺"
            "开。带 `_nms` 的模式会先把误差收细成一条线，因此泼溅落在最清晰的边缘上，"
            "而不是散布在它周围的模糊里。"),
    ZH_HANT("用哪種誤差決定在哪裡新增潑濺。`ssim_structure` 忽略明暗差異，專門"
            "盯住圖案和邊緣的不一致。`ssim_full`、`ssim_cs`、`loss_full` 依次"
            "納入更多原始顏色誤差。`edge_aware` 追蹤參考照片中的邊緣，無論那裡"
            "是否已經重建得很好。`robust_edge_aware` 與之相同，但忽略最不吻合"
            "的像素，因此移動的行人和車輛不會吸引潑濺。`none` 則把新潑濺均勻鋪"
            "開。帶 `_nms` 的模式會先把誤差收細成一條線，因此潑濺落在最清晰的邊緣上，"
            "而不是散布在它周圍的模糊裡。"),
    KO("새 스플랫을 어디에 추가할지 어떤 종류의 오차로 정할지입니다. `ssim_structure`는"
       " 밝기 차이를 무시하고 무늬와 가장자리의 불일치를 노립니다. `ssim_full`, "
       "`ssim_cs`, `loss_full` 순으로 원본 색 오차를 점점 더 많이 반영합니다. "
       "`edge_aware`는 이미 잘 복원됐는지와 상관없이 원본 사진의 가장자리를 좇"
       "습니다. `robust_edge_aware`는 같지만 가장 안 맞는 픽셀을 무시하므로 움"
       "직이는 사람과 차에 스플랫이 몰리지 않습니다. `none`은 새 스플랫을 고르"
       "게 흩뿌립니다. `_nms`가 붙은 방식은 오차를 먼저 능선만 남기고 얇게 깎아"
       "내므로, 흐릿하게 번진 곳이 아니라 가장 날카로운 가장자리에 스플랫이 놓입니다."),
    DE("Welche Art von Fehler entscheidet, wo neue Splats hinzukommen. `ssim_structure` "
       "zielt auf abweichende Muster und Kanten und ignoriert Helligkeitsunterschiede. "
       "`ssim_full`, `ssim_cs` und `loss_full` nehmen zunehmend mehr vom reinen "
       "Farbfehler auf. `edge_aware` verfolgt Kanten in den Vorlagefotos, gleich "
       "ob sie schon gut rekonstruiert sind. `robust_edge_aware` tut dasselbe, "
       "ignoriert aber die am schlechtesten passenden Pixel, sodass bewegte Personen "
       "und Autos keine Splats anziehen. `none` verteilt neue Splats gleichmäßig. Jeder Modus mit `_nms` dünnt den "
       "Fehler zuerst auf seine Gratlinien aus, sodass Splats auf der schärfsten Kante landen statt in der Unschärfe darum herum."),
    FR("Quel type d'erreur décide où ajouter de nouveaux splats. `ssim_structure` "
       "vise les motifs et les contours qui ne correspondent pas, en ignorant "
       "les écarts de luminosité. `ssim_full`, `ssim_cs` et `loss_full` intègrent "
       "progressivement plus d'erreur de couleur brute. `edge_aware` suit les "
       "contours des photos de référence, qu'ils soient déjà bien reconstruits "
       "ou non. `robust_edge_aware` fait de même mais ignore les pixels qui correspondent "
       "le moins, de sorte que passants et voitures n'attirent pas de splats. "
       "`none` répartit les nouveaux splats uniformément. Tout mode avec `_nms` "
       "amincit d'abord l'erreur jusqu'à ses lignes de crête, de sorte que les splats se posent sur le contour le plus net plutôt que dans le flou autour."),
    ES("Qué tipo de error decide dónde se añaden splats nuevos. `ssim_structure` "
       "apunta a los patrones y bordes que no coinciden e ignora las diferencias "
       "de brillo. `ssim_full`, `ssim_cs` y `loss_full` incorporan progresivamente "
       "más error de color puro. `edge_aware` persigue los bordes de las fotos "
       "de referencia, estén ya bien reconstruidos o no. `robust_edge_aware` "
       "hace lo mismo pero ignora los píxeles que peor coinciden, de modo que "
       "personas y coches en movimiento no atraen splats. `none` reparte los "
       "splats nuevos de manera uniforme. Cualquier modo con `_nms` adelgaza primero "
       "el error hasta sus líneas de cresta, de modo que los splats caen en el borde más nítido y no en el desenfoque que lo rodea."),
    PT("Que tipo de erro decide onde novos splats são acrescentados. `ssim_structure` "
       "mira os padrões e bordas que não coincidem e ignora diferenças de brilho. "
       "`ssim_full`, `ssim_cs` e `loss_full` incorporam progressivamente mais "
       "erro de cor puro. `edge_aware` persegue as bordas das fotos de referência, "
       "estejam ou não já bem reconstruídas. `robust_edge_aware` faz o mesmo, "
       "mas ignora os pixels que pior coincidem, de modo que pessoas e carros "
       "em movimento não atraem splats. `none` espalha os novos splats de forma "
       "uniforme. Qualquer modo com `_nms` afina primeiro o erro até às suas linhas "
       "de crista, de modo que os splats assentam na borda mais nítida e não no desfoque à volta dela."),
    IT("Che tipo di errore decide dove aggiungere nuovi splat. `ssim_structure` "
       "punta a motivi e bordi che non corrispondono e ignora le differenze di "
       "luminosità. `ssim_full`, `ssim_cs` e `loss_full` includono via via più "
       "errore di colore puro. `edge_aware` insegue i bordi nelle foto di riferimento, "
       "che siano già ricostruiti bene o no. `robust_edge_aware` fa lo stesso "
       "ma ignora i pixel che corrispondono peggio, così persone e automobili "
       "in movimento non attirano splat. `none` distribuisce i nuovi splat in "
       "modo uniforme. Ogni modalità con `_nms` assottiglia prima l'errore fino alle "
       "sue linee di cresta, così gli splat si posano sul bordo più nitido e non nella sfocatura attorno."),
    NL("Welk soort fout bepaalt waar nieuwe splats bij komen. `ssim_structure` "
       "mikt op patronen en randen die niet kloppen en negeert helderheidsverschillen. "
       "`ssim_full`, `ssim_cs` en `loss_full` nemen stapsgewijs meer van de ruwe "
       "kleurfout mee. `edge_aware` volgt randen in de referentiefoto's, of die "
       "nu al goed gereconstrueerd zijn of niet. `robust_edge_aware` doet hetzelfde "
       "maar negeert de slechtst passende pixels, zodat bewegende mensen en auto's "
       "geen splats aantrekken. `none` verdeelt nieuwe splats gelijkmatig. Elke modus met `_nms` dunt de fout "
       "eerst uit tot haar ribbellijnen, zodat splats op de scherpste rand landen en niet in de onscherpte eromheen."),
    RU("Какая именно ошибка решает, куда добавлять новые сплаты. `ssim_structure` "
       "целится в несовпадающие узоры и края и не смотрит на разницу яркости. "
       "`ssim_full`, `ssim_cs` и `loss_full` последовательно добавляют всё больше "
       "чистой ошибки цвета. `edge_aware` гонится за краями на исходных снимках "
       "независимо от того, восстановлены они уже хорошо или нет. `robust_edge_aware` "
       "делает то же, но пропускает хуже всего совпадающие пиксели, поэтому идущие "
       "люди и машины не притягивают сплаты. `none` распределяет новые сплаты "
       "равномерно. Любой режим с `_nms` сначала утончает ошибку до её гребней, "
       "поэтому сплаты ложатся на самый резкий край, а не расплываются вокруг него."),
    TR("Yeni splat'ların nereye ekleneceğine hangi tür hatanın karar vereceği. "
       "`ssim_structure` parlaklık farklarını yok sayar ve uyuşmayan desenlerle "
       "kenarları hedefler. `ssim_full`, `ssim_cs` ve `loss_full` sırayla ham "
       "renk hatasından daha fazlasını katar. `edge_aware`, iyi yeniden oluşturulmuş "
       "olsun olmasın kaynak fotoğraflardaki kenarların peşine düşer. `robust_edge_aware` "
       "aynısını yapar ama en kötü uyuşan pikselleri yok sayar; böylece hareket "
       "eden insanlar ve arabalar splat çekmez. `none` ise yeni splat'ları eşit "
       "dağıtır. `_nms` içeren her kip, hatayı önce sırt çizgilerine kadar inceltir; "
       "böylece splat'lar çevredeki bulanıklığa yayılmak yerine en keskin kenara oturur."));

SS_MSG(densify_robust_edge_aware_quantile,
    EN("Ignored worst-error share"), JA("無視する誤差上位の割合"),
    ZH_HANS("忽略的最差误差比例"), ZH_HANT("忽略的最差誤差比例"),
    KO("무시할 최악 오차 비율"),
    DE("Anteil des ignorierten größten Fehlers"),
    FR("Part de la pire erreur ignorée"),
    ES("Proporción del peor error que se ignora"),
    PT("Proporção do pior erro ignorada"),
    IT("Quota dell'errore peggiore ignorata"),
    NL("Deel van de grootste fout dat wordt genegeerd"),
    RU("Доля наихудшей ошибки, которая игнорируется"),
    TR("Yok sayılan en kötü hata oranı"));
SS_MSG(densify_robust_edge_aware_quantile_help,
    EN("How much of the worst-matching image area is ignored when placing splats "
       "in `robust_edge_aware` mode. Lower ignores more, which suits captures "
       "full of moving distractions; higher keeps more, which suits clean captures "
       "where large errors are real detail."),
    JA("`robust_edge_aware` でスプラットを置くとき、いちばん合っていない領域を"
       "どれだけ無視するかです。下げるほど多く無視するので、動く邪魔物の多い撮"
       "影に向きます。上げるほど残すので、大きな誤差が本物の細部である、きれい"
       "な撮影に向きます。"),
    ZH_HANS("在 `robust_edge_aware` 模式下放置泼溅时，忽略多少最不匹配的图像区"
            "域。数值越低忽略得越多，适合动态干扰很多的拍摄；数值越高保留得越"
            "多，适合大误差就是真实细节的干净拍摄。"),
    ZH_HANT("在 `robust_edge_aware` 模式下放置潑濺時，忽略多少最不吻合的影像區"
            "域。數值越低忽略得越多，適合動態干擾很多的拍攝；數值越高保留得越"
            "多，適合大誤差就是真實細節的乾淨拍攝。"),
    KO("`robust_edge_aware` 모드에서 스플랫을 놓을 때 가장 안 맞는 이미지 영역"
       "을 얼마나 무시할지입니다. 낮추면 더 많이 무시하므로 움직이는 방해물이"
       " 많은 촬영에 맞고, 높이면 더 많이 남기므로 큰 오차가 곧 실제 디테일인"
       " 깨끗한 촬영에 맞습니다."),
    DE("Wie viel der am schlechtesten passenden Bildfläche beim Setzen von Splats "
       "im Modus `robust_edge_aware` ignoriert wird. Niedriger ignoriert mehr, "
       "was zu Aufnahmen voller bewegter Störungen passt; höher behält mehr, "
       "was zu sauberen Aufnahmen passt, in denen große Fehler echtes Detail "
       "sind."),
    FR("Quelle part de la zone d'image la moins conforme est ignorée lors du "
       "placement des splats en mode `robust_edge_aware`. Plus bas en ignore "
       "davantage, ce qui convient aux prises pleines de gêneurs mobiles ; plus "
       "haut en garde davantage, ce qui convient aux prises propres où les grosses "
       "erreurs sont du vrai détail."),
    ES("Qué parte de la zona de imagen que peor coincide se ignora al colocar "
       "splats en el modo `robust_edge_aware`. Más bajo ignora más, lo que va "
       "bien con capturas llenas de elementos en movimiento; más alto conserva "
       "más, lo que va bien con capturas limpias donde los errores grandes son "
       "detalle real."),
    PT("Que parte da área de imagem que pior coincide é ignorada ao colocar splats "
       "no modo `robust_edge_aware`. Mais baixo ignora mais, o que serve para "
       "capturas cheias de elementos em movimento; mais alto mantém mais, o que "
       "serve para capturas limpas em que erros grandes são detalhe real."),
    IT("Quanta parte dell'area d'immagine che corrisponde peggio viene ignorata "
       "nel collocare gli splat in modalità `robust_edge_aware`. Più basso ne "
       "ignora di più, il che si adatta a riprese piene di disturbi in movimento; "
       "più alto ne conserva di più, il che si adatta a riprese pulite in cui "
       "gli errori grandi sono dettaglio reale."),
    NL("Hoeveel van het slechtst passende beeldgebied wordt genegeerd bij het "
       "plaatsen van splats in de modus `robust_edge_aware`. Lager negeert meer, "
       "wat past bij opnamen vol bewegende stoorelementen; hoger houdt meer, "
       "wat past bij schone opnamen waar grote fouten echt detail zijn."),
    RU("Какая доля хуже всего совпадающей области изображения игнорируется при "
       "размещении сплатов в режиме `robust_edge_aware`. Меньше — игнорируется "
       "больше, что подходит съёмке с множеством движущихся помех; больше — сохраняется "
       "больше, что подходит чистой съёмке, где крупные ошибки и есть настоящие "
       "детали."),
    TR("`robust_edge_aware` kipinde splat yerleştirirken en kötü uyuşan görüntü "
       "alanının ne kadarının yok sayılacağı. Düşük değerler daha çoğunu yok "
       "sayar; hareketli istenmeyenlerle dolu çekimlere uyar. Yüksek değerler "
       "daha çoğunu tutar; büyük hataların gerçek ayrıntı olduğu temiz çekimlere "
       "uyar."));

SS_MSG(densify_nms_falloff,
    EN("Edge thinning softness"), JA("エッジ細線化の強さ"),
    ZH_HANS("边缘细化的柔和度"), ZH_HANT("邊緣細化的柔和度"),
    KO("가장자리 세선화 강도"),
    DE("Weichheit der Kantenverdünnung"),
    FR("Douceur de l'amincissement des contours"),
    ES("Suavidad del adelgazamiento de bordes"),
    PT("Suavidade do afinamento de bordas"),
    IT("Morbidezza dell'assottigliamento dei bordi"),
    NL("Zachtheid van het randen uitdunnen"),
    RU("Мягкость утончения краёв"),
    TR("Kenar inceltmenin yumuşaklığı"));
SS_MSG(densify_nms_falloff_help,
    EN("Only used by the `_nms` error measures. Each of the two neighbours across "
       "an edge that beats a pixel multiplies its error by this. 0 is the strict "
       "version, which keeps only the crest and can break up leafy, grainy scenes "
       "into speckle. 1 turns the thinning off. Values near the middle keep edges "
       "sharp while leaving noisy areas smooth."),
    JA("`_nms` の誤差の測り方だけで使います。エッジをまたぐ両隣のうち、その画"
       "素より大きいものひとつにつき、誤差をこの値で掛けます。0 は厳密版で、"
       "尾根だけを残すため、葉や細かい模様のシーンが点々に崩れることがありま"
       "す。1 "
       "は細線化を切ります。中くらいの値なら、エッジは鋭いまま、ざらついた場"
       "所はなめらかに保てます。"),
    ZH_HANS("仅供 `_nms` 误差度量使用。跨越边缘的两个相邻像素中，每有一个比该"
            "像素大，就把它的误差乘以这个值。0 是严格版，只保留最高的一条线，可"
            "能把树叶这类细碎的场景打散成小点。1 关闭细化。取中间值可以让"
            "边缘保持清晰，同时让噪声多的区域保持平滑。"),
    ZH_HANT("僅供 `_nms` 誤差度量使用。跨越邊緣的兩個相鄰像素中，每有一個比該"
            "像素大，就把它的誤差乘以這個值。0 是嚴格版，只保留最高的一條線，可"
            "能把樹葉這類細碎的場景打散成小點。1 關閉細化。取中間值可以讓"
            "邊緣保持清晰，同時讓雜訊多的區域保持平滑。"),
    KO("`_nms` 오차 측정 방식에서만 쓰입니다. 가장자리를 사이에 둔 두 이웃 화소"
       " 중 해당 화소보다 큰 것 하나마다 오차에 이 값을 곱합니다. 0은 엄격한 방"
       "식으로 마루만 남기므로 잎이나 거친 질감의 장면이 잔점으로 부서질 수 있"
       "습니다. 1은 세선화를 끕니다. 중간값이면 가장자리는 또렷하게, 잡음이 많은"
       " 영역은 매끄럽게 유지됩니다."),
    DE("Nur von den `_nms`-Fehlermaßen benutzt. Jeder der beiden Nachbarn quer "
       "zur Kante, der ein Pixel übertrifft, multipliziert dessen Fehler damit. "
       "0 ist die strenge Variante, die nur den Grat behält und laubige, körnige "
       "Szenen in Sprenkel zerlegen kann. 1 schaltet die Verdünnung ab. Mittlere "
       "Werte halten Kanten scharf und verrauschte Flächen glatt."),
    FR("Utilisé uniquement par les mesures d'erreur `_nms`. Chacun des deux voisins "
       "en travers d'un contour qui dépasse un pixel multiplie son erreur par "
       "cette valeur. 0 est la version stricte, qui ne garde que la crête et peut "
       "réduire les scènes feuillues ou granuleuses en mouchetis. 1 désactive "
       "l'amincissement. Les valeurs moyennes gardent les contours nets tout en "
       "laissant lisses les zones bruitées."),
    ES("Solo lo usan las medidas de error `_nms`. Cada uno de los dos vecinos al "
       "otro lado de un borde que supere a un píxel multiplica su error por este "
       "valor. 0 es la versión estricta, que solo conserva la cresta y puede romper "
       "las escenas frondosas o granulosas en motas. 1 desactiva el adelgazamiento. "
       "Los valores intermedios mantienen los bordes nítidos y las zonas ruidosas "
       "suaves."),
    PT("Só é usado pelas medidas de erro `_nms`. Cada um dos dois vizinhos do outro "
       "lado de uma borda que supere um pixel multiplica o erro dele por este valor. "
       "0 é a versão estrita, que guarda apenas a crista e pode partir cenas "
       "folhosas ou granulosas em salpicos. 1 desliga o afinamento. Valores "
       "intermédios mantêm as bordas nítidas e as zonas ruidosas suaves."),
    IT("Usato solo dalle misure d'errore `_nms`. Ciascuno dei due vicini attraverso "
       "un bordo che supera un pixel ne moltiplica l'errore per questo valore. 0 "
       "è la versione rigida, che tiene solo la cresta e può sbriciolare in "
       "puntini le scene fogliose o granulose. 1 disattiva l'assottigliamento. "
       "I valori intermedi tengono i bordi nitidi e lisce le zone rumorose."),
    NL("Alleen gebruikt door de `_nms`-foutmaten. Elk van de twee buren aan de "
       "overkant van een rand die een pixel overtreft, vermenigvuldigt diens fout "
       "hiermee. 0 is de strikte versie, die alleen de kam bewaart en bladerrijke, "
       "korrelige scènes tot spikkels kan breken. 1 zet het uitdunnen uit. "
       "Middenwaarden houden randen scherp en ruisrijke vlakken glad."),
    RU("Используется только мерами ошибки `_nms`. Каждый из двух соседей поперёк "
       "края, превосходящий пиксель, умножает его ошибку на это число. 0 — строгий "
       "вариант: остаётся только гребень, из-за чего лиственные и зернистые сцены "
       "могут рассыпаться в крапинки. 1 отключает утончение. Средние значения "
       "оставляют края резкими, а шумные участки гладкими."),
    TR("Yalnızca `_nms` hata ölçüleri kullanır. Bir kenarın karşısındaki iki "
       "komşudan pikseli aşan her biri, o pikselin hatasını bununla çarpar. 0 katı "
       "sürümdür; yalnızca sırtı tutar ve yapraklı, taneli sahneleri beneklere "
       "ayırabilir. 1 inceltmeyi kapatır. Orta değerler kenarları keskin, gürültülü "
       "alanları düz bırakır."));

SS_MSG(densify_loss_map_normalize,
    EN("Level error across photos"), JA("写真ごとに誤差をそろえる"),
    ZH_HANS("统一各照片的误差尺度"), ZH_HANT("統一各照片的誤差尺度"),
    KO("사진 간 오차 수준 맞추기"),
    DE("Fehler zwischen Fotos angleichen"),
    FR("Égaliser l'erreur entre les photos"),
    ES("Igualar el error entre fotos"),
    PT("Igualar o erro entre fotos"),
    IT("Uniformare l'errore tra le foto"),
    NL("Fout tussen foto's gelijktrekken"),
    RU("Выравнивать ошибку между фото"),
    TR("Hatayı fotoğraflar arasında eşitle"));
SS_MSG(densify_loss_map_normalize_help,
    EN("Divide each photo's detail error by its own middle value before it decides "
       "where splats are added, so a dark or cluttered photo counts as much as a "
       "bright, simple one. Off leaves the raw error, and a few photos can then "
       "decide most of where detail goes."),
    JA("どこにスプラットを足すかを決める前に、各写真の細部の誤差をその写真自身"
       "の中央値で割ります。暗い写真や込み入った写真も、明るく単純な写真と同じ"
       "だけ効きます。切ると生の誤差のままなので、一部の写真が細部の行き先をほ"
       "ぼ決めてしまうことがあります。"),
    ZH_HANS("在决定往哪里加泼溅之前，先用每张照片自己的中位值去除它的细节误差，"
            "这样较暗或细节密集的照片和明亮简单的照片同样有分量。关闭则保留原始误"
            "差，少数照片可能决定大部分细节的去向。"),
    ZH_HANT("在決定往哪裡加潑濺之前，先用每張照片自己的中位值去除它的細節誤差，"
            "這樣較暗或細節密集的照片和明亮簡單的照片同樣有分量。關閉則保留原始誤"
            "差，少數照片可能決定大部分細節的去向。"),
    KO("스플랫을 어디에 더할지 정하기 전에, 사진마다 그 사진 자신의 중간값으로 "
       "디테일 오차를 나눕니다. 그러면 어둡거나 복잡한 사진도 밝고 단순한 사진"
       "만큼 반영됩니다. 끄면 원래 오차를 그대로 쓰므로 몇 장의 사진이 디테일의"
       " 행선지를 대부분 정해 버릴 수 있습니다."),
    DE("Den Detailfehler jedes Fotos durch dessen eigenen mittleren Wert teilen, "
       "bevor er entscheidet, wo Splats ergänzt werden. So zählt ein dunkles oder "
       "unruhiges Foto so viel wie ein helles, einfaches. Aus bleibt der rohe "
       "Fehler, und wenige Fotos bestimmen dann den größten Teil davon, wohin "
       "Detail geht."),
    FR("Diviser l'erreur de détail de chaque photo par sa propre valeur médiane "
       "avant qu'elle ne décide où ajouter des splats, pour qu'une photo sombre "
       "ou chargée compte autant qu'une photo claire et simple. Désactivé laisse "
       "l'erreur brute, et quelques photos décident alors de l'essentiel du "
       "placement du détail."),
    ES("Dividir el error de detalle de cada foto por su propio valor central antes "
       "de que decida dónde se añaden splats, para que una foto oscura o recargada "
       "cuente tanto como una clara y sencilla. Desactivado deja el error en bruto, "
       "y unas pocas fotos deciden entonces casi todo el reparto del detalle."),
    PT("Dividir o erro de detalhe de cada foto pelo seu próprio valor central antes "
       "de decidir onde acrescentar splats, para que uma foto escura ou carregada "
       "conte tanto como uma clara e simples. Desligado deixa o erro em bruto, e "
       "então poucas fotos decidem quase todo o destino do detalhe."),
    IT("Dividere l'errore di dettaglio di ogni foto per il proprio valore centrale "
       "prima che decida dove aggiungere gli splat, così una foto scura o affollata "
       "pesa quanto una chiara e semplice. Spento lascia l'errore grezzo, e poche "
       "foto decidono quasi tutta la destinazione del dettaglio."),
    NL("De detailfout van elke foto delen door haar eigen middenwaarde voordat die "
       "bepaalt waar splats bij komen, zodat een donkere of drukke foto net zoveel "
       "meetelt als een heldere, eenvoudige. Uit laat de ruwe fout staan, en dan "
       "bepalen een paar foto's het grootste deel van waar detail heen gaat."),
    RU("Делить ошибку детализации каждого фото на его собственное срединное "
       "значение, прежде чем она решает, куда добавлять сплаты: тогда тёмный или "
       "загромождённый кадр весит столько же, сколько светлый и простой. Выключено "
       "оставляет исходную ошибку, и тогда несколько кадров решают почти всё "
       "распределение деталей."),
    TR("Splat'ların nereye ekleneceğine karar vermeden önce her fotoğrafın ayrıntı "
       "hatasını kendi orta değerine böler; böylece karanlık ya da kalabalık bir "
       "fotoğraf, aydınlık ve yalın bir fotoğraf kadar sayılır. Kapalıyken ham hata "
       "kalır ve ayrıntının nereye gideceğini birkaç fotoğraf belirleyebilir."));

SS_MSG(densify_loss_map_clip_quantile,
    EN("Error spike cutoff"), JA("誤差スパイクの上限"),
    ZH_HANS("误差尖峰上限"), ZH_HANT("誤差尖峰上限"), KO("오차 급등 상한"),
    DE("Obergrenze für Fehlerspitzen"),
    FR("Plafond des pics d'erreur"),
    ES("Tope de los picos de error"),
    PT("Limite dos picos de erro"),
    IT("Tetto dei picchi di errore"),
    NL("Bovengrens voor foutpieken"),
    RU("Порог обрезки всплесков ошибки"),
    TR("Hata sıçraması üst sınırı"));
SS_MSG(densify_loss_map_clip_quantile_help,
    EN("Cap each photo's detail error at this share of its own pixels, so a handful "
       "of extreme pixels cannot pull every new splat toward them. 1 leaves it "
       "uncapped and costs nothing; 0.99 trims the worst one percent of each photo."),
    JA("各写真の細部の誤差を、その写真の画素のこの割合のところで頭打ちにします。"
       "ごく少数の極端な画素に新しいスプラットが引き寄せられなくなります。1 は"
       "頭打ちなしで、処理も増えません。0.99 は各写真の上位 1 パーセントを削り"
       "ます。"),
    ZH_HANS("把每张照片的细节误差截断在自身像素的这一比例处，使极少数极端像素无"
            "法把新泼溅全都吸引过去。1 表示不截断，也不增加开销；0.99 会削掉每"
            "张照片最差的百分之一。"),
    ZH_HANT("把每張照片的細節誤差截斷在自身像素的這一比例處，使極少數極端像素無"
            "法把新潑濺全都吸引過去。1 表示不截斷，也不增加開銷；0.99 會削掉每"
            "張照片最差的百分之一。"),
    KO("각 사진의 디테일 오차를 그 사진 화소의 이 비율에서 잘라 냅니다. 그러면 "
       "극단적인 화소 몇 개가 새 스플랫을 전부 끌어당기지 못합니다. 1은 자르지 "
       "않으며 비용도 없고, 0.99는 사진마다 가장 나쁜 1퍼센트를 깎습니다."),
    DE("Den Detailfehler jedes Fotos bei diesem Anteil seiner eigenen Pixel "
       "abschneiden, damit eine Handvoll extremer Pixel nicht jeden neuen Splat zu "
       "sich zieht. 1 schneidet nichts ab und kostet nichts; 0,99 kappt das "
       "schlechteste Prozent jedes Fotos."),
    FR("Plafonner l'erreur de détail de chaque photo à cette part de ses propres "
       "pixels, pour qu'une poignée de pixels extrêmes n'attire pas vers eux chaque "
       "nouveau splat. 1 ne plafonne rien et ne coûte rien ; 0,99 rogne le pire "
       "pour cent de chaque photo."),
    ES("Limitar el error de detalle de cada foto a esta proporción de sus propios "
       "píxeles, para que un puñado de píxeles extremos no atraiga hacia sí cada "
       "splat nuevo. 1 no limita nada y no cuesta nada; 0,99 recorta el peor uno "
       "por ciento de cada foto."),
    PT("Limitar o erro de detalhe de cada foto a esta proporção dos seus próprios "
       "pixels, para que um punhado de pixels extremos não puxe para si cada splat "
       "novo. 1 não limita nada e não custa nada; 0,99 corta o pior um por cento "
       "de cada foto."),
    IT("Limitare l'errore di dettaglio di ogni foto a questa quota dei suoi stessi "
       "pixel, così una manciata di pixel estremi non tira verso di sé ogni nuovo "
       "splat. 1 non limita nulla e non costa nulla; 0,99 taglia l'uno per cento "
       "peggiore di ogni foto."),
    NL("De detailfout van elke foto afkappen bij dit deel van haar eigen pixels, "
       "zodat een handvol extreme pixels niet elke nieuwe splat naar zich toe trekt. "
       "1 kapt niets af en kost niets; 0,99 snoeit de slechtste procent van elke "
       "foto weg."),
    RU("Обрезать ошибку детализации каждого фото на этой доле его собственных "
       "пикселей, чтобы горстка крайних пикселей не стягивала к себе каждый новый "
       "сплат. 1 не обрезает ничего и ничего не стоит; 0,99 срезает худший процент "
       "каждого кадра."),
    TR("Her fotoğrafın ayrıntı hatasını kendi piksellerinin bu oranında keser; "
       "böylece bir avuç uç piksel her yeni splat'ı kendine çekemez. 1 hiç kesmez "
       "ve maliyeti yoktur; 0,99 her fotoğrafın en kötü yüzde birini budar."));

SS_MSG(densify_loss_map_power,
    EN("Error contrast"), JA("誤差のコントラスト"),
    ZH_HANS("误差对比度"), ZH_HANT("誤差對比度"), KO("오차 대비"),
    DE("Fehlerkontrast"),
    FR("Contraste de l'erreur"),
    ES("Contraste del error"),
    PT("Contraste do erro"),
    IT("Contrasto dell'errore"),
    NL("Foutcontrast"),
    RU("Контраст ошибки"),
    TR("Hata karşıtlığı"));
SS_MSG(densify_loss_map_power_help,
    EN("Raise every pixel's detail error to this power before splats are scored by "
       "it. Above 1 sharpens the contrast between good and bad pixels, so new "
       "splats crowd into the worst spots; below 1 flattens it and spreads them "
       "out. 1 leaves the error as it is."),
    JA("スプラットの点数を付ける前に、各画素の細部の誤差をこの指数で累乗します。"
       "1 より大きいと良い画素と悪い画素の差が強調され、新しいスプラットがいちばん"
       "悪い場所に集まります。1 より小さいと差がならされて広く散ります。1 は誤差を"
       "そのまま使います。"),
    ZH_HANS("在用细节误差给泼溅打分之前，先把每个像素的误差取这个次方。大于 1 会"
            "放大好像素与差像素之间的差距，新泼溅会集中到最差的地方；小于 1 会削平"
            "差距，让它们散开。1 表示保持误差原样。"),
    ZH_HANT("在用細節誤差給潑濺打分之前，先把每個像素的誤差取這個次方。大於 1 會"
            "放大好像素與差像素之間的差距，新潑濺會集中到最差的地方；小於 1 會削平"
            "差距，讓它們散開。1 表示保持誤差原樣。"),
    KO("스플랫에 점수를 매기기 전에 각 화소의 디테일 오차를 이 거듭제곱만큼 올립"
       "니다. 1보다 크면 좋은 화소와 나쁜 화소의 차이가 커져 새 스플랫이 가장 "
       "나쁜 곳으로 몰리고, 1보다 작으면 차이가 평평해져 넓게 퍼집니다. 1은 오차를 "
       "그대로 둡니다."),
    DE("Den Detailfehler jedes Pixels mit dieser Potenz versehen, bevor Splats "
       "danach bewertet werden. Über 1 verschärft den Kontrast zwischen guten und "
       "schlechten Pixeln, sodass neue Splats sich an den schlimmsten Stellen "
       "drängen; unter 1 ebnet ihn ein und verteilt sie. 1 lässt den Fehler, wie "
       "er ist."),
    FR("Élever l'erreur de détail de chaque pixel à cette puissance avant que les "
       "splats en soient notés. Au-dessus de 1, le contraste entre bons et mauvais "
       "pixels se durcit et les nouveaux splats se pressent aux pires endroits ; "
       "en dessous de 1, il s'aplatit et ils se dispersent. 1 laisse l'erreur "
       "telle quelle."),
    ES("Elevar el error de detalle de cada píxel a esta potencia antes de puntuar "
       "los splats con él. Por encima de 1 se acentúa el contraste entre píxeles "
       "buenos y malos, y los splats nuevos se agolpan en los peores sitios; por "
       "debajo de 1 se aplana y se reparten. 1 deja el error tal cual."),
    PT("Elevar o erro de detalhe de cada pixel a esta potência antes de pontuar os "
       "splats com ele. Acima de 1 acentua o contraste entre pixels bons e maus, e "
       "os splats novos amontoam-se nos piores sítios; abaixo de 1 achata-o e eles "
       "espalham-se. 1 deixa o erro como está."),
    IT("Elevare l'errore di dettaglio di ogni pixel a questa potenza prima che gli "
       "splat vengano valutati su di esso. Sopra 1 il contrasto fra pixel buoni e "
       "cattivi si accentua e i nuovi splat si accalcano nei punti peggiori; sotto "
       "1 si appiattisce e si distribuiscono. 1 lascia l'errore com'è."),
    NL("De detailfout van elke pixel tot deze macht verheffen voordat splats "
       "erop worden beoordeeld. Boven 1 wordt het verschil tussen goede en slechte "
       "pixels scherper en dringen nieuwe splats samen op de slechtste plekken; "
       "onder 1 vlakt het af en spreiden ze zich. 1 laat de fout zoals ze is."),
    RU("Возводить ошибку детализации каждого пикселя в эту степень, прежде чем по "
       "ней оцениваются сплаты. Больше 1 усиливает разницу между хорошими и плохими "
       "пикселями, и новые сплаты сбиваются в худшие места; меньше 1 сглаживает её, "
       "и они расходятся. 1 оставляет ошибку как есть."),
    TR("Splat'lar buna göre puanlanmadan önce her pikselin ayrıntı hatasını bu "
       "kuvvete yükseltir. 1'in üstünde iyi ve kötü pikseller arasındaki fark "
       "keskinleşir, yeni splat'lar en kötü noktalara toplanır; 1'in altında fark "
       "düzleşir ve dağılırlar. 1 hatayı olduğu gibi bırakır."));

SS_MSG(densify_accum_mode,
    EN("Combine error over a photo"), JA("写真内での誤差のまとめ方"),
    ZH_HANS("单张照片内的误差合并方式"), ZH_HANT("單張照片內的誤差合併方式"),
    KO("사진 안에서 오차를 합치는 방식"),
    DE("Fehler über ein Foto zusammenfassen"),
    FR("Combiner l'erreur sur une photo"),
    ES("Combinar el error en una foto"),
    PT("Combinar o erro numa foto"),
    IT("Combinare l'errore su una foto"),
    NL("Fout over een foto samenvoegen"),
    RU("Сведение ошибки по фотографии"),
    TR("Hatayı bir fotoğraf boyunca birleştirme"));
SS_MSG(densify_accum_mode_help,
    EN("How the error under one splat is turned into its score. `max` takes its "
       "worst pixel, so a splat covering one bad spot still counts. `sum` adds the "
       "error up, which favours large splats and busy areas. `avg` takes the "
       "average over the area it covers, which judges big and small splats alike."),
    JA("1 つのスプラットが覆う誤差を、どうやってそのスプラットの点数にするかで"
       "す。`max` はいちばん悪い画素を取るので、悪い部分が 1 つでもあれば効きま"
       "す。`sum` は誤差を足すので、大きなスプラットや込み入った場所が有利にな"
       "ります。`avg` は覆っている範囲の平均を取るので、大小のスプラットを同じ"
       "ものさしで測ります。"),
    ZH_HANS("如何把一个泼溅覆盖范围内的误差变成它的分数。`max` 取其中最差的像"
            "素，因此只要覆盖到一处很差的地方就会计入。`sum` 把误差累加，会偏"
            "向大泼溅和繁杂区域。`avg` 取覆盖范围内的平均值，对大小泼溅一视同"
            "仁。"),
    ZH_HANT("如何把一個潑濺覆蓋範圍內的誤差變成它的分數。`max` 取其中最差的像"
            "素，因此只要覆蓋到一處很差的地方就會計入。`sum` 把誤差累加，會偏"
            "向大潑濺和繁雜區域。`avg` 取覆蓋範圍內的平均值，對大小潑濺一視同"
            "仁。"),
    KO("스플랫 하나가 덮은 오차를 그 스플랫의 점수로 바꾸는 방식입니다. `max` "
       "는 가장 나쁜 화소를 취하므로 나쁜 지점 하나만 덮어도 반영됩니다. `sum` "
       "은 오차를 더하므로 큰 스플랫과 복잡한 영역에 유리합니다. `avg` 는 덮은 "
       "범위의 평균을 취하므로 크고 작은 스플랫을 같은 기준으로 봅니다."),
    DE("Wie der Fehler unter einem Splat zu dessen Punktzahl wird. `max` nimmt "
       "sein schlechtestes Pixel, sodass ein Splat schon zählt, wenn er eine "
       "schlechte Stelle überdeckt. `sum` addiert den Fehler auf, was große Splats "
       "und unruhige Flächen bevorzugt. `avg` nimmt den Mittelwert über die "
       "überdeckte Fläche und misst große wie kleine Splats gleich."),
    FR("Comment l'erreur sous un splat devient sa note. `max` prend son pire "
       "pixel, donc un splat qui couvre un seul mauvais endroit compte quand même. "
       "`sum` additionne l'erreur, ce qui favorise les gros splats et les zones "
       "chargées. `avg` prend la moyenne sur la surface couverte, ce qui juge "
       "petits et gros splats de la même façon."),
    ES("Cómo el error bajo un splat se convierte en su puntuación. `max` toma su "
       "peor píxel, así que un splat que cubre un solo punto malo ya cuenta. `sum` "
       "suma el error, lo que favorece a los splats grandes y las zonas recargadas. "
       "`avg` toma la media sobre el área que cubre, que juzga igual a grandes y "
       "pequeños."),
    PT("Como o erro sob um splat se torna a sua pontuação. `max` toma o seu pior "
       "pixel, por isso um splat que cobre um único ponto mau já conta. `sum` soma "
       "o erro, o que favorece splats grandes e zonas carregadas. `avg` toma a "
       "média sobre a área que cobre, julgando grandes e pequenos por igual."),
    IT("Come l'errore sotto uno splat diventa il suo punteggio. `max` prende il "
       "suo pixel peggiore, così uno splat che copre anche un solo punto brutto "
       "conta lo stesso. `sum` somma l'errore, il che favorisce splat grandi e "
       "zone affollate. `avg` prende la media sull'area coperta, giudicando grandi "
       "e piccoli allo stesso modo."),
    NL("Hoe de fout onder een splat zijn score wordt. `max` neemt het slechtste "
       "pixel, dus een splat die één slechte plek bedekt telt al mee. `sum` telt "
       "de fout op, wat grote splats en drukke vlakken bevoordeelt. `avg` neemt "
       "het gemiddelde over het bedekte gebied en meet grote en kleine splats "
       "gelijk."),
    RU("Как ошибка под сплатом превращается в его оценку. `max` берёт худший "
       "пиксель, поэтому сплат засчитывается, даже если задевает лишь одно плохое "
       "место. `sum` складывает ошибку, что выгодно крупным сплатам и насыщенным "
       "участкам. `avg` берёт среднее по покрытой площади и судит крупные и мелкие "
       "сплаты одинаково."),
    TR("Bir splat'ın altındaki hatanın onun puanına nasıl dönüştüğü. `max` en kötü "
       "pikseli alır; böylece tek bir kötü noktayı örten splat da sayılır. `sum` "
       "hatayı toplar, bu da büyük splat'ları ve kalabalık alanları öne çıkarır. "
       "`avg` örttüğü alanın ortalamasını alır ve büyük ile küçük splat'ları aynı "
       "ölçüyle değerlendirir."));

SS_MSG(densify_score_power,
    EN("Splat score contrast"), JA("スプラット点数のコントラスト"),
    ZH_HANS("泼溅分数对比度"), ZH_HANT("潑濺分數對比度"), KO("스플랫 점수 대비"),
    DE("Kontrast der Splat-Bewertung"),
    FR("Contraste des notes de splat"),
    ES("Contraste de la puntuación de splats"),
    PT("Contraste da pontuação dos splats"),
    IT("Contrasto del punteggio degli splat"),
    NL("Contrast van de splatscore"),
    RU("Контраст оценки сплатов"),
    TR("Splat puanı karşıtlığı"));
SS_MSG(densify_score_power_help,
    EN("Raise each splat's score for one step to this power before it is averaged "
       "over the steps between refinements. Above 1 lets a single bad photo carry "
       "a splat's score; below 1 makes a splat have to look bad in many photos "
       "before it counts. 1 leaves the score as it is."),
    JA("リファインメントの間のステップで平均する前に、1 ステップ分のスプラットの"
       "点数をこの指数で累乗します。1 より大きいと写真 1 枚の悪さでそのスプラット"
       "の点数が決まり、1 より小さいと多くの写真で悪く見えないと効きません。1 は"
       "点数をそのまま使います。"),
    ZH_HANS("在把两次细化之间各步的分数取平均之前，先把单步的泼溅分数取这个次"
            "方。大于 1 时，一张很差的照片就能决定该泼溅的分数；小于 1 时，泼溅"
            "要在很多照片里都表现差才算数。1 表示保持分数原样。"),
    ZH_HANT("在把兩次細化之間各步的分數取平均之前，先把單步的潑濺分數取這個次"
            "方。大於 1 時，一張很差的照片就能決定該潑濺的分數；小於 1 時，潑濺"
            "要在很多照片裡都表現差才算數。1 表示保持分數原樣。"),
    KO("정제 사이의 스텝들에 대해 평균 내기 전에, 한 스텝의 스플랫 점수를 이 "
       "거듭제곱만큼 올립니다. 1보다 크면 나쁜 사진 한 장이 그 스플랫의 점수를 "
       "좌우하고, 1보다 작으면 여러 사진에서 나빠야 점수에 반영됩니다. 1은 점수를 "
       "그대로 둡니다."),
    DE("Die Bewertung eines Splats für einen Schritt mit dieser Potenz versehen, "
       "bevor sie über die Schritte zwischen zwei Verfeinerungen gemittelt wird. "
       "Über 1 kann ein einziges schlechtes Foto die Bewertung tragen; unter 1 "
       "muss ein Splat in vielen Fotos schlecht aussehen, um zu zählen. 1 lässt "
       "die Bewertung, wie sie ist."),
    FR("Élever à cette puissance la note d'un splat pour une étape avant qu'elle "
       "soit moyennée sur les étapes entre deux raffinements. Au-dessus de 1, une "
       "seule mauvaise photo suffit à porter la note ; en dessous de 1, le splat "
       "doit paraître mauvais sur beaucoup de photos pour compter. 1 laisse la "
       "note telle quelle."),
    ES("Elevar a esta potencia la puntuación de cada splat en un paso antes de "
       "promediarla sobre los pasos entre refinamientos. Por encima de 1, una sola "
       "foto mala basta para sostener la puntuación; por debajo de 1, el splat "
       "tiene que verse mal en muchas fotos para contar. 1 deja la puntuación tal "
       "cual."),
    PT("Elevar a esta potência a pontuação de cada splat num passo antes de ser "
       "medida sobre os passos entre refinamentos. Acima de 1, uma única foto má "
       "chega para sustentar a pontuação; abaixo de 1, o splat tem de parecer mau "
       "em muitas fotos para contar. 1 deixa a pontuação como está."),
    IT("Elevare a questa potenza il punteggio di uno splat in un passo prima che "
       "venga mediato sui passi fra due raffinamenti. Sopra 1 basta una sola foto "
       "cattiva a reggere il punteggio; sotto 1 lo splat deve apparire cattivo in "
       "molte foto per contare. 1 lascia il punteggio com'è."),
    NL("De score van een splat in één stap tot deze macht verheffen voordat ze "
       "wordt gemiddeld over de stappen tussen twee verfijningen. Boven 1 kan één "
       "slechte foto de score dragen; onder 1 moet een splat er in veel foto's "
       "slecht uitzien om mee te tellen. 1 laat de score zoals ze is."),
    RU("Возводить оценку сплата за один шаг в эту степень, прежде чем усреднять её "
       "по шагам между уточнениями. Больше 1 — одного плохого кадра хватает, чтобы "
       "вытянуть оценку; меньше 1 — сплат должен выглядеть плохо на многих кадрах, "
       "чтобы это засчиталось. 1 оставляет оценку как есть."),
    TR("Bir splat'ın tek adımdaki puanını, iki iyileştirme arasındaki adımlar "
       "boyunca ortalanmadan önce bu kuvvete yükseltir. 1'in üstünde tek bir kötü "
       "fotoğraf puanı taşıyabilir; 1'in altında splat'ın sayılması için birçok "
       "fotoğrafta kötü görünmesi gerekir. 1 puanı olduğu gibi bırakır."));

SS_MSG(densify_score_clip_quantile,
    EN("Score spike cutoff"), JA("点数スパイクの上限"),
    ZH_HANS("分数尖峰上限"), ZH_HANT("分數尖峰上限"), KO("점수 급등 상한"),
    DE("Obergrenze für Punktzahlspitzen"),
    FR("Plafond des pics de note"),
    ES("Tope de los picos de puntuación"),
    PT("Limite dos picos de pontuação"),
    IT("Tetto dei picchi di punteggio"),
    NL("Bovengrens voor scorepieken"),
    RU("Порог обрезки всплесков оценки"),
    TR("Puan sıçraması üst sınırı"));
SS_MSG(densify_score_clip_quantile_help,
    EN("Cap the finished per-splat score at this share of the splats, so a few "
       "runaway splats cannot soak up every new one. Applies after the error has "
       "been combined over each photo and across steps, and is what the "
       "refinement-score view draws. 1 leaves it uncapped and costs nothing."),
    JA("仕上がったスプラットごとの点数を、スプラット全体のこの割合のところで頭"
       "打ちにします。ごく少数の突出したスプラットが、新しいスプラットを独り占"
       "めしなくなります。写真ごとの合算と各ステップ間の合算のあとに効き、点数"
       "表示もこの値を映します。1 は頭打ちなしで、処理も増えません。"),
    ZH_HANS("把最终的每个泼溅分数截断在全体泼溅的这一比例处，使少数失控的泼溅"
            "无法吞掉所有新增额度。它在按照片合并以及跨步合并之后生效，细化分"
            "数视图显示的也是它。1 表示不截断，也不增加开销。"),
    ZH_HANT("把最終的每個潑濺分數截斷在全體潑濺的這一比例處，使少數失控的潑濺"
            "無法吞掉所有新增額度。它在按照片合併以及跨步合併之後生效，細化分"
            "數檢視顯示的也是它。1 表示不截斷，也不增加開銷。"),
    KO("완성된 스플랫별 점수를 전체 스플랫의 이 비율에서 잘라 냅니다. 그러면 몇"
       "몇 개의 큰 스플랫이 새 스플랫을 독차지하지 못합니다. 사진별 합산과 단계 "
       "합산이 끝난 뒤에 적용되며, 정밀화 점수 보기에 그려지는 값도 이것입니다. "
       "1은 자르지 않으며 비용도 없습니다."),
    DE("Die fertige Punktzahl je Splat bei diesem Anteil der Splats abschneiden, "
       "damit wenige ausreißende Splats nicht jeden neuen aufsaugen. Wirkt, nachdem "
       "der Fehler je Foto und über die Schritte zusammengefasst wurde, und ist "
       "das, was die Punktzahl-Ansicht zeichnet. 1 schneidet nichts ab."),
    FR("Plafonner la note finale par splat à cette part des splats, pour qu'une "
       "poignée de splats emballés n'absorbe pas tous les nouveaux. S'applique "
       "après la combinaison par photo et entre les étapes, et c'est ce que "
       "dessine la vue des notes. 1 ne plafonne rien et ne coûte rien."),
    ES("Limitar la puntuación final por splat a esta proporción de los splats, "
       "para que unos pocos splats desbocados no absorban todos los nuevos. Se "
       "aplica tras combinar el error por foto y entre pasos, y es lo que dibuja "
       "la vista de puntuación. 1 no limita nada y no cuesta nada."),
    PT("Limitar a pontuação final por splat a esta proporção dos splats, para que "
       "uns poucos splats descontrolados não absorvam todos os novos. Aplica-se "
       "depois de combinar o erro por foto e entre passos, e é o que a vista de "
       "pontuação desenha. 1 não limita nada e não custa nada."),
    IT("Limitare il punteggio finale per splat a questa quota degli splat, così "
       "pochi splat impazziti non si prendono tutti quelli nuovi. Si applica dopo "
       "aver combinato l'errore per foto e tra i passi, ed è ciò che disegna la "
       "vista dei punteggi. 1 non limita nulla e non costa nulla."),
    NL("De uiteindelijke score per splat afkappen bij dit deel van de splats, "
       "zodat een paar op hol geslagen splats niet alle nieuwe opslokken. Werkt "
       "nadat de fout per foto en over de stappen is samengevoegd, en is wat de "
       "scoreweergave tekent. 1 kapt niets af en kost niets."),
    RU("Обрезать итоговую оценку сплата на этой доле сплатов, чтобы несколько "
       "разогнавшихся сплатов не забрали себе все новые. Действует после сведения "
       "ошибки по фотографии и по шагам, и именно это рисует вид оценок. "
       "1 не обрезает ничего и ничего не стоит."),
    TR("Bitmiş splat başına puanı, splat'ların bu oranında keser; böylece birkaç "
       "kontrolden çıkmış splat yeni splat'ların tümünü yutamaz. Hata fotoğraf "
       "başına ve adımlar arasında birleştirildikten sonra uygulanır ve iyileştirme "
       "puanı görünümünün çizdiği de budur. 1 hiç kesmez ve maliyeti yoktur."));

SS_MSG(densify_final_score_power,
    EN("Splat pick sharpness"), JA("スプラット選択の鋭さ"),
    ZH_HANS("泼溅挑选的集中度"), ZH_HANT("潑濺挑選的集中度"), KO("스플랫 선택의 예리함"),
    DE("Schärfe der Splat-Auswahl"),
    FR("Netteté du tirage des splats"),
    ES("Nitidez de la elección de splats"),
    PT("Nitidez da escolha dos splats"),
    IT("Nitidezza della scelta degli splat"),
    NL("Scherpte van de splatkeuze"),
    RU("Резкость отбора сплатов"),
    TR("Splat seçiminin keskinliği"));
SS_MSG(densify_final_score_power_help,
    EN("Raise the finished per-splat score to this power just before the draw that "
       "picks which splats to split. Above 1 concentrates new detail on the "
       "highest-scoring splats; below 1 spreads it more evenly over the scene. "
       "1 draws straight from the score."),
    JA("どのスプラットを分割するかを選ぶ抽選の直前に、出来上がったスプラットごとの"
       "点数をこの指数で累乗します。1 より大きいと新しい細部が点数の高いスプラット"
       "に集まり、1 より小さいとシーン全体に均されます。1 は点数のまま抽選します。"),
    ZH_HANS("在抽选要分裂哪些泼溅之前，把算好的每个泼溅的分数取这个次方。大于 1 "
            "会把新增细节集中到分数最高的泼溅上；小于 1 会让细节在场景里分布得更"
            "均匀。1 表示直接按分数抽选。"),
    ZH_HANT("在抽選要分裂哪些潑濺之前，把算好的每個潑濺的分數取這個次方。大於 1 "
            "會把新增細節集中到分數最高的潑濺上；小於 1 會讓細節在場景裡分布得更"
            "均勻。1 表示直接按分數抽選。"),
    KO("어떤 스플랫을 나눌지 뽑기 직전에, 완성된 스플랫별 점수를 이 거듭제곱"
       "만큼 올립니다. 1보다 크면 새 디테일이 점수가 높은 스플랫에 몰리고, 1보다 "
       "작으면 장면 전체에 고르게 퍼집니다. 1은 점수 그대로 뽑습니다."),
    DE("Die fertige Bewertung je Splat mit dieser Potenz versehen, direkt vor der "
       "Ziehung, die entscheidet, welche Splats geteilt werden. Über 1 bündelt "
       "neues Detail auf den am höchsten bewerteten Splats; unter 1 verteilt es "
       "sich gleichmäßiger über die Szene. 1 zieht direkt aus der Bewertung."),
    FR("Élever la note finale de chaque splat à cette puissance juste avant le "
       "tirage qui choisit les splats à diviser. Au-dessus de 1, le détail neuf se "
       "concentre sur les splats les mieux notés ; en dessous de 1, il se répartit "
       "plus également sur la scène. 1 tire directement d'après la note."),
    ES("Elevar a esta potencia la puntuación final de cada splat justo antes del "
       "sorteo que elige qué splats dividir. Por encima de 1 el detalle nuevo se "
       "concentra en los splats mejor puntuados; por debajo de 1 se reparte más "
       "por la escena. 1 sortea directamente según la puntuación."),
    PT("Elevar a esta potência a pontuação final de cada splat mesmo antes do "
       "sorteio que escolhe quais splats dividir. Acima de 1 o detalhe novo "
       "concentra-se nos splats com pontuação mais alta; abaixo de 1 espalha-se "
       "mais pela cena. 1 sorteia diretamente pela pontuação."),
    IT("Elevare a questa potenza il punteggio finale di ogni splat subito prima "
       "dell'estrazione che sceglie quali splat dividere. Sopra 1 il dettaglio "
       "nuovo si concentra sugli splat con il punteggio più alto; sotto 1 si "
       "distribuisce più uniformemente sulla scena. 1 estrae direttamente dal "
       "punteggio."),
    NL("De uiteindelijke score per splat tot deze macht verheffen vlak voor de "
       "trekking die bepaalt welke splats worden gesplitst. Boven 1 komt nieuw "
       "detail samen op de hoogst scorende splats; onder 1 verdeelt het zich "
       "gelijkmatiger over de scène. 1 trekt rechtstreeks uit de score."),
    RU("Возводить итоговую оценку каждого сплата в эту степень прямо перед "
       "жеребьёвкой, которая выбирает сплаты для деления. Больше 1 — новая "
       "детализация собирается на сплатах с высшими оценками; меньше 1 — "
       "расходится по сцене ровнее. 1 тянет прямо по оценке."),
    TR("Hangi splat'ların bölüneceğini seçen çekilişin hemen öncesinde, splat "
       "başına biten puanı bu kuvvete yükseltir. 1'in üstünde yeni ayrıntı en "
       "yüksek puanlı splat'larda toplanır; 1'in altında sahneye daha eşit "
       "dağılır. 1 doğrudan puana göre çeker."));
SS_MSG(densify_oversize_split_fraction,
    EN("Split share for oversized splats"),
    JA("大きすぎるスプラットに回す分割の割合"),
    ZH_HANS("分给过大泼溅的分裂比例"), ZH_HANT("分給過大潑濺的分裂比例"),
    KO("지나치게 큰 스플랫에 주는 분할 비율"),
    DE("Teilungsanteil für übergroße Splats"),
    FR("Part des divisions pour les splats trop grands"),
    ES("Parte de las divisiones para splats demasiado grandes"),
    PT("Parte das divisões para splats grandes demais"),
    IT("Quota di divisioni per gli splat troppo grandi"),
    NL("Splitsingsaandeel voor te grote splats"),
    RU("Доля делений для слишком крупных сплатов"),
    TR("Aşırı büyük splat'lara ayrılan bölme payı"));
SS_MSG(densify_oversize_split_fraction_help,
    EN("The share of each round's new splats spent on splats over the "
       "on-screen size limit rather than on the error score. Splitting an "
       "oversized splat is what lets it get smaller without leaving a hole. 0 "
       "spends the whole budget on the error score."),
    JA("一回の追加でつくるスプラットのうち、誤差の点数ではなく画面サイズの制限"
       "を超えたスプラットに回す割合です。大きすぎるスプラットは分割してはじめ"
       "て、穴を残さずに小さくなれます。0 なら全部を誤差の点数に使います。"),
    ZH_HANS("每一轮新增的泼溅里，有多大比例分给超出屏幕尺寸限制的泼溅，而不是"
            "按误差分数来分。过大的泼溅只有分裂之后，才能在不留下空洞的情况下"
            "变小。0 表示全部按误差分数来分。"),
    ZH_HANT("每一輪新增的潑濺裡，有多大比例分給超出螢幕尺寸限制的潑濺，而不是"
            "按誤差分數來分。過大的潑濺只有分裂之後，才能在不留下空洞的情況下"
            "變小。0 表示全部按誤差分數來分。"),
    KO("한 번에 늘리는 스플랫 가운데, 오차 점수가 아니라 화면 크기 제한을 넘은 "
       "스플랫에 주는 비율입니다. 지나치게 큰 스플랫은 나눠야 구멍을 남기지 않"
       "고 작아질 수 있습니다. 0이면 전부 오차 점수에 씁니다."),
    DE("Der Anteil der neuen Splats jeder Runde, der auf Splats über der "
       "Bildschirmgrößen-Grenze entfällt statt auf die Fehlerbewertung. Erst "
       "das Teilen lässt einen übergroßen Splat kleiner werden, ohne ein Loch "
       "zu hinterlassen. 0 gibt das ganze Budget der Fehlerbewertung."),
    FR("La part des nouveaux splats de chaque tour consacrée aux splats "
       "au-delà de la limite de taille à l'écran plutôt qu'à la note d'erreur. "
       "C'est la division qui permet à un splat trop grand de rétrécir sans "
       "laisser de trou. 0 donne tout le budget à la note d'erreur."),
    ES("La parte de los splats nuevos de cada ronda que se dedica a los "
       "splats por encima del límite de tamaño en pantalla en vez de a la "
       "puntuación de error. Dividir es lo que permite a un splat demasiado "
       "grande encogerse sin dejar un hueco. 0 dedica todo el presupuesto a "
       "la puntuación de error."),
    PT("A parte dos splats novos de cada rodada dedicada aos splats acima do "
       "limite de tamanho na tela em vez da pontuação de erro. Dividir é o "
       "que permite a um splat grande demais encolher sem deixar um buraco. 0 "
       "dedica todo o orçamento à pontuação de erro."),
    IT("La quota degli splat nuovi di ogni giro destinata agli splat oltre il "
       "limite di dimensione a schermo invece che al punteggio di errore. È "
       "la divisione che permette a uno splat troppo grande di rimpicciolirsi "
       "senza lasciare un buco. 0 destina tutto il budget al punteggio di "
       "errore."),
    NL("Het deel van de nieuwe splats per ronde dat naar splats boven de "
       "schermgroottelimiet gaat in plaats van naar de foutscore. Splitsen is "
       "wat een te grote splat laat krimpen zonder een gat achter te laten. 0 "
       "geeft het hele budget aan de foutscore."),
    RU("Доля новых сплатов каждого круга, которая уходит на сплаты сверх "
       "предела размера на экране, а не на оценку ошибки. Именно деление "
       "позволяет слишком крупному сплату уменьшиться, не оставив дыры. 0 "
       "отдаёт весь запас оценке ошибки."),
    TR("Her turda eklenen splat'ların, hata puanı yerine ekran boyutu "
       "sınırını aşmış splat'lara ayrılan payı. Aşırı büyük bir splat ancak "
       "bölününce delik bırakmadan küçülebilir. 0 bütün payı hata puanına "
       "verir."));

SS_MSG(densify_oversize_score_blend,
    EN("Error weight in the oversized draw"),
    JA("大きすぎる分の抽選での誤差の重み"),
    ZH_HANS("过大泼溅抽选里的误差权重"), ZH_HANT("過大潑濺抽選裡的誤差權重"),
    KO("큰 스플랫 뽑기에서 오차의 비중"),
    DE("Fehlergewicht in der Ziehung für übergroße Splats"),
    FR("Poids de l'erreur dans le tirage des splats trop grands"),
    ES("Peso del error en el sorteo de splats grandes"),
    PT("Peso do erro no sorteio dos splats grandes"),
    IT("Peso dell'errore nell'estrazione degli splat troppo grandi"),
    NL("Foutgewicht in de trekking voor te grote splats"),
    RU("Вес ошибки в отборе слишком крупных сплатов"),
    TR("Aşırı büyük splat çekilişinde hatanın ağırlığı"));
SS_MSG(densify_oversize_score_blend_help,
    EN("How much the error score still counts when picking which oversized "
       "splats to split. 0 picks purely by how far over the limit a splat is, "
       "which spends splats on empty sky; 1 picks exactly like the ordinary "
       "draw. In between prefers oversized splats that also carry error."),
    JA("どの大きすぎるスプラットを分割するか選ぶとき、誤差の点数をどれだけ効か"
       "せるかです。0 は制限をどれだけ超えたかだけで選ぶので、何もない空にスプ"
       "ラットを使ってしまいます。1 は普通の抽選と同じです。間の値は、誤差もあ"
       "わせて持つ大きすぎるスプラットを先に選びます。"),
    ZH_HANS("挑选要分裂哪些过大的泼溅时，误差分数还占多少分量。0 只按超出限制"
            "的程度来挑，会把泼溅花在没有细节的天空上；1 和普通抽选完全一样。"
            "取中间值会优先挑同时带着误差的过大泼溅。"),
    ZH_HANT("挑選要分裂哪些過大的潑濺時，誤差分數還佔多少分量。0 只按超出限制"
            "的程度來挑，會把潑濺花在沒有細節的天空上；1 和普通抽選完全一樣。"
            "取中間值會優先挑同時帶著誤差的過大潑濺。"),
    KO("어떤 큰 스플랫을 나눌지 고를 때 오차 점수를 얼마나 반영할지입니다. 0은 "
       "제한을 얼마나 넘었는지만 보고 뽑아서 빈 하늘에 스플랫을 씁니다. 1은 보"
       "통 뽑기와 똑같습니다. 그 사이 값은 오차까지 함께 지닌 큰 스플랫을 먼저 "
       "고릅니다."),
    DE("Wie stark die Fehlerbewertung noch zählt, wenn ausgewählt wird, "
       "welche übergroßen Splats geteilt werden. 0 wählt allein danach, wie "
       "weit ein Splat über der Grenze liegt, und verbraucht Splats am leeren "
       "Himmel; 1 wählt genau wie die gewöhnliche Ziehung. Dazwischen kommen "
       "übergroße Splats zuerst, die auch Fehler tragen."),
    FR("Combien la note d'erreur compte encore au moment de choisir quels "
       "splats trop grands diviser. 0 choisit uniquement selon le dépassement "
       "de la limite, ce qui dépense des splats sur un ciel vide ; 1 choisit "
       "exactement comme le tirage ordinaire. Entre les deux, les splats trop "
       "grands qui portent aussi de l'erreur passent d'abord."),
    ES("Cuánto cuenta todavía la puntuación de error al elegir qué splats "
       "demasiado grandes dividir. 0 elige solo por cuánto se pasa del límite, "
       "lo que gasta splats en cielo vacío; 1 elige igual que el sorteo "
       "normal. En medio prefiere los splats grandes que además cargan "
       "error."),
    PT("Quanto a pontuação de erro ainda conta ao escolher quais splats "
       "grandes demais dividir. 0 escolhe só pelo quanto passa do limite, o "
       "que gasta splats em céu vazio; 1 escolhe igual ao sorteio comum. No "
       "meio prefere os splats grandes que também carregam erro."),
    IT("Quanto conta ancora il punteggio di errore nello scegliere quali "
       "splat troppo grandi dividere. 0 sceglie solo in base a quanto un "
       "splat supera il limite, e spende splat sul cielo vuoto; 1 sceglie "
       "come l'estrazione ordinaria. Nel mezzo vengono prima gli splat troppo "
       "grandi che portano anche errore."),
    NL("Hoeveel de foutscore nog meetelt bij het kiezen welke te grote splats "
       "worden gesplitst. 0 kiest alleen op hoever een splat over de limiet "
       "zit, wat splats aan lege lucht besteedt; 1 kiest net als de gewone "
       "trekking. Ertussenin gaan te grote splats voor die ook fout dragen."),
    RU("Насколько оценка ошибки ещё учитывается при выборе слишком крупных "
       "сплатов для деления. 0 выбирает только по превышению предела и тратит "
       "сплаты на пустое небо; 1 выбирает так же, как обычная жеребьёвка. "
       "Промежуточные значения сначала берут крупные сплаты, несущие и "
       "ошибку."),
    TR("Hangi aşırı büyük splat'ların bölüneceği seçilirken hata puanının ne "
       "kadar sayıldığı. 0 yalnızca sınırın ne kadar aşıldığına bakar ve "
       "splat'ları boş gökyüzüne harcar; 1 sıradan çekilişin tıpkısıdır. "
       "Aradaki değerler hata da taşıyan aşırı büyük splat'ları öne alır."));

SS_MSG(use_long_axis_split,
    EN("Split along the long axis"), JA("長い軸で分割する"),
    ZH_HANS("沿长轴分裂"), ZH_HANT("沿長軸分裂"), KO("긴 축을 따라 분할"),
    DE("Entlang der langen Achse teilen"), FR("Diviser selon le grand axe"),
    ES("Dividir por el eje largo"), PT("Dividir pelo eixo longo"),
    IT("Dividere lungo l'asse maggiore"), NL("Splitsen langs de lange as"),
    RU("Делить вдоль длинной оси"), TR("Uzun eksen boyunca böl"));
SS_MSG(use_long_axis_split_help,
    EN("Split stretched splats along their long axis when adding detail, rather "
       "than splitting them evenly. Gives less blurry distant background in large "
       "outdoor scenes."),
    JA("細部を足すとき、細長いスプラットを均等にではなく長い軸に沿って分割しま"
       "す。広い屋外のシーンで、遠景のぼやけが減ります。"),
    ZH_HANS("在补充细节时，沿长轴分裂被拉长的泼溅，而不是均匀分裂。可以让大型"
            "户外场景中的远景背景不那么糊。"),
    ZH_HANT("在補充細節時，沿長軸分裂被拉長的潑濺，而不是均勻分裂。可以讓大型"
            "戶外場景中的遠景背景不那麼糊。"),
    KO("디테일을 더할 때 길쭉한 스플랫을 균등하게가 아니라 긴 축을 따라 나눕니"
       "다. 넓은 실외 장면에서 먼 배경이 덜 뭉개집니다."),
    DE("Gestreckte Splats beim Ergänzen von Detail entlang ihrer langen Achse "
       "teilen statt gleichmäßig. Ergibt in großen Außenszenen einen weniger "
       "unscharfen fernen Hintergrund."),
    FR("Diviser les splats étirés selon leur grand axe quand on ajoute du détail, "
       "au lieu de les couper également. Donne un arrière-plan lointain moins "
       "flou dans les grandes scènes extérieures."),
    ES("Dividir los splats estirados por su eje largo al añadir detalle, en vez "
       "de partirlos por igual. Da un fondo lejano menos borroso en grandes escenas "
       "exteriores."),
    PT("Dividir os splats esticados pelo seu eixo longo ao acrescentar detalhe, "
       "em vez de parti-los igualmente. Dá um fundo distante menos borrado em "
       "grandes cenas ao ar livre."),
    IT("Dividere gli splat allungati lungo il loro asse maggiore quando si aggiunge "
       "dettaglio, invece di tagliarli a metà uniforme. Dà uno sfondo lontano "
       "meno sfocato nelle grandi scene all'aperto."),
    NL("Uitgerekte splats bij het toevoegen van detail langs hun lange as splitsen "
       "in plaats van gelijk. Geeft een minder wazige verre achtergrond in grote "
       "buitenscènes."),
    RU("При добавлении деталей делить вытянутые сплаты вдоль длинной оси, а не "
       "пополам поровну. В больших уличных сценах дальний фон получается менее "
       "размытым."),
    TR("Ayrıntı eklerken uzamış splat'ları eşit bölmek yerine uzun eksenleri "
       "boyunca böler. Büyük dış mekân sahnelerinde uzak arka planı daha az bulanık "
       "yapar."));

SS_MSG(long_axis_split_opacity_k,
    EN("Opacity kept when splitting"), JA("分割後に残す不透明度"),
    ZH_HANS("分裂后保留的不透明度"), ZH_HANT("分裂後保留的不透明度"),
    KO("분할 후 남기는 불투명도"), DE("Beim Teilen erhaltene Deckkraft"),
    FR("Opacité conservée à la division"),
    ES("Opacidad conservada al dividir"), PT("Opacidade mantida ao dividir"),
    IT("Opacità mantenuta nella divisione"),
    NL("Dekking die bij splitsen behouden blijft"),
    RU("Непрозрачность, сохраняемая при делении"),
    TR("Bölünmede korunan saydamsızlık"));
SS_MSG(long_axis_split_opacity_k_help,
    EN("How much opacity each half keeps when a splat is split. Given as a starting "
       "value, a final value, and how many steps to move between them. Higher "
       "keeps the halves denser and sharper; lower encourages floaters to fade "
       "and relocate to where details are needed."),
    JA("スプラットを分割したとき、それぞれの半分がどれだけ不透明度を保つかです。"
       "開始値、終了値、その間を移る歩数の順に指定します。高いほど分割後も濃く"
       "はっきりしますが、低いほど浮遊物が薄れて、細部が必要な場所へ移りやすく"
       "なります。"),
    ZH_HANS("泼溅分裂时每一半保留多少不透明度。依次给出起始值、终止值，以及从"
            "前者过渡到后者所用的步数。数值越高，两半越致密清晰；越低则漂浮物"
            "越容易淡去，转移到需要细节的地方。"),
    ZH_HANT("潑濺分裂時每一半保留多少不透明度。依序給出起始值、終止值，以及從"
            "前者過渡到後者所用的步數。數值越高，兩半越緻密清晰；越低則漂浮物"
            "越容易淡去，轉移到需要細節的地方。"),
    KO("스플랫이 나뉠 때 각 반쪽이 불투명도를 얼마나 유지하는지입니다. 시작값"
       ", 최종값, 그리고 그 사이를 옮겨 가는 스텝 수 순으로 지정합니다. 값이 "
       "크면 반쪽들이 더 짙고 또렷하게 남고, 작으면 부유물이 흐려져 디테일이 "
       "필요한 곳으로 옮겨 갑니다."),
    DE("Wie viel Deckkraft jede Hälfte behält, wenn ein Splat geteilt wird. Angegeben "
       "als Startwert, Endwert und Zahl der Schritte für den Übergang. Höher "
       "hält die Hälften dichter und schärfer; niedriger lässt Schwebeteile eher "
       "verblassen und dorthin wandern, wo Detail gebraucht wird."),
    FR("Quelle opacité chaque moitié conserve quand un splat est divisé. Donné "
       "sous la forme valeur de départ, valeur finale, et nombre d'étapes pour "
       "passer de l'une à l'autre. Plus haut garde les moitiés plus denses et "
       "plus nettes ; plus bas encourage les flotteurs à s'estomper et à se déplacer "
       "là où le détail manque."),
    ES("Cuánta opacidad conserva cada mitad cuando se divide un splat. Se indica "
       "como valor inicial, valor final y número de pasos para pasar de uno a "
       "otro. Más alto mantiene las mitades más densas y nítidas; más bajo hace "
       "que los restos flotantes se desvanezcan y se trasladen a donde hace falta "
       "detalle."),
    PT("Quanta opacidade cada metade mantém quando um splat é dividido. Indicado "
       "como valor inicial, valor final e número de passos para passar de um "
       "ao outro. Mais alto mantém as metades mais densas e nítidas; mais baixo "
       "faz os resíduos flutuantes desvanecerem e se mudarem para onde falta "
       "detalhe."),
    IT("Quanta opacità conserva ogni metà quando uno splat viene diviso. Si indica "
       "come valore iniziale, valore finale e numero di passi per passare dall'uno "
       "all'altro. Più alto tiene le metà più dense e nitide; più basso fa sbiadire "
       "i frammenti fluttuanti e li sposta dove serve dettaglio."),
    NL("Hoeveel dekking elke helft houdt als een splat wordt gesplitst. Opgegeven "
       "als beginwaarde, eindwaarde en het aantal stappen om van de een naar "
       "de ander te gaan. Hoger houdt de helften dichter en scherper; lager laat "
       "zwevers vervagen en verhuizen naar waar detail nodig is."),
    RU("Сколько непрозрачности сохраняет каждая половина при делении сплата. "
       "Задаётся как начальное значение, конечное значение и число шагов перехода "
       "между ними. Больше — половины остаются плотнее и резче; меньше — «летающие» "
       "артефакты быстрее гаснут и перемещаются туда, где нужны детали."),
    TR("Bir splat bölündüğünde her yarının ne kadar saydamsızlık koruyacağı. "
       "Başlangıç değeri, son değer ve ikisi arasında geçilecek adım sayısı olarak "
       "verilir. Yüksek değerler yarıları daha yoğun ve keskin tutar; düşük değerler "
       "uçuşan artıkların solup ayrıntı gereken yere taşınmasını sağlar."));

SS_MSG(max_screen_size,
    EN("Maximum on-screen size"), JA("画面上の最大サイズ"),
    ZH_HANS("屏幕上的最大尺寸"), ZH_HANT("螢幕上的最大尺寸"),
    KO("화면상 최대 크기"), DE("Maximale Bildschirmgröße"),
    FR("Taille maximale à l'écran"), ES("Tamaño máximo en pantalla"),
    PT("Tamanho máximo na tela"), IT("Dimensione massima a schermo"),
    NL("Maximale schermgrootte"), RU("Максимальный размер на экране"),
    TR("Ekrandaki en büyük boyut"));
SS_MSG(max_screen_size_help,
    EN("Shrink splats that cover more than this share of the screen instead of "
       "letting them stay huge. Keeps big blobby splats from smearing across "
       "the image."),
    JA("画面のこの割合より大きく写るスプラットを、そのままにせず小さくします。"
       "大きなにじんだスプラットが画面いっぱいに広がるのを防ぎます。"),
    ZH_HANS("把占屏幕比例超过该值的泼溅缩小，而不是任由它们保持巨大。可以避免"
            "大团泼溅在画面上糊开。"),
    ZH_HANT("把佔螢幕比例超過該值的潑濺縮小，而不是任由它們保持巨大。可以避免"
            "大團潑濺在畫面上糊開。"),
    KO("화면에서 이 비율보다 크게 차지하는 스플랫을 그대로 두지 않고 줄입니다"
       ". 큰 덩어리 스플랫이 화면에 번지는 것을 막습니다."),
    DE("Splats, die mehr als diesen Anteil des Bildschirms bedecken, verkleinern, "
       "statt sie riesig zu lassen. Verhindert, dass große, klumpige Splats über "
       "das Bild schmieren."),
    FR("Réduire les splats qui couvrent plus que cette part de l'écran au lieu "
       "de les laisser énormes. Empêche de gros splats pâteux de baver sur l'image."),
    ES("Reducir los splats que cubren más de esta proporción de la pantalla en "
       "vez de dejarlos enormes. Evita que splats grandes y pastosos se emborronen "
       "por la imagen."),
    PT("Reduzir os splats que cobrem mais do que esta proporção da tela em vez "
       "de deixá-los enormes. Evita que splats grandes e pastosos borrem a imagem."),
    IT("Ridurre gli splat che coprono più di questa quota dello schermo invece "
       "di lasciarli enormi. Evita che splat grandi e pastosi si spalmino sull'immagine."),
    NL("Splats die meer dan dit deel van het scherm bedekken verkleinen in plaats "
       "van ze reusachtig te laten. Voorkomt dat grote, klonterige splats over "
       "het beeld uitsmeren."),
    RU("Уменьшать сплаты, занимающие больше этой доли экрана, вместо того чтобы "
       "оставлять их огромными. Не даёт крупным расплывчатым сплатам размазываться "
       "по изображению."),
    TR("Ekranın bu orandan fazlasını kaplayan splat'ları kocaman bırakmak yerine "
       "küçültür. Büyük, lapa gibi splat'ların görüntüye bulaşmasını engeller."));

SS_MSG(max_screen_size_clip_hardness,
    EN("On-screen size limit hardness"), JA("画面サイズ制限の強さ"),
    ZH_HANS("屏幕尺寸限制的强度"), ZH_HANT("螢幕尺寸限制的強度"),
    KO("화면 크기 제한 강도"), DE("Härte der Bildschirmgrößen-Grenze"),
    FR("Fermeté de la limite de taille à l'écran"),
    ES("Dureza del límite de tamaño en pantalla"),
    PT("Rigidez do limite de tamanho na tela"),
    IT("Rigidità del limite di dimensione a schermo"),
    NL("Hardheid van de schermgroottelimiet"),
    RU("Жёсткость предела размера на экране"),
    TR("Ekran boyutu sınırının katılığı"));
SS_MSG(max_screen_size_clip_hardness_help,
    EN("How firmly the screen-size limit is enforced, from 1 upward. Higher clamps "
       "oversized splats decisively; lower eases them down."),
    JA("画面サイズの制限をどれだけ強く効かせるかです。1 以上で指定します。高い"
       "ほど大きすぎるスプラットをきっぱり抑え、低いほど緩やかに縮めます。"),
    ZH_HANS("屏幕尺寸限制执行得有多严格，取值从 1 起。数值越高越果断地夹住过大"
            "的泼溅；越低则缓慢地把它们收下去。"),
    ZH_HANT("螢幕尺寸限制執行得有多嚴格，取值從 1 起。數值越高越果斷地夾住過大"
            "的潑濺；越低則緩慢地把它們收下去。"),
    KO("화면 크기 제한을 얼마나 강하게 적용할지이며 1 이상으로 지정합니다. 값"
       "이 크면 지나치게 큰 스플랫을 단호하게 잘라내고, 작으면 서서히 줄입니다"
       "."),
    DE("Wie streng die Bildschirmgrößen-Grenze durchgesetzt wird, ab 1 aufwärts. "
       "Höher begrenzt übergroße Splats entschieden; niedriger führt sie sanft "
       "herunter."),
    FR("Avec quelle fermeté la limite de taille à l'écran est appliquée, à partir "
       "de 1. Plus haut ramène brutalement les splats trop grands ; plus bas "
       "les fait redescendre en douceur."),
    ES("Con qué firmeza se aplica el límite de tamaño en pantalla, a partir de "
       "1. Más alto recorta los splats demasiado grandes de forma tajante; más "
       "bajo los va bajando con suavidad."),
    PT("Com que firmeza o limite de tamanho na tela é aplicado, a partir de 1. "
       "Mais alto corta os splats grandes demais de forma decidida; mais baixo "
       "os reduz suavemente."),
    IT("Con quanta fermezza si applica il limite di dimensione a schermo, da "
       "1 in su. Più alto taglia gli splat troppo grandi in modo netto; più basso "
       "li riporta giù con dolcezza."),
    NL("Hoe streng de schermgroottelimiet wordt afgedwongen, vanaf 1. Hoger kapt "
       "te grote splats resoluut af; lager brengt ze geleidelijk omlaag."),
    RU("Насколько жёстко действует предел размера на экране; значения от 1 и "
       "выше. Больше — переросшие сплаты обрезаются решительно; меньше — плавно "
       "уменьшаются."),
    TR("Ekran boyutu sınırının ne kadar katı uygulandığı; 1'den başlar. Yüksek "
       "değerler aşırı büyük splat'ları kararlıca kırpar; düşük değerler onları "
       "yavaşça küçültür."));
SS_MSG(max_screen_size_penalty,
    EN("On-screen size pressure"), JA("画面サイズ制限の効き"),
    ZH_HANS("屏幕尺寸限制的推力"), ZH_HANT("螢幕尺寸限制的推力"),
    KO("화면 크기 제한의 압력"), DE("Druck der Bildschirmgrößen-Grenze"),
    FR("Pression de la limite de taille à l'écran"),
    ES("Presión del límite de tamaño en pantalla"),
    PT("Pressão do limite de tamanho na tela"),
    IT("Pressione del limite di dimensione a schermo"),
    NL("Druk van de schermgroottelimiet"),
    RU("Нажим предела размера на экране"),
    TR("Ekran boyutu sınırının baskısı"));
SS_MSG(max_screen_size_penalty_help,
    EN("How hard the optimizer pushes a splat back under the on-screen size "
       "limit, per doubling past it. This push is weighed against the photo "
       "error, so a splat that has to be large -- sky, a wide flat wall -- "
       "can stay large. 0 turns it off and clamps oversized splats on every "
       "step instead."),
    JA("画面サイズの制限を超えたスプラットを、制限の 2 倍ごとにどれだけ強く小"
       "さくするかです。この力は写真との誤差と釣り合うので、空や広い壁のように"
       "大きいままでよいスプラットは残ります。0 にすると力を切り、毎ステップき"
       "っぱり抑える動きに戻ります。"),
    ZH_HANS("超过屏幕尺寸限制的泼溅，每超出一倍要被推回多大的力。这个力和照片"
            "误差相权衡，所以天空或大片平墙这类本该很大的泼溅可以保持很大。0 "
            "关掉这个力，改为每一步都夹住过大的泼溅。"),
    ZH_HANT("超過螢幕尺寸限制的潑濺，每超出一倍要被推回多大的力。這個力和照片"
            "誤差相權衡，所以天空或大片平牆這類本該很大的潑濺可以保持很大。0 "
            "關掉這個力，改為每一步都夾住過大的潑濺。"),
    KO("화면 크기 제한을 넘은 스플랫을 제한의 두 배마다 얼마나 세게 도로 누를"
       "지입니다. 이 힘은 사진 오차와 견주므로 하늘이나 넓은 벽처럼 커야 하는 "
       "스플랫은 큰 채로 남습니다. 0은 힘을 끄고 매 단계마다 잘라내는 방식으로 "
       "돌아갑니다."),
    DE("Wie stark der Optimierer einen Splat unter die Bildschirmgrößen-Grenz"
       "e zurückdrückt, je Verdopplung darüber. Dieser Druck wird gegen den "
       "Bildfehler abgewogen, sodass ein Splat, der groß sein muss -- Himmel, "
       "eine breite flache Wand -- groß bleiben darf. 0 schaltet ihn ab und "
       "begrenzt übergroße Splats stattdessen in jedem Schritt."),
    FR("Avec quelle force l'optimiseur ramène un splat sous la limite de "
       "taille à l'écran, par doublement au-delà. Cette pression est mise en "
       "balance avec l'erreur photométrique, si bien qu'un splat qui doit "
       "être grand -- le ciel, un large mur plat -- peut le rester. 0 la "
       "coupe et rabat les splats trop grands à chaque pas."),
    ES("Con cuánta fuerza el optimizador devuelve un splat por debajo del "
       "límite de tamaño en pantalla, por cada duplicación por encima. Esta "
       "presión se sopesa contra el error fotométrico, así que un splat que "
       "debe ser grande -- el cielo, un muro plano y ancho -- puede seguir "
       "siéndolo. 0 la apaga y recorta los splats demasiado grandes en cada "
       "paso."),
    PT("Com quanta força o otimizador puxa um splat de volta para baixo do "
       "limite de tamanho na tela, por duplicação acima dele. Essa pressão é "
       "pesada contra o erro fotométrico, então um splat que precisa ser "
       "grande -- o céu, uma parede plana e larga -- pode continuar grande. 0 "
       "desliga a pressão e corta os splats grandes demais a cada passo."),
    IT("Con quanta forza l'ottimizzatore riporta uno splat sotto il limite di "
       "dimensione a schermo, per ogni raddoppio oltre di esso. Questa "
       "pressione viene pesata contro l'errore fotometrico, così uno splat "
       "che deve essere grande -- il cielo, un muro piatto e largo -- può "
       "restare grande. 0 la spegne e taglia gli splat troppo grandi a ogni "
       "passo."),
    NL("Hoe hard de optimalisator een splat terugduwt onder de "
       "schermgroottelimiet, per verdubbeling erboven. Deze druk wordt "
       "afgewogen tegen de fotofout, zodat een splat die groot moet zijn -- "
       "lucht, een brede vlakke muur -- groot mag blijven. 0 zet hem uit en "
       "kapt te grote splats in plaats daarvan elke stap af."),
    RU("Насколько сильно оптимизатор возвращает сплат под предел размера на "
       "экране за каждое удвоение сверх него. Этот нажим уравновешивается "
       "ошибкой по снимку, поэтому сплат, которому положено быть большим -- "
       "небо, широкая плоская стена, -- остаётся большим. 0 отключает его и "
       "вместо этого обрезает слишком крупные сплаты на каждом шаге."),
    TR("İyileştiricinin bir splat'ı ekran boyutu sınırının altına ne kadar "
       "sert geri ittiği; sınırın her katına bir kez. Bu baskı fotoğraf "
       "hatasıyla tartılır, böylece büyük olmak zorunda olan bir splat -- "
       "gökyüzü, geniş düz bir duvar -- büyük kalabilir. 0 baskıyı kapatır ve "
       "bunun yerine her adımda aşırı büyük splat'ları kırpar."));

SS_MSG(max_world_size,
    EN("Maximum world size"), JA("ワールド上の最大サイズ"),
    ZH_HANS("世界坐标下的最大尺寸"), ZH_HANT("世界座標下的最大尺寸"),
    KO("월드 좌표 최대 크기"), DE("Maximale Größe in Weltkoordinaten"),
    FR("Taille maximale en unités du monde"),
    ES("Tamaño máximo en unidades del mundo"),
    PT("Tamanho máximo em unidades do mundo"),
    IT("Dimensione massima in unità del mondo"),
    NL("Maximale grootte in wereldeenheden"),
    RU("Максимальный размер в мировых единицах"),
    TR("Dünya birimlerinde en büyük boyut"));
SS_MSG(max_world_size_help,
    EN("Shrink splats bigger than this in world units. Set it when huge floaters "
       "show up in the distance in large indoor spaces."),
    JA("ワールド単位でこれより大きいスプラットを小さくします。広い屋内空間で遠"
       "くに巨大な浮遊物が出るときに設定してください。"),
    ZH_HANS("把世界坐标下大于该值的泼溅缩小。当宽敞的室内空间远处出现巨大漂浮"
            "物时可以设置它。"),
    ZH_HANT("把世界座標下大於該值的潑濺縮小。當寬敞的室內空間遠處出現巨大漂浮"
            "物時可以設定它。"),
    KO("월드 단위로 이보다 큰 스플랫을 줄입니다. 넓은 실내 공간에서 멀리 거대"
       "한 부유물이 나타날 때 설정하십시오."),
    DE("Splats verkleinern, die in Welteinheiten größer als dieser Wert sind. "
       "Setzen, wenn in großen Innenräumen riesige Schwebeteile in der Ferne "
       "auftauchen."),
    FR("Réduire les splats plus grands que cette valeur en unités du monde. À "
       "régler quand d'énormes flotteurs apparaissent au loin dans de grands "
       "espaces intérieurs."),
    ES("Reducir los splats mayores que este valor en unidades del mundo. Ajústelo "
       "cuando aparezcan restos flotantes enormes a lo lejos en grandes espacios "
       "interiores."),
    PT("Reduzir os splats maiores que este valor em unidades do mundo. Ajuste-o "
       "quando aparecerem resíduos flutuantes enormes ao longe em grandes espaços "
       "interiores."),
    IT("Ridurre gli splat più grandi di questo valore in unità del mondo. Da "
       "impostare quando compaiono enormi frammenti fluttuanti in lontananza "
       "in grandi spazi interni."),
    NL("Splats verkleinen die in wereldeenheden groter zijn dan deze waarde. "
       "Stel dit in als er in grote binnenruimtes reusachtige zwevers in de verte "
       "opduiken."),
    RU("Уменьшать сплаты крупнее этого значения в мировых единицах. Задавайте, "
       "когда в больших помещениях вдали появляются огромные «летающие» артефакты."),
    TR("Dünya biriminde bundan büyük splat'ları küçültür. Geniş iç mekânlarda "
       "uzakta devasa uçuşan artıklar belirdiğinde ayarlayın."));


// ===========================================================================
// Image Loss
// ===========================================================================

SS_MSG(ssim_lambda,
    EN("Structure weight (SSIM)"), JA("構造の重み（SSIM）"),
    ZH_HANS("结构权重（SSIM）"), ZH_HANT("結構權重（SSIM）"),
    KO("구조 가중치(SSIM)"), DE("Gewicht der Struktur (SSIM)"),
    FR("Poids de la structure (SSIM)"), ES("Peso de la estructura (SSIM)"),
    PT("Peso da estrutura (SSIM)"), IT("Peso della struttura (SSIM)"),
    NL("Gewicht van de structuur (SSIM)"), RU("Вес структуры (SSIM)"),
    TR("Yapı ağırlığı (SSIM)"));
SS_MSG(ssim_lambda_help,
    EN("How much the loss cares about local structure instead of exact pixel "
       "color. Higher brings out fine texture and high-frequency detail; lower "
       "gives a smoother, less noisy background, which sometimes looks better "
       "in outdoor scenes."),
    JA("損失が、画素ごとの正確な色よりも局所の構造をどれだけ重視するかです。高"
       "いほど細かな質感や高周波の細部が出ます。低いほど背景がなめらかでノイズ"
       "が少なくなり、屋外のシーンではそのほうがよく見えることがあります。"),
    ZH_HANS("损失在多大程度上关注局部结构而非逐像素的精确颜色。数值越高，细腻"
            "纹理和高频细节越突出；越低则背景更平滑、噪点更少，在户外场景中有"
            "时更好看。"),
    ZH_HANT("損失在多大程度上關注局部結構而非逐像素的精確顏色。數值越高，細緻"
            "紋理和高頻細節越突出；越低則背景更平滑、雜訊更少，在戶外場景中有"
            "時更好看。"),
    KO("손실이 픽셀별 정확한 색보다 국소 구조를 얼마나 중시할지입니다. 값이 크"
       "면 미세한 질감과 고주파 디테일이 살아나고, 작으면 배경이 더 매끄럽고 "
       "노이즈가 적어져 실외 장면에서는 그편이 더 나아 보이기도 합니다."),
    DE("Wie stark der Verlust auf lokale Struktur statt auf die exakte Pixelfarbe "
       "achtet. Höher holt feine Textur und hochfrequentes Detail heraus; niedriger "
       "ergibt einen glatteren, rauschärmeren Hintergrund, was in Außenszenen "
       "manchmal besser aussieht."),
    FR("Dans quelle mesure la fonction de coût s'attache à la structure locale "
       "plutôt qu'à la couleur exacte des pixels. Plus haut fait ressortir la "
       "texture fine et les détails à haute fréquence ; plus bas donne un arrière-plan "
       "plus lisse et moins bruité, ce qui rend parfois mieux en extérieur."),
    ES("Cuánto atiende la función de pérdida a la estructura local en vez de "
       "al color exacto de cada píxel. Más alto realza la textura fina y el detalle "
       "de alta frecuencia; más bajo da un fondo más suave y menos ruidoso, que "
       "a veces queda mejor en exteriores."),
    PT("O quanto a função de perda se importa com a estrutura local em vez da "
       "cor exata de cada pixel. Mais alto realça a textura fina e o detalhe "
       "de alta frequência; mais baixo dá um fundo mais suave e menos ruidoso, "
       "que às vezes fica melhor em cenas ao ar livre."),
    IT("Quanto la funzione di perdita si interessa alla struttura locale invece "
       "che al colore esatto dei pixel. Più alto fa emergere la texture fine "
       "e il dettaglio ad alta frequenza; più basso dà uno sfondo più liscio "
       "e meno rumoroso, che a volte rende meglio negli esterni."),
    NL("Hoeveel de verliesfunctie let op lokale structuur in plaats van op de "
       "exacte pixelkleur. Hoger haalt fijne textuur en hoogfrequent detail naar "
       "voren; lager geeft een gladdere, minder ruizige achtergrond, wat in buitenscènes "
       "soms beter oogt."),
    RU("Насколько функция потерь смотрит на локальную структуру, а не на точный "
       "цвет пикселя. Больше — сильнее проявляется мелкая фактура и высокочастотные "
       "детали; меньше — фон глаже и менее шумный, что на улице иногда выглядит "
       "лучше."),
    TR("Kaybın piksel rengindeki kesinlik yerine yerel yapıya ne kadar önem verdiği. "
       "Yüksek değerler ince dokuyu ve yüksek frekanslı ayrıntıyı öne çıkarır; "
       "düşük değerler daha pürüzsüz, daha az gürültülü bir arka plan verir ve "
       "bu dış mekân sahnelerinde bazen daha iyi görünür."));

SS_MSG(l1_weight,
    EN("Color error weight (L1)"), JA("色の誤差の重み（L1）"),
    ZH_HANS("颜色误差权重（L1）"), ZH_HANT("顏色誤差權重（L1）"),
    KO("색 오차 가중치(L1)"), DE("Gewicht des Farbfehlers (L1)"),
    FR("Poids de l'erreur de couleur (L1)"),
    ES("Peso del error de color (L1)"), PT("Peso do erro de cor (L1)"),
    IT("Peso dell'errore di colore (L1)"),
    NL("Gewicht van de kleurfout (L1)"), RU("Вес ошибки цвета (L1)"),
    TR("Renk hatası ağırlığı (L1)"));
SS_MSG(l1_weight_help,
    EN("Weight of plain per-pixel color error. This is the main term driving "
       "color accuracy."),
    JA("画素ごとの単純な色の誤差の重みです。色の正確さを支える主な項です。"),
    ZH_HANS("逐像素普通颜色误差的权重。这是驱动颜色准确性的主要项。"),
    ZH_HANT("逐像素普通顏色誤差的權重。這是驅動顏色準確性的主要項。"),
    KO("픽셀별 단순 색 오차의 가중치입니다. 색 정확도를 이끄는 주된 항입니다."),
    DE("Gewicht des einfachen Farbfehlers pro Pixel. Das ist der Hauptterm für "
       "die Farbtreue."),
    FR("Poids de l'erreur de couleur simple, pixel par pixel. C'est le terme "
       "principal qui porte la fidélité des couleurs."),
    ES("Peso del error de color simple, píxel a píxel. Es el término principal "
       "que impulsa la fidelidad del color."),
    PT("Peso do erro de cor simples, pixel a pixel. É o termo principal que conduz "
       "a fidelidade da cor."),
    IT("Peso dell'errore di colore semplice, pixel per pixel. È il termine principale "
       "che guida la fedeltà del colore."),
    NL("Gewicht van de gewone kleurfout per pixel. Dit is de hoofdterm die de "
       "kleurgetrouwheid stuurt."),
    RU("Вес обычной попиксельной ошибки цвета. Это главное слагаемое, отвечающее "
       "за точность цвета."),
    TR("Piksel başına düz renk hatasının ağırlığı. Renk doğruluğunu sürükleyen "
       "ana terimdir."));

SS_MSG(l2_weight,
    EN("Squared color error weight (L2)"), JA("色の二乗誤差の重み（L2）"),
    ZH_HANS("颜色平方误差权重（L2）"), ZH_HANT("顏色平方誤差權重（L2）"),
    KO("색 제곱 오차 가중치(L2)"),
    DE("Gewicht des quadratischen Farbfehlers (L2)"),
    FR("Poids de l'erreur de couleur au carré (L2)"),
    ES("Peso del error de color al cuadrado (L2)"),
    PT("Peso do erro de cor ao quadrado (L2)"),
    IT("Peso dell'errore di colore al quadrato (L2)"),
    NL("Gewicht van de kwadratische kleurfout (L2)"),
    RU("Вес квадратичной ошибки цвета (L2)"),
    TR("Karesel renk hatası ağırlığı (L2)"));
SS_MSG(l2_weight_help,
    EN("Weight of squared per-pixel color error. It punishes large mistakes harder "
       "than l1_weight, which makes color settle faster but also chases outliers "
       "such as moving objects."),
    JA("画素ごとの色の二乗誤差の重みです。l1_weight より大きな誤りを強く罰する"
       "ので色は早く落ち着きますが、動く物体などの外れ値も追いかけてしまいます。"),
    ZH_HANS("逐像素颜色平方误差的权重。它比 l1_weight 更严厉地惩罚大误差，使颜"
            "色更快稳定，但也会去追逐移动物体之类的离群值。"),
    ZH_HANT("逐像素顏色平方誤差的權重。它比 l1_weight 更嚴厲地懲罰大誤差，使顏"
            "色更快穩定，但也會去追逐移動物體之類的離群值。"),
    KO("픽셀별 색 제곱 오차의 가중치입니다. l1_weight보다 큰 오류를 더 세게 벌"
       "하므로 색이 빨리 자리 잡지만, 움직이는 물체 같은 이상치도 쫓아갑니다."),
    DE("Gewicht des quadratischen Farbfehlers pro Pixel. Es bestraft große Fehler "
       "härter als l1_weight, wodurch sich die Farbe schneller einpendelt, verfolgt "
       "aber auch Ausreißer wie bewegte Objekte."),
    FR("Poids de l'erreur de couleur au carré, pixel par pixel. Elle punit les "
       "grosses erreurs plus sévèrement que l1_weight, ce qui stabilise la couleur "
       "plus vite mais poursuit aussi les valeurs aberrantes comme les objets "
       "en mouvement."),
    ES("Peso del error de color al cuadrado, píxel a píxel. Castiga los errores "
       "grandes con más dureza que l1_weight, lo que asienta el color antes, "
       "pero también persigue valores atípicos como los objetos en movimiento."),
    PT("Peso do erro de cor ao quadrado, pixel a pixel. Pune erros grandes com "
       "mais rigor que l1_weight, o que assenta a cor mais depressa, mas também "
       "persegue valores discrepantes como objetos em movimento."),
    IT("Peso dell'errore di colore al quadrato, pixel per pixel. Punisce gli "
       "errori grandi più severamente di l1_weight, il che fa assestare il colore "
       "prima, ma insegue anche valori anomali come gli oggetti in movimento."),
    NL("Gewicht van de kwadratische kleurfout per pixel. Het straft grote fouten "
       "harder af dan l1_weight, waardoor de kleur sneller tot rust komt, maar "
       "het jaagt ook op uitschieters zoals bewegende voorwerpen."),
    RU("Вес квадратичной попиксельной ошибки цвета. Он наказывает большие промахи "
       "сильнее, чем l1_weight, поэтому цвет устанавливается быстрее, но заодно "
       "гонится за выбросами вроде движущихся объектов."),
    TR("Piksel başına karesel renk hatasının ağırlığı. Büyük hataları l1_weight'ten "
       "daha sert cezalandırır; bu, rengin daha çabuk oturmasını sağlar ama hareketli "
       "nesneler gibi aykırı değerlerin de peşine düşer."));

SS_MSG(l1_weight_y,
    EN("Brightness error weight"), JA("明るさの誤差の重み"),
    ZH_HANS("亮度误差权重"), ZH_HANT("亮度誤差權重"), KO("밝기 오차 가중치"),
    DE("Gewicht des Helligkeitsfehlers"),
    FR("Poids de l'erreur de luminosité"), ES("Peso del error de brillo"),
    PT("Peso do erro de brilho"), IT("Peso dell'errore di luminosità"),
    NL("Gewicht van de helderheidsfout"), RU("Вес ошибки яркости"),
    TR("Parlaklık hatası ağırlığı"));
SS_MSG(l1_weight_y_help,
    EN("Extra weight on brightness error alone, ignoring hue. Raising it favors "
       "luminance detail over color accuracy."),
    JA("色合いを無視して、明るさの誤差だけに追加の重みをかけます。上げると色の"
       "正確さより輝度の細部が優先されます。"),
    ZH_HANS("只对亮度误差施加额外权重，忽略色相。调高会让亮度细节优先于颜色准"
            "确性。"),
    ZH_HANT("只對亮度誤差施加額外權重，忽略色相。調高會讓亮度細節優先於顏色準"
            "確性。"),
    KO("색상은 무시하고 밝기 오차에만 가중치를 더 줍니다. 올리면 색 정확도보다"
       " 휘도 디테일이 우선됩니다."),
    DE("Zusätzliches Gewicht allein auf den Helligkeitsfehler, ohne den Farbton. "
       "Erhöhen bevorzugt Luminanzdetail gegenüber Farbtreue."),
    FR("Poids supplémentaire sur la seule erreur de luminosité, en ignorant la "
       "teinte. L'augmenter privilégie le détail de luminance sur la fidélité "
       "des couleurs."),
    ES("Peso adicional solo sobre el error de brillo, ignorando el tono. Subirlo "
       "favorece el detalle de luminancia frente a la fidelidad del color."),
    PT("Peso adicional apenas sobre o erro de brilho, ignorando o matiz. Aumentá-lo "
       "favorece o detalhe de luminância em vez da fidelidade da cor."),
    IT("Peso aggiuntivo sul solo errore di luminosità, ignorando la tinta. Alzarlo "
       "privilegia il dettaglio di luminanza rispetto alla fedeltà del colore."),
    NL("Extra gewicht op alleen de helderheidsfout, met voorbijgaan aan de tint. "
       "Verhogen bevoordeelt luminantiedetail boven kleurgetrouwheid."),
    RU("Дополнительный вес только на ошибку яркости, без учёта оттенка. Повышение "
       "отдаёт предпочтение яркостным деталям перед точностью цвета."),
    TR("Yalnızca parlaklık hatasına, tonu göz ardı ederek verilen ek ağırlık. "
       "Yükseltmek renk doğruluğu yerine parlaklık ayrıntısını öne çıkarır."));

SS_MSG(l2_weight_y,
    EN("Squared brightness error weight"), JA("明るさの二乗誤差の重み"),
    ZH_HANS("亮度平方误差权重"), ZH_HANT("亮度平方誤差權重"),
    KO("밝기 제곱 오차 가중치"),
    DE("Gewicht des quadratischen Helligkeitsfehlers"),
    FR("Poids de l'erreur de luminosité au carré"),
    ES("Peso del error de brillo al cuadrado"),
    PT("Peso do erro de brilho ao quadrado"),
    IT("Peso dell'errore di luminosità al quadrato"),
    NL("Gewicht van de kwadratische helderheidsfout"),
    RU("Вес квадратичной ошибки яркости"),
    TR("Karesel parlaklık hatası ağırlığı"));
SS_MSG(l2_weight_y_help,
    EN("Extra weight on squared brightness error. Same idea as l1_weight_y, but "
       "large brightness mistakes count for much more."),
    JA("明るさの二乗誤差に追加の重みをかけます。l1_weight_y と同じ考え方ですが、"
       "大きな明るさの誤りがはるかに重く数えられます。"),
    ZH_HANS("对亮度平方误差施加额外权重。思路与 l1_weight_y 相同，但大的亮度误"
            "差被计得重得多。"),
    ZH_HANT("對亮度平方誤差施加額外權重。思路與 l1_weight_y 相同，但大的亮度誤"
            "差被計得重得多。"),
    KO("밝기 제곱 오차에 가중치를 더 줍니다. l1_weight_y와 같은 발상이지만 큰"
       " 밝기 오류가 훨씬 무겁게 셈해집니다."),
    DE("Zusätzliches Gewicht auf den quadratischen Helligkeitsfehler. Derselbe "
       "Gedanke wie l1_weight_y, aber große Helligkeitsfehler zählen weit mehr."),
    FR("Poids supplémentaire sur l'erreur de luminosité au carré. Même idée que "
       "l1_weight_y, mais les grosses erreurs de luminosité comptent bien plus."),
    ES("Peso adicional sobre el error de brillo al cuadrado. La misma idea que "
       "l1_weight_y, pero los errores grandes de brillo pesan mucho más."),
    PT("Peso adicional sobre o erro de brilho ao quadrado. A mesma ideia de l1_weight_y, "
       "mas os erros grandes de brilho contam bem mais."),
    IT("Peso aggiuntivo sull'errore di luminosità al quadrato. Stessa idea di "
       "l1_weight_y, ma gli errori grandi di luminosità contano molto di più."),
    NL("Extra gewicht op de kwadratische helderheidsfout. Hetzelfde idee als "
       "l1_weight_y, maar grote helderheidsfouten tellen veel zwaarder."),
    RU("Дополнительный вес на квадратичную ошибку яркости. Та же идея, что и "
       "l1_weight_y, но крупные промахи по яркости весят гораздо больше."),
    TR("Karesel parlaklık hatasına verilen ek ağırlık. l1_weight_y ile aynı düşünce, "
       "ama büyük parlaklık hataları çok daha ağır sayılır."));

SS_MSG(l2_weight_u,
    EN("Blue-yellow error weight"), JA("青と黄の誤差の重み"),
    ZH_HANS("蓝黄误差权重"), ZH_HANT("藍黃誤差權重"),
    KO("파랑-노랑 오차 가중치"), DE("Gewicht des Blau-Gelb-Fehlers"),
    FR("Poids de l'erreur bleu-jaune"), ES("Peso del error azul-amarillo"),
    PT("Peso do erro azul-amarelo"), IT("Peso dell'errore blu-giallo"),
    NL("Gewicht van de blauw-gele fout"), RU("Вес ошибки «синий — жёлтый»"),
    TR("Mavi-sarı hata ağırlığı"));
SS_MSG(l2_weight_u_help,
    EN("Extra weight on the blue-versus-yellow color error. Raising it tightens "
       "hue accuracy in that direction at the expense of detail elsewhere."),
    JA("青と黄の方向の色の誤差に追加の重みをかけます。上げるとその方向の色合い"
       "は正確になりますが、ほかの細部が犠牲になります。"),
    ZH_HANS("对蓝黄方向的颜色误差施加额外权重。调高会让该方向的色相更准，但会"
            "牺牲其他细节。"),
    ZH_HANT("對藍黃方向的顏色誤差施加額外權重。調高會讓該方向的色相更準，但會"
            "犧牲其他細節。"),
    KO("파랑-노랑 방향의 색 오차에 가중치를 더 줍니다. 올리면 그 방향의 색상은"
       " 정확해지지만 다른 디테일이 희생됩니다."),
    DE("Zusätzliches Gewicht auf den Farbfehler in Blau-Gelb-Richtung. Erhöhen "
       "schärft den Farbton in dieser Richtung auf Kosten von Detail anderswo."),
    FR("Poids supplémentaire sur l'erreur de couleur dans l'axe bleu-jaune. L'augmenter "
       "resserre la teinte dans cette direction au détriment du détail ailleurs."),
    ES("Peso adicional sobre el error de color en el eje azul-amarillo. Subirlo "
       "ajusta el tono en esa dirección a costa de detalle en otros sitios."),
    PT("Peso adicional sobre o erro de cor no eixo azul-amarelo. Aumentá-lo aperta "
       "o matiz nessa direção às custas de detalhe noutros lugares."),
    IT("Peso aggiuntivo sull'errore di colore nell'asse blu-giallo. Alzarlo stringe "
       "la tinta in quella direzione a scapito del dettaglio altrove."),
    NL("Extra gewicht op de kleurfout in de blauw-gele richting. Verhogen scherpt "
       "de tint in die richting aan, ten koste van detail elders."),
    RU("Дополнительный вес на ошибку цвета по оси «синий — жёлтый». Повышение "
       "подтягивает оттенок в этом направлении за счёт деталей в другом."),
    TR("Mavi-sarı ekseninde renk hatasına verilen ek ağırlık. Yükseltmek o yöndeki "
       "ton doğruluğunu artırır, karşılığında başka yerlerdeki ayrıntıdan verir."));

SS_MSG(l2_weight_v,
    EN("Red-cyan error weight"), JA("赤とシアンの誤差の重み"),
    ZH_HANS("红青误差权重"), ZH_HANT("紅青誤差權重"),
    KO("빨강-청록 오차 가중치"), DE("Gewicht des Rot-Cyan-Fehlers"),
    FR("Poids de l'erreur rouge-cyan"), ES("Peso del error rojo-cian"),
    PT("Peso do erro vermelho-ciano"), IT("Peso dell'errore rosso-ciano"),
    NL("Gewicht van de rood-cyaanfout"),
    RU("Вес ошибки «красный — голубой»"),
    TR("Kırmızı-camgöbeği hata ağırlığı"));
SS_MSG(l2_weight_v_help,
    EN("Extra weight on the red-versus-cyan color error. Raising it tightens "
       "hue accuracy in that direction at the expense of detail elsewhere."),
    JA("赤とシアンの方向の色の誤差に追加の重みをかけます。上げるとその方向の色"
       "合いは正確になりますが、ほかの細部が犠牲になります。"),
    ZH_HANS("对红青方向的颜色误差施加额外权重。调高会让该方向的色相更准，但会"
            "牺牲其他细节。"),
    ZH_HANT("對紅青方向的顏色誤差施加額外權重。調高會讓該方向的色相更準，但會"
            "犧牲其他細節。"),
    KO("빨강-청록 방향의 색 오차에 가중치를 더 줍니다. 올리면 그 방향의 색상은"
       " 정확해지지만 다른 디테일이 희생됩니다."),
    DE("Zusätzliches Gewicht auf den Farbfehler in Rot-Cyan-Richtung. Erhöhen "
       "schärft den Farbton in dieser Richtung auf Kosten von Detail anderswo."),
    FR("Poids supplémentaire sur l'erreur de couleur dans l'axe rouge-cyan. L'augmenter "
       "resserre la teinte dans cette direction au détriment du détail ailleurs."),
    ES("Peso adicional sobre el error de color en el eje rojo-cian. Subirlo ajusta "
       "el tono en esa dirección a costa de detalle en otros sitios."),
    PT("Peso adicional sobre o erro de cor no eixo vermelho-ciano. Aumentá-lo "
       "aperta o matiz nessa direção às custas de detalhe noutros lugares."),
    IT("Peso aggiuntivo sull'errore di colore nell'asse rosso-ciano. Alzarlo "
       "stringe la tinta in quella direzione a scapito del dettaglio altrove."),
    NL("Extra gewicht op de kleurfout in de rood-cyaanrichting. Verhogen scherpt "
       "de tint in die richting aan, ten koste van detail elders."),
    RU("Дополнительный вес на ошибку цвета по оси «красный — голубой». Повышение "
       "подтягивает оттенок в этом направлении за счёт деталей в другом."),
    TR("Kırmızı-camgöbeği ekseninde renk hatasına verilen ek ağırlık. Yükseltmek "
       "o yöndeki ton doğruluğunu artırır, karşılığında başka yerlerdeki ayrıntıdan "
       "verir."));

SS_MSG(loss_scale_min_pixels,
    EN("Smallest loss scale, in pixels"), JA("最小スケールの短辺画素数"),
    ZH_HANS("最小尺度的短边像素数"), ZH_HANT("最小尺度的短邊像素數"),
    KO("가장 작은 배율의 짧은 변 픽셀 수"),
    DE("Kleinste Verlustskala in Pixeln"),
    FR("Plus petite échelle de perte, en pixels"),
    ES("Escala de pérdida más pequeña, en píxeles"),
    PT("Menor escala de perda, em pixels"),
    IT("Scala di perdita più piccola, in pixel"),
    NL("Kleinste verliesschaal, in pixels"),
    RU("Наименьший масштаб потерь, в пикселях"),
    TR("En küçük kayıp ölçeği, piksel"));
SS_MSG(loss_scale_min_pixels_help,
    EN("Pick the number of loss scales automatically from image size. Images "
       "are halved until the shorter side is near this many pixels, so a dataset "
       "that mixes resolutions gets the right amount for each image. Set to 0 "
       "to use num_loss_scales instead."),
    JA("損失スケールの数を画像サイズから自動で決めます。短辺がこの画素数に近づ"
       "くまで画像を半分にしていくので、解像度が混ざったデータセットでも画像ご"
       "とに適切な数になります。0 にすると代わりに num_loss_scales が使われま"
       "す。"),
    ZH_HANS("根据图像尺寸自动决定损失尺度的数量。图像会不断减半，直到短边接近"
            "这个像素数，因此分辨率混杂的数据集也能为每张图取到合适的数量。设"
            "为 0 则改用 num_loss_scales。"),
    ZH_HANT("根據影像尺寸自動決定損失尺度的數量。影像會不斷減半，直到短邊接近"
            "這個像素數，因此解析度混雜的資料集也能為每張圖取到合適的數量。設"
            "為 0 則改用 num_loss_scales。"),
    KO("손실 배율의 개수를 이미지 크기에서 자동으로 정합니다. 짧은 변이 이 픽"
       "셀 수에 가까워질 때까지 이미지를 절반씩 줄이므로, 해상도가 섞인 데이터"
       "셋도 이미지마다 알맞은 개수를 얻습니다. 0으로 두면 대신 num_loss_scales를"
       " 씁니다."),
    DE("Die Zahl der Verlustskalen automatisch aus der Bildgröße wählen. Bilder "
       "werden halbiert, bis die kürzere Seite nahe an dieser Pixelzahl liegt, "
       "sodass ein Datensatz mit gemischten Auflösungen für jedes Bild die passende "
       "Menge erhält. 0 nutzt stattdessen num_loss_scales."),
    FR("Choisir automatiquement le nombre d'échelles de perte d'après la taille "
       "de l'image. Les images sont divisées par deux jusqu'à ce que le petit "
       "côté approche ce nombre de pixels, si bien qu'un jeu de résolutions mélangées "
       "obtient la bonne quantité pour chaque image. 0 utilise num_loss_scales "
       "à la place."),
    ES("Elegir automáticamente el número de escalas de pérdida a partir del tamaño "
       "de imagen. Las imágenes se reducen a la mitad hasta que el lado corto "
       "se acerca a este número de píxeles, de modo que un conjunto con resoluciones "
       "mezcladas obtiene la cantidad adecuada para cada imagen. Con 0 se usa "
       "num_loss_scales."),
    PT("Escolher automaticamente o número de escalas de perda a partir do tamanho "
       "da imagem. As imagens são reduzidas à metade até o lado curto se aproximar "
       "deste número de pixels, de modo que um conjunto com resoluções misturadas "
       "obtém a quantidade certa para cada imagem. Com 0 usa-se num_loss_scales."),
    IT("Scegliere automaticamente il numero di scale di perdita dalla dimensione "
       "dell'immagine. Le immagini vengono dimezzate finché il lato corto si "
       "avvicina a questo numero di pixel, così un set con risoluzioni miste "
       "ottiene la quantità giusta per ogni immagine. Con 0 si usa invece num_loss_scales."),
    NL("Het aantal verliesschalen automatisch uit de beeldgrootte kiezen. Beelden "
       "worden gehalveerd tot de korte zijde in de buurt van dit aantal pixels "
       "komt, zodat een dataset met gemengde resoluties per beeld de juiste hoeveelheid "
       "krijgt. Met 0 wordt num_loss_scales gebruikt."),
    RU("Выбирать число масштабов потерь автоматически по размеру изображения. "
       "Изображения делятся пополам, пока короткая сторона не приблизится к этому "
       "числу пикселей, так что набор со смешанными разрешениями получает подходящее "
       "число для каждого снимка. При 0 используется num_loss_scales."),
    TR("Kayıp ölçeği sayısını görüntü boyutundan kendiliğinden seçer. Kısa kenar "
       "bu piksel sayısına yaklaşana dek görüntüler yarıya indirilir; böylece "
       "çözünürlükleri karışık bir veri kümesinde her görüntü doğru sayıyı alır. "
       "0 yapılırsa bunun yerine num_loss_scales kullanılır."));

SS_MSG(num_loss_scales,
    EN("Number of loss scales"), JA("損失スケールの数"),
    ZH_HANS("损失尺度数量"), ZH_HANT("損失尺度數量"), KO("손실 배율 개수"),
    DE("Anzahl der Verlustskalen"), FR("Nombre d'échelles de perte"),
    ES("Número de escalas de pérdida"), PT("Número de escalas de perda"),
    IT("Numero di scale di perdita"), NL("Aantal verliesschalen"),
    RU("Число масштабов потерь"), TR("Kayıp ölçeği sayısı"));
SS_MSG(num_loss_scales_help,
    EN("How many progressively smaller copies of each image the loss also compares. "
       "Looking at several sizes helps large smooth areas converge on high-resolution "
       "datasets instead of only fine detail. Normally left for loss_scale_min_pixels "
       "to decide."),
    JA("損失が比較する、各画像の段階的に小さくしたコピーの数です。複数の大きさ"
       "を見ることで、高解像度のデータセットでは細部だけでなく広く滑らかな面も"
       "うまく収束します。ふつうは loss_scale_min_pixels に任せます。"),
    ZH_HANS("损失还会比较每张图像逐级缩小的多少份副本。同时看多个尺度，可以让"
            "高分辨率数据集里大片平滑区域也收敛，而不只是细节。通常交给 loss_scale_min_pixels "
            "决定。"),
    ZH_HANT("損失還會比較每張影像逐級縮小的多少份副本。同時看多個尺度，可以讓"
            "高解析度資料集裡大片平滑區域也收斂，而不只是細節。通常交給 loss_scale_min_pixels "
            "決定。"),
    KO("손실이 함께 비교하는, 각 이미지를 단계적으로 줄인 사본의 개수입니다. "
       "여러 크기를 보면 고해상도 데이터셋에서 세부뿐 아니라 넓고 매끈한 영역"
       "도 잘 수렴합니다. 보통은 loss_scale_min_pixels에 맡깁니다."),
    DE("Wie viele zunehmend kleinere Kopien jedes Bildes der Verlust zusätzlich "
       "vergleicht. Der Blick auf mehrere Größen hilft, dass bei hochaufgelösten "
       "Datensätzen auch große glatte Flächen konvergieren und nicht nur feines "
       "Detail. Normalerweise überlässt man das loss_scale_min_pixels."),
    FR("Combien de copies de plus en plus petites de chaque image la fonction "
       "de coût compare également. Regarder plusieurs tailles aide les grandes "
       "zones lisses à converger sur les jeux haute résolution, et pas seulement "
       "le détail fin. Normalement laissé à loss_scale_min_pixels."),
    ES("Cuántas copias cada vez más pequeñas de cada imagen compara también la "
       "función de pérdida. Mirar varios tamaños ayuda a que las grandes zonas "
       "suaves converjan en conjuntos de alta resolución, no solo el detalle "
       "fino. Normalmente se deja en manos de loss_scale_min_pixels."),
    PT("Quantas cópias cada vez menores de cada imagem a função de perda também "
       "compara. Olhar vários tamanhos ajuda as grandes áreas suaves a convergir "
       "em conjuntos de alta resolução, e não só o detalhe fino. Normalmente "
       "deixa-se a cargo de loss_scale_min_pixels."),
    IT("Quante copie via via più piccole di ogni immagine la funzione di perdita "
       "confronta in aggiunta. Guardare più dimensioni aiuta le grandi aree lisce "
       "a convergere sui set ad alta risoluzione, non solo il dettaglio fine. "
       "Di norma si lascia decidere a loss_scale_min_pixels."),
    NL("Hoeveel steeds kleinere kopieën van elk beeld de verliesfunctie ook vergelijkt. "
       "Naar meerdere formaten kijken helpt grote gladde vlakken convergeren "
       "bij datasets met hoge resolutie, niet alleen fijn detail. Normaal laat "
       "je dit aan loss_scale_min_pixels over."),
    RU("Сколько всё более мелких копий каждого изображения дополнительно сравнивает "
       "функция потерь. Взгляд на несколько масштабов помогает сойтись большим "
       "гладким областям на наборах высокого разрешения, а не только мелким деталям. "
       "Обычно это оставляют на loss_scale_min_pixels."),
    TR("Kaybın ayrıca karşılaştırdığı, her görüntünün giderek küçülen kopyalarının "
       "sayısı. Birkaç boyuta birden bakmak, yüksek çözünürlüklü veri kümelerinde "
       "yalnızca ince ayrıntının değil, geniş düz alanların da yakınsamasına "
       "yardım eder. Normalde bu iş loss_scale_min_pixels'e bırakılır."));

SS_MSG(alpha_loss_weight,
    EN("Empty-area weight"), JA("空にすべき領域の重み"),
    ZH_HANS("应为空区域的权重"), ZH_HANT("應為空區域的權重"),
    KO("비어야 할 영역 가중치"), DE("Gewicht der leeren Bereiche"),
    FR("Poids des zones vides"), ES("Peso de las zonas vacías"),
    PT("Peso das áreas vazias"), IT("Peso delle aree vuote"),
    NL("Gewicht van de lege gebieden"), RU("Вес пустых областей"),
    TR("Boş alan ağırlığı"));
SS_MSG(alpha_loss_weight_help,
    EN("How firmly to clear splats out of areas the mask says should be empty. "
       "Raise it if background creeps back in."),
    JA("マスクが空だとしている領域から、どれだけしっかりスプラットを取り除くか"
       "です。背景が戻ってきてしまうときは上げてください。"),
    ZH_HANS("对蒙版认定应为空的区域，清除泼溅的力度。如果背景又冒出来，就把它"
            "调高。"),
    ZH_HANT("對遮罩認定應為空的區域，清除潑濺的力度。如果背景又冒出來，就把它"
            "調高。"),
    KO("마스크가 비어 있어야 한다고 표시한 영역에서 스플랫을 얼마나 확실히 걷"
       "어낼지입니다. 배경이 다시 스며 나오면 올리십시오."),
    DE("Wie entschieden Splats aus Bereichen entfernt werden, die die Maske als "
       "leer ausweist. Erhöhen, wenn sich der Hintergrund wieder einschleicht."),
    FR("Avec quelle fermeté les splats sont retirés des zones que le masque déclare "
       "vides. À augmenter si l'arrière-plan revient."),
    ES("Con qué firmeza se retiran los splats de las zonas que la máscara declara "
       "vacías. Súbalo si el fondo vuelve a colarse."),
    PT("Com que firmeza os splats são retirados das áreas que a máscara declara "
       "vazias. Aumente se o fundo voltar a aparecer."),
    IT("Con quanta fermezza gli splat vengono tolti dalle aree che la maschera "
       "dichiara vuote. Da alzare se lo sfondo torna a farsi vedere."),
    NL("Hoe stellig splats worden weggehaald uit gebieden die het masker leeg "
       "noemt. Verhoog dit als de achtergrond weer terugkruipt."),
    RU("Насколько решительно сплаты убираются из областей, помеченных маской "
       "как пустые. Повышайте, если фон снова проступает."),
    TR("Maskenin boş olması gerektiğini söylediği alanlardan splat'ların ne kadar "
       "kararlıca temizleneceği. Arka plan geri sızıyorsa yükseltin."));

SS_MSG(alpha_loss_weight_under,
    EN("Solid-area weight"), JA("埋めるべき領域の重み"),
    ZH_HANS("应为实心区域的权重"), ZH_HANT("應為實心區域的權重"),
    KO("채워야 할 영역 가중치"), DE("Gewicht der gefüllten Bereiche"),
    FR("Poids des zones pleines"), ES("Peso de las zonas sólidas"),
    PT("Peso das áreas sólidas"), IT("Peso delle aree piene"),
    NL("Gewicht van de gevulde gebieden"), RU("Вес заполненных областей"),
    TR("Dolu alan ağırlığı"));
SS_MSG(alpha_loss_weight_under_help,
    EN("How firmly to fill in areas the mask says should be solid but the render "
       "leaves empty. Raise it if holes appear in the subject."),
    JA("マスクが埋まっているとしているのに描画では空いている領域を、どれだけし"
       "っかり埋めるかです。被写体に穴があくときは上げてください。"),
    ZH_HANS("对蒙版认定应为实心、但渲染结果却空着的区域，填充的力度。如果主体"
            "上出现空洞，就把它调高。"),
    ZH_HANT("對遮罩認定應為實心、但算圖結果卻空著的區域，填補的力度。如果主體"
            "上出現空洞，就把它調高。"),
    KO("마스크는 채워져 있어야 한다고 하는데 렌더 결과가 비어 있는 영역을 얼마"
       "나 확실히 메울지입니다. 피사체에 구멍이 생기면 올리십시오."),
    DE("Wie entschieden Bereiche gefüllt werden, die die Maske als massiv ausweist, "
       "das Rendering aber leer lässt. Erhöhen, wenn im Motiv Löcher auftauchen."),
    FR("Avec quelle fermeté sont remplies les zones que le masque déclare pleines "
       "mais que le rendu laisse vides. À augmenter si des trous apparaissent "
       "dans le sujet."),
    ES("Con qué firmeza se rellenan las zonas que la máscara declara sólidas "
       "pero el render deja vacías. Súbalo si aparecen agujeros en el sujeto."),
    PT("Com que firmeza são preenchidas as áreas que a máscara declara sólidas "
       "mas a renderização deixa vazias. Aumente se aparecerem buracos no sujeito."),
    IT("Con quanta fermezza vengono riempite le aree che la maschera dichiara "
       "piene ma che il render lascia vuote. Da alzare se compaiono buchi nel "
       "soggetto."),
    NL("Hoe stellig gebieden worden opgevuld die het masker vol noemt maar de "
       "rendering leeg laat. Verhoog dit als er gaten in het onderwerp verschijnen."),
    RU("Насколько решительно заполняются области, которые маска считает сплошными, "
       "а рендер оставляет пустыми. Повышайте, если в объекте появляются дыры."),
    TR("Maskenin dolu olması gerektiğini söylediği ama çizimin boş bıraktığı "
       "alanların ne kadar kararlıca doldurulacağı. Öznede delikler beliriyorsa "
       "yükseltin."));


// ===========================================================================
// Geometry & Surfaces
// ===========================================================================

SS_MSG(floater_suppression,
    EN("Floater suppression"), JA("浮遊ノイズの抑制"), ZH_HANS("抑制漂浮物"),
    ZH_HANT("抑制漂浮物"), KO("부유물 억제"),
    DE("Unterdrückung von Schwebeteilen"),
    FR("Suppression des artefacts flottants"),
    ES("Supresión de restos flotantes"),
    PT("Supressão de resíduos flutuantes"),
    IT("Soppressione dei frammenti fluttuanti"),
    NL("Onderdrukking van zwevende artefacten"),
    RU("Подавление «летающих» артефактов"),
    TR("Uçuşan artıkların bastırılması"));
SS_MSG(floater_suppression_help,
    EN("Clean up floating blobs and see-through surfaces. Tightens the depth "
       "and colour consistency penalties and holds view-dependent colour "
       "back; `strong` gives the crispest geometry but can flatten thin or "
       "genuinely translucent detail."),
    JA("浮いた塊や透けて見える面を整理します。深度と色の一貫性ペナルティを強め、"
       "視点依存の色を抑えます。`strong` はもっともくっきりしたジオメトリにな"
       "りますが、薄いものや本当に半透明なものが平坦になることがあります。"),
    ZH_HANS("清理漂浮的团块和穿透的表面。它会加强深度和颜色一致性惩罚，并抑制"
            "视角相关颜色；`strong` 给出最清爽的几何，但可能压平细薄或真正半透"
            "明的细节。"),
    ZH_HANT("清理漂浮的團塊和穿透的表面。它會加強深度和顏色一致性懲罰，並抑制"
            "視角相關顏色；`strong` 給出最清爽的幾何，但可能壓平細薄或真正半透"
            "明的細節。"),
    KO("떠다니는 덩어리와 비쳐 보이는 면을 정리합니다. 깊이와 색 일관성 페널티"
       "를 강화하고 시점 의존 색을 억제하며, `strong`은 가장 또렷한 지오메트리"
       "를 주지만 얇거나 실제로 반투명한 디테일을 납작하게 만들 수 있습니다."),
    DE("Schwebende Klumpen und durchscheinende Flächen aufräumen. Verschärft "
       "die Strafen für Tiefen- und Farbkonsistenz und hält blickabhängige "
       "Farbe zurück; `strong` liefert die knackigste Geometrie, kann aber "
       "dünne oder tatsächlich lichtdurchlässige Details flach machen."),
    FR("Nettoyer les amas flottants et les surfaces qui transparaissent. "
       "Resserre les pénalités de cohérence de profondeur et de couleur et "
       "bride la couleur dépendante de la vue ; `strong` donne la géométrie "
       "la plus nette mais peut aplatir les détails fins ou réellement "
       "translucides."),
    ES("Limpiar los grumos flotantes y las superficies que se transparentan. "
       "Aprieta las penalizaciones de coherencia de profundidad y color y "
       "frena el color dependiente de la vista; `strong` da la geometría más "
       "nítida pero puede aplanar detalles finos o realmente translúcidos."),
    PT("Limpar os grumos flutuantes e as superfícies que ficam transparentes. "
       "Aperta as penalidades de coerência de profundidade e cor e segura a "
       "cor dependente da vista; `strong` dá a geometria mais nítida, mas "
       "pode achatar detalhes finos ou realmente translúcidos."),
    IT("Ripulire i grumi fluttuanti e le superfici che si vedono attraverso. "
       "Stringe le penalità di coerenza di profondità e colore e trattiene il "
       "colore dipendente dalla vista; `strong` dà la geometria più nitida ma "
       "può appiattire i dettagli sottili o davvero traslucidi."),
    NL("Zwevende klodders en doorschijnende oppervlakken opruimen. Het scherpt "
       "de straffen voor diepte- en kleurconsistentie aan en houdt "
       "kijkrichtingafhankelijke kleur in toom; `strong` geeft de strakste "
       "geometrie maar kan dun of werkelijk doorschijnend detail platslaan."),
    RU("Убрать висящие сгустки и просвечивающие поверхности. Усиливает штрафы "
       "за согласованность глубины и цвета и придерживает цвет, зависящий от "
       "вида; `strong` даёт самую чёткую геометрию, но может сплющить тонкие "
       "или по-настоящему полупрозрачные детали."),
    TR("Havada duran topakları ve içi görünen yüzeyleri temizler. Derinlik ve "
       "renk tutarlılığı cezalarını sıkar, bakışa bağlı rengi dizginler; "
       "`strong` en keskin geometriyi verir ama ince ya da gerçekten yarı "
       "saydam ayrıntıları düzleştirebilir."));

SS_MSG(depth_distortion_reg,
    EN("Depth consistency"), JA("深度の一貫性"), ZH_HANS("深度一致性"),
    ZH_HANT("深度一致性"), KO("깊이 일관성"), DE("Tiefenkonsistenz"),
    FR("Cohérence de la profondeur"), ES("Coherencia de la profundidad"),
    PT("Coerência da profundidade"), IT("Coerenza della profondità"),
    NL("Diepteconsistentie"), RU("Согласованность глубины"),
    TR("Derinlik tutarlılığı"));
SS_MSG(depth_distortion_reg_help,
    EN("Encourage each pixel's depth to come from one surface and discourage "
       "floaters. Gives crisper geometry and better meshes; too much flattens "
       "fine translucent detail."),
    JA("各画素の深度が一つの面から来るように促し、浮遊物を抑えます。ジオメトリ"
       "がくっきりしてメッシュもよくなりますが、効かせすぎると薄い半透明の細部"
       "が平坦になります。"),
    ZH_HANS("促使每个像素的深度来自同一个表面，抑制漂浮物。几何更清晰、网格更"
            "好；过强则会压平细薄的半透明细节。"),
    ZH_HANT("促使每個像素的深度來自同一個表面，抑制漂浮物。幾何更清晰、網格更"
            "好；過強則會壓平細薄的半透明細節。"),
    KO("각 픽셀의 깊이가 한 면에서 오도록 유도하고 부유물을 억제합니다. 지오메"
       "트리가 또렷해지고 메시가 좋아지지만, 지나치면 얇은 반투명 디테일이 납"
       "작해집니다."),
    DE("Dazu anhalten, dass die Tiefe jedes Pixels von einer Fläche stammt, und "
       "Schwebeteile unterdrücken. Ergibt knackigere Geometrie und bessere Netze; "
       "zu viel macht feines, lichtdurchlässiges Detail flach."),
    FR("Inciter la profondeur de chaque pixel à provenir d'une seule surface "
       "et décourager les flotteurs. Donne une géométrie plus nette et de meilleurs "
       "maillages ; trop fort aplatit les détails fins et translucides."),
    ES("Animar a que la profundidad de cada píxel provenga de una sola superficie "
       "y desalentar los restos flotantes. Da una geometría más nítida y mejores "
       "mallas; demasiado aplana el detalle fino y translúcido."),
    PT("Incentivar que a profundidade de cada pixel venha de uma única superfície "
       "e desencorajar resíduos flutuantes. Dá geometria mais nítida e malhas "
       "melhores; em excesso achata o detalhe fino e translúcido."),
    IT("Incoraggiare la profondità di ogni pixel a provenire da una sola superficie "
       "e scoraggiare i frammenti fluttuanti. Dà geometria più nitida e mesh "
       "migliori; troppo appiattisce il dettaglio fine e traslucido."),
    NL("Aanmoedigen dat de diepte van elke pixel van één oppervlak komt en zwevers "
       "ontmoedigen. Geeft strakkere geometrie en betere meshes; te veel slaat "
       "fijn doorschijnend detail plat."),
    RU("Побуждать глубину каждого пикселя приходить с одной поверхности и подавлять "
       "«летающие» артефакты. Даёт более чёткую геометрию и лучшие меши; перебор "
       "сплющивает тонкие полупрозрачные детали."),
    TR("Her pikselin derinliğinin tek bir yüzeyden gelmesini teşvik eder ve uçuşan "
       "artıkları caydırır. Daha keskin geometri ve daha iyi ağ verir; fazlası "
       "ince, yarı saydam ayrıntıyı düzleştirir."));

SS_MSG(normal_distortion_reg,
    EN("Surface direction consistency"), JA("面の向きの一貫性"),
    ZH_HANS("表面朝向一致性"), ZH_HANT("表面朝向一致性"),
    KO("표면 방향 일관성"), DE("Konsistenz der Oberflächenrichtung"),
    FR("Cohérence de l'orientation des surfaces"),
    ES("Coherencia de la orientación de las superficies"),
    PT("Coerência da orientação das superfícies"),
    IT("Coerenza dell'orientamento delle superfici"),
    NL("Consistentie van de oppervlakterichting"),
    RU("Согласованность направления поверхности"),
    TR("Yüzey yönü tutarlılığı"));
SS_MSG(normal_distortion_reg_help,
    EN("The same idea as depth distortion, applied to surface direction. Each "
       "pixel settles on one consistent orientation instead of several."),
    JA("深度の一貫性と同じ考え方を、面の向きに当てはめます。各画素が複数ではな"
       "く一つの向きに落ち着きます。"),
    ZH_HANS("与深度一致性同样的思路，用在表面朝向上。每个像素会稳定到单一朝向，"
            "而不是若干个。"),
    ZH_HANT("與深度一致性同樣的思路，用在表面朝向上。每個像素會穩定到單一朝向，"
            "而不是若干個。"),
    KO("깊이 일관성과 같은 발상을 표면 방향에 적용합니다. 각 픽셀이 여러 방향"
       "이 아니라 하나의 방향으로 자리 잡습니다."),
    DE("Derselbe Gedanke wie bei der Tiefenkonsistenz, auf die Oberflächenrichtung "
       "angewandt. Jedes Pixel legt sich auf eine einheitliche Ausrichtung fest "
       "statt auf mehrere."),
    FR("La même idée que la cohérence de profondeur, appliquée à l'orientation "
       "des surfaces. Chaque pixel se fixe sur une seule orientation au lieu "
       "de plusieurs."),
    ES("La misma idea que la coherencia de profundidad, aplicada a la orientación "
       "de las superficies. Cada píxel se asienta en una sola orientación en "
       "vez de varias."),
    PT("A mesma ideia da coerência de profundidade, aplicada à orientação das "
       "superfícies. Cada pixel se fixa numa única orientação em vez de várias."),
    IT("La stessa idea della coerenza di profondità, applicata all'orientamento "
       "delle superfici. Ogni pixel si assesta su un solo orientamento invece "
       "che su più."),
    NL("Hetzelfde idee als diepteconsistentie, maar dan voor de oppervlakterichting. "
       "Elke pixel komt tot rust op één richting in plaats van meerdere."),
    RU("Та же идея, что и согласованность глубины, применённая к направлению "
       "поверхности. Каждый пиксель останавливается на одном направлении, а не "
       "на нескольких."),
    TR("Derinlik tutarlılığıyla aynı düşüncenin yüzey yönüne uygulanışı. Her "
       "piksel birkaç yön yerine tek bir yönde karar kılar."));

SS_MSG(rgb_distortion_reg,
    EN("Color consistency"), JA("色の一貫性"), ZH_HANS("颜色一致性"),
    ZH_HANT("顏色一致性"), KO("색 일관성"), DE("Farbkonsistenz"),
    FR("Cohérence de la couleur"), ES("Coherencia del color"),
    PT("Coerência da cor"), IT("Coerenza del colore"),
    NL("Kleurconsistentie"), RU("Согласованность цвета"),
    TR("Renk tutarlılığı"));
SS_MSG(rgb_distortion_reg_help,
    EN("Encourage each pixel's color to come from one surface rather than blended "
       "layers. Helps discourage false transparency."),
    JA("各画素の色が、重なった層ではなく一つの面から来るように促します。偽の透"
       "明感を抑えるのに役立ちます。"),
    ZH_HANS("促使每个像素的颜色来自同一个表面，而不是多层叠加。有助于抑制虚假"
            "的透明感。"),
    ZH_HANT("促使每個像素的顏色來自同一個表面，而不是多層疊加。有助於抑制虛假"
            "的透明感。"),
    KO("각 픽셀의 색이 여러 겹이 아니라 한 면에서 오도록 유도합니다. 가짜 투명"
       "함을 줄이는 데 도움이 됩니다."),
    DE("Dazu anhalten, dass die Farbe jedes Pixels von einer Fläche stammt statt "
       "aus überlagerten Schichten. Hilft gegen falsche Transparenz."),
    FR("Inciter la couleur de chaque pixel à provenir d'une seule surface plutôt "
       "que de couches mélangées. Aide à décourager la fausse transparence."),
    ES("Animar a que el color de cada píxel provenga de una sola superficie en "
       "vez de capas mezcladas. Ayuda a desalentar la transparencia falsa."),
    PT("Incentivar que a cor de cada pixel venha de uma única superfície em vez "
       "de camadas misturadas. Ajuda a desencorajar a transparência falsa."),
    IT("Incoraggiare il colore di ogni pixel a provenire da una sola superficie "
       "invece che da strati sovrapposti. Aiuta a scoraggiare la falsa trasparenza."),
    NL("Aanmoedigen dat de kleur van elke pixel van één oppervlak komt in plaats "
       "van van vermengde lagen. Helpt valse doorzichtigheid tegengaan."),
    RU("Побуждать цвет каждого пикселя приходить с одной поверхности, а не из "
       "смешанных слоёв. Помогает бороться с ложной прозрачностью."),
    TR("Her pikselin renginin karışmış katmanlar yerine tek bir yüzeyden gelmesini "
       "teşvik eder. Sahte saydamlığı caydırmaya yardımcı olur."));

SS_MSG(distortion_reg_warmup,
    EN("Consistency warm-up"), JA("一貫性ペナルティの立ち上がり"),
    ZH_HANS("一致性惩罚预热"), ZH_HANT("一致性懲罰預熱"),
    KO("일관성 페널티 워밍업"), DE("Anlauf der Konsistenzstrafen"),
    FR("Montée des pénalités de cohérence"),
    ES("Arranque de las penalizaciones de coherencia"),
    PT("Aquecimento das penalidades de coerência"),
    IT("Avvio delle penalità di coerenza"),
    NL("Opbouw van de consistentiestraffen"),
    RU("Разгон штрафов согласованности"),
    TR("Tutarlılık cezalarının ısınması"));
SS_MSG(distortion_reg_warmup_help,
    EN("How many steps the distortion penalties take to reach full strength. "
       "Ramping in lets coarse structure form before geometry is tightened."),
    JA("一貫性のペナルティが最大になるまでのステップ数です。徐々に効かせること"
       "で、ジオメトリを締める前に大まかな形ができあがります。"),
    ZH_HANS("一致性惩罚达到最大强度所需的步数。逐步加力可以让粗略结构先成形，"
            "再收紧几何。"),
    ZH_HANT("一致性懲罰達到最大強度所需的步數。逐步加力可以讓粗略結構先成形，"
            "再收緊幾何。"),
    KO("일관성 페널티가 최대 강도에 이르기까지의 스텝 수입니다. 서서히 올리면"
       " 지오메트리를 조이기 전에 굵직한 구조가 먼저 잡힙니다."),
    DE("Wie viele Schritte die Konsistenzstrafen brauchen, um volle Stärke zu "
       "erreichen. Das langsame Einblenden lässt die grobe Struktur entstehen, "
       "bevor die Geometrie festgezurrt wird."),
    FR("Combien d'étapes les pénalités de cohérence mettent à atteindre leur "
       "pleine force. Une montée progressive laisse la structure grossière se "
       "former avant de resserrer la géométrie."),
    ES("Cuántos pasos tardan las penalizaciones de coherencia en alcanzar toda "
       "su fuerza. Subirlas poco a poco deja que se forme la estructura burda "
       "antes de apretar la geometría."),
    PT("Quantos passos as penalidades de coerência levam para atingir força total. "
       "Subir aos poucos deixa a estrutura grosseira se formar antes de apertar "
       "a geometria."),
    IT("Quanti passi impiegano le penalità di coerenza a raggiungere la piena "
       "forza. Salire per gradi lascia formare la struttura grossolana prima "
       "di stringere la geometria."),
    NL("Hoeveel stappen de consistentiestraffen nodig hebben om op volle sterkte "
       "te komen. Geleidelijk opvoeren laat de grove structuur ontstaan voordat "
       "de geometrie wordt aangetrokken."),
    RU("За сколько шагов штрафы согласованности набирают полную силу. Постепенное "
       "включение даёт сложиться грубой структуре, прежде чем геометрию затянут."),
    TR("Tutarlılık cezalarının tam güce ulaşması için gereken adım sayısı. Kademeli "
       "devreye girmek, geometri sıkılmadan önce kaba yapının oluşmasını sağlar."));

SS_MSG(normal_reg_weight,
    EN("Flatten splats onto surfaces"), JA("スプラットを面に沿わせる"),
    ZH_HANS("让泼溅贴合表面"), ZH_HANT("讓潑濺貼合表面"),
    KO("스플랫을 표면에 눕히기"), DE("Splats an die Oberflächen anlegen"),
    FR("Aplatir les splats sur les surfaces"),
    ES("Aplanar los splats sobre las superficies"),
    PT("Achatar os splats sobre as superfícies"),
    IT("Appiattire gli splat sulle superfici"),
    NL("Splats plat op de oppervlakken leggen"),
    RU("Прижимать сплаты к поверхностям"), TR("Splat'ları yüzeylere yatır"));
SS_MSG(normal_reg_weight_help,
    EN("Encourage splats to lie flat along the surfaces they represent. Higher "
       "gives cleaner geometry and better meshes; too high flattens fine detail."),
    JA("スプラットが、表している面に沿って平らに寝るように促します。高いほどジ"
       "オメトリがきれいになりメッシュもよくなりますが、上げすぎると細部が平坦"
       "になります。"),
    ZH_HANS("促使泼溅沿着它所代表的表面平铺。数值越高几何越干净、网格越好；过"
            "高则会压平细节。"),
    ZH_HANT("促使潑濺沿著它所代表的表面平鋪。數值越高幾何越乾淨、網格越好；過"
            "高則會壓平細節。"),
    KO("스플랫이 자기가 나타내는 표면을 따라 납작하게 눕도록 유도합니다. 값이"
       " 크면 지오메트리가 깔끔해지고 메시가 좋아지지만, 너무 크면 세부가 평평"
       "해집니다."),
    DE("Splats dazu anhalten, flach an den Flächen zu liegen, die sie darstellen. "
       "Höher ergibt sauberere Geometrie und bessere Netze; zu hoch macht feines "
       "Detail flach."),
    FR("Inciter les splats à s'aplatir le long des surfaces qu'ils représentent. "
       "Plus haut donne une géométrie plus propre et de meilleurs maillages ; "
       "trop haut aplatit le détail fin."),
    ES("Animar a los splats a tenderse planos a lo largo de las superficies que "
       "representan. Más alto da una geometría más limpia y mejores mallas; demasiado "
       "aplana el detalle fino."),
    PT("Incentivar os splats a se deitarem planos ao longo das superfícies que "
       "representam. Mais alto dá geometria mais limpa e malhas melhores; alto "
       "demais achata o detalhe fino."),
    IT("Incoraggiare gli splat a stendersi piatti lungo le superfici che rappresentano. "
       "Più alto dà geometria più pulita e mesh migliori; troppo alto appiattisce "
       "il dettaglio fine."),
    NL("Splats aanmoedigen plat langs de oppervlakken te liggen die ze weergeven. "
       "Hoger geeft schonere geometrie en betere meshes; te hoog slaat fijn detail "
       "plat."),
    RU("Побуждать сплаты ложиться плашмя вдоль поверхностей, которые они изображают. "
       "Больше — чище геометрия и лучше меши; слишком много — мелкие детали сплющиваются."),
    TR("Splat'ları temsil ettikleri yüzeyler boyunca yatmaya teşvik eder. Yüksek "
       "değerler daha temiz geometri ve daha iyi ağ verir; aşırısı ince ayrıntıyı "
       "düzleştirir."));

SS_MSG(normal_reg_warmup,
    EN("Surface alignment warm-up"), JA("面沿わせの立ち上がり"),
    ZH_HANS("表面对齐预热"), ZH_HANT("表面對齊預熱"), KO("표면 정렬 워밍업"),
    DE("Anlauf der Oberflächenausrichtung"),
    FR("Montée de l'alignement aux surfaces"),
    ES("Arranque de la alineación con las superficies"),
    PT("Aquecimento do alinhamento às superfícies"),
    IT("Avvio dell'allineamento alle superfici"),
    NL("Opbouw van de oppervlakte-uitlijning"),
    RU("Разгон выравнивания по поверхности"),
    TR("Yüzeye hizalamanın ısınması"));
SS_MSG(normal_reg_warmup_help,
    EN("How many steps the surface-alignment penalty takes to reach full strength."),
    JA("面沿わせのペナルティが最大になるまでのステップ数です。"),
    ZH_HANS("表面对齐惩罚达到最大强度所需的步数。"),
    ZH_HANT("表面對齊懲罰達到最大強度所需的步數。"),
    KO("표면 정렬 페널티가 최대 강도에 이르기까지의 스텝 수입니다."),
    DE("Wie viele Schritte die Oberflächenausrichtungsstrafe braucht, um volle "
       "Stärke zu erreichen."),
    FR("Combien d'étapes la pénalité d'alignement aux surfaces met à atteindre "
       "sa pleine force."),
    ES("Cuántos pasos tarda la penalización de alineación con las superficies "
       "en alcanzar toda su fuerza."),
    PT("Quantos passos a penalidade de alinhamento às superfícies leva para atingir "
       "força total."),
    IT("Quanti passi impiega la penalità di allineamento alle superfici a raggiungere "
       "la piena forza."),
    NL("Hoeveel stappen de straf voor oppervlakte-uitlijning nodig heeft om op "
       "volle sterkte te komen."),
    RU("За сколько шагов штраф за выравнивание по поверхности набирает полную "
       "силу."),
    TR("Yüzeye hizalama cezasının tam güce ulaşması için gereken adım sayısı."));

SS_MSG(alpha_reg_weight,
    EN("Solid-or-empty coverage"), JA("被覆を不透明か空かに寄せる"),
    ZH_HANS("让覆盖趋向全实或全空"), ZH_HANT("讓覆蓋趨向全實或全空"),
    KO("커버리지를 채움 또는 비움으로"),
    DE("Deckung ganz voll oder ganz leer"), FR("Couverture pleine ou vide"),
    ES("Cobertura llena o vacía"), PT("Cobertura cheia ou vazia"),
    IT("Copertura piena o vuota"), NL("Dekking vol of leeg"),
    RU("Покрытие либо полное, либо пустое"), TR("Kaplama ya dolu ya boş"));
SS_MSG(alpha_reg_weight_help,
    EN("Push each pixel's coverage toward fully solid or fully empty instead "
       "of half transparent. Clears haze and gives cleaner background cutouts, "
       "at the risk of less stable training."),
    JA("各画素の被覆を、半透明ではなく完全に不透明か完全に空かへ寄せます。もや"
       "が消えて背景の切り抜きもきれいになりますが、学習は不安定になりやすくな"
       "ります。"),
    ZH_HANS("把每个像素的覆盖推向完全实心或完全空白，而不是半透明。可以清除雾"
            "感、让背景抠图更干净，但训练可能变得不那么稳定。"),
    ZH_HANT("把每個像素的覆蓋推向完全實心或完全空白，而不是半透明。可以清除霧"
            "感、讓背景去背更乾淨，但訓練可能變得不那麼穩定。"),
    KO("각 픽셀의 커버리지를 반투명이 아니라 완전히 채워짐 또는 완전히 비움 쪽"
       "으로 밀어냅니다. 안개가 걷히고 배경 오려내기가 깔끔해지지만 학습이 덜"
       " 안정될 수 있습니다."),
    DE("Die Deckung jedes Pixels zu ganz voll oder ganz leer drängen statt halbdurchsichtig. "
       "Räumt Schleier weg und ergibt sauberere Freisteller, auf die Gefahr eines "
       "unruhigeren Trainings."),
    FR("Pousser la couverture de chaque pixel vers le plein ou le vide plutôt "
       "que le semi-transparent. Fait disparaître le voile et donne des détourages "
       "plus propres, au risque d'un entraînement moins stable."),
    ES("Empujar la cobertura de cada píxel hacia lleno o vacío en vez de semitransparente. "
       "Elimina la neblina y da recortes de fondo más limpios, a riesgo de un "
       "entrenamiento menos estable."),
    PT("Empurrar a cobertura de cada pixel para cheio ou vazio em vez de semitransparente. "
       "Elimina a névoa e dá recortes de fundo mais limpos, ao risco de um treinamento "
       "menos estável."),
    IT("Spingere la copertura di ogni pixel verso il pieno o il vuoto invece "
       "che il semitrasparente. Elimina la foschia e dà scontorni più puliti, "
       "a rischio di un addestramento meno stabile."),
    NL("De dekking van elke pixel naar helemaal vol of helemaal leeg duwen in "
       "plaats van halfdoorzichtig. Ruimt waas op en geeft schonere uitsnedes, "
       "met het risico van minder stabiele training."),
    RU("Смещать покрытие каждого пикселя к полностью плотному или полностью пустому "
       "вместо полупрозрачного. Убирает дымку и даёт более чистые вырезы фона "
       "ценой менее устойчивого обучения."),
    TR("Her pikselin kaplamasını yarı saydam yerine tamamen dolu ya da tamamen "
       "boş olmaya iter. Pusu giderir ve arka plan kesiklerini temizler; karşılığında "
       "eğitim daha az kararlı olabilir."));

SS_MSG(alpha_reg_warmup,
    EN("Coverage penalty warm-up"), JA("被覆ペナルティの立ち上がり"),
    ZH_HANS("覆盖惩罚预热"), ZH_HANT("覆蓋懲罰預熱"),
    KO("커버리지 페널티 워밍업"), DE("Anlauf der Deckungsstrafe"),
    FR("Montée de la pénalité de couverture"),
    ES("Arranque de la penalización de cobertura"),
    PT("Aquecimento da penalidade de cobertura"),
    IT("Avvio della penalità di copertura"),
    NL("Opbouw van de dekkingsstraf"), RU("Разгон штрафа за покрытие"),
    TR("Kaplama cezasının ısınması"));
SS_MSG(alpha_reg_warmup_help,
    EN("How many steps the coverage penalty takes to reach full strength."),
    JA("被覆のペナルティが最大になるまでのステップ数です。"),
    ZH_HANS("覆盖惩罚达到最大强度所需的步数。"),
    ZH_HANT("覆蓋懲罰達到最大強度所需的步數。"),
    KO("커버리지 페널티가 최대 강도에 이르기까지의 스텝 수입니다."),
    DE("Wie viele Schritte die Deckungsstrafe braucht, um volle Stärke zu erreichen."),
    FR("Combien d'étapes la pénalité de couverture met à atteindre sa pleine "
       "force."),
    ES("Cuántos pasos tarda la penalización de cobertura en alcanzar toda su "
       "fuerza."),
    PT("Quantos passos a penalidade de cobertura leva para atingir força total."),
    IT("Quanti passi impiega la penalità di copertura a raggiungere la piena "
       "forza."),
    NL("Hoeveel stappen de dekkingsstraf nodig heeft om op volle sterkte te komen."),
    RU("За сколько шагов штраф за покрытие набирает полную силу."),
    TR("Kaplama cezasının tam güce ulaşması için gereken adım sayısı."));

SS_MSG(reg_warmup_length,
    EN("Geometry penalty delay"), JA("ジオメトリのペナルティを遅らせる長さ"),
    ZH_HANS("几何惩罚延迟步数"), ZH_HANT("幾何懲罰延遲步數"),
    KO("지오메트리 페널티 지연"), DE("Verzögerung der Geometriestrafen"),
    FR("Retard des pénalités de géométrie"),
    ES("Retraso de las penalizaciones de geometría"),
    PT("Atraso das penalidades de geometria"),
    IT("Ritardo delle penalità di geometria"),
    NL("Vertraging van de geometriestraffen"),
    RU("Задержка геометрических штрафов"),
    TR("Geometri cezalarının gecikmesi"));
SS_MSG(reg_warmup_length_help,
    EN("Hold the depth, normal and coverage penalties off for this many steps. "
       "Lets the scene take shape before geometry constraints start pulling on "
       "it."),
    JA("深度・法線・被覆のペナルティを、このステップ数のあいだ効かせません。ジ"
       "オメトリの制約が引っ張り始める前に、シーンの形ができあがります。"),
    ZH_HANS("在这么多步内先不施加深度、法线和覆盖惩罚。让场景先成形，几何约束"
            "再开始拉扯。"),
    ZH_HANT("在這麼多步內先不施加深度、法線和覆蓋懲罰。讓場景先成形，幾何約束"
            "再開始拉扯。"),
    KO("깊이·노멀·커버리지 페널티를 이 스텝 수 동안 걸지 않습니다. 지오메트리"
       " 제약이 잡아당기기 전에 장면이 먼저 모양을 갖춥니다."),
    DE("Die Tiefen-, Normalen- und Deckungsstrafen für so viele Schritte aussetzen. "
       "So nimmt die Szene Gestalt an, bevor Geometriebeschränkungen an ihr ziehen."),
    FR("Suspendre les pénalités de profondeur, de normales et de couverture pendant "
       "ce nombre d'étapes. La scène prend forme avant que les contraintes de "
       "géométrie ne tirent dessus."),
    ES("Suspender las penalizaciones de profundidad, normales y cobertura durante "
       "este número de pasos. La escena toma forma antes de que las restricciones "
       "de geometría tiren de ella."),
    PT("Suspender as penalidades de profundidade, normais e cobertura durante "
       "este número de passos. A cena toma forma antes que as restrições de geometria "
       "puxem por ela."),
    IT("Sospendere le penalità di profondità, normali e copertura per questo "
       "numero di passi. La scena prende forma prima che i vincoli di geometria "
       "comincino a tirarla."),
    NL("De diepte-, normalen- en dekkingsstraffen dit aantal stappen uitstellen. "
       "Zo krijgt de scène vorm voordat geometriebeperkingen eraan gaan trekken."),
    RU("Не включать штрафы по глубине, нормалям и покрытию столько шагов. Сцена "
       "успевает обрести форму, прежде чем геометрические ограничения начнут "
       "её тянуть."),
    TR("Derinlik, normal ve kaplama cezalarını bu kadar adım boyunca devre dışı "
       "bırakır. Böylece geometri kısıtları çekiştirmeye başlamadan önce sahne "
       "biçimlenir."));

SS_MSG(depth_supervision_weight,
    EN("Predicted depth guidance"), JA("推定深度による誘導"),
    ZH_HANS("预测深度的引导强度"), ZH_HANT("預測深度的引導強度"),
    KO("예측 깊이 안내"), DE("Führung durch geschätzte Tiefe"),
    FR("Guidage par la profondeur estimée"),
    ES("Guía por la profundidad estimada"),
    PT("Orientação pela profundidade estimada"),
    IT("Guida dalla profondità stimata"),
    NL("Sturing door de voorspelde diepte"),
    RU("Направление по предсказанной глубине"),
    TR("Tahmini derinlik rehberliği"));
SS_MSG(depth_supervision_weight_help,
    EN("How strongly AI-predicted depth guides the geometry. Helps in textureless "
       "areas, but a heavily biased prediction can pull quality down."),
    JA("AI が推定した深度が、どれだけ強くジオメトリを導くかです。模様のない場"
       "所で役に立ちますが、推定が大きく偏っていると品質を落とすことがあります。"),
    ZH_HANS("AI 预测的深度对几何的引导强度。在缺少纹理的地方很有用，但如果预测"
            "偏差很大，反而会拉低质量。"),
    ZH_HANT("AI 預測的深度對幾何的引導強度。在缺少紋理的地方很有用，但如果預測"
            "偏差很大，反而會拉低品質。"),
    KO("AI가 예측한 깊이가 지오메트리를 얼마나 강하게 이끄는지입니다. 무늬가 "
       "없는 곳에서 도움이 되지만, 예측이 크게 치우쳐 있으면 품질을 떨어뜨릴 "
       "수 있습니다."),
    DE("Wie stark die KI-geschätzte Tiefe die Geometrie führt. Hilft in texturlosen "
       "Bereichen, doch eine stark verzerrte Schätzung kann die Qualität drücken."),
    FR("Avec quelle force la profondeur estimée par l'IA guide la géométrie. "
       "Utile dans les zones sans texture, mais une estimation très biaisée peut "
       "faire baisser la qualité."),
    ES("Con qué fuerza la profundidad estimada por la IA guía la geometría. Útil "
       "en zonas sin textura, pero una estimación muy sesgada puede rebajar la "
       "calidad."),
    PT("Com que força a profundidade estimada pela IA guia a geometria. Útil "
       "em áreas sem textura, mas uma estimativa muito enviesada pode derrubar "
       "a qualidade."),
    IT("Con quanta forza la profondità stimata dall'IA guida la geometria. Utile "
       "nelle zone senza texture, ma una stima molto distorta può abbassare la "
       "qualità."),
    NL("Hoe sterk de door AI voorspelde diepte de geometrie stuurt. Nuttig in "
       "gebieden zonder textuur, maar een sterk vertekende voorspelling kan de "
       "kwaliteit omlaag trekken."),
    RU("Насколько сильно предсказанная ИИ глубина направляет геометрию. Помогает "
       "на однотонных участках, но сильно смещённое предсказание способно снизить "
       "качество."),
    TR("Yapay zekânın kestirdiği derinliğin geometriyi ne kadar güçlü yönlendirdiği. "
       "Dokusuz alanlarda işe yarar, ama ağır yanlı bir kestirim kaliteyi düşürebilir."));

SS_MSG(normal_supervision_weight,
    EN("Predicted normal guidance"), JA("推定法線による誘導"),
    ZH_HANS("预测法线的引导强度"), ZH_HANT("預測法線的引導強度"),
    KO("예측 노멀 안내"), DE("Führung durch geschätzte Normalen"),
    FR("Guidage par les normales estimées"),
    ES("Guía por las normales estimadas"),
    PT("Orientação pelas normais estimadas"),
    IT("Guida dalle normali stimate"),
    NL("Sturing door de voorspelde normalen"),
    RU("Направление по предсказанным нормалям"),
    TR("Tahmini normal rehberliği"));
SS_MSG(normal_supervision_weight_help,
    EN("How strongly AI-predicted surface direction guides the geometry. Helps "
       "flat surfaces come out flat."),
    JA("AI が推定した面の向きが、どれだけ強くジオメトリを導くかです。平らな面"
       "が平らに仕上がりやすくなります。"),
    ZH_HANS("AI 预测的表面朝向对几何的引导强度。有助于让平坦的表面真正变平。"),
    ZH_HANT("AI 預測的表面朝向對幾何的引導強度。有助於讓平坦的表面真正變平。"),
    KO("AI가 예측한 표면 방향이 지오메트리를 얼마나 강하게 이끄는지입니다. 평"
       "평한 면이 평평하게 나오도록 돕습니다."),
    DE("Wie stark die KI-geschätzte Oberflächenrichtung die Geometrie führt. "
       "Hilft, dass flache Flächen auch flach werden."),
    FR("Avec quelle force l'orientation des surfaces estimée par l'IA guide la "
       "géométrie. Aide les surfaces planes à ressortir planes."),
    ES("Con qué fuerza la orientación de superficie estimada por la IA guía la "
       "geometría. Ayuda a que las superficies planas salgan planas."),
    PT("Com que força a orientação de superfície estimada pela IA guia a geometria. "
       "Ajuda as superfícies planas a saírem planas."),
    IT("Con quanta forza l'orientamento delle superfici stimato dall'IA guida "
       "la geometria. Aiuta le superfici piane a venire piane."),
    NL("Hoe sterk de door AI voorspelde oppervlakterichting de geometrie stuurt. "
       "Helpt vlakke oppervlakken ook vlak te worden."),
    RU("Насколько сильно предсказанное ИИ направление поверхности направляет "
       "геометрию. Помогает плоским поверхностям выйти плоскими."),
    TR("Yapay zekânın kestirdiği yüzey yönünün geometriyi ne kadar güçlü yönlendirdiği. "
       "Düz yüzeylerin düz çıkmasına yardımcı olur."));

SS_MSG(input_depth_is_ray_depth,
    EN("Depth maps measure ray distance"), JA("深度は光線に沿った距離"),
    ZH_HANS("深度图记录沿光线的距离"), ZH_HANT("深度圖記錄沿光線的距離"),
    KO("깊이 맵이 광선 거리 기준"),
    DE("Tiefenkarten messen die Strahlentfernung"),
    FR("Les cartes de profondeur mesurent la distance au rayon"),
    ES("Los mapas de profundidad miden la distancia del rayo"),
    PT("Os mapas de profundidade medem a distância do raio"),
    IT("Le mappe di profondità misurano la distanza lungo il raggio"),
    NL("Dieptekaarten meten de straalafstand"),
    RU("Карты глубины измеряют расстояние вдоль луча"),
    TR("Derinlik haritaları ışın mesafesini ölçer"));
SS_MSG(input_depth_is_ray_depth_help,
    EN("Whether the supplied depth maps measure distance along the camera ray "
       "instead of distance straight ahead. Leave this unset and the lens decides: "
       "distance along the ray for a capture wide enough that `spirula geometry` "
       "had to split it into pinhole faces, straight ahead otherwise."),
    JA("与えた深度マップが、正面方向の距離ではなくカメラの光線に沿った距離かど"
       "うかです。未指定のままにするとレンズから判断します。`spirula geometry` が"
       "ピンホール面に分割するほど広い撮影なら光線に沿った距離、それ以外は正面方向です。"),
    ZH_HANS("提供的深度图记录的是沿相机光线的距离，还是正前方的距离。留空则"
            "由镜头决定：宽到 `spirula geometry` 必须拆成针孔面的拍摄用沿光线的"
            "距离，其余用正前方。"),
    ZH_HANT("提供的深度圖記錄的是沿相機光線的距離，還是正前方的距離。留空則"
            "由鏡頭決定：寬到 `spirula geometry` 必須拆成針孔面的拍攝用沿光線的"
            "距離，其餘用正前方。"),
    KO("제공한 깊이 맵이 정면 거리 대신 카메라 광선을 따라 잰 거리인지 여부입"
       "니다. 비워 두면 렌즈가 정합니다. `spirula geometry` 가 핀홀 면으로 나눠야 할 "
       "만큼 넓은 촬영은 광선 거리, 그 밖은 정면 거리입니다."),
    DE("Ob die gelieferten Tiefenkarten den Abstand entlang des Kamerastrahls "
       "statt geradeaus messen. Ohne Angabe entscheidet das Objektiv: Abstand "
       "entlang des Strahls bei einer Aufnahme, die `spirula geometry` in "
       "Lochkamera-Flächen zerlegen musste, sonst geradeaus."),
    FR("Les cartes de profondeur fournies mesurent-elles la distance le long "
       "du rayon de la caméra plutôt que la distance droit devant. Sans valeur, "
       "l'objectif décide : distance le long du rayon pour une prise assez large "
       "pour que `spirula geometry` ait dû la découper en faces sténopé, droit "
       "devant sinon."),
    ES("Si los mapas de profundidad suministrados miden la distancia a lo largo "
       "del rayo de la cámara en vez de la distancia hacia delante. Sin valor "
       "decide el objetivo: distancia a lo largo del rayo para una toma tan "
       "abierta que `spirula geometry` tuvo que dividirla en caras estenopeicas, "
       "y hacia delante en los demás casos."),
    PT("Se os mapas de profundidade fornecidos medem a distância ao longo do "
       "raio da câmera em vez da distância em frente. Sem valor quem decide é a "
       "lente: distância ao longo do raio para uma captura tão aberta que o "
       "`spirula geometry` teve de dividi-la em faces estenopeicas, em frente "
       "nos outros casos."),
    IT("Se le mappe di profondità fornite misurano la distanza lungo il raggio "
       "della camera invece della distanza in avanti. Senza valore decide "
       "l'obiettivo: distanza lungo il raggio per una ripresa così ampia che "
       "`spirula geometry` ha dovuto dividerla in facce stenopeiche, in avanti "
       "altrimenti."),
    NL("Of de aangeleverde dieptekaarten de afstand langs de camerastraal meten "
       "in plaats van recht vooruit. Laat dit leeg en de lens beslist: afstand "
       "langs de straal bij een opname die `spirula geometry` in "
       "gaatjescamera-vlakken moest splitsen, anders recht vooruit."),
    RU("Измеряют ли поданные карты глубины расстояние вдоль луча камеры, "
       "а не прямо вперёд. Если не задано, решает объектив: расстояние вдоль "
       "луча для съёмки настолько широкой, что `spirula geometry` пришлось "
       "разбить её на пинхол-грани, иначе — прямо вперёд."),
    TR("Verilen derinlik haritalarının ileri doğru mesafe yerine kamera ışını "
       "boyunca mesafeyi ölçüp ölçmediği. Boş bırakılırsa objektif karar verir: "
       "`spirula geometry` ın iğne deliği yüzlerine bölmek zorunda kaldığı kadar "
       "geniş çekimlerde ışın boyunca mesafe, diğerlerinde ileri doğru."));

SS_MSG(supervision_warmup,
    EN("Predicted-geometry guidance start"), JA("推定ジオメトリの誘導開始"),
    ZH_HANS("预测几何引导起始步"), ZH_HANT("預測幾何引導起始步"),
    KO("예측 지오메트리 안내 시작"),
    DE("Beginn der Führung durch geschätzte Geometrie"),
    FR("Début du guidage par la géométrie estimée"),
    ES("Inicio de la guía por la geometría estimada"),
    PT("Início da orientação pela geometria estimada"),
    IT("Inizio della guida dalla geometria stimata"),
    NL("Start van de sturing door de voorspelde geometrie"),
    RU("Начало направления по предсказанной геометрии"),
    TR("Tahmini geometri rehberliğinin başlangıcı"));
SS_MSG(supervision_warmup_help,
    EN("Step at which AI-predicted depth and normals start guiding training. "
       "Waiting lets photometric detail establish itself first."),
    JA("AI が推定した深度と法線が学習を導き始めるステップです。待つことで、ま"
       "ず写真から得られる細部が定着します。"),
    ZH_HANS("AI 预测的深度和法线开始引导训练的步数。先等一等，可以让来自照片本"
            "身的细节先立住。"),
    ZH_HANT("AI 預測的深度和法線開始引導訓練的步數。先等一等，可以讓來自照片本"
            "身的細節先立住。"),
    KO("AI가 예측한 깊이와 노멀이 학습을 이끌기 시작하는 스텝입니다. 기다리면"
       " 사진에서 얻는 디테일이 먼저 자리를 잡습니다."),
    DE("Schritt, ab dem KI-geschätzte Tiefe und Normalen das Training führen. "
       "Zu warten lässt zuerst das photometrische Detail Fuß fassen."),
    FR("Étape à partir de laquelle la profondeur et les normales estimées par "
       "l'IA guident l'entraînement. Attendre laisse d'abord le détail photométrique "
       "s'installer."),
    ES("Paso a partir del cual la profundidad y las normales estimadas por la "
       "IA guían el entrenamiento. Esperar deja que primero se afiance el detalle "
       "fotométrico."),
    PT("Passo a partir do qual a profundidade e as normais estimadas pela IA "
       "guiam o treinamento. Esperar deixa o detalhe fotométrico se firmar primeiro."),
    IT("Passo dal quale profondità e normali stimate dall'IA guidano l'addestramento. "
       "Aspettare lascia prima consolidarsi il dettaglio fotometrico."),
    NL("Stap vanaf welke door AI voorspelde diepte en normalen de training sturen. "
       "Wachten laat eerst het fotometrische detail zich vestigen."),
    RU("Шаг, с которого предсказанные ИИ глубина и нормали начинают направлять "
       "обучение. Ожидание даёт сначала закрепиться фотометрическим деталям."),
    TR("Yapay zekânın kestirdiği derinlik ve normallerin eğitimi yönlendirmeye "
       "başladığı adım. Beklemek, önce fotometrik ayrıntının yerleşmesini sağlar."));

SS_MSG(mean_median_depth_weight,
    EN("Average-to-solid depth agreement"), JA("平均深度と最密深度の一致"),
    ZH_HANS("平均深度与最实深度的一致性"),
    ZH_HANT("平均深度與最實深度的一致性"), KO("평균 깊이와 최밀 깊이 일치"),
    DE("Übereinstimmung von mittlerer und dichtester Tiefe"),
    FR("Accord entre profondeur moyenne et profondeur pleine"),
    ES("Acuerdo entre profundidad media y profundidad sólida"),
    PT("Concordância entre profundidade média e sólida"),
    IT("Accordo tra profondità media e profondità piena"),
    NL("Overeenkomst tussen gemiddelde en dichtste diepte"),
    RU("Согласие средней и наиболее плотной глубины"),
    TR("Ortalama ve en dolu derinliğin uyumu"));
SS_MSG(mean_median_depth_weight_help,
    EN("Encourage a pixel's average depth and its most-solid depth to agree, "
       "which pulls splats onto a single surface. Useful when the result is destined "
       "for a mesh."),
    JA("画素の平均深度と、いちばん密な深度が一致するように促します。スプラット"
       "が一つの面に引き寄せられます。結果をメッシュにするときに役立ちます。"),
    ZH_HANS("促使一个像素的平均深度与其最实处的深度取得一致，从而把泼溅拉到同"
            "一个表面上。当结果要用来生成网格时很有用。"),
    ZH_HANT("促使一個像素的平均深度與其最實處的深度取得一致，從而把潑濺拉到同"
            "一個表面上。當結果要用來產生網格時很有用。"),
    KO("한 픽셀의 평균 깊이와 가장 밀도가 높은 깊이가 일치하도록 유도해 스플랫"
       "을 하나의 면으로 끌어당깁니다. 결과를 메시로 만들 때 유용합니다."),
    DE("Dazu anhalten, dass die mittlere Tiefe eines Pixels und seine dichteste "
       "Tiefe übereinstimmen, was Splats auf eine einzige Fläche zieht. Nützlich, "
       "wenn aus dem Ergebnis ein Netz werden soll."),
    FR("Inciter la profondeur moyenne d'un pixel et sa profondeur la plus pleine "
       "à s'accorder, ce qui ramène les splats sur une seule surface. Utile quand "
       "le résultat est destiné à un maillage."),
    ES("Animar a que la profundidad media de un píxel y su profundidad más sólida "
       "coincidan, lo que atrae los splats a una sola superficie. Útil cuando "
       "el resultado va a convertirse en malla."),
    PT("Incentivar que a profundidade média de um pixel e a sua profundidade "
       "mais sólida concordem, o que puxa os splats para uma única superfície. "
       "Útil quando o resultado se destina a uma malha."),
    IT("Incoraggiare la profondità media di un pixel e la sua profondità più "
       "piena ad accordarsi, il che tira gli splat su una sola superficie. Utile "
       "quando il risultato è destinato a una mesh."),
    NL("Aanmoedigen dat de gemiddelde diepte van een pixel en de dichtste diepte "
       "overeenkomen, wat splats naar één oppervlak trekt. Nuttig als het resultaat "
       "een mesh moet worden."),
    RU("Побуждать среднюю глубину пикселя и его самую плотную глубину совпадать "
       "— это стягивает сплаты на одну поверхность. Полезно, когда результат "
       "пойдёт в меш."),
    TR("Bir pikselin ortalama derinliği ile en dolu derinliğinin uyuşmasını teşvik "
       "eder; bu, splat'ları tek bir yüzeye çeker. Sonuç ağa dönüştürülecekse "
       "işe yarar."));

SS_MSG(median_depth_normal_reg_weight,
    EN("Solid-depth direction agreement"), JA("最密深度での向きの一致"),
    ZH_HANS("最实深度处的朝向一致性"), ZH_HANT("最實深度處的朝向一致性"),
    KO("최밀 깊이 방향 일치"),
    DE("Richtungsübereinstimmung an der dichtesten Tiefe"),
    FR("Accord d'orientation à la profondeur pleine"),
    ES("Acuerdo de orientación en la profundidad sólida"),
    PT("Concordância de orientação na profundidade sólida"),
    IT("Accordo di orientamento alla profondità piena"),
    NL("Richtingsovereenkomst op de dichtste diepte"),
    RU("Согласие направлений на самой плотной глубине"),
    TR("En dolu derinlikte yön uyumu"));
SS_MSG(median_depth_normal_reg_weight_help,
    EN("Encourage surface direction to agree between the average depth and the "
       "most-solid depth. Another crispness dial for meshing."),
    JA("平均深度と最も密な深度のあいだで、面の向きが一致するように促します。メ"
       "ッシュ化に向けたもう一つのくっきり具合のつまみです。"),
    ZH_HANS("促使平均深度处与最实深度处的表面朝向取得一致。这是面向网格化的另"
            "一个清晰度旋钮。"),
    ZH_HANT("促使平均深度處與最實深度處的表面朝向取得一致。這是面向網格化的另"
            "一個清晰度旋鈕。"),
    KO("평균 깊이와 가장 밀도가 높은 깊이 사이에서 표면 방향이 일치하도록 유도"
       "합니다. 메시 변환을 위한 또 하나의 선명도 조절값입니다."),
    DE("Dazu anhalten, dass die Oberflächenrichtung zwischen mittlerer und dichtester "
       "Tiefe übereinstimmt. Ein weiterer Schärferegler für die Netzerzeugung."),
    FR("Inciter l'orientation des surfaces à s'accorder entre la profondeur moyenne "
       "et la profondeur la plus pleine. Un autre réglage de netteté en vue du "
       "maillage."),
    ES("Animar a que la orientación de la superficie coincida entre la profundidad "
       "media y la más sólida. Otro mando de nitidez de cara al mallado."),
    PT("Incentivar que a orientação da superfície coincida entre a profundidade "
       "média e a mais sólida. Outro controle de nitidez com vista à malha."),
    IT("Incoraggiare l'orientamento della superficie ad accordarsi tra la profondità "
       "media e quella più piena. Un'altra manopola di nitidezza in vista della "
       "mesh."),
    NL("Aanmoedigen dat de oppervlakterichting overeenkomt tussen de gemiddelde "
       "en de dichtste diepte. Nog een scherpteknop met het oog op mesh-generatie."),
    RU("Побуждать направление поверхности совпадать между средней и самой плотной "
       "глубиной. Ещё один регулятор чёткости для построения меша."),
    TR("Yüzey yönünün ortalama derinlik ile en dolu derinlik arasında uyuşmasını "
       "teşvik eder. Ağ çıkarmaya yönelik bir keskinlik ayarı daha."));

SS_MSG(median_normal_supervision_weight,
    EN("Solid-depth normal guidance"), JA("最密深度の法線を推定に合わせる"),
    ZH_HANS("最实深度法线对齐预测值"), ZH_HANT("最實深度法線對齊預測值"),
    KO("최밀 깊이 노멀을 예측에 맞춤"),
    DE("Normalen an der dichtesten Tiefe an die Schätzung angleichen"),
    FR("Aligner les normales à la profondeur pleine sur l'estimation"),
    ES("Alinear las normales de la profundidad sólida con la estimación"),
    PT("Alinhar as normais da profundidade sólida à estimativa"),
    IT("Allineare le normali alla profondità piena con la stima"),
    NL("Normalen op de dichtste diepte op de schatting afstemmen"),
    RU("Согласовать нормали на плотной глубине с предсказанием"),
    TR("En dolu derinlikteki normalleri tahminle hizala"));
SS_MSG(median_normal_supervision_weight_help,
    EN("Match the surface direction at the most-solid depth to AI-predicted normals."),
    JA("いちばん密な深度における面の向きを、AI が推定した法線に合わせます。"),
    ZH_HANS("让最实深度处的表面朝向对齐到 AI 预测的法线。"),
    ZH_HANT("讓最實深度處的表面朝向對齊到 AI 預測的法線。"),
    KO("가장 밀도가 높은 깊이에서의 표면 방향을 AI가 예측한 노멀에 맞춥니다."),
    DE("Die Oberflächenrichtung an der dichtesten Tiefe an die KI-geschätzten "
       "Normalen angleichen."),
    FR("Aligner l'orientation des surfaces à la profondeur la plus pleine sur "
       "les normales estimées par l'IA."),
    ES("Alinear la orientación de la superficie en la profundidad más sólida "
       "con las normales estimadas por la IA."),
    PT("Alinhar a orientação da superfície na profundidade mais sólida com as "
       "normais estimadas pela IA."),
    IT("Allineare l'orientamento della superficie alla profondità più piena con "
       "le normali stimate dall'IA."),
    NL("De oppervlakterichting op de dichtste diepte afstemmen op de door AI "
       "voorspelde normalen."),
    RU("Согласовать направление поверхности на самой плотной глубине с предсказанными "
       "ИИ нормалями."),
    TR("En dolu derinlikteki yüzey yönünü yapay zekânın kestirdiği normallerle "
       "hizalar."));

SS_MSG(median_render_normal_reg_weight,
    EN("Solid-depth rendered normal agreement"),
    JA("最密深度の法線を描画法線に合わせる"),
    ZH_HANS("最实深度法线对齐渲染法线"), ZH_HANT("最實深度法線對齊算圖法線"),
    KO("최밀 깊이 노멀을 렌더 노멀에 맞춤"),
    DE("Normalen an der dichtesten Tiefe an die gerenderten angleichen"),
    FR("Aligner les normales à la profondeur pleine sur celles du rendu"),
    ES("Alinear las normales de la profundidad sólida con las del render"),
    PT("Alinhar as normais da profundidade sólida às renderizadas"),
    IT("Allineare le normali alla profondità piena con quelle del render"),
    NL("Normalen op de dichtste diepte op de gerenderde afstemmen"),
    RU("Согласовать нормали на плотной глубине с отрисованными"),
    TR("En dolu derinlikteki normalleri çizilenlerle hizala"));
SS_MSG(median_render_normal_reg_weight_help,
    EN("Match the surface direction at the most-solid depth to the rendered normals."),
    JA("いちばん密な深度における面の向きを、描画された法線に合わせます。"),
    ZH_HANS("让最实深度处的表面朝向对齐到渲染出的法线。"),
    ZH_HANT("讓最實深度處的表面朝向對齊到算圖出的法線。"),
    KO("가장 밀도가 높은 깊이에서의 표면 방향을 렌더된 노멀에 맞춥니다."),
    DE("Die Oberflächenrichtung an der dichtesten Tiefe an die gerenderten Normalen "
       "angleichen."),
    FR("Aligner l'orientation des surfaces à la profondeur la plus pleine sur "
       "les normales du rendu."),
    ES("Alinear la orientación de la superficie en la profundidad más sólida "
       "con las normales del render."),
    PT("Alinhar a orientação da superfície na profundidade mais sólida com as "
       "normais da renderização."),
    IT("Allineare l'orientamento della superficie alla profondità più piena con "
       "le normali del render."),
    NL("De oppervlakterichting op de dichtste diepte afstemmen op de gerenderde "
       "normalen."),
    RU("Согласовать направление поверхности на самой плотной глубине с отрисованными "
       "нормалями."),
    TR("En dolu derinlikteki yüzey yönünü çizilen normallerle hizalar."));

SS_MSG(median_warmup,
    EN("Solid-depth penalty warm-up"), JA("最密深度ペナルティの立ち上がり"),
    ZH_HANS("最实深度惩罚预热"), ZH_HANT("最實深度懲罰預熱"),
    KO("최밀 깊이 페널티 워밍업"),
    DE("Anlauf der Strafen an der dichtesten Tiefe"),
    FR("Montée des pénalités à la profondeur pleine"),
    ES("Arranque de las penalizaciones de profundidad sólida"),
    PT("Aquecimento das penalidades de profundidade sólida"),
    IT("Avvio delle penalità alla profondità piena"),
    NL("Opbouw van de straffen op de dichtste diepte"),
    RU("Разгон штрафов на плотной глубине"),
    TR("En dolu derinlik cezalarının ısınması"));
SS_MSG(median_warmup_help,
    EN("How many steps the four most-solid-depth penalties take to reach full "
       "strength."),
    JA("最も密な深度に関する 4 つのペナルティが最大の強さになるまでのステップ"
       "数です。"),
    ZH_HANS("与最实深度相关的四项惩罚达到最大强度所需的步数。"),
    ZH_HANT("與最實深度相關的四項懲罰達到最大強度所需的步數。"),
    KO("가장 밀도가 높은 깊이에 관한 네 가지 페널티가 최대 강도에 이르기까지의"
       " 스텝 수입니다."),
    DE("Wie viele Schritte die vier Strafen an der dichtesten Tiefe brauchen, "
       "um volle Stärke zu erreichen."),
    FR("Combien d'étapes les quatre pénalités liées à la profondeur la plus pleine "
       "mettent à atteindre leur pleine force."),
    ES("Cuántos pasos tardan las cuatro penalizaciones de la profundidad más "
       "sólida en alcanzar toda su fuerza."),
    PT("Quantos passos as quatro penalidades da profundidade mais sólida levam "
       "para atingir força total."),
    IT("Quanti passi impiegano le quattro penalità alla profondità più piena "
       "a raggiungere la piena forza."),
    NL("Hoeveel stappen de vier straffen op de dichtste diepte nodig hebben om "
       "op volle sterkte te komen."),
    RU("За сколько шагов четыре штрафа, связанные с самой плотной глубиной, набирают "
       "полную силу."),
    TR("En dolu derinlikle ilgili dört cezanın tam güce ulaşması için gereken "
       "adım sayısı."));


// ===========================================================================
// Splat Shape
// ===========================================================================

SS_MSG(opacity_reg,
    EN("Opacity penalty"), JA("不透明度のペナルティ"),
    ZH_HANS("不透明度惩罚"), ZH_HANT("不透明度懲罰"), KO("불투명도 페널티"),
    DE("Deckkraftstrafe"), FR("Pénalité d'opacité"),
    ES("Penalización de opacidad"), PT("Penalidade de opacidade"),
    IT("Penalità di opacità"), NL("Dekkingsstraf"),
    RU("Штраф за непрозрачность"), TR("Saydamsızlık cezası"));
SS_MSG(opacity_reg_help,
    EN("Gently push splat opacity down so weak splats get recycled where they "
       "are needed more. Higher recycles more aggressively."),
    JA("スプラットの不透明度をゆるやかに下げ、弱いスプラットをもっと必要な場所"
       "へ回します。高いほど積極的に入れ替わります。"),
    ZH_HANS("温和地压低泼溅的不透明度，让弱泼溅被回收到更需要的地方。数值越高"
            "回收越积极。"),
    ZH_HANT("溫和地壓低潑濺的不透明度，讓弱潑濺被回收到更需要的地方。數值越高"
            "回收越積極。"),
    KO("스플랫의 불투명도를 완만하게 낮춰 약한 스플랫이 더 필요한 곳으로 재활"
       "용되게 합니다. 값이 클수록 더 적극적으로 재활용합니다."),
    DE("Die Deckkraft der Splats sanft senken, damit schwache Splats dorthin "
       "recycelt werden, wo sie nötiger sind. Höher recycelt entschlossener."),
    FR("Faire baisser doucement l'opacité des splats pour que les plus faibles "
       "soient recyclés là où ils manquent. Plus haut recycle plus vigoureusement."),
    ES("Bajar poco a poco la opacidad de los splats para que los más débiles "
       "se reciclen donde hacen más falta. Más alto recicla con más energía."),
    PT("Baixar suavemente a opacidade dos splats para que os mais fracos sejam "
       "reciclados onde fazem mais falta. Mais alto recicla com mais energia."),
    IT("Abbassare dolcemente l'opacità degli splat perché i più deboli vengano "
       "riciclati dove servono di più. Più alto ricicla con più decisione."),
    NL("De dekking van splats zachtjes omlaag duwen zodat zwakke splats worden "
       "hergebruikt waar ze harder nodig zijn. Hoger hergebruikt feller."),
    RU("Плавно снижать непрозрачность сплатов, чтобы слабые перерабатывались "
       "туда, где нужнее. Больше — переработка активнее."),
    TR("Splat'ların saydamsızlığını yumuşakça düşürerek zayıf olanların daha "
       "çok gerekli olduğu yere geri dönüştürülmesini sağlar. Yüksek değerler "
       "daha atak geri dönüştürür."));

SS_MSG(scale_reg,
    EN("Size penalty"), JA("大きさのペナルティ"), ZH_HANS("尺寸惩罚"),
    ZH_HANT("尺寸懲罰"), KO("크기 페널티"), DE("Größenstrafe"),
    FR("Pénalité de taille"), ES("Penalización de tamaño"),
    PT("Penalidade de tamanho"), IT("Penalità di dimensione"),
    NL("Groottestraf"), RU("Штраф за размер"), TR("Boyut cezası"));
SS_MSG(scale_reg_help,
    EN("Gently push splat size down. Keeps splats compact so detail stays local; "
       "too high leaves large flat areas underfilled."),
    JA("スプラットの大きさをゆるやかに下げます。スプラットがまとまり、細部が局"
       "所にとどまりますが、上げすぎると広く平らな面が埋まりきりません。"),
    ZH_HANS("温和地压低泼溅的尺寸。让泼溅保持紧凑，细节留在局部；过高则会让大"
            "片平坦区域填不满。"),
    ZH_HANT("溫和地壓低潑濺的尺寸。讓潑濺保持緊湊，細節留在局部；過高則會讓大"
            "片平坦區域填不滿。"),
    KO("스플랫 크기를 완만하게 줄입니다. 스플랫이 조밀해져 디테일이 국소에 머"
       "무르지만, 너무 크면 넓고 평평한 면이 덜 채워집니다."),
    DE("Die Größe der Splats sanft senken. Hält Splats kompakt, sodass Detail "
       "lokal bleibt; zu hoch lässt große glatte Flächen unterfüllt."),
    FR("Faire baisser doucement la taille des splats. Les garde compacts pour "
       "que le détail reste local ; trop haut laisse les grandes surfaces planes "
       "mal remplies."),
    ES("Bajar poco a poco el tamaño de los splats. Los mantiene compactos para "
       "que el detalle sea local; demasiado deja mal rellenas las superficies "
       "planas grandes."),
    PT("Baixar suavemente o tamanho dos splats. Mantém-nos compactos para o detalhe "
       "ficar local; alto demais deixa grandes áreas planas mal preenchidas."),
    IT("Abbassare dolcemente la dimensione degli splat. Li tiene compatti così "
       "il dettaglio resta locale; troppo alto lascia mal riempite le grandi "
       "superfici piane."),
    NL("De grootte van splats zachtjes omlaag duwen. Houdt splats compact zodat "
       "detail lokaal blijft; te hoog laat grote vlakke gebieden ondergevuld."),
    RU("Плавно уменьшать размер сплатов. Держит их компактными, так что детали "
       "остаются локальными; перебор оставляет большие ровные площади недозаполненными."),
    TR("Splat boyutunu yumuşakça düşürür. Splat'ları derli toplu tutar, böylece "
       "ayrıntı yerel kalır; aşırısı büyük düz alanları eksik doldurur."));

SS_MSG(opacity_decay,
    EN("Opacity decay"), JA("不透明度の減衰"), ZH_HANS("不透明度衰减"),
    ZH_HANT("不透明度衰減"), KO("불투명도 감쇠"),
    DE("Abklingen der Deckkraft"), FR("Décroissance de l'opacité"),
    ES("Decaimiento de la opacidad"), PT("Decaimento da opacidade"),
    IT("Decadimento dell'opacità"), NL("Afname van de dekking"),
    RU("Затухание непрозрачности"), TR("Saydamsızlık sönümü"));
SS_MSG(opacity_decay_help,
    EN("Fade every splat's opacity slightly each step, freeing weak ones for "
       "reuse. An alternative to opacity_reg that acts on all splats equally."),
    JA("すべてのスプラットの不透明度を毎ステップ少しずつ薄め、弱いものを再利用"
       "に回します。opacity_reg の代わりに使え、すべてのスプラットに等しく効き"
       "ます。"),
    ZH_HANS("每一步都把所有泼溅的不透明度稍稍淡化，把弱的释放出来重新利用。可"
            "作为 opacity_reg 的替代，对所有泼溅一视同仁。"),
    ZH_HANT("每一步都把所有潑濺的不透明度稍稍淡化，把弱的釋放出來重新利用。可"
            "作為 opacity_reg 的替代，對所有潑濺一視同仁。"),
    KO("매 스텝마다 모든 스플랫의 불투명도를 조금씩 낮춰 약한 것을 재사용으로"
       " 돌립니다. opacity_reg의 대안이며 모든 스플랫에 똑같이 작용합니다."),
    DE("Die Deckkraft jedes Splats bei jedem Schritt ein wenig verblassen lassen "
       "und schwache so zur Wiederverwendung freigeben. Eine Alternative zu opacity_reg, "
       "die alle Splats gleich behandelt."),
    FR("Estomper légèrement l'opacité de chaque splat à chaque étape, libérant "
       "les plus faibles pour réemploi. Une solution de rechange à opacity_reg, "
       "qui agit sur tous les splats de la même façon."),
    ES("Atenuar un poco la opacidad de cada splat en cada paso, liberando los "
       "débiles para reutilizarlos. Una alternativa a opacity_reg que actúa igual "
       "sobre todos los splats."),
    PT("Esmaecer um pouco a opacidade de cada splat a cada passo, liberando os "
       "fracos para reutilização. Uma alternativa ao opacity_reg que age igualmente "
       "sobre todos os splats."),
    IT("Sbiadire un poco l'opacità di ogni splat a ogni passo, liberando i più "
       "deboli per il riuso. Un'alternativa a opacity_reg che agisce allo stesso "
       "modo su tutti gli splat."),
    NL("De dekking van elke splat bij elke stap iets laten vervagen, waardoor "
       "zwakke splats vrijkomen voor hergebruik. Een alternatief voor opacity_reg "
       "dat alle splats gelijk behandelt."),
    RU("Слегка гасить непрозрачность каждого сплата на каждом шаге, освобождая "
       "слабые для повторного использования. Замена opacity_reg, действующая "
       "одинаково на все сплаты."),
    TR("Her adımda her splat'ın saydamsızlığını biraz soldurur ve zayıf olanları "
       "yeniden kullanıma açar. opacity_reg'e alternatiftir ve tüm splat'lara "
       "eşit davranır."));

SS_MSG(scale_decay,
    EN("Size decay"), JA("大きさの減衰"), ZH_HANS("尺寸衰减"),
    ZH_HANT("尺寸衰減"), KO("크기 감쇠"), DE("Abklingen der Größe"),
    FR("Décroissance de la taille"), ES("Decaimiento del tamaño"),
    PT("Decaimento do tamanho"), IT("Decadimento della dimensione"),
    NL("Afname van de grootte"), RU("Затухание размера"), TR("Boyut sönümü"));
SS_MSG(scale_decay_help,
    EN("Shrink every splat slightly each step. An alternative to scale_reg that "
       "acts on all splats equally."),
    JA("すべてのスプラットを毎ステップ少しずつ縮めます。scale_reg の代わりに使"
       "え、すべてのスプラットに等しく効きます。"),
    ZH_HANS("每一步都把所有泼溅稍稍缩小。可作为 scale_reg 的替代，对所有泼溅一"
            "视同仁。"),
    ZH_HANT("每一步都把所有潑濺稍稍縮小。可作為 scale_reg 的替代，對所有潑濺一"
            "視同仁。"),
    KO("매 스텝마다 모든 스플랫을 조금씩 줄입니다. scale_reg의 대안이며 모든 "
       "스플랫에 똑같이 작용합니다."),
    DE("Jeden Splat bei jedem Schritt ein wenig verkleinern. Eine Alternative "
       "zu scale_reg, die alle Splats gleich behandelt."),
    FR("Rétrécir légèrement chaque splat à chaque étape. Une solution de rechange "
       "à scale_reg, qui agit sur tous les splats de la même façon."),
    ES("Encoger un poco cada splat en cada paso. Una alternativa a scale_reg "
       "que actúa igual sobre todos los splats."),
    PT("Encolher um pouco cada splat a cada passo. Uma alternativa ao scale_reg "
       "que age igualmente sobre todos os splats."),
    IT("Rimpicciolire un poco ogni splat a ogni passo. Un'alternativa a scale_reg "
       "che agisce allo stesso modo su tutti gli splat."),
    NL("Elke splat bij elke stap iets verkleinen. Een alternatief voor scale_reg "
       "dat alle splats gelijk behandelt."),
    RU("Слегка уменьшать каждый сплат на каждом шаге. Замена scale_reg, действующая "
       "одинаково на все сплаты."),
    TR("Her adımda her splat'ı biraz küçültür. scale_reg'e alternatiftir ve tüm "
       "splat'lara eşit davranır."));

SS_MSG(erank_reg,
    EN("Roundness (erank)"), JA("丸さ（erank）"), ZH_HANS("圆度（erank）"),
    ZH_HANT("圓度（erank）"), KO("둥글기(erank)"), DE("Rundheit (erank)"),
    FR("Rondeur (erank)"), ES("Redondez (erank)"),
    PT("Arredondamento (erank)"), IT("Rotondità (erank)"),
    NL("Rondheid (erank)"), RU("Округлость (erank)"),
    TR("Yuvarlaklık (erank)"));
SS_MSG(erank_reg_help,
    EN("Discourage needle-like splats in favor of rounder ones. Reduces spiky "
       "artifacts and flicker as the camera moves."),
    JA("針のように細長いスプラットを避け、丸みのある形を促します。とがったノイ"
       "ズや、カメラを動かしたときのちらつきが減ります。"),
    ZH_HANS("抑制针状的细长泼溅，鼓励更圆的形状。可减少尖刺状伪影以及相机移动"
            "时的闪烁。"),
    ZH_HANT("抑制針狀的細長潑濺，鼓勵更圓的形狀。可減少尖刺狀偽影以及相機移動"
            "時的閃爍。"),
    KO("바늘처럼 가늘고 긴 스플랫을 억제하고 둥근 형태를 권장합니다. 뾰족한 아"
       "티팩트와 카메라를 움직일 때의 깜빡임이 줄어듭니다."),
    DE("Nadelförmige Splats zugunsten rundlicherer zurückdrängen. Verringert "
       "stachelige Artefakte und Flimmern bei Kamerabewegung."),
    FR("Décourager les splats en aiguille au profit de formes plus rondes. Réduit "
       "les artefacts pointus et le scintillement quand la caméra bouge."),
    ES("Desalentar los splats en forma de aguja en favor de otros más redondos. "
       "Reduce los artefactos puntiagudos y el parpadeo al mover la cámara."),
    PT("Desencorajar splats em forma de agulha em favor de outros mais redondos. "
       "Reduz artefatos pontiagudos e a cintilação ao mover a câmera."),
    IT("Scoraggiare gli splat ad ago a favore di forme più tonde. Riduce gli "
       "artefatti appuntiti e lo sfarfallio quando la camera si muove."),
    NL("Naaldvormige splats ontmoedigen ten gunste van rondere. Vermindert stekelige "
       "artefacten en flikkering als de camera beweegt."),
    RU("Отговаривать сплаты от иглообразной формы в пользу более округлой. Уменьшает "
       "колючие артефакты и мерцание при движении камеры."),
    TR("İğne gibi splat'ları caydırıp daha yuvarlak olanları destekler. Sivri "
       "bozulmaları ve kamera hareket ederken oluşan titremeyi azaltır."));

SS_MSG(erank_reg_s3,
    EN("Flatness (erank)"), JA("平坦さ（erank）"), ZH_HANS("扁平度（erank）"),
    ZH_HANT("扁平度（erank）"), KO("납작함(erank)"), DE("Flachheit (erank)"),
    FR("Aplatissement (erank)"), ES("Aplanamiento (erank)"),
    PT("Achatamento (erank)"), IT("Appiattimento (erank)"),
    NL("Vlakheid (erank)"), RU("Плоскостность (erank)"),
    TR("Yassılık (erank)"));
SS_MSG(erank_reg_s3_help,
    EN("Encourage splats to collapse into flat sheets, so they line up with "
       "surfaces better."),
    JA("スプラットが平らな板に潰れるように促し、面によく沿うようにします。"),
    ZH_HANS("鼓励泼溅塌成扁平薄片，让它更好地贴合表面。"),
    ZH_HANT("鼓勵潑濺塌成扁平薄片，讓它更好地貼合表面。"),
    KO("스플랫이 납작한 판으로 무너지도록 유도해 표면에 더 잘 들어맞게 "
       "합니다."),
    DE("Splats dazu bringen, zu flachen Blättern zusammenzufallen, damit sie "
       "sich besser an Oberflächen anlegen."),
    FR("Encourager les splats à s'effondrer en feuilles plates, pour qu'ils "
       "épousent mieux les surfaces."),
    ES("Animar a los splats a aplastarse en láminas planas, para que se "
       "ajusten mejor a las superficies."),
    PT("Incentivar os splats a desabarem em folhas planas, para que se ajustem "
       "melhor às superfícies."),
    IT("Incoraggiare il collasso degli splat in fogli piatti, così da "
       "aderire meglio alle superfici."),
    NL("Splats aanmoedigen tot platte vellen in te storten, zodat ze beter op "
       "oppervlakken aansluiten."),
    RU("Побуждать сплаты схлопываться в плоские листы, чтобы они лучше "
       "ложились на поверхности."),
    TR("Splat'ları düz levhalara çökmeye özendirir, böylece yüzeylere daha "
       "iyi otururlar."));

SS_MSG(scale_regularization_weight,
    EN("Stretched splat penalty"), JA("細長いスプラットのペナルティ"),
    ZH_HANS("拉长泼溅的惩罚"), ZH_HANT("拉長潑濺的懲罰"),
    KO("길쭉한 스플랫 페널티"), DE("Strafe für gestreckte Splats"),
    FR("Pénalité des splats étirés"),
    ES("Penalización de los splats estirados"),
    PT("Penalidade dos splats esticados"),
    IT("Penalità degli splat allungati"), NL("Straf voor uitgerekte splats"),
    RU("Штраф за вытянутые сплаты"), TR("Uzamış splat cezası"));
SS_MSG(scale_regularization_weight_help,
    EN("Penalize splats that are far longer in one direction than another, which "
       "suppresses long spiky artifacts."),
    JA("ある方向にだけ極端に長いスプラットを罰します。長くとがったノイズを抑え"
       "られます。"),
    ZH_HANS("惩罚在某一方向上远比其他方向长的泼溅，可以抑制细长的尖刺状伪影。"),
    ZH_HANT("懲罰在某一方向上遠比其他方向長的潑濺，可以抑制細長的尖刺狀偽影。"),
    KO("한 방향으로만 유난히 긴 스플랫에 벌을 줍니다. 길고 뾰족한 아티팩트를 "
       "억제합니다."),
    DE("Splats bestrafen, die in einer Richtung weit länger sind als in einer "
       "anderen, was lange stachelige Artefakte unterdrückt."),
    FR("Pénaliser les splats bien plus longs dans une direction que dans une "
       "autre, ce qui supprime les longs artefacts pointus."),
    ES("Penalizar los splats mucho más largos en una dirección que en otra, lo "
       "que suprime los artefactos largos y puntiagudos."),
    PT("Penalizar os splats muito mais longos numa direção do que noutra, o que "
       "suprime artefatos longos e pontiagudos."),
    IT("Penalizzare gli splat molto più lunghi in una direzione che in un'altra, "
       "il che elimina i lunghi artefatti appuntiti."),
    NL("Splats bestraffen die in de ene richting veel langer zijn dan in de andere, "
       "wat lange stekelige artefacten onderdrukt."),
    RU("Штрафовать сплаты, вытянутые в одном направлении гораздо сильнее, чем "
       "в другом, — это подавляет длинные колючие артефакты."),
    TR("Bir yönde diğerine göre çok daha uzun olan splat'ları cezalandırır; bu, "
       "uzun sivri bozulmaları bastırır."));

SS_MSG(max_gauss_ratio,
    EN("Allowed stretch"), JA("許容する細長さ"), ZH_HANS("允许的拉伸比"),
    ZH_HANT("允許的拉伸比"), KO("허용 신장 비율"), DE("Zulässige Streckung"),
    FR("Étirement autorisé"), ES("Estiramiento permitido"),
    PT("Alongamento permitido"), IT("Allungamento consentito"),
    NL("Toegestane rekking"), RU("Допустимая вытянутость"),
    TR("İzin verilen uzama"));
SS_MSG(max_gauss_ratio_help,
    EN("How stretched a splat may get before the spiky-splat penalty applies. "
       "Lower forces rounder splats."),
    JA("とがったスプラットへのペナルティがかかり始める、細長さの上限です。下げ"
       "るほど丸いスプラットが強制されます。"),
    ZH_HANS("泼溅在多长的拉伸比之后才会受到尖刺惩罚。数值越低，越强制泼溅变圆。"),
    ZH_HANT("潑濺在多長的拉伸比之後才會受到尖刺懲罰。數值越低，越強制潑濺變圓。"),
    KO("뾰족한 스플랫 페널티가 걸리기 시작하는 신장 한계입니다. 낮출수록 더 둥"
       "근 스플랫을 강제합니다."),
    DE("Wie gestreckt ein Splat werden darf, bevor die Strafe für stachelige "
       "Splats greift. Niedriger erzwingt rundere Splats."),
    FR("Jusqu'où un splat peut s'étirer avant que la pénalité anti-pointes ne "
       "s'applique. Plus bas force des splats plus ronds."),
    ES("Cuánto puede estirarse un splat antes de que actúe la penalización antipuntas. "
       "Más bajo fuerza splats más redondos."),
    PT("Quanto um splat pode se esticar antes de a penalidade antiponta atuar. "
       "Mais baixo força splats mais redondos."),
    IT("Quanto uno splat può allungarsi prima che scatti la penalità anti-punta. "
       "Più basso forza splat più tondi."),
    NL("Hoever een splat mag uitrekken voordat de straf tegen stekels aanslaat. "
       "Lager dwingt rondere splats af."),
    RU("Насколько сплат может вытянуться, прежде чем сработает штраф за колючесть. "
       "Меньше — сплаты вынуждены быть круглее."),
    TR("Sivri splat cezası devreye girmeden önce bir splat'ın ne kadar uzayabileceği. "
       "Düşük değerler daha yuvarlak splat'lar dayatır."));

SS_MSG(dc_reg,
    EN("Base color range penalty"), JA("基本色の範囲のペナルティ"),
    ZH_HANS("基础颜色范围的惩罚"), ZH_HANT("基礎顏色範圍的懲罰"),
    KO("기본 색 범위 페널티"), DE("Strafe für den Bereich der Grundfarbe"),
    FR("Pénalité de plage de la couleur de base"),
    ES("Penalización del rango del color base"),
    PT("Penalidade do intervalo da cor base"),
    IT("Penalità dell'intervallo del colore base"),
    NL("Straf voor het bereik van de basiskleur"),
    RU("Штраф за диапазон базового цвета"), TR("Temel renk aralığı cezası"));
SS_MSG(dc_reg_help,
    EN("Hold each splat's own base color -- the part that stays the same from "
       "every angle -- inside the 0 to 1 range. Higher keeps surfaces clean; "
       "too high clips the real range between bright and dark, so lower it "
       "when the scene needs that room."),
    JA("どの角度から見ても変わらないスプラット自身の基本色を、0 から 1 の範囲"
       "に収めます。高いほど面がきれいになりますが、上げすぎると本当の明暗の幅"
       "まで切り落とすので、その幅が要る場面では下げてください。"),
    ZH_HANS("把泼溅自身的基础颜色约束在 0 到 1 的范围内，也就是从任何角度看都"
            "不变的那部分。数值越高，表面越干净；过高则会切掉真实的明暗范围，"
            "场景需要这个范围时请调低。"),
    ZH_HANT("把潑濺自身的基礎顏色約束在 0 到 1 的範圍內，也就是從任何角度看都"
            "不變的那部分。數值越高，表面越乾淨；過高則會切掉真實的明暗範圍，"
            "場景需要這個範圍時請調低。"),
    KO("어느 각도에서도 달라지지 않는 스플랫 자신의 기본 색을 0 에서 1 사이"
       "로 붙잡아 둡니다. 값이 크면 면이 깨끗해지지만, 너무 크면 진짜 밝고 어두"
       "운 폭까지 잘리므로 그 폭이 필요한 장면에서는 낮추세요."),
    DE("Die eigene Grundfarbe jedes Splats -- den Teil, der aus jedem Winkel "
       "gleich bleibt -- im Bereich 0 bis 1 halten. Höher hält Flächen sauber; "
       "zu hoch schneidet den echten Abstand zwischen hell und dunkel ab, also "
       "senken, wenn die Szene diesen Spielraum braucht."),
    FR("Maintenir la couleur de base propre à chaque splat -- la part qui ne "
       "change pas avec l'angle -- dans la plage 0 à 1. Plus haut garde les "
       "surfaces propres ; trop haut rogne l'écart réel entre clair et sombre, "
       "donc baissez-le quand la scène a besoin de cette marge."),
    ES("Mantener el color base propio de cada splat -- la parte que no cambia "
       "con el ángulo -- dentro del rango de 0 a 1. Más alto mantiene limpias "
       "las superficies; demasiado recorta el rango real entre claro y oscuro, "
       "así que bájalo cuando la escena necesite ese margen."),
    PT("Manter a cor base de cada splat -- a parte que não muda com o ângulo "
       "-- dentro do intervalo de 0 a 1. Mais alto mantém as superfícies limpas; "
       "alto demais corta o intervalo real entre claro e escuro, então baixe "
       "quando a cena precisar dessa margem."),
    IT("Tenere il colore base proprio di ogni splat -- la parte che non cambia "
       "con l'angolazione -- nell'intervallo da 0 a 1. Più alto mantiene pulite "
       "le superfici; troppo alto taglia l'intervallo reale tra chiaro e scuro, "
       "quindi abbassalo quando la scena ha bisogno di quel margine."),
    NL("De eigen basiskleur van elke splat -- het deel dat vanuit elke hoek "
       "gelijk blijft -- binnen het bereik 0 tot 1 houden. Hoger houdt vlakken "
       "schoon; te hoog snijdt het echte bereik tussen licht en donker af, dus "
       "verlaag het wanneer de scene die ruimte nodig heeft."),
    RU("Удерживать собственный базовый цвет сплата -- ту часть, что не меняется "
       "с углом, -- в пределах от 0 до 1. Больше -- поверхности чище; слишком "
       "много -- срезается настоящий размах между светлым и тёмным, поэтому "
       "снижайте, когда сцене нужен этот запас."),
    TR("Her splat'ın kendi temel rengini -- açıya göre değişmeyen kısmını -- "
       "0 ile 1 aralığında tutar. Yüksek değerler yüzeyleri temiz tutar; aşırısı "
       "aydınlık ile karanlık arasındaki gerçek aralığı kırpar, bu yüzden sahne "
       "o payı gerektirdiğinde düşürün."));

SS_MSG(sh_reg,
    EN("View-dependent color penalty"), JA("視点依存色のペナルティ"),
    ZH_HANS("视角相关颜色的惩罚"), ZH_HANT("視角相關顏色的懲罰"),
    KO("시점 의존 색 페널티"), DE("Strafe für blickabhängige Farbe"),
    FR("Pénalité de la couleur dépendante de la vue"),
    ES("Penalización del color dependiente de la vista"),
    PT("Penalidade da cor dependente da vista"),
    IT("Penalità del colore dipendente dalla vista"),
    NL("Straf voor kijkrichtingafhankelijke kleur"),
    RU("Штраф за цвет, зависящий от вида"), TR("Bakışa bağlı renk cezası"));
SS_MSG(sh_reg_help,
    EN("Hold view-dependent color in check so it does not absorb shifts the per-photo "
       "correction should handle. Higher gives more consistent color from every "
       "angle and better results on unseen views; too high flattens genuine reflections."),
    JA("視点によって変わる色を抑え、写真ごとの補正が担うべきずれを吸い込まない"
       "ようにします。高いほどどの角度からも色が揃い、学習に使っていない視点で"
       "も良い結果になりますが、上げすぎると本物の反射まで平坦になります。"),
    ZH_HANS("约束视角相关颜色，避免它去吸收本应由逐张照片校正处理的偏差。数值"
            "越高，各个角度的颜色越一致，未见视角的效果也越好；过高则会压平真"
            "实的反射。"),
    ZH_HANT("約束視角相關顏色，避免它去吸收本應由逐張照片校正處理的偏差。數值"
            "越高，各個角度的顏色越一致，未見視角的效果也越好；過高則會壓平真"
            "實的反射。"),
    KO("시점에 따라 달라지는 색을 억제해, 사진별 보정이 맡아야 할 편차를 대신"
       " 흡수하지 않게 합니다. 값이 크면 어느 각도에서도 색이 일정하고 학습에"
       " 없던 시점에서도 결과가 좋아지지만, 너무 크면 진짜 반사까지 평평해집니"
       "다."),
    DE("Blickabhängige Farbe im Zaum halten, damit sie nicht die Abweichungen "
       "aufsaugt, die die Korrektur pro Foto übernehmen sollte. Höher gibt aus "
       "jedem Winkel gleichmäßigere Farbe und bessere Ergebnisse auf ungesehenen "
       "Ansichten; zu hoch macht echte Reflexe flach."),
    FR("Brider la couleur dépendante de la vue pour qu'elle n'absorbe pas les "
       "écarts que la correction par photo doit traiter. Plus haut donne une "
       "couleur plus constante sous tous les angles et de meilleurs résultats "
       "sur les vues nouvelles ; trop haut aplatit les vrais reflets."),
    ES("Frenar el color dependiente de la vista para que no absorba las desviaciones "
       "que debe tratar la corrección por foto. Más alto da un color más constante "
       "desde cualquier ángulo y mejores resultados en vistas no vistas; demasiado "
       "aplana los reflejos genuinos."),
    PT("Segurar a cor dependente da vista para que ela não absorva os desvios "
       "que a correção por foto deve tratar. Mais alto dá uma cor mais constante "
       "de qualquer ângulo e melhores resultados em vistas não vistas; alto demais "
       "achata os reflexos genuínos."),
    IT("Tenere a freno il colore dipendente dalla vista perché non assorba gli "
       "scarti che deve gestire la correzione per foto. Più alto dà un colore "
       "più costante da ogni angolazione e risultati migliori sulle viste mai "
       "viste; troppo alto appiattisce i riflessi veri."),
    NL("Kijkrichtingafhankelijke kleur in toom houden zodat die niet de verschuivingen "
       "opslokt die de correctie per foto hoort af te handelen. Hoger geeft vanuit "
       "elke hoek constantere kleur en betere resultaten op ongeziene beelden; "
       "te hoog slaat echte reflecties plat."),
    RU("Придерживать цвет, зависящий от вида, чтобы он не вбирал сдвиги, которыми "
       "должна заниматься покадровая коррекция. Больше — цвет ровнее со всех "
       "сторон и лучше результат на новых видах; слишком много — настоящие блики "
       "сплющиваются."),
    TR("Bakışa bağlı rengi dizginler; böylece fotoğraf başına düzeltmenin üstlenmesi "
       "gereken kaymaları içine çekmez. Yüksek değerler her açıdan daha tutarlı "
       "renk ve görülmemiş görünümlerde daha iyi sonuç verir; aşırısı gerçek "
       "yansımaları düzleştirir."));

SS_MSG(overexposure_reg,
    EN("Out-of-range color penalty"), JA("表示範囲外の色のペナルティ"),
    ZH_HANS("超出可显示范围的颜色惩罚"), ZH_HANT("超出可顯示範圍的顏色懲罰"),
    KO("표시 범위 밖 색 페널티"),
    DE("Strafe für Farben außerhalb des Bereichs"),
    FR("Pénalité des couleurs hors gamme"),
    ES("Penalización de los colores fuera de rango"),
    PT("Penalidade das cores fora do intervalo"),
    IT("Penalità dei colori fuori intervallo"),
    NL("Straf voor kleuren buiten bereik"),
    RU("Штраф за цвета вне диапазона"), TR("Aralık dışı renk cezası"));
SS_MSG(overexposure_reg_help,
    EN("Penalize splat colors that fall outside the displayable range. Keeps "
       "blown-out highlights from hiding inside the splats, which matters when "
       "exporting or meshing."),
    JA("表示できる範囲から外れたスプラットの色を罰します。白飛びしたハイライト"
       "がスプラットの中に隠れないので、書き出しやメッシュ化のときに効いてきま"
       "す。"),
    ZH_HANS("惩罚落在可显示范围之外的泼溅颜色。这样过曝的高光就不会藏在泼溅内"
            "部，导出或生成网格时尤其重要。"),
    ZH_HANT("懲罰落在可顯示範圍之外的潑濺顏色。這樣過曝的高光就不會藏在潑濺內"
            "部，匯出或產生網格時尤其重要。"),
    KO("표시할 수 있는 범위를 벗어난 스플랫 색에 벌을 줍니다. 날아간 하이라이"
       "트가 스플랫 안에 숨지 않으므로 내보내기나 메시 변환에서 중요합니다."),
    DE("Splatfarben bestrafen, die außerhalb des darstellbaren Bereichs liegen. "
       "So verstecken sich ausgefressene Lichter nicht in den Splats, was beim "
       "Export oder bei der Netzerzeugung zählt."),
    FR("Pénaliser les couleurs de splat qui sortent de la plage affichable. Évite "
       "que des hautes lumières cramées se cachent dans les splats, ce qui compte "
       "à l'export ou au maillage."),
    ES("Penalizar los colores de splat que caen fuera del rango mostrable. Evita "
       "que las altas luces quemadas se escondan dentro de los splats, algo que "
       "importa al exportar o mallar."),
    PT("Penalizar as cores de splat que ficam fora do intervalo exibível. Evita "
       "que altas luzes estouradas se escondam dentro dos splats, o que importa "
       "ao exportar ou gerar malha."),
    IT("Penalizzare i colori degli splat che escono dall'intervallo visualizzabile. "
       "Evita che le alte luci bruciate si nascondano dentro gli splat, cosa "
       "che conta all'esportazione o nella mesh."),
    NL("Splatkleuren bestraffen die buiten het weergeefbare bereik vallen. Voorkomt "
       "dat overbelichte highlights zich in de splats verstoppen, wat telt bij "
       "export of mesh-generatie."),
    RU("Штрафовать цвета сплатов, выходящие за отображаемый диапазон. Не даёт "
       "выбитым светам прятаться внутри сплатов, что важно при экспорте и построении "
       "меша."),
    TR("Görüntülenebilir aralığın dışına düşen splat renklerini cezalandırır. "
       "Yanmış parlak alanların splat'ların içinde saklanmasını önler; dışa aktarırken "
       "ya da ağ çıkarırken önemlidir."));

SS_MSG(quat_norm_reg,
    EN("Rotation normalization"), JA("回転の正規化"), ZH_HANS("旋转归一化"),
    ZH_HANT("旋轉正規化"), KO("회전 정규화"),
    DE("Normierung der Rotationen"), FR("Normalisation des rotations"),
    ES("Normalización de las rotaciones"), PT("Normalização das rotações"),
    IT("Normalizzazione delle rotazioni"),
    NL("Normalisatie van de rotaties"), RU("Нормировка вращений"),
    TR("Dönüşlerin normalleştirilmesi"));
SS_MSG(quat_norm_reg_help,
    EN("Keep splat rotations well formed. Guards against numerical drift and "
       "rarely needs changing."),
    JA("スプラットの回転を正しい形に保ちます。数値のずれを防ぐためのもので、変"
       "更が必要になることはめったにありません。"),
    ZH_HANS("让泼溅的旋转保持规范。它用于防止数值漂移，很少需要修改。"),
    ZH_HANT("讓潑濺的旋轉保持規範。它用於防止數值漂移，很少需要修改。"),
    KO("스플랫의 회전을 올바른 형태로 유지합니다. 수치 오차가 쌓이는 것을 막기"
       " 위한 값이라 바꿀 일이 거의 없습니다."),
    DE("Die Rotationen der Splats wohlgeformt halten. Schützt vor numerischer "
       "Drift und muss selten geändert werden."),
    FR("Garder les rotations des splats bien formées. Prémunit contre la dérive "
       "numérique et n'a que rarement besoin d'être modifié."),
    ES("Mantener bien formadas las rotaciones de los splats. Protege de la deriva "
       "numérica y rara vez hace falta cambiarlo."),
    PT("Manter bem formadas as rotações dos splats. Protege contra a deriva numérica "
       "e raramente precisa ser alterado."),
    IT("Tenere ben formate le rotazioni degli splat. Protegge dalla deriva numerica "
       "e raramente va cambiato."),
    NL("De rotaties van splats welgevormd houden. Beschermt tegen numerieke drift "
       "en hoeft zelden te worden aangepast."),
    RU("Держать вращения сплатов корректными. Защищает от численного дрейфа и "
       "почти никогда не требует правки."),
    TR("Splat dönüşlerini düzgün biçimde tutar. Sayısal kaymaya karşı korur ve "
       "nadiren değiştirilmesi gerekir."));


// ===========================================================================
// Camera & Color Correction
// ===========================================================================

SS_MSG(use_bilateral_grid,
    EN("Bilateral Grid color correction"),
    JA("バイラテラルグリッドによる色補正"), ZH_HANS("双边网格颜色校正"),
    ZH_HANT("雙邊網格色彩校正"), KO("양방향 그리드 색 보정"),
    DE("Farbkorrektur per Bilateral Grid"),
    FR("Correction des couleurs par grille bilatérale"),
    ES("Corrección de color con rejilla bilateral"),
    PT("Correção de cor com grade bilateral"),
    IT("Correzione del colore con griglia bilaterale"),
    NL("Kleurcorrectie met bilateraal raster"),
    RU("Цветокоррекция билатеральной сеткой"),
    TR("Çift yönlü ızgarayla renk düzeltme"));
SS_MSG(use_bilateral_grid_help,
    EN("Give each photo its own smooth color correction, absorbing exposure and "
       "white balance drift between shots. The splats then keep one consistent "
       "color instead of averaging every camera's quirks. Turn off for synthetic "
       "or already-consistent datasets."),
    JA("写真ごとになめらかな色補正を持たせ、露出やホワイトバランスのずれを吸収"
       "します。スプラット側は一貫した色を保てるようになり、カメラごとの癖を平"
       "均してしまうことがなくなります。合成データや、もともと色が揃っているデ"
       "ータセットではオフにしてください。"),
    ZH_HANS("让每张照片拥有自己的平滑颜色校正，吸收各张之间的曝光和白平衡漂移。"
            "这样泼溅就能保持一致的颜色，而不是把各台相机的偏差平均掉。合成数"
            "据或本来就色彩一致的数据集可以关闭。"),
    ZH_HANT("讓每張照片擁有自己的平滑色彩校正，吸收各張之間的曝光和白平衡漂移。"
            "這樣潑濺就能保持一致的顏色，而不是把各台相機的偏差平均掉。合成資"
            "料或本來就色彩一致的資料集可以關閉。"),
    KO("사진마다 매끄러운 색 보정을 따로 두어 노출과 화이트밸런스의 어긋남을 "
       "흡수합니다. 그러면 스플랫은 카메라별 버릇을 평균 내지 않고 일관된 색을"
       " 유지합니다. 합성 데이터나 이미 색이 고른 데이터셋에서는 끄십시오."),
    DE("Jedem Foto eine eigene, sanfte Farbkorrektur geben, die Belichtungs- "
       "und Weißabgleichsdrift zwischen den Aufnahmen aufnimmt. Die Splats behalten "
       "dann eine einheitliche Farbe, statt die Eigenheiten jeder Kamera zu mitteln. "
       "Bei synthetischen oder bereits einheitlichen Datensätzen abschalten."),
    FR("Donner à chaque photo sa propre correction de couleur douce, qui absorbe "
       "les dérives d'exposition et de balance des blancs d'une prise à l'autre. "
       "Les splats gardent alors une couleur cohérente au lieu de moyenner les "
       "travers de chaque appareil. À désactiver pour les jeux de données de "
       "synthèse ou déjà homogènes."),
    ES("Dar a cada foto su propia corrección de color suave, que absorbe las "
       "derivas de exposición y balance de blancos entre tomas. Los splats conservan "
       "entonces un color coherente en vez de promediar las manías de cada cámara. "
       "Desactívelo para conjuntos sintéticos o ya homogéneos."),
    PT("Dar a cada foto a sua própria correção de cor suave, que absorve os desvios "
       "de exposição e balanço de branco entre as tomadas. Os splats mantêm então "
       "uma cor coerente em vez de fazer a média das manias de cada câmera. Desligue "
       "para conjuntos sintéticos ou já homogêneos."),
    IT("Dare a ogni foto una propria correzione di colore morbida, che assorbe "
       "le derive di esposizione e bilanciamento del bianco tra gli scatti. Gli "
       "splat mantengono così un colore coerente invece di mediare le stranezze "
       "di ogni fotocamera. Disattivare per set di dati sintetici o già omogenei."),
    NL("Elke foto een eigen, zachte kleurcorrectie geven die de drift in belichting "
       "en witbalans tussen opnamen opvangt. De splats houden dan één samenhangende "
       "kleur in plaats van de eigenaardigheden van elke camera uit te middelen. "
       "Zet uit bij synthetische of al gelijkmatige datasets."),
    RU("Дать каждому снимку собственную плавную цветокоррекцию, которая вбирает "
       "разброс экспозиции и баланса белого между кадрами. Тогда сплаты сохраняют "
       "единый цвет, а не усредняют причуды каждой камеры. Отключите для синтетических "
       "или уже согласованных наборов."),
    TR("Her fotoğrafa kendi yumuşak renk düzeltmesini verir; böylece çekimler "
       "arasındaki pozlama ve beyaz dengesi kaymalarını soğurur. Splat'lar da "
       "her kameranın huyunu ortalamak yerine tutarlı tek bir renk korur. Sentetik "
       "ya da hâlihazırda tutarlı veri kümelerinde kapatın."));

SS_MSG(bilagrid_type,
    EN("Color correction model"), JA("色補正のモデル"),
    ZH_HANS("颜色校正模型"), ZH_HANT("色彩校正模型"), KO("색 보정 모델"),
    DE("Modell der Farbkorrektur"), FR("Modèle de correction des couleurs"),
    ES("Modelo de corrección de color"), PT("Modelo de correção de cor"),
    IT("Modello di correzione del colore"),
    NL("Model van de kleurcorrectie"), RU("Модель цветокоррекции"),
    TR("Renk düzeltme modeli"));
SS_MSG(bilagrid_type_help,
    EN("What the per-photo color correction is allowed to do. `ppisp` adjusts "
       "exposure and color gain and shifts hue the least. `affine` is a full "
       "color matrix, the most flexible but the most prone to color drift. `loglinear` "
       "sits in between."),
    JA("写真ごとの色補正に何を許すかです。`ppisp` は露出と色ゲインを調整し、色"
       "相のずれがいちばん小さい方式です。`affine` は完全な色行列で、いちばん"
       "柔軟ですが色が流れやすい方式です。`loglinear` はその中間です。"),
    ZH_HANS("允许逐张照片的颜色校正做什么。`ppisp` 调整曝光和颜色增益，对色相"
            "的改动最小；`affine` 是完整的颜色矩阵，最灵活但最容易造成色偏；`loglinear` "
            "介于两者之间。"),
    ZH_HANT("允許逐張照片的色彩校正做什麼。`ppisp` 調整曝光和顏色增益，對色相"
            "的改動最小；`affine` 是完整的顏色矩陣，最靈活但最容易造成色偏；`loglinear` "
            "介於兩者之間。"),
    KO("사진별 색 보정이 무엇까지 할 수 있는지입니다. `ppisp`는 노출과 색 게인"
       "을 조정하며 색상 변화가 가장 적고, `affine`은 완전한 색 행렬로 가장 유"
       "연하지만 색이 흐르기 쉬우며, `loglinear`는 그 중간입니다."),
    DE("Was die Farbkorrektur pro Foto tun darf. `ppisp` passt Belichtung und "
       "Farbverstärkung an und verschiebt den Farbton am wenigsten. `affine` "
       "ist eine volle Farbmatrix, am flexibelsten, aber am anfälligsten für "
       "Farbdrift. `loglinear` liegt dazwischen."),
    FR("Ce que la correction de couleur par photo a le droit de faire. `ppisp` "
       "ajuste l'exposition et le gain de couleur et déplace le moins la teinte. "
       "`affine` est une matrice de couleur complète, la plus souple mais la "
       "plus sujette à la dérive de couleur. `loglinear` se situe entre les deux."),
    ES("Qué se le permite hacer a la corrección de color por foto. `ppisp` ajusta "
       "la exposición y la ganancia de color y desplaza el tono lo mínimo. `affine` "
       "es una matriz de color completa, la más flexible pero la más propensa "
       "a la deriva de color. `loglinear` queda en medio."),
    PT("O que a correção de cor por foto tem permissão de fazer. `ppisp` ajusta "
       "a exposição e o ganho de cor e desloca o menos possível o matiz. `affine` "
       "é uma matriz de cor completa, a mais flexível mas a mais sujeita a desvio "
       "de cor. `loglinear` fica no meio."),
    IT("Che cosa può fare la correzione di colore per foto. `ppisp` regola esposizione "
       "e guadagno di colore e sposta al minimo la tinta. `affine` è una matrice "
       "di colore completa, la più flessibile ma la più soggetta a deriva di "
       "colore. `loglinear` sta nel mezzo."),
    NL("Wat de kleurcorrectie per foto mag doen. `ppisp` past belichting en kleurversterking "
       "aan en verschuift de tint het minst. `affine` is een volledige kleurmatrix, "
       "het flexibelst maar het gevoeligst voor kleurdrift. `loglinear` zit ertussenin."),
    RU("Что позволено покадровой цветокоррекции. `ppisp` правит экспозицию и "
       "усиление цвета и меньше всего смещает оттенок. `affine` — полная цветовая "
       "матрица: самая гибкая, но и самая склонная к уходу цвета. `loglinear` "
       "находится посередине."),
    TR("Fotoğraf başına renk düzeltmesinin neler yapabileceği. `ppisp` pozlamayı "
       "ve renk kazancını ayarlar ve tonu en az kaydırır. `affine` tam bir renk "
       "matrisidir; en esnek olanıdır ama renk kaymasına en yatkınıdır. `loglinear` "
       "ikisinin arasındadır."));

SS_MSG(bilagrid_shape,
    EN("Color correction resolution"), JA("色補正の細かさ"),
    ZH_HANS("颜色校正的分辨率"), ZH_HANT("色彩校正的解析度"),
    KO("색 보정 해상도"), DE("Auflösung der Farbkorrektur"),
    FR("Résolution de la correction des couleurs"),
    ES("Resolución de la corrección de color"),
    PT("Resolução da correção de cor"),
    IT("Risoluzione della correzione del colore"),
    NL("Resolutie van de kleurcorrectie"), RU("Разрешение цветокоррекции"),
    TR("Renk düzeltme çözünürlüğü"));
SS_MSG(bilagrid_shape_help,
    EN("How finely the per-photo color correction may vary, as width, height "
       "and brightness steps. Finer grids fix more localized shifts and use more "
       "VRAM; coarser is safer on flat, low-texture surfaces where a fine grid "
       "starts eating real detail."),
    JA("写真ごとの色補正がどこまで細かく変化してよいかを、幅・高さ・明るさの分"
       "割数で指定します。細かいほど局所的なずれを直せますが VRAM を多く使いま"
       "す。粗いほうが、模様の少ない平らな面で細かいグリッドが本物の細部を食べ"
       "てしまうのを避けられます。"),
    ZH_HANS("逐张照片的颜色校正可以有多细的变化，按宽、高和亮度的分档数给出。"
            "网格越细，能修正的局部偏差越多，占用的显存也越大；越粗则更安全，"
            "因为在缺少纹理的平坦表面上，过细的网格会吞掉真实细节。"),
    ZH_HANT("逐張照片的色彩校正可以有多細的變化，按寬、高和亮度的分檔數給出。"
            "網格越細，能修正的局部偏差越多，佔用的顯示記憶體也越大；越粗則更"
            "安全，因為在缺少紋理的平坦表面上，過細的網格會吞掉真實細節。"),
    KO("사진별 색 보정이 얼마나 세밀하게 변할 수 있는지를 가로·세로·밝기 단계"
       " 수로 지정합니다. 격자가 촘촘할수록 국소적인 편차를 더 잘 잡지만 VRAM을"
       " 더 씁니다. 성긴 편이 안전한데, 무늬가 적은 평평한 면에서는 촘촘한 격"
       "자가 실제 디테일을 먹기 시작하기 때문입니다."),
    DE("Wie fein die Farbkorrektur pro Foto variieren darf, als Breite, Höhe "
       "und Helligkeitsstufen. Feinere Gitter beheben stärker örtliche Abweichungen "
       "und brauchen mehr VRAM; gröber ist sicherer auf glatten, texturarmen "
       "Flächen, wo ein feines Gitter beginnt, echtes Detail zu verschlucken."),
    FR("À quel point la correction de couleur par photo peut varier, en pas de "
       "largeur, de hauteur et de luminosité. Une grille plus fine corrige des "
       "écarts plus locaux et consomme plus de VRAM ; une grille plus grossière "
       "est plus sûre sur les surfaces planes et peu texturées, où une grille "
       "fine se met à manger du vrai détail."),
    ES("Con cuánto detalle puede variar la corrección de color por foto, en pasos "
       "de ancho, alto y brillo. Las rejillas más finas corrigen desviaciones "
       "más locales y consumen más VRAM; las más gruesas son más seguras en superficies "
       "planas y sin textura, donde una rejilla fina empieza a comerse el detalle "
       "real."),
    PT("Com quanto detalhe a correção de cor por foto pode variar, em passos "
       "de largura, altura e brilho. Grades mais finas corrigem desvios mais "
       "locais e usam mais VRAM; grades mais grossas são mais seguras em superfícies "
       "planas e sem textura, onde uma grade fina começa a comer o detalhe real."),
    IT("Con quanta finezza può variare la correzione di colore per foto, in passi "
       "di larghezza, altezza e luminosità. Griglie più fini correggono scarti "
       "più locali e usano più VRAM; quelle più grosse sono più sicure su superfici "
       "piatte e poco testurizzate, dove una griglia fine comincia a mangiarsi "
       "il dettaglio vero."),
    NL("Hoe fijn de kleurcorrectie per foto mag variëren, in stappen van breedte, "
       "hoogte en helderheid. Fijnere rasters verhelpen plaatselijker verschuivingen "
       "en gebruiken meer VRAM; grover is veiliger op vlakke, textuurarme oppervlakken, "
       "waar een fijn raster echt detail begint op te eten."),
    RU("Насколько тонко может меняться покадровая цветокоррекция — в шагах по "
       "ширине, высоте и яркости. Более мелкая сетка исправляет более локальные "
       "сдвиги и требует больше видеопамяти; более крупная безопаснее на ровных "
       "малотекстурных поверхностях, где мелкая сетка начинает съедать настоящие "
       "детали."),
    TR("Fotoğraf başına renk düzeltmesinin ne kadar ince değişebileceği; genişlik, "
       "yükseklik ve parlaklık adımı olarak verilir. İnce ızgaralar daha yerel "
       "kaymaları düzeltir ve daha çok VRAM kullanır; kaba ızgaralar ise dokusu "
       "az, düz yüzeylerde daha güvenlidir, çünkü orada ince ızgara gerçek ayrıntıyı "
       "yemeye başlar."));

SS_MSG(bilagrid_tv_loss_weight,
    EN("Color correction smoothness"), JA("色補正のなめらかさ"),
    ZH_HANS("颜色校正的平滑度"), ZH_HANT("色彩校正的平滑度"),
    KO("색 보정 매끄러움"), DE("Glätte der Farbkorrektur"),
    FR("Douceur de la correction des couleurs"),
    ES("Suavidad de la corrección de color"),
    PT("Suavidade da correção de cor"),
    IT("Morbidezza della correzione del colore"),
    NL("Gladheid van de kleurcorrectie"), RU("Плавность цветокоррекции"),
    TR("Renk düzeltmenin yumuşaklığı"));
SS_MSG(bilagrid_tv_loss_weight_help,
    EN("How smooth the per-photo color correction has to be. Higher keeps corrections "
       "gentle and global; lower lets them vary from place to place, which can "
       "start absorbing real image detail."),
    JA("写真ごとの色補正がどれだけなめらかでなければならないかです。高いほど補"
       "正はおだやかで全体的になり、低いほど場所ごとに変えられますが、本物の画"
       "像の細部を吸い込み始めることがあります。"),
    ZH_HANS("逐张照片的颜色校正必须有多平滑。数值越高，校正越温和、越全局；越"
            "低则可以逐处变化，但可能开始吸收真实的图像细节。"),
    ZH_HANT("逐張照片的色彩校正必須有多平滑。數值越高，校正越溫和、越全域；越"
            "低則可以逐處變化，但可能開始吸收真實的影像細節。"),
    KO("사진별 색 보정이 얼마나 매끄러워야 하는지입니다. 값이 크면 보정이 부드"
       "럽고 전체적이며, 작으면 자리마다 달라질 수 있지만 실제 이미지 디테일을"
       " 흡수하기 시작할 수 있습니다."),
    DE("Wie glatt die Farbkorrektur pro Foto sein muss. Höher hält Korrekturen "
       "sanft und global; niedriger lässt sie von Ort zu Ort variieren, was beginnen "
       "kann, echtes Bilddetail aufzusaugen."),
    FR("À quel point la correction de couleur par photo doit être lisse. Plus "
       "haut la garde douce et globale ; plus bas la laisse varier d'un endroit "
       "à l'autre, au risque d'absorber du vrai détail d'image."),
    ES("Cuán suave debe ser la corrección de color por foto. Más alto la mantiene "
       "leve y global; más bajo la deja variar de un sitio a otro, con el riesgo "
       "de absorber detalle real de la imagen."),
    PT("Quão suave a correção de cor por foto tem de ser. Mais alto a mantém "
       "leve e global; mais baixo deixa-a variar de lugar para lugar, com o risco "
       "de absorver detalhe real da imagem."),
    IT("Quanto deve essere morbida la correzione di colore per foto. Più alto "
       "la tiene leggera e globale; più basso la lascia variare da punto a punto, "
       "col rischio di assorbire vero dettaglio dell'immagine."),
    NL("Hoe glad de kleurcorrectie per foto moet zijn. Hoger houdt correcties "
       "zacht en globaal; lager laat ze van plek tot plek variëren, met het risico "
       "dat ze echt beelddetail opslokken."),
    RU("Насколько плавной должна быть покадровая цветокоррекция. Больше — коррекция "
       "мягкая и глобальная; меньше — она может меняться от места к месту и начать "
       "вбирать настоящие детали изображения."),
    TR("Fotoğraf başına renk düzeltmesinin ne kadar yumuşak olması gerektiği. "
       "Yüksek değerler düzeltmeleri hafif ve genel tutar; düşük değerler yerden "
       "yere değişmesine izin verir ve gerçek görüntü ayrıntısını içine çekmeye "
       "başlayabilir."));

SS_MSG(color_shift_reg_weight,
    EN("Overall tint penalty"), JA("全体的な色かぶりのペナルティ"),
    ZH_HANS("整体色偏惩罚"), ZH_HANT("整體色偏懲罰"),
    KO("전체 색조 치우침 페널티"), DE("Strafe für einen Gesamtfarbstich"),
    FR("Pénalité de teinte globale"), ES("Penalización de tinte global"),
    PT("Penalidade de tonalidade geral"),
    IT("Penalità di dominante di colore"),
    NL("Straf voor een algehele kleurzweem"), RU("Штраф за общий оттенок"),
    TR("Genel renk kayması cezası"));
SS_MSG(color_shift_reg_weight_help,
    EN("Keep the per-photo corrections from tinting the result overall. Raise "
       "it if the finished splats come out consistently warmer, cooler, darker "
       "or brighter than the photos. 0 turns it off, and 0.01 to 1 is the useful "
       "range."),
    JA("写真ごとの補正が結果全体に色かぶりを与えないようにします。仕上がったス"
       "プラットが写真より一様に暖かい・冷たい・暗い・明るいときは上げてくださ"
       "い。0 で無効になり、実用範囲は 0.01 から 1 です。"),
    ZH_HANS("防止逐张照片的校正给整体结果带来色偏。如果最终泼溅始终比照片更暖、"
            "更冷、更暗或更亮，就把它调高。0 表示关闭，实用范围是 0.01 到 1。"),
    ZH_HANT("防止逐張照片的校正給整體結果帶來色偏。如果最終潑濺始終比照片更暖、"
            "更冷、更暗或更亮，就把它調高。0 表示關閉，實用範圍是 0.01 到 1。"),
    KO("사진별 보정이 결과 전체에 색조를 입히지 않도록 합니다. 완성된 스플랫이"
       " 사진보다 한결같이 따뜻하거나 차갑거나 어둡거나 밝으면 올리십시오. 0이"
       "면 꺼지고, 쓸 만한 범위는 0.01에서 1입니다."),
    DE("Verhindern, dass die Korrekturen pro Foto dem Ergebnis insgesamt einen "
       "Farbstich geben. Erhöhen, wenn die fertigen Splats durchweg wärmer, kühler, "
       "dunkler oder heller als die Fotos ausfallen. 0 schaltet es ab, brauchbar "
       "sind 0,01 bis 1."),
    FR("Empêcher les corrections par photo de teinter le résultat dans son ensemble. "
       "À augmenter si les splats finis ressortent uniformément plus chauds, "
       "plus froids, plus sombres ou plus clairs que les photos. 0 le désactive, "
       "et la plage utile va de 0,01 à 1."),
    ES("Impedir que las correcciones por foto tiñan el resultado en conjunto. "
       "Súbalo si los splats terminados salen de forma constante más cálidos, "
       "más fríos, más oscuros o más claros que las fotos. 0 lo desactiva y el "
       "rango útil va de 0,01 a 1."),
    PT("Impedir que as correções por foto tingam o resultado como um todo. Aumente "
       "se os splats terminados saírem sistematicamente mais quentes, mais frios, "
       "mais escuros ou mais claros que as fotos. 0 desativa e a faixa útil vai "
       "de 0,01 a 1."),
    IT("Impedire che le correzioni per foto diano una dominante all'intero risultato. "
       "Da alzare se gli splat finiti escono costantemente più caldi, più freddi, "
       "più scuri o più chiari delle foto. 0 lo disattiva e l'intervallo utile "
       "va da 0,01 a 1."),
    NL("Voorkomen dat de correcties per foto het resultaat als geheel een kleurzweem "
       "geven. Verhoog dit als de afgeronde splats stelselmatig warmer, koeler, "
       "donkerder of lichter uitvallen dan de foto's. 0 schakelt het uit en het "
       "bruikbare bereik is 0,01 tot 1."),
    RU("Не давать покадровым коррекциям окрашивать результат в целом. Повышайте, "
       "если готовые сплаты выходят стабильно теплее, холоднее, темнее или светлее "
       "фотографий. 0 отключает, рабочий диапазон — от 0,01 до 1."),
    TR("Fotoğraf başına düzeltmelerin sonuca genel bir renk kayması vermesini "
       "engeller. Bitmiş splat'lar fotoğraflara göre sürekli daha sıcak, daha "
       "soğuk, daha koyu ya da daha açık çıkıyorsa yükseltin. 0 kapatır; kullanışlı "
       "aralık 0,01 ile 1 arasıdır."));

SS_MSG(color_shift_reg_ema_period,
    EN("Tint check window"), JA("色かぶり判定の平均期間"),
    ZH_HANS("色偏检查的平均窗口"), ZH_HANT("色偏檢查的平均視窗"),
    KO("색조 검사 평균 구간"), DE("Fenster der Farbstichprüfung"),
    FR("Fenêtre de contrôle de la teinte"),
    ES("Ventana de comprobación del tinte"),
    PT("Janela de verificação da tonalidade"),
    IT("Finestra di controllo della dominante"),
    NL("Venster van de kleurzweemcontrole"), RU("Окно проверки оттенка"),
    TR("Renk kayması denetim penceresi"));
SS_MSG(color_shift_reg_ema_period_help,
    EN("How many steps the color-shift check averages over. It should be roughly "
       "one pass over the dataset so the average reflects every photo. Ignored "
       "when color_shift_reg_weight is 0."),
    JA("色かぶりの判定を何ステップ分平均するかです。すべての写真が平均に反映さ"
       "れるよう、データセットをおおよそ一巡する長さにしてください。color_shift_reg_weight "
       "が 0 のときは無視されます。"),
    ZH_HANS("色偏检查在多少步上取平均。它大致应当等于遍历一遍数据集的长度，这"
            "样每张照片都能进入平均。当 color_shift_reg_weight 为 0 时该项被忽"
            "略。"),
    ZH_HANT("色偏檢查在多少步上取平均。它大致應當等於走完一遍資料集的長度，這"
            "樣每張照片都能進入平均。當 color_shift_reg_weight 為 0 時該項被忽"
            "略。"),
    KO("색조 검사를 몇 스텝에 걸쳐 평균낼지입니다. 모든 사진이 평균에 들어가도"
       "록 데이터셋을 한 바퀴 도는 길이쯤으로 두십시오. color_shift_reg_weight가"
       " 0이면 무시됩니다."),
    DE("Über wie viele Schritte die Farbstichprüfung mittelt. Es sollte etwa "
       "einem Durchlauf durch den Datensatz entsprechen, damit der Mittelwert "
       "jedes Foto erfasst. Wird ignoriert, wenn color_shift_reg_weight 0 ist."),
    FR("Sur combien d'étapes le contrôle de teinte fait sa moyenne. Cela devrait "
       "valoir à peu près un passage sur le jeu de données, pour que la moyenne "
       "reflète chaque photo. Ignoré quand color_shift_reg_weight vaut 0."),
    ES("Sobre cuántos pasos promedia la comprobación del tinte. Debería equivaler "
       "más o menos a una pasada por el conjunto de datos, para que la media "
       "refleje cada foto. Se ignora cuando color_shift_reg_weight es 0."),
    PT("Sobre quantos passos a verificação de tonalidade faz a média. Deveria "
       "equivaler a mais ou menos uma passagem pelo conjunto de dados, para que "
       "a média reflita cada foto. É ignorado quando color_shift_reg_weight é "
       "0."),
    IT("Su quanti passi la verifica della dominante fa la media. Dovrebbe valere "
       "all'incirca un passaggio sul set di dati, così la media rispecchia ogni "
       "foto. Viene ignorato quando color_shift_reg_weight è 0."),
    NL("Over hoeveel stappen de kleurzweemcontrole middelt. Dat zou ongeveer "
       "één doorloop van de dataset moeten zijn, zodat het gemiddelde elke foto "
       "weerspiegelt. Wordt genegeerd als color_shift_reg_weight 0 is."),
    RU("По скольким шагам усредняется проверка оттенка. Это должно быть примерно "
       "один проход по набору данных, чтобы среднее охватило каждый снимок. Игнорируется, "
       "когда color_shift_reg_weight равен 0."),
    TR("Renk kayması denetiminin kaç adım üzerinden ortalama aldığı. Ortalamanın "
       "her fotoğrafı yansıtması için kabaca veri kümesinde bir geçiş kadar olmalıdır. "
       "color_shift_reg_weight 0 iken yok sayılır."));

SS_MSG(use_bilateral_grid_for_geometry,
    EN("Correct depth and normals too"), JA("深度と法線も補正する"),
    ZH_HANS("同时校正深度和法线"), ZH_HANT("同時校正深度和法線"),
    KO("깊이와 노멀도 보정"), DE("Auch Tiefe und Normalen korrigieren"),
    FR("Corriger aussi profondeur et normales"),
    ES("Corregir también profundidad y normales"),
    PT("Corrigir também profundidade e normais"),
    IT("Correggere anche profondità e normali"),
    NL("Ook diepte en normalen corrigeren"),
    RU("Корректировать также глубину и нормали"),
    TR("Derinlik ve normalleri de düzelt"));
SS_MSG(use_bilateral_grid_for_geometry_help,
    EN("Apply the same per-photo correction to depth and normal maps. Biased "
       "AI-generated maps can then still be used without dragging the geometry "
       "off."),
    JA("同じ写真ごとの補正を、深度マップと法線マップにも当てはめます。偏りのあ"
       "る AI 生成のマップでも、ジオメトリを狂わせずに使えるようになります。"),
    ZH_HANS("把同样的逐张照片校正也应用到深度图和法线图上。这样即使 AI 生成的"
            "图有偏差，也仍能使用而不至于把几何带偏。"),
    ZH_HANT("把同樣的逐張照片校正也套用到深度圖和法線圖上。這樣即使 AI 產生的"
            "圖有偏差，也仍能使用而不至於把幾何帶偏。"),
    KO("같은 사진별 보정을 깊이 맵과 노멀 맵에도 적용합니다. 치우친 AI 생성 맵"
       "이라도 지오메트리를 어긋나게 하지 않고 쓸 수 있습니다."),
    DE("Dieselbe Korrektur pro Foto auch auf Tiefen- und Normalenkarten anwenden. "
       "Verzerrte KI-erzeugte Karten lassen sich dann nutzen, ohne die Geometrie "
       "zu verziehen."),
    FR("Appliquer la même correction par photo aux cartes de profondeur et de "
       "normales. Des cartes générées par IA et biaisées restent alors utilisables "
       "sans tirer la géométrie de travers."),
    ES("Aplicar la misma corrección por foto a los mapas de profundidad y normales. "
       "Así, mapas generados por IA con sesgo siguen siendo utilizables sin torcer "
       "la geometría."),
    PT("Aplicar a mesma correção por foto aos mapas de profundidade e normais. "
       "Assim, mapas gerados por IA com viés continuam utilizáveis sem entortar "
       "a geometria."),
    IT("Applicare la stessa correzione per foto anche alle mappe di profondità "
       "e normali. Mappe generate dall'IA e distorte restano così utilizzabili "
       "senza storcere la geometria."),
    NL("Dezelfde correctie per foto ook op diepte- en normalenkaarten toepassen. "
       "Vertekende, door AI gemaakte kaarten blijven dan bruikbaar zonder de "
       "geometrie scheef te trekken."),
    RU("Применять ту же покадровую коррекцию к картам глубины и нормалей. Тогда "
       "смещённые карты, созданные ИИ, можно использовать, не уводя геометрию."),
    TR("Aynı fotoğraf başına düzeltmeyi derinlik ve normal haritalarına da uygular. "
       "Böylece yanlı yapay zekâ haritaları geometriyi bozmadan kullanılabilir."));

SS_MSG(bilagrid_shape_geometry,
    EN("Depth and normal correction resolution"),
    JA("深度・法線補正の細かさ"), ZH_HANS("深度与法线校正的分辨率"),
    ZH_HANT("深度與法線校正的解析度"), KO("깊이·노멀 보정 해상도"),
    DE("Auflösung der Tiefen- und Normalenkorrektur"),
    FR("Résolution de la correction profondeur et normales"),
    ES("Resolución de la corrección de profundidad y normales"),
    PT("Resolução da correção de profundidade e normais"),
    IT("Risoluzione della correzione di profondità e normali"),
    NL("Resolutie van de diepte- en normalencorrectie"),
    RU("Разрешение коррекции глубины и нормалей"),
    TR("Derinlik ve normal düzeltme çözünürlüğü"));
SS_MSG(bilagrid_shape_geometry_help,
    EN("How finely the depth and normal correction may vary. Same meaning as "
       "bilagrid_shape, for geometry rather than color."),
    JA("深度と法線の補正がどこまで細かく変化してよいかです。意味は bilagrid_shape "
       "と同じで、対象が色ではなくジオメトリです。"),
    ZH_HANS("深度和法线校正可以有多细的变化。含义与 bilagrid_shape 相同，只是"
            "作用于几何而非颜色。"),
    ZH_HANT("深度和法線校正可以有多細的變化。含意與 bilagrid_shape 相同，只是"
            "作用於幾何而非顏色。"),
    KO("깊이와 노멀 보정이 얼마나 세밀하게 변할 수 있는지입니다. 의미는 bilagrid_shape와"
       " 같고 대상이 색이 아니라 지오메트리입니다."),
    DE("Wie fein die Tiefen- und Normalenkorrektur variieren darf. Gleiche Bedeutung "
       "wie bilagrid_shape, nur für Geometrie statt Farbe."),
    FR("À quel point la correction de profondeur et de normales peut varier. "
       "Même sens que bilagrid_shape, mais pour la géométrie plutôt que la couleur."),
    ES("Con cuánto detalle puede variar la corrección de profundidad y normales. "
       "Mismo significado que bilagrid_shape, pero para la geometría en vez del "
       "color."),
    PT("Com quanto detalhe a correção de profundidade e normais pode variar. "
       "Mesmo significado de bilagrid_shape, mas para a geometria em vez da cor."),
    IT("Con quanta finezza può variare la correzione di profondità e normali. "
       "Stesso significato di bilagrid_shape, ma per la geometria invece che "
       "per il colore."),
    NL("Hoe fijn de diepte- en normalencorrectie mag variëren. Dezelfde betekenis "
       "als bilagrid_shape, maar voor geometrie in plaats van kleur."),
    RU("Насколько тонко может меняться коррекция глубины и нормалей. Смысл тот "
       "же, что у bilagrid_shape, только для геометрии, а не цвета."),
    TR("Derinlik ve normal düzeltmesinin ne kadar ince değişebileceği. Anlamı "
       "bilagrid_shape ile aynıdır; renk yerine geometri içindir."));

SS_MSG(bilagrid_tv_loss_weight_geometry,
    EN("Depth and normal correction smoothness"),
    JA("深度・法線補正のなめらかさ"), ZH_HANS("深度与法线校正的平滑度"),
    ZH_HANT("深度與法線校正的平滑度"), KO("깊이·노멀 보정 매끄러움"),
    DE("Glätte der Tiefen- und Normalenkorrektur"),
    FR("Douceur de la correction profondeur et normales"),
    ES("Suavidad de la corrección de profundidad y normales"),
    PT("Suavidade da correção de profundidade e normais"),
    IT("Morbidezza della correzione di profondità e normali"),
    NL("Gladheid van de diepte- en normalencorrectie"),
    RU("Плавность коррекции глубины и нормалей"),
    TR("Derinlik ve normal düzeltmenin yumuşaklığı"));
SS_MSG(bilagrid_tv_loss_weight_geometry_help,
    EN("How smooth the depth and normal correction has to be. Higher keeps it "
       "gentle and global; lower lets it vary from place to place."),
    JA("深度と法線の補正がどれだけなめらかでなければならないかです。高いほどお"
       "だやかで全体的になり、低いほど場所ごとに変えられます。"),
    ZH_HANS("深度和法线校正必须有多平滑。数值越高越温和、越全局；越低则可以逐"
            "处变化。"),
    ZH_HANT("深度和法線校正必須有多平滑。數值越高越溫和、越全域；越低則可以逐"
            "處變化。"),
    KO("깊이와 노멀 보정이 얼마나 매끄러워야 하는지입니다. 값이 크면 부드럽고"
       " 전체적이며, 작으면 자리마다 달라질 수 있습니다."),
    DE("Wie glatt die Tiefen- und Normalenkorrektur sein muss. Höher hält sie "
       "sanft und global; niedriger lässt sie von Ort zu Ort variieren."),
    FR("À quel point la correction de profondeur et de normales doit être lisse. "
       "Plus haut la garde douce et globale ; plus bas la laisse varier d'un "
       "endroit à l'autre."),
    ES("Cuán suave debe ser la corrección de profundidad y normales. Más alto "
       "la mantiene leve y global; más bajo la deja variar de un sitio a otro."),
    PT("Quão suave a correção de profundidade e normais tem de ser. Mais alto "
       "a mantém leve e global; mais baixo deixa-a variar de lugar para lugar."),
    IT("Quanto deve essere morbida la correzione di profondità e normali. Più "
       "alto la tiene leggera e globale; più basso la lascia variare da punto "
       "a punto."),
    NL("Hoe glad de diepte- en normalencorrectie moet zijn. Hoger houdt haar "
       "zacht en globaal; lager laat haar van plek tot plek variëren."),
    RU("Насколько плавной должна быть коррекция глубины и нормалей. Больше — "
       "мягче и глобальнее; меньше — она может меняться от места к месту."),
    TR("Derinlik ve normal düzeltmesinin ne kadar yumuşak olması gerektiği. Yüksek "
       "değerler hafif ve genel tutar; düşük değerler yerden yere değişmesine "
       "izin verir."));

SS_MSG(use_adagrad_bilagrid_optim,
    EN("Steadier color correction updates"), JA("色補正の更新を安定させる"),
    ZH_HANS("更平稳地更新颜色校正"), ZH_HANT("更平穩地更新色彩校正"),
    KO("색 보정 갱신을 안정적으로"),
    DE("Ruhigere Aktualisierung der Farbkorrektur"),
    FR("Mise à jour plus stable de la correction"),
    ES("Actualización más estable de la corrección"),
    PT("Atualização mais estável da correção"),
    IT("Aggiornamento più stabile della correzione"),
    NL("Stabielere update van de kleurcorrectie"),
    RU("Более ровное обновление цветокоррекции"),
    TR("Renk düzeltmede daha durağan güncelleme"));
SS_MSG(use_adagrad_bilagrid_optim_help,
    EN("Use a steadier update rule for the per-photo color correction. Generally "
       "more stable and needs less tuning; turn off to use the scheduled learning "
       "rates instead."),
    JA("写真ごとの色補正に、より落ち着いた更新則を使います。おおむね安定してい"
       "て調整も少なくて済みます。オフにすると、代わりにスケジュールされた学習"
       "率が使われます。"),
    ZH_HANS("为逐张照片的颜色校正采用更平稳的更新规则。通常更稳定，也更少需要"
            "调参；关闭后改用按计划变化的学习率。"),
    ZH_HANT("為逐張照片的色彩校正採用更平穩的更新規則。通常更穩定，也更少需要"
            "調參；關閉後改用按排程變化的學習率。"),
    KO("사진별 색 보정에 더 안정적인 갱신 규칙을 씁니다. 대체로 더 안정적이고"
       " 손볼 일이 적습니다. 끄면 대신 스케줄된 학습률을 사용합니다."),
    DE("Für die Farbkorrektur pro Foto eine ruhigere Aktualisierungsregel verwenden. "
       "Meist stabiler und weniger einstellungsbedürftig; abgeschaltet gelten "
       "stattdessen die geplanten Lernraten."),
    FR("Utiliser une règle de mise à jour plus stable pour la correction de couleur "
       "par photo. Généralement plus sûre et demandant moins de réglage ; décoché, "
       "ce sont les taux d'apprentissage programmés qui s'appliquent."),
    ES("Usar una regla de actualización más estable para la corrección de color "
       "por foto. Suele ser más estable y necesitar menos ajuste; sin marcar "
       "se usan en su lugar las tasas de aprendizaje programadas."),
    PT("Usar uma regra de atualização mais estável para a correção de cor por "
       "foto. Costuma ser mais estável e exigir menos ajuste; desmarcado, usam-se "
       "as taxas de aprendizado programadas."),
    IT("Usare una regola di aggiornamento più stabile per la correzione di colore "
       "per foto. In genere più stabile e con meno bisogno di regolazioni; deselezionato "
       "si usano invece i tassi di apprendimento pianificati."),
    NL("Voor de kleurcorrectie per foto een stabielere updateregel gebruiken. "
       "Meestal stabieler en met minder afstemwerk; uitgevinkt gelden in plaats "
       "daarvan de geplande leersnelheden."),
    RU("Использовать более ровное правило обновления для покадровой цветокоррекции. "
       "Обычно устойчивее и требует меньше настройки; без флажка применяются "
       "запланированные скорости обучения."),
    TR("Fotoğraf başına renk düzeltmesi için daha durağan bir güncelleme kuralı "
       "kullanır. Genelde daha kararlıdır ve daha az ayar ister; kapatılırsa "
       "çizelgelenmiş öğrenme oranları kullanılır."));

SS_MSG(use_ppisp,
    EN("PPISP camera correction"), JA("PPISP によるカメラ補正"),
    ZH_HANS("PPISP 相机校正"), ZH_HANT("PPISP 相機校正"),
    KO("PPISP 카메라 보정"), DE("PPISP-Kamerakorrektur"),
    FR("Correction caméra PPISP"), ES("Corrección de cámara PPISP"),
    PT("Correção de câmera PPISP"), IT("Correzione camera PPISP"),
    NL("PPISP-cameracorrectie"), RU("Коррекция камеры PPISP"),
    TR("PPISP kamera düzeltmesi"));
SS_MSG(use_ppisp_help,
    EN("Model per-pixel camera effects such as vignetting, exposure and lens "
       "color response. Keeps darkened corners and per-photo exposure shifts "
       "out of the splats themselves."),
    JA("周辺減光、露出、レンズの色特性といった画素ごとのカメラ効果をモデル化し"
       "ます。四隅の暗がりや写真ごとの露出のずれが、スプラット自体に焼き付くの"
       "を防ぎます。"),
    ZH_HANS("对暗角、曝光、镜头色彩响应等逐像素的相机效应建模。可以避免暗角和"
            "逐张曝光偏差被烙进泼溅本身。"),
    ZH_HANT("對暗角、曝光、鏡頭色彩響應等逐像素的相機效應建模。可以避免暗角和"
            "逐張曝光偏差被烙進潑濺本身。"),
    KO("비네팅, 노출, 렌즈 색 특성 같은 픽셀별 카메라 효과를 모델링합니다. 어"
       "두워진 모서리와 사진별 노출 차이가 스플랫 자체에 새겨지지 않게 합니다"
       "."),
    DE("Pixelweise Kameraeffekte wie Vignettierung, Belichtung und Farbverhalten "
       "des Objektivs modellieren. Hält abgedunkelte Ecken und Belichtungsverschiebungen "
       "pro Foto aus den Splats selbst heraus."),
    FR("Modéliser les effets caméra pixel par pixel : vignetage, exposition, "
       "réponse chromatique de l'objectif. Évite que les coins assombris et les "
       "écarts d'exposition par photo se gravent dans les splats eux-mêmes."),
    ES("Modelar los efectos de cámara píxel a píxel: viñeteado, exposición y "
       "respuesta de color del objetivo. Evita que las esquinas oscurecidas y "
       "las variaciones de exposición por foto se graben en los propios splats."),
    PT("Modelar os efeitos de câmera pixel a pixel: vinhetagem, exposição e resposta "
       "de cor da lente. Evita que cantos escurecidos e variações de exposição "
       "por foto fiquem gravados nos próprios splats."),
    IT("Modellare gli effetti della camera pixel per pixel: vignettatura, esposizione "
       "e risposta cromatica dell'obiettivo. Evita che angoli scuriti e sbalzi "
       "di esposizione per foto finiscano negli splat stessi."),
    NL("Cameraeffecten per pixel modelleren, zoals vignettering, belichting en "
       "de kleurrespons van de lens. Houdt donkere hoeken en belichtingsverschuivingen "
       "per foto uit de splats zelf."),
    RU("Моделировать попиксельные эффекты камеры: виньетирование, экспозицию "
       "и цветовой отклик объектива. Не даёт затемнённым углам и покадровым сдвигам "
       "экспозиции впечататься в сами сплаты."),
    TR("Vinyet, pozlama ve merceğin renk tepkisi gibi piksel başına kamera etkilerini "
       "modeller. Kararmış köşelerin ve fotoğraf başına pozlama kaymalarının "
       "splat'lara işlemesini önler."));

SS_MSG(ppisp_param_type,
    EN("Camera effects modeled"), JA("モデル化するカメラ効果"),
    ZH_HANS("建模的相机效应"), ZH_HANT("建模的相機效應"),
    KO("모델링할 카메라 효과"), DE("Modellierte Kameraeffekte"),
    FR("Effets caméra modélisés"), ES("Efectos de cámara modelados"),
    PT("Efeitos de câmera modelados"), IT("Effetti della camera modellati"),
    NL("Gemodelleerde cameraeffecten"), RU("Моделируемые эффекты камеры"),
    TR("Modellenen kamera etkileri"));
SS_MSG(ppisp_param_type_help,
    EN("Which camera effects get modeled. `no_crf` covers exposure, vignetting "
       "and color, then simply clips the result. `original` adds a tone curve "
       "on top. `rqs` uses a tone curve that behaves better in dark areas."),
    JA("どのカメラ効果をモデル化するかです。`no_crf` は露出・周辺減光・色を扱"
       "い、結果はそのまま切り詰めます。`original` はさらにトーンカーブを重ね"
       "ます。`rqs` は暗部での振る舞いがよいトーンカーブを使います。"),
    ZH_HANS("要建模哪些相机效应。`no_crf` 处理曝光、暗角和颜色，然后直接截断结"
            "果；`original` 在此基础上再加一条色调曲线；`rqs` 使用在暗部表现更"
            "好的色调曲线。"),
    ZH_HANT("要建模哪些相機效應。`no_crf` 處理曝光、暗角和顏色，然後直接截斷結"
            "果；`original` 在此基礎上再加一條色調曲線；`rqs` 使用在暗部表現更"
            "好的色調曲線。"),
    KO("어떤 카메라 효과를 모델링할지입니다. `no_crf`는 노출·비네팅·색을 다루"
       "고 결과를 그대로 잘라냅니다. `original`은 그 위에 톤 커브를 더합니다. "
       "`rqs`는 어두운 영역에서 더 잘 동작하는 톤 커브를 씁니다."),
    DE("Welche Kameraeffekte modelliert werden. `no_crf` deckt Belichtung, Vignettierung "
       "und Farbe ab und beschneidet das Ergebnis dann einfach. `original` legt "
       "eine Tonwertkurve darauf. `rqs` nutzt eine Tonwertkurve, die sich in "
       "dunklen Bereichen besser verhält."),
    FR("Quels effets caméra sont modélisés. `no_crf` couvre l'exposition, le "
       "vignetage et la couleur, puis écrête simplement le résultat. `original` "
       "y ajoute une courbe de tons. `rqs` emploie une courbe de tons qui se "
       "comporte mieux dans les zones sombres."),
    ES("Qué efectos de cámara se modelan. `no_crf` cubre exposición, viñeteado "
       "y color, y luego recorta el resultado sin más. `original` añade encima "
       "una curva de tonos. `rqs` usa una curva de tonos que se comporta mejor "
       "en las zonas oscuras."),
    PT("Que efeitos de câmera são modelados. `no_crf` cobre exposição, vinhetagem "
       "e cor e depois simplesmente corta o resultado. `original` acrescenta "
       "por cima uma curva tonal. `rqs` usa uma curva tonal que se comporta melhor "
       "nas áreas escuras."),
    IT("Quali effetti della camera vengono modellati. `no_crf` copre esposizione, "
       "vignettatura e colore, poi taglia semplicemente il risultato. `original` "
       "vi aggiunge una curva tonale. `rqs` usa una curva tonale che si comporta "
       "meglio nelle zone scure."),
    NL("Welke cameraeffecten worden gemodelleerd. `no_crf` dekt belichting, vignettering "
       "en kleur en kapt het resultaat daarna gewoon af. `original` legt daar "
       "een tooncurve overheen. `rqs` gebruikt een tooncurve die zich in donkere "
       "partijen beter gedraagt."),
    RU("Какие эффекты камеры моделируются. `no_crf` охватывает экспозицию, виньетирование "
       "и цвет, а затем просто обрезает результат. `original` добавляет сверху "
       "тоновую кривую. `rqs` использует тоновую кривую, которая лучше ведёт "
       "себя в тенях."),
    TR("Hangi kamera etkilerinin modelleneceği. `no_crf` pozlamayı, vinyeti ve "
       "rengi kapsar, ardından sonucu düpedüz kırpar. `original` bunun üstüne "
       "bir ton eğrisi ekler. `rqs` ise karanlık bölgelerde daha iyi davranan "
       "bir ton eğrisi kullanır."));

SS_MSG(ppisp_exposure_from_exif,
    EN("Exposure init from EXIF"), JA("EXIF による露出の初期化"),
    ZH_HANS("用 EXIF 初始化曝光"), ZH_HANT("用 EXIF 初始化曝光"),
    KO("EXIF로 노출 초기화"), DE("Belichtungsstart aus EXIF"),
    FR("Exposition initiale depuis l'EXIF"), ES("Exposición inicial desde EXIF"),
    PT("Exposição inicial do EXIF"), IT("Esposizione iniziale da EXIF"),
    NL("Belichting starten vanuit EXIF"), RU("Начальная экспозиция из EXIF"),
    TR("EXIF'ten pozlama başlangıcı"));
SS_MSG(ppisp_exposure_from_exif_help,
    EN("Seed each photo's exposure correction from the shutter, aperture and "
       "ISO its EXIF records, relative to the set's average, instead of from "
       "zero. Photos without those tags start at the average. Helps when "
       "exposure varies across the capture."),
    JA("各写真の露出補正を、ゼロからではなく EXIF に記録されたシャッター速度・"
       "絞り・ISO から、セットの平均を基準に初期化します。タグのない写真は平均"
       "から始まります。撮影中に露出が変わる場合に有効です。"),
    ZH_HANS("用 EXIF 记录的快门、光圈和 ISO（相对于整组的平均值）来初始化每张"
            "照片的曝光校正，而不是从零开始。没有这些标签的照片从平均值开始。"
            "在拍摄过程中曝光有变化时有帮助。"),
    ZH_HANT("用 EXIF 記錄的快門、光圈和 ISO（相對於整組的平均值）來初始化每張"
            "照片的曝光校正，而不是從零開始。沒有這些標籤的照片從平均值開始。"
            "在拍攝過程中曝光有變化時有幫助。"),
    KO("각 사진의 노출 보정을 0이 아니라 EXIF에 기록된 셔터·조리개·ISO에서, 전"
       "체 평균을 기준으로 초기화합니다. 태그가 없는 사진은 평균에서 시작합니"
       "다. 촬영 중 노출이 달라질 때 도움이 됩니다."),
    DE("Die Belichtungskorrektur jedes Fotos aus Verschlusszeit, Blende und ISO "
       "seiner EXIF-Daten starten, relativ zum Durchschnitt des Satzes, statt "
       "bei null. Fotos ohne diese Tags starten beim Durchschnitt. Hilft, wenn "
       "die Belichtung über die Aufnahme variiert."),
    FR("Amorcer la correction d'exposition de chaque photo à partir de la vitesse, "
       "de l'ouverture et de l'ISO enregistrés dans son EXIF, par rapport à la "
       "moyenne de l'ensemble, plutôt qu'à zéro. Les photos sans ces balises "
       "partent de la moyenne. Utile quand l'exposition varie au fil de la prise "
       "de vue."),
    ES("Inicializar la corrección de exposición de cada foto a partir del obturador, "
       "la apertura y el ISO que registra su EXIF, relativos a la media del conjunto, "
       "en lugar de desde cero. Las fotos sin esas etiquetas parten de la media. "
       "Ayuda cuando la exposición varía a lo largo de la captura."),
    PT("Inicializar a correção de exposição de cada foto a partir do obturador, "
       "da abertura e do ISO registrados no EXIF, relativos à média do conjunto, "
       "em vez de partir do zero. Fotos sem essas tags partem da média. Ajuda "
       "quando a exposição varia ao longo da captura."),
    IT("Inizializzare la correzione di esposizione di ogni foto da otturatore, "
       "apertura e ISO registrati nel suo EXIF, relativi alla media del set, invece "
       "che da zero. Le foto senza quei tag partono dalla media. Aiuta quando "
       "l'esposizione varia durante la ripresa."),
    NL("De belichtingscorrectie van elke foto starten vanuit de sluitertijd, het "
       "diafragma en de ISO uit de EXIF, relatief aan het gemiddelde van de set, "
       "in plaats van vanaf nul. Foto's zonder die tags starten op het gemiddelde. "
       "Helpt wanneer de belichting tijdens de opname varieert."),
    RU("Инициализировать коррекцию экспозиции каждого фото по выдержке, диафрагме "
       "и ISO из его EXIF относительно среднего по набору, а не с нуля. Фото без "
       "этих тегов начинают со среднего. Помогает, когда экспозиция меняется по "
       "ходу съёмки."),
    TR("Her fotoğrafın pozlama düzeltmesini sıfırdan değil, EXIF'inde kayıtlı "
       "enstantane, diyafram ve ISO'dan, kümenin ortalamasına göre başlatır. Bu "
       "etiketleri olmayan fotoğraflar ortalamadan başlar. Pozlama çekim boyunca "
       "değişiyorsa yardımcı olur."));

SS_MSG(apply_ppisp_before_bilagrid,
    EN("Camera correction first"), JA("カメラ補正を先に適用"),
    ZH_HANS("先做相机校正"), ZH_HANT("先做相機校正"),
    KO("카메라 보정을 먼저"), DE("Kamerakorrektur zuerst"),
    FR("Correction caméra en premier"), ES("Corrección de cámara primero"),
    PT("Correção de câmera primeiro"), IT("Correzione camera per prima"),
    NL("Cameracorrectie eerst"), RU("Сначала коррекция камеры"),
    TR("Önce kamera düzeltmesi"));
SS_MSG(apply_ppisp_before_bilagrid_help,
    EN("Run the camera-effect model before the per-photo color correction rather "
       "than after. Only matters when both are on, and decides which of the two "
       "absorbs a given color difference."),
    JA("カメラ効果のモデルを、写真ごとの色補正のあとではなく前に適用します。両"
       "方を有効にしているときだけ意味があり、ある色の差をどちらが吸収するかを"
       "決めます。"),
    ZH_HANS("让相机效应模型在逐张照片的颜色校正之前运行，而不是之后。只有两者"
            "都开启时才有意义，它决定某个颜色差异由哪一方吸收。"),
    ZH_HANT("讓相機效應模型在逐張照片的色彩校正之前執行，而不是之後。只有兩者"
            "都開啟時才有意義，它決定某個顏色差異由哪一方吸收。"),
    KO("카메라 효과 모델을 사진별 색 보정 뒤가 아니라 앞에서 적용합니다. 둘 다"
       " 켰을 때만 의미가 있으며, 어떤 색 차이를 어느 쪽이 흡수할지 정합니다."),
    DE("Das Kameraeffektmodell vor statt nach der Farbkorrektur pro Foto ausführen. "
       "Nur von Belang, wenn beide an sind, und entscheidet, welches von beiden "
       "einen gegebenen Farbunterschied aufnimmt."),
    FR("Exécuter le modèle d'effets caméra avant la correction de couleur par "
       "photo plutôt qu'après. N'a d'importance que si les deux sont actifs, "
       "et détermine lequel absorbe une différence de couleur donnée."),
    ES("Ejecutar el modelo de efectos de cámara antes de la corrección de color "
       "por foto en vez de después. Solo importa si ambos están activos, y decide "
       "cuál de los dos absorbe una diferencia de color dada."),
    PT("Executar o modelo de efeitos de câmera antes da correção de cor por foto "
       "em vez de depois. Só importa quando ambos estão ligados e decide qual "
       "dos dois absorve uma dada diferença de cor."),
    IT("Eseguire il modello degli effetti della camera prima invece che dopo "
       "la correzione di colore per foto. Conta solo quando entrambi sono attivi "
       "e decide quale dei due assorbe una data differenza di colore."),
    NL("Het cameraeffectmodel vóór in plaats van na de kleurcorrectie per foto "
       "uitvoeren. Doet er alleen toe als beide aan staan en bepaalt welke van "
       "de twee een bepaald kleurverschil opvangt."),
    RU("Запускать модель эффектов камеры до, а не после покадровой цветокоррекции. "
       "Имеет значение, только когда включены оба, и решает, кто из двоих вбирает "
       "данную разницу цвета."),
    TR("Kamera etkisi modelini fotoğraf başına renk düzeltmesinden sonra değil "
       "önce çalıştırır. Yalnızca ikisi de açıkken önemlidir ve belirli bir renk "
       "farkını hangisinin soğuracağını belirler."));

SS_MSG(use_adagrad_ppisp_optim,
    EN("Steadier camera correction updates"),
    JA("カメラ補正の更新を安定させる"), ZH_HANS("更平稳地更新相机校正"),
    ZH_HANT("更平穩地更新相機校正"), KO("카메라 보정 갱신을 안정적으로"),
    DE("Ruhigere Aktualisierung der Kamerakorrektur"),
    FR("Mise à jour plus stable de la correction caméra"),
    ES("Actualización más estable de la corrección de cámara"),
    PT("Atualização mais estável da correção de câmera"),
    IT("Aggiornamento più stabile della correzione camera"),
    NL("Stabielere update van de cameracorrectie"),
    RU("Более ровное обновление коррекции камеры"),
    TR("Kamera düzeltmede daha durağan güncelleme"));
SS_MSG(use_adagrad_ppisp_optim_help,
    EN("Use a steadier update rule for the camera-effect model. Generally more "
       "stable, needs less tuning, and leads to fewer floaters; turn off to use "
       "the scheduled learning rate instead."),
    JA("カメラ効果のモデルに、より落ち着いた更新則を使います。おおむね安定して"
       "いて調整も少なくて済み、浮遊ノイズも減ります。オフにすると、代わりにス"
       "ケジュールされた学習率が使われます。"),
    ZH_HANS("为相机效应模型采用更平稳的更新规则。通常更稳定、更少需要调参，漂"
            "浮物也更少；关闭后改用按计划变化的学习率。"),
    ZH_HANT("為相機效應模型採用更平穩的更新規則。通常更穩定、更少需要調參，漂"
            "浮物也更少；關閉後改用按排程變化的學習率。"),
    KO("카메라 효과 모델에 더 안정적인 갱신 규칙을 씁니다. 대체로 더 안정적이"
       "고 손볼 일이 적으며 부유물도 줄어듭니다. 끄면 대신 스케줄된 학습률을 "
       "사용합니다."),
    DE("Für das Kameraeffektmodell eine ruhigere Aktualisierungsregel verwenden. "
       "Meist stabiler, weniger einstellungsbedürftig und mit weniger Schwebeteilen; "
       "abgeschaltet gilt stattdessen die geplante Lernrate."),
    FR("Utiliser une règle de mise à jour plus stable pour le modèle d'effets "
       "caméra. Généralement plus sûre, demandant moins de réglage et produisant "
       "moins de flotteurs ; décoché, c'est le taux d'apprentissage programmé "
       "qui s'applique."),
    ES("Usar una regla de actualización más estable para el modelo de efectos "
       "de cámara. Suele ser más estable, necesitar menos ajuste y dejar menos "
       "restos flotantes; sin marcar se usa en su lugar la tasa de aprendizaje "
       "programada."),
    PT("Usar uma regra de atualização mais estável para o modelo de efeitos de "
       "câmera. Costuma ser mais estável, exigir menos ajuste e deixar menos "
       "resíduos flutuantes; desmarcado, usa-se a taxa de aprendizado programada."),
    IT("Usare una regola di aggiornamento più stabile per il modello degli effetti "
       "della camera. In genere più stabile, con meno bisogno di regolazioni "
       "e meno frammenti fluttuanti; deselezionato si usa invece il tasso di "
       "apprendimento pianificato."),
    NL("Voor het cameraeffectmodel een stabielere updateregel gebruiken. Meestal "
       "stabieler, met minder afstemwerk en minder zwevers; uitgevinkt geldt "
       "in plaats daarvan de geplande leersnelheid."),
    RU("Использовать более ровное правило обновления для модели эффектов камеры. "
       "Обычно устойчивее, требует меньше настройки и даёт меньше «летающих» "
       "артефактов; без флажка применяется запланированная скорость обучения."),
    TR("Kamera etkisi modeli için daha durağan bir güncelleme kuralı kullanır. "
       "Genelde daha kararlıdır, daha az ayar ister ve daha az uçuşan artık bırakır; "
       "kapatılırsa çizelgelenmiş öğrenme oranı kullanılır."));

SS_MSG(ppisp_reg_exposure_mean,
    EN("Neutral exposure penalty"), JA("露出を中立に保つ強さ"),
    ZH_HANS("保持曝光中性的强度"), ZH_HANT("保持曝光中性的強度"),
    KO("노출을 중립으로 유지"), DE("Strafe für nicht neutrale Belichtung"),
    FR("Pénalité d'exposition non neutre"),
    ES("Penalización de exposición no neutra"),
    PT("Penalidade de exposição não neutra"),
    IT("Penalità di esposizione non neutra"),
    NL("Straf voor niet-neutrale belichting"),
    RU("Штраф за смещение экспозиции"), TR("Nötr olmayan pozlama cezası"));
SS_MSG(ppisp_reg_exposure_mean_help,
    EN("Keep estimated exposures centered around neutral. Stops overall brightness "
       "from being counted twice between the splats and the camera model."),
    JA("推定された露出を中立のあたりに保ちます。全体の明るさが、スプラット側と"
       "カメラモデル側で二重に数えられるのを防ぎます。"),
    ZH_HANS("让估计出的曝光保持在中性附近。避免整体亮度在泼溅和相机模型之间被"
            "重复计算两次。"),
    ZH_HANT("讓估計出的曝光保持在中性附近。避免整體亮度在潑濺和相機模型之間被"
            "重複計算兩次。"),
    KO("추정된 노출을 중립 근처로 유지합니다. 전체 밝기가 스플랫과 카메라 모델"
       " 양쪽에서 두 번 계산되는 것을 막습니다."),
    DE("Die geschätzten Belichtungen um neutral herum halten. Verhindert, dass "
       "die Gesamthelligkeit zwischen Splats und Kameramodell doppelt gezählt "
       "wird."),
    FR("Garder les expositions estimées centrées sur le neutre. Évite que la "
       "luminosité globale soit comptée deux fois, entre les splats et le modèle "
       "de caméra."),
    ES("Mantener las exposiciones estimadas centradas en lo neutro. Evita que "
       "el brillo general se cuente dos veces, entre los splats y el modelo de "
       "cámara."),
    PT("Manter as exposições estimadas centradas no neutro. Evita que o brilho "
       "geral seja contado duas vezes, entre os splats e o modelo de câmera."),
    IT("Tenere le esposizioni stimate centrate sul neutro. Evita che la luminosità "
       "complessiva venga contata due volte, tra gli splat e il modello della "
       "camera."),
    NL("De geschatte belichtingen rond neutraal houden. Voorkomt dat de algehele "
       "helderheid dubbel wordt geteld, tussen de splats en het cameramodel."),
    RU("Держать оценённые экспозиции около нейтрали. Не даёт общей яркости учитываться "
       "дважды — и в сплатах, и в модели камеры."),
    TR("Kestirilen pozlamaları nötr çevresinde tutar. Genel parlaklığın splat'lar "
       "ile kamera modeli arasında iki kez sayılmasını önler."));

SS_MSG(ppisp_reg_color_mean,
    EN("Neutral color penalty"), JA("色を中立に保つ強さ"),
    ZH_HANS("保持颜色中性的强度"), ZH_HANT("保持顏色中性的強度"),
    KO("색을 중립으로 유지"), DE("Strafe für nicht neutrale Farbe"),
    FR("Pénalité de couleur non neutre"),
    ES("Penalización de color no neutro"),
    PT("Penalidade de cor não neutra"), IT("Penalità di colore non neutro"),
    NL("Straf voor niet-neutrale kleur"), RU("Штраф за смещение цвета"),
    TR("Nötr olmayan renk cezası"));
SS_MSG(ppisp_reg_color_mean_help,
    EN("Keep the per-photo color corrections centered, so no overall tint gets "
       "baked into the splats."),
    JA("写真ごとの色補正を中立のあたりに保ち、全体の色かぶりがスプラットに焼き"
       "付かないようにします。"),
    ZH_HANS("让逐张照片的颜色校正保持居中，避免整体色偏被烙进泼溅里。"),
    ZH_HANT("讓逐張照片的色彩校正保持置中，避免整體色偏被烙進潑濺裡。"),
    KO("사진별 색 보정을 중립 근처로 유지해 전체적인 색조가 스플랫에 새겨지지"
       " 않게 합니다."),
    DE("Die Farbkorrekturen pro Foto zentriert halten, damit sich kein Gesamtfarbstich "
       "in die Splats einbrennt."),
    FR("Garder les corrections de couleur par photo centrées, pour qu'aucune "
       "teinte globale ne se grave dans les splats."),
    ES("Mantener centradas las correcciones de color por foto, para que ningún "
       "tinte global se grabe en los splats."),
    PT("Manter centradas as correções de cor por foto, para que nenhuma tonalidade "
       "geral fique gravada nos splats."),
    IT("Tenere centrate le correzioni di colore per foto, così nessuna dominante "
       "complessiva si imprime negli splat."),
    NL("De kleurcorrecties per foto gecentreerd houden, zodat er geen algehele "
       "kleurzweem in de splats wordt gebrand."),
    RU("Держать покадровые цветовые коррекции по центру, чтобы общий оттенок "
       "не впечатался в сплаты."),
    TR("Fotoğraf başına renk düzeltmelerini nötr çevresinde tutar; böylece splat'lara "
       "genel bir renk kayması işlenmez."));

SS_MSG(ppisp_reg_vig_center,
    EN("Vignetting centering penalty"), JA("周辺減光の中心ずれのペナルティ"),
    ZH_HANS("暗角中心偏移的惩罚"), ZH_HANT("暗角中心偏移的懲罰"),
    KO("비네팅 중심 이탈 페널티"),
    DE("Strafe für außermittige Vignettierung"),
    FR("Pénalité de vignetage décentré"),
    ES("Penalización de viñeteado descentrado"),
    PT("Penalidade de vinhetagem descentrada"),
    IT("Penalità di vignettatura fuori centro"),
    NL("Straf voor niet-gecentreerde vignettering"),
    RU("Штраф за смещение центра виньетирования"),
    TR("Merkezden kaymış vinyet cezası"));
SS_MSG(ppisp_reg_vig_center_help,
    EN("Keep estimated vignetting centered near the middle of the image rather "
       "than drifting toward a corner."),
    JA("推定される周辺減光の中心を、隅に流れずに画像の中央近くにとどめます。"),
    ZH_HANS("让估计出的暗角中心留在画面中部附近，而不是漂向某个角落。"),
    ZH_HANT("讓估計出的暗角中心留在畫面中部附近，而不是漂向某個角落。"),
    KO("추정된 비네팅의 중심이 모서리로 흘러가지 않고 이미지 가운데 근처에 머"
       "물게 합니다."),
    DE("Die geschätzte Vignettierung nahe der Bildmitte halten, statt sie zu "
       "einer Ecke driften zu lassen."),
    FR("Garder le vignetage estimé centré près du milieu de l'image plutôt que "
       "de le laisser dériver vers un coin."),
    ES("Mantener el viñeteado estimado cerca del centro de la imagen en vez de "
       "dejar que derive hacia una esquina."),
    PT("Manter a vinhetagem estimada perto do centro da imagem em vez de deixá-la "
       "derivar para um canto."),
    IT("Tenere la vignettatura stimata vicino al centro dell'immagine invece "
       "di lasciarla derivare verso un angolo."),
    NL("De geschatte vignettering dicht bij het midden van het beeld houden in "
       "plaats van naar een hoek te laten afdrijven."),
    RU("Держать оценённое виньетирование около центра кадра, не давая ему уплывать "
       "к углу."),
    TR("Kestirilen vinyeti bir köşeye kaymak yerine görüntünün ortasına yakın "
       "tutar."));

SS_MSG(ppisp_reg_vig_non_pos,
    EN("Vignetting darkening penalty"), JA("周辺が明るくなるのを防ぐ強さ"),
    ZH_HANS("防止边角变亮的强度"), ZH_HANT("防止邊角變亮的強度"),
    KO("가장자리가 밝아지는 것 방지"),
    DE("Strafe für aufhellende Vignettierung"),
    FR("Pénalité de vignetage éclaircissant"),
    ES("Penalización de viñeteado que aclara"),
    PT("Penalidade de vinhetagem que clareia"),
    IT("Penalità di vignettatura schiarente"),
    NL("Straf voor oplichtende vignettering"),
    RU("Штраф за осветляющее виньетирование"),
    TR("Köşeleri aydınlatan vinyet cezası"));
SS_MSG(ppisp_reg_vig_non_pos_help,
    EN("Keep vignetting darkening the corners rather than brightening them, which "
       "is what real lenses do."),
    JA("周辺減光が隅を明るくするのではなく暗くするように保ちます。実際のレンズ"
       "はそう振る舞います。"),
    ZH_HANS("让暗角只压暗边角，而不是把它们提亮——真实镜头就是这样表现的。"),
    ZH_HANT("讓暗角只壓暗邊角，而不是把它們提亮——真實鏡頭就是這樣表現的。"),
    KO("비네팅이 가장자리를 밝히지 않고 어둡게 만들도록 유지합니다. 실제 렌즈"
       "가 그렇게 동작합니다."),
    DE("Dafür sorgen, dass die Vignettierung die Ecken abdunkelt statt aufhellt, "
       "wie es echte Objektive tun."),
    FR("Faire en sorte que le vignetage assombrisse les coins au lieu de les "
       "éclaircir, comme le font les vrais objectifs."),
    ES("Procurar que el viñeteado oscurezca las esquinas en vez de aclararlas, "
       "que es lo que hacen los objetivos reales."),
    PT("Fazer com que a vinhetagem escureça os cantos em vez de clareá-los, como "
       "fazem as lentes reais."),
    IT("Fare in modo che la vignettatura scurisca gli angoli invece di schiarirli, "
       "come fanno gli obiettivi veri."),
    NL("Ervoor zorgen dat de vignettering de hoeken donkerder maakt in plaats "
       "van lichter, zoals echte lenzen doen."),
    RU("Следить, чтобы виньетирование затемняло углы, а не осветляло их, — так "
       "ведут себя настоящие объективы."),
    TR("Vinyetin köşeleri aydınlatmak yerine karartmasını sağlar; gerçek mercekler "
       "böyle davranır."));

SS_MSG(ppisp_reg_vig_channel_var,
    EN("Vignetting color cast penalty"), JA("周辺減光の色ずれのペナルティ"),
    ZH_HANS("暗角色偏的惩罚"), ZH_HANT("暗角色偏的懲罰"),
    KO("비네팅 색 치우침 페널티"), DE("Strafe für farbige Vignettierung"),
    FR("Pénalité de vignetage coloré"),
    ES("Penalización de viñeteado con dominante"),
    PT("Penalidade de vinhetagem com dominante"),
    IT("Penalità di vignettatura colorata"),
    NL("Straf voor gekleurde vignettering"),
    RU("Штраф за цветное виньетирование"), TR("Renkli vinyet cezası"));
SS_MSG(ppisp_reg_vig_channel_var_help,
    EN("Keep vignetting similar across red, green and blue, so image corners "
       "do not pick up a color cast."),
    JA("周辺減光を赤・緑・青でそろえ、画像の隅に色かぶりが出ないようにします。"),
    ZH_HANS("让暗角在红、绿、蓝三个通道上保持一致，避免画面边角染上色偏。"),
    ZH_HANT("讓暗角在紅、綠、藍三個通道上保持一致，避免畫面邊角染上色偏。"),
    KO("비네팅을 빨강·초록·파랑에서 비슷하게 유지해 이미지 모서리에 색조가 끼"
       "지 않게 합니다."),
    DE("Die Vignettierung über Rot, Grün und Blau ähnlich halten, damit die Bildecken "
       "keinen Farbstich bekommen."),
    FR("Garder le vignetage semblable sur le rouge, le vert et le bleu, pour "
       "que les coins de l'image ne prennent pas de dominante."),
    ES("Mantener el viñeteado parecido en rojo, verde y azul, para que las esquinas "
       "de la imagen no cojan una dominante."),
    PT("Manter a vinhetagem parecida em vermelho, verde e azul, para que os cantos "
       "da imagem não ganhem uma dominante."),
    IT("Tenere la vignettatura simile su rosso, verde e blu, così gli angoli "
       "dell'immagine non prendono una dominante."),
    NL("De vignettering voor rood, groen en blauw gelijk houden, zodat de beeldhoeken "
       "geen kleurzweem krijgen."),
    RU("Держать виньетирование одинаковым по красному, зелёному и синему, чтобы "
       "углы кадра не окрашивались."),
    TR("Vinyeti kırmızı, yeşil ve mavide birbirine yakın tutar; böylece görüntü "
       "köşeleri renk kaymasına uğramaz."));

SS_MSG(ppisp_reg_crf_channel_var,
    EN("Tone curve color cast penalty"),
    JA("トーンカーブの色ずれのペナルティ"), ZH_HANS("色调曲线色偏的惩罚"),
    ZH_HANT("色調曲線色偏的懲罰"), KO("톤 커브 색 치우침 페널티"),
    DE("Strafe für farbige Tonwertkurve"),
    FR("Pénalité de courbe de tons colorée"),
    ES("Penalización de curva de tonos con dominante"),
    PT("Penalidade de curva tonal com dominante"),
    IT("Penalità di curva tonale colorata"),
    NL("Straf voor gekleurde tooncurve"),
    RU("Штраф за цветной сдвиг тоновой кривой"),
    TR("Renkli ton eğrisi cezası"));
SS_MSG(ppisp_reg_crf_channel_var_help,
    EN("Keep the tone curve similar across red, green and blue, so brightness "
       "changes do not shift hue."),
    JA("トーンカーブを赤・緑・青でそろえ、明るさの変化で色合いがずれないように"
       "します。"),
    ZH_HANS("让色调曲线在红、绿、蓝三个通道上保持一致，避免亮度变化引起色相偏"
            "移。"),
    ZH_HANT("讓色調曲線在紅、綠、藍三個通道上保持一致，避免亮度變化引起色相偏"
            "移。"),
    KO("톤 커브를 빨강·초록·파랑에서 비슷하게 유지해 밝기 변화가 색상을 틀지 "
       "않게 합니다."),
    DE("Die Tonwertkurve über Rot, Grün und Blau ähnlich halten, damit Helligkeitsänderungen "
       "den Farbton nicht verschieben."),
    FR("Garder la courbe de tons semblable sur le rouge, le vert et le bleu, "
       "pour que les changements de luminosité ne décalent pas la teinte."),
    ES("Mantener la curva de tonos parecida en rojo, verde y azul, para que los "
       "cambios de brillo no desplacen el tono."),
    PT("Manter a curva tonal parecida em vermelho, verde e azul, para que as "
       "mudanças de brilho não desloquem o matiz."),
    IT("Tenere la curva tonale simile su rosso, verde e blu, così i cambi di "
       "luminosità non spostano la tinta."),
    NL("De tooncurve voor rood, groen en blauw gelijk houden, zodat helderheidsveranderingen "
       "de tint niet verschuiven."),
    RU("Держать тоновую кривую одинаковой по красному, зелёному и синему, чтобы "
       "изменения яркости не смещали оттенок."),
    TR("Ton eğrisini kırmızı, yeşil ve mavide birbirine yakın tutar; böylece "
       "parlaklık değişimleri tonu kaydırmaz."));

SS_MSG(bilagrid_adagrad_lr,
    EN("Color correction rate (steady)"), JA("色補正の学習率（安定版）"),
    ZH_HANS("颜色校正学习率（平稳）"), ZH_HANT("色彩校正學習率（平穩）"),
    KO("색 보정 학습률(안정)"), DE("Lernrate der Farbkorrektur (ruhig)"),
    FR("Taux de la correction des couleurs (stable)"),
    ES("Tasa de la corrección de color (estable)"),
    PT("Taxa da correção de cor (estável)"),
    IT("Tasso della correzione del colore (stabile)"),
    NL("Snelheid van de kleurcorrectie (stabiel)"),
    RU("Скорость цветокоррекции (ровная)"),
    TR("Renk düzeltme hızı (durağan)"));
SS_MSG(bilagrid_adagrad_lr_help,
    EN("How fast the per-photo color correction adapts when use_adagrad_bilagrid_optim "
       "is on. This rate is constant, with no schedule or warmup."),
    JA("use_adagrad_bilagrid_optim が有効なときに、写真ごとの色補正がどれだけ"
       "速く適応するかです。この学習率は一定で、スケジュールもウォームアップも"
       "ありません。"),
    ZH_HANS("当 use_adagrad_bilagrid_optim 打开时，逐张照片的颜色校正适应得有"
            "多快。该学习率恒定，没有调度也没有预热。"),
    ZH_HANT("當 use_adagrad_bilagrid_optim 開啟時，逐張照片的色彩校正適應得有"
            "多快。該學習率恆定，沒有排程也沒有預熱。"),
    KO("use_adagrad_bilagrid_optim이 켜져 있을 때 사진별 색 보정이 얼마나 빨리"
       " 적응하는지입니다. 이 학습률은 일정하며 스케줄도 워밍업도 없습니다."),
    DE("Wie schnell sich die Farbkorrektur pro Foto anpasst, wenn use_adagrad_bilagrid_optim "
       "an ist. Diese Rate ist konstant, ohne Zeitplan und ohne Anlauf."),
    FR("À quelle vitesse la correction de couleur par photo s'adapte quand use_adagrad_bilagrid_optim "
       "est activé. Ce taux est constant, sans calendrier ni montée en régime."),
    ES("Con qué rapidez se adapta la corrección de color por foto cuando use_adagrad_bilagrid_optim "
       "está activo. Esta tasa es constante, sin calendario ni arranque progresivo."),
    PT("Com que rapidez a correção de cor por foto se adapta quando use_adagrad_bilagrid_optim "
       "está ligado. Esta taxa é constante, sem cronograma nem aquecimento."),
    IT("Con quanta rapidità si adatta la correzione di colore per foto quando "
       "use_adagrad_bilagrid_optim è attivo. Questo tasso è costante, senza pianificazione "
       "né avvio graduale."),
    NL("Hoe snel de kleurcorrectie per foto zich aanpast als use_adagrad_bilagrid_optim "
       "aan staat. Deze snelheid is constant, zonder schema of opbouw."),
    RU("Насколько быстро подстраивается покадровая цветокоррекция, когда включён "
       "use_adagrad_bilagrid_optim. Эта скорость постоянна: без расписания и "
       "без разгона."),
    TR("use_adagrad_bilagrid_optim açıkken fotoğraf başına renk düzeltmesinin "
       "ne kadar hızlı uyum sağladığı. Bu oran sabittir; çizelge ya da ısınma "
       "yoktur."));

SS_MSG(bilagrid_adagrad_depth_lr,
    EN("Depth correction rate (steady)"), JA("深度補正の学習率（安定版）"),
    ZH_HANS("深度校正学习率（平稳）"), ZH_HANT("深度校正學習率（平穩）"),
    KO("깊이 보정 학습률(안정)"), DE("Lernrate der Tiefenkorrektur (ruhig)"),
    FR("Taux de la correction de profondeur (stable)"),
    ES("Tasa de la corrección de profundidad (estable)"),
    PT("Taxa da correção de profundidade (estável)"),
    IT("Tasso della correzione di profondità (stabile)"),
    NL("Snelheid van de dieptecorrectie (stabiel)"),
    RU("Скорость коррекции глубины (ровная)"),
    TR("Derinlik düzeltme hızı (durağan)"));
SS_MSG(bilagrid_adagrad_depth_lr_help,
    EN("How fast the per-photo depth correction adapts when use_adagrad_bilagrid_optim "
       "is on. This rate is constant, with no schedule or warmup."),
    JA("use_adagrad_bilagrid_optim が有効なときに、写真ごとの深度補正がどれだ"
       "け速く適応するかです。この学習率は一定で、スケジュールもウォームアップ"
       "もありません。"),
    ZH_HANS("当 use_adagrad_bilagrid_optim 打开时，逐张照片的深度校正适应得有"
            "多快。该学习率恒定，没有调度也没有预热。"),
    ZH_HANT("當 use_adagrad_bilagrid_optim 開啟時，逐張照片的深度校正適應得有"
            "多快。該學習率恆定，沒有排程也沒有預熱。"),
    KO("use_adagrad_bilagrid_optim이 켜져 있을 때 사진별 깊이 보정이 얼마나 빨"
       "리 적응하는지입니다. 이 학습률은 일정하며 스케줄도 워밍업도 없습니다."),
    DE("Wie schnell sich die Tiefenkorrektur pro Foto anpasst, wenn use_adagrad_bilagrid_optim "
       "an ist. Diese Rate ist konstant, ohne Zeitplan und ohne Anlauf."),
    FR("À quelle vitesse la correction de profondeur par photo s'adapte quand "
       "use_adagrad_bilagrid_optim est activé. Ce taux est constant, sans calendrier "
       "ni montée en régime."),
    ES("Con qué rapidez se adapta la corrección de profundidad por foto cuando "
       "use_adagrad_bilagrid_optim está activo. Esta tasa es constante, sin calendario "
       "ni arranque progresivo."),
    PT("Com que rapidez a correção de profundidade por foto se adapta quando "
       "use_adagrad_bilagrid_optim está ligado. Esta taxa é constante, sem cronograma "
       "nem aquecimento."),
    IT("Con quanta rapidità si adatta la correzione di profondità per foto quando "
       "use_adagrad_bilagrid_optim è attivo. Questo tasso è costante, senza pianificazione "
       "né avvio graduale."),
    NL("Hoe snel de dieptecorrectie per foto zich aanpast als use_adagrad_bilagrid_optim "
       "aan staat. Deze snelheid is constant, zonder schema of opbouw."),
    RU("Насколько быстро подстраивается покадровая коррекция глубины, когда включён "
       "use_adagrad_bilagrid_optim. Эта скорость постоянна: без расписания и "
       "без разгона."),
    TR("use_adagrad_bilagrid_optim açıkken fotoğraf başına derinlik düzeltmesinin "
       "ne kadar hızlı uyum sağladığı. Bu oran sabittir; çizelge ya da ısınma "
       "yoktur."));

SS_MSG(bilagrid_adagrad_normal_lr,
    EN("Normal correction rate (steady)"), JA("法線補正の学習率（安定版）"),
    ZH_HANS("法线校正学习率（平稳）"), ZH_HANT("法線校正學習率（平穩）"),
    KO("노멀 보정 학습률(안정)"),
    DE("Lernrate der Normalenkorrektur (ruhig)"),
    FR("Taux de la correction des normales (stable)"),
    ES("Tasa de la corrección de normales (estable)"),
    PT("Taxa da correção de normais (estável)"),
    IT("Tasso della correzione delle normali (stabile)"),
    NL("Snelheid van de normalencorrectie (stabiel)"),
    RU("Скорость коррекции нормалей (ровная)"),
    TR("Normal düzeltme hızı (durağan)"));
SS_MSG(bilagrid_adagrad_normal_lr_help,
    EN("How fast the per-photo normal correction adapts when use_adagrad_bilagrid_optim "
       "is on. This rate is constant, with no schedule or warmup."),
    JA("use_adagrad_bilagrid_optim が有効なときに、写真ごとの法線補正がどれだ"
       "け速く適応するかです。この学習率は一定で、スケジュールもウォームアップ"
       "もありません。"),
    ZH_HANS("当 use_adagrad_bilagrid_optim 打开时，逐张照片的法线校正适应得有"
            "多快。该学习率恒定，没有调度也没有预热。"),
    ZH_HANT("當 use_adagrad_bilagrid_optim 開啟時，逐張照片的法線校正適應得有"
            "多快。該學習率恆定，沒有排程也沒有預熱。"),
    KO("use_adagrad_bilagrid_optim이 켜져 있을 때 사진별 노멀 보정이 얼마나 빨"
       "리 적응하는지입니다. 이 학습률은 일정하며 스케줄도 워밍업도 없습니다."),
    DE("Wie schnell sich die Normalenkorrektur pro Foto anpasst, wenn use_adagrad_bilagrid_optim "
       "an ist. Diese Rate ist konstant, ohne Zeitplan und ohne Anlauf."),
    FR("À quelle vitesse la correction de normales par photo s'adapte quand use_adagrad_bilagrid_optim "
       "est activé. Ce taux est constant, sans calendrier ni montée en régime."),
    ES("Con qué rapidez se adapta la corrección de normales por foto cuando use_adagrad_bilagrid_optim "
       "está activo. Esta tasa es constante, sin calendario ni arranque progresivo."),
    PT("Com que rapidez a correção de normais por foto se adapta quando use_adagrad_bilagrid_optim "
       "está ligado. Esta taxa é constante, sem cronograma nem aquecimento."),
    IT("Con quanta rapidità si adatta la correzione delle normali per foto quando "
       "use_adagrad_bilagrid_optim è attivo. Questo tasso è costante, senza pianificazione "
       "né avvio graduale."),
    NL("Hoe snel de normalencorrectie per foto zich aanpast als use_adagrad_bilagrid_optim "
       "aan staat. Deze snelheid is constant, zonder schema of opbouw."),
    RU("Насколько быстро подстраивается покадровая коррекция нормалей, когда "
       "включён use_adagrad_bilagrid_optim. Эта скорость постоянна: без расписания "
       "и без разгона."),
    TR("use_adagrad_bilagrid_optim açıkken fotoğraf başına normal düzeltmesinin "
       "ne kadar hızlı uyum sağladığı. Bu oran sabittir; çizelge ya da ısınma "
       "yoktur."));

SS_MSG(ppisp_adagrad_lr,
    EN("Camera correction rate (steady)"),
    JA("カメラ補正の学習率（安定版）"), ZH_HANS("相机校正学习率（平稳）"),
    ZH_HANT("相機校正學習率（平穩）"), KO("카메라 보정 학습률(안정)"),
    DE("Lernrate der Kamerakorrektur (ruhig)"),
    FR("Taux de la correction caméra (stable)"),
    ES("Tasa de la corrección de cámara (estable)"),
    PT("Taxa da correção de câmera (estável)"),
    IT("Tasso della correzione camera (stabile)"),
    NL("Snelheid van de cameracorrectie (stabiel)"),
    RU("Скорость коррекции камеры (ровная)"),
    TR("Kamera düzeltme hızı (durağan)"));
SS_MSG(ppisp_adagrad_lr_help,
    EN("How fast the camera-effect model adapts when use_adagrad_ppisp_optim "
       "is on. This rate is constant, with no schedule or warmup."),
    JA("use_adagrad_ppisp_optim が有効なときに、カメラ効果のモデルがどれだけ速"
       "く適応するかです。この学習率は一定で、スケジュールもウォームアップもあ"
       "りません。"),
    ZH_HANS("当 use_adagrad_ppisp_optim 打开时，相机效应模型适应得有多快。该学"
            "习率恒定，没有调度也没有预热。"),
    ZH_HANT("當 use_adagrad_ppisp_optim 開啟時，相機效應模型適應得有多快。該學"
            "習率恆定，沒有排程也沒有預熱。"),
    KO("use_adagrad_ppisp_optim이 켜져 있을 때 카메라 효과 모델이 얼마나 빨리"
       " 적응하는지입니다. 이 학습률은 일정하며 스케줄도 워밍업도 없습니다."),
    DE("Wie schnell sich das Kameraeffektmodell anpasst, wenn use_adagrad_ppisp_optim "
       "an ist. Diese Rate ist konstant, ohne Zeitplan und ohne Anlauf."),
    FR("À quelle vitesse le modèle d'effets caméra s'adapte quand use_adagrad_ppisp_optim "
       "est activé. Ce taux est constant, sans calendrier ni montée en régime."),
    ES("Con qué rapidez se adapta el modelo de efectos de cámara cuando use_adagrad_ppisp_optim "
       "está activo. Esta tasa es constante, sin calendario ni arranque progresivo."),
    PT("Com que rapidez o modelo de efeitos de câmera se adapta quando use_adagrad_ppisp_optim "
       "está ligado. Esta taxa é constante, sem cronograma nem aquecimento."),
    IT("Con quanta rapidità si adatta il modello degli effetti della camera quando "
       "use_adagrad_ppisp_optim è attivo. Questo tasso è costante, senza pianificazione "
       "né avvio graduale."),
    NL("Hoe snel het cameraeffectmodel zich aanpast als use_adagrad_ppisp_optim "
       "aan staat. Deze snelheid is constant, zonder schema of opbouw."),
    RU("Насколько быстро подстраивается модель эффектов камеры, когда включён "
       "use_adagrad_ppisp_optim. Эта скорость постоянна: без расписания и без "
       "разгона."),
    TR("use_adagrad_ppisp_optim açıkken kamera etkisi modelinin ne kadar hızlı "
       "uyum sağladığı. Bu oran sabittir; çizelge ya da ısınma yoktur."));

SS_MSG(bilagrid_lr,
    EN("Color correction rate"), JA("色補正の学習率"),
    ZH_HANS("颜色校正学习率"), ZH_HANT("色彩校正學習率"),
    KO("색 보정 학습률"), DE("Lernrate der Farbkorrektur"),
    FR("Taux de la correction des couleurs"),
    ES("Tasa de la corrección de color"), PT("Taxa da correção de cor"),
    IT("Tasso della correzione del colore"),
    NL("Snelheid van de kleurcorrectie"), RU("Скорость цветокоррекции"),
    TR("Renk düzeltme hızı"));
SS_MSG(bilagrid_lr_help,
    EN("How fast the per-photo color correction adapts. Higher tracks exposure "
       "changes sooner but can start absorbing real detail. Ignored when use_adagrad_bilagrid_optim "
       "is on."),
    JA("写真ごとの色補正がどれだけ速く適応するかです。高いほど露出の変化に早く"
       "追いつきますが、本物の細部を吸い込み始めることがあります。use_adagrad_bilagrid_optim "
       "が有効なときは無視されます。"),
    ZH_HANS("逐张照片的颜色校正适应得有多快。数值越高越快跟上曝光变化，但可能"
            "开始吸收真实细节。当 use_adagrad_bilagrid_optim 打开时该项被忽略。"),
    ZH_HANT("逐張照片的色彩校正適應得有多快。數值越高越快跟上曝光變化，但可能"
            "開始吸收真實細節。當 use_adagrad_bilagrid_optim 開啟時該項被忽略。"),
    KO("사진별 색 보정이 얼마나 빨리 적응하는지입니다. 값이 크면 노출 변화를 "
       "빨리 따라가지만 실제 디테일을 흡수하기 시작할 수 있습니다. use_adagrad_bilagrid_optim이"
       " 켜져 있으면 무시됩니다."),
    DE("Wie schnell sich die Farbkorrektur pro Foto anpasst. Höher folgt Belichtungsänderungen "
       "früher, kann aber beginnen, echtes Detail aufzusaugen. Wird ignoriert, "
       "wenn use_adagrad_bilagrid_optim an ist."),
    FR("À quelle vitesse la correction de couleur par photo s'adapte. Plus haut "
       "suit plus tôt les changements d'exposition mais peut commencer à absorber "
       "du vrai détail. Ignoré quand use_adagrad_bilagrid_optim est activé."),
    ES("Con qué rapidez se adapta la corrección de color por foto. Más alto sigue "
       "antes los cambios de exposición pero puede empezar a absorber detalle "
       "real. Se ignora cuando use_adagrad_bilagrid_optim está activo."),
    PT("Com que rapidez a correção de cor por foto se adapta. Mais alto acompanha "
       "as mudanças de exposição mais cedo, mas pode começar a absorver detalhe "
       "real. É ignorado quando use_adagrad_bilagrid_optim está ligado."),
    IT("Con quanta rapidità si adatta la correzione di colore per foto. Più alto "
       "segue prima i cambi di esposizione ma può cominciare ad assorbire dettaglio "
       "vero. Viene ignorato quando use_adagrad_bilagrid_optim è attivo."),
    NL("Hoe snel de kleurcorrectie per foto zich aanpast. Hoger volgt belichtingsveranderingen "
       "eerder maar kan echt detail gaan opslokken. Wordt genegeerd als use_adagrad_bilagrid_optim "
       "aan staat."),
    RU("Насколько быстро подстраивается покадровая цветокоррекция. Больше — быстрее "
       "догоняет изменения экспозиции, но может начать вбирать настоящие детали. "
       "Игнорируется при включённом use_adagrad_bilagrid_optim."),
    TR("Fotoğraf başına renk düzeltmesinin ne kadar hızlı uyum sağladığı. Yüksek "
       "değerler pozlama değişimlerini daha erken yakalar ama gerçek ayrıntıyı "
       "içine çekmeye başlayabilir. use_adagrad_bilagrid_optim açıkken yok sayılır."));

SS_MSG(bilagrid_lr_final,
    EN("Final color correction rate"), JA("最後の色補正の学習率"),
    ZH_HANS("最终颜色校正学习率"), ZH_HANT("最終色彩校正學習率"),
    KO("최종 색 보정 학습률"), DE("Lernrate der Farbkorrektur am Ende"),
    FR("Taux final de la correction des couleurs"),
    ES("Tasa final de la corrección de color"),
    PT("Taxa final da correção de cor"),
    IT("Tasso finale della correzione del colore"),
    NL("Eindsnelheid van de kleurcorrectie"),
    RU("Итоговая скорость цветокоррекции"), TR("Son renk düzeltme hızı"));
SS_MSG(bilagrid_lr_final_help,
    EN("How fast the per-photo color correction adapts by the end of training. "
       "Set to none to keep the rate constant."),
    JA("学習の終わりごろに、写真ごとの色補正がどれだけ速く適応するかです。none "
       "にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时，逐张照片的颜色校正适应得有多快。设为 none 可保持"
            "学习率不变。"),
    ZH_HANT("訓練接近結束時，逐張照片的色彩校正適應得有多快。設為 none 可保持"
            "學習率不變。"),
    KO("학습이 끝날 무렵 사진별 색 보정이 얼마나 빨리 적응하는지입니다. none으"
       "로 두면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell sich die Farbkorrektur pro Foto gegen Ende des Trainings "
       "anpasst. Auf none setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse la correction de couleur par photo s'adapte en fin d'entraînement. "
       "Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez se adapta la corrección de color por foto al final del "
       "entrenamiento. Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez a correção de cor por foto se adapta no fim do treinamento. "
       "Defina como none para manter a taxa constante."),
    IT("Con quanta rapidità si adatta la correzione di colore per foto verso "
       "la fine dell'addestramento. Impostare a none per tenere il tasso costante."),
    NL("Hoe snel de kleurcorrectie per foto zich aan het eind van de training "
       "aanpast. Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро подстраивается покадровая цветокоррекция к концу обучения. "
       "Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru fotoğraf başına renk düzeltmesinin ne kadar hızlı "
       "uyum sağladığı. Oranı sabit tutmak için none yapın."));

SS_MSG(bilagrid_lr_warmup,
    EN("Color correction warm-up"), JA("色補正の立ち上がり"),
    ZH_HANS("颜色校正预热"), ZH_HANT("色彩校正預熱"), KO("색 보정 워밍업"),
    DE("Anlauf der Farbkorrektur"),
    FR("Montée de la correction des couleurs"),
    ES("Arranque de la corrección de color"),
    PT("Aquecimento da correção de cor"),
    IT("Avvio della correzione del colore"),
    NL("Opbouw van de kleurcorrectie"), RU("Разгон цветокоррекции"),
    TR("Renk düzeltmenin ısınması"));
SS_MSG(bilagrid_lr_warmup_help,
    EN("How many steps the per-photo color correction takes to reach full adaptation "
       "speed. Ramping in stops it from claiming color before the splats have "
       "any."),
    JA("写真ごとの色補正が最大の適応速度に達するまでのステップ数です。徐々に効"
       "かせることで、スプラットがまだ色を持たないうちに補正が色を取ってしまう"
       "のを防ぎます。"),
    ZH_HANS("逐张照片的颜色校正达到最快适应速度所需的步数。逐步加力可以避免它"
            "在泼溅还没有颜色时就先把颜色占为己有。"),
    ZH_HANT("逐張照片的色彩校正達到最快適應速度所需的步數。逐步加力可以避免它"
            "在潑濺還沒有顏色時就先把顏色占為己有。"),
    KO("사진별 색 보정이 최대 적응 속도에 이르기까지의 스텝 수입니다. 서서히 "
       "올리면 스플랫이 아직 색을 갖기 전에 보정이 색을 가져가 버리는 일을 막"
       "습니다."),
    DE("Wie viele Schritte die Farbkorrektur pro Foto braucht, um volle Anpassungsgeschwindigkeit "
       "zu erreichen. Das langsame Einblenden hindert sie daran, Farbe zu beanspruchen, "
       "bevor die Splats welche haben."),
    FR("Combien d'étapes la correction de couleur par photo met à atteindre sa "
       "pleine vitesse d'adaptation. Une montée progressive l'empêche de s'approprier "
       "la couleur avant que les splats n'en aient."),
    ES("Cuántos pasos tarda la corrección de color por foto en alcanzar su máxima "
       "velocidad de adaptación. Subirla poco a poco evita que se apropie del "
       "color antes de que los splats tengan alguno."),
    PT("Quantos passos a correção de cor por foto leva para atingir a velocidade "
       "máxima de adaptação. Subir aos poucos evita que ela tome a cor antes "
       "de os splats terem alguma."),
    IT("Quanti passi impiega la correzione di colore per foto a raggiungere la "
       "piena velocità di adattamento. Salire per gradi le impedisce di prendersi "
       "il colore prima che gli splat ne abbiano."),
    NL("Hoeveel stappen de kleurcorrectie per foto nodig heeft om op volle aanpassingssnelheid "
       "te komen. Geleidelijk opvoeren belet haar de kleur op te eisen voordat "
       "de splats die hebben."),
    RU("За сколько шагов покадровая цветокоррекция выходит на полную скорость "
       "подстройки. Постепенное включение мешает ей забрать цвет раньше, чем "
       "тот появится у сплатов."),
    TR("Fotoğraf başına renk düzeltmesinin tam uyum hızına ulaşması için gereken "
       "adım sayısı. Kademeli devreye girmek, splat'lar henüz renk kazanmadan "
       "düzeltmenin rengi sahiplenmesini engeller."));

SS_MSG(bilagrid_depth_lr,
    EN("Depth correction rate"), JA("深度補正の学習率"),
    ZH_HANS("深度校正学习率"), ZH_HANT("深度校正學習率"),
    KO("깊이 보정 학습률"), DE("Lernrate der Tiefenkorrektur"),
    FR("Taux de la correction de profondeur"),
    ES("Tasa de la corrección de profundidad"),
    PT("Taxa da correção de profundidade"),
    IT("Tasso della correzione di profondità"),
    NL("Snelheid van de dieptecorrectie"), RU("Скорость коррекции глубины"),
    TR("Derinlik düzeltme hızı"));
SS_MSG(bilagrid_depth_lr_help,
    EN("How fast the per-photo depth correction adapts. Ignored when use_adagrad_bilagrid_optim "
       "is on."),
    JA("写真ごとの深度補正がどれだけ速く適応するかです。use_adagrad_bilagrid_optim "
       "が有効なときは無視されます。"),
    ZH_HANS("逐张照片的深度校正适应得有多快。当 use_adagrad_bilagrid_optim 打"
            "开时该项被忽略。"),
    ZH_HANT("逐張照片的深度校正適應得有多快。當 use_adagrad_bilagrid_optim 開"
            "啟時該項被忽略。"),
    KO("사진별 깊이 보정이 얼마나 빨리 적응하는지입니다. use_adagrad_bilagrid_optim이"
       " 켜져 있으면 무시됩니다."),
    DE("Wie schnell sich die Tiefenkorrektur pro Foto anpasst. Wird ignoriert, "
       "wenn use_adagrad_bilagrid_optim an ist."),
    FR("À quelle vitesse la correction de profondeur par photo s'adapte. Ignoré "
       "quand use_adagrad_bilagrid_optim est activé."),
    ES("Con qué rapidez se adapta la corrección de profundidad por foto. Se ignora "
       "cuando use_adagrad_bilagrid_optim está activo."),
    PT("Com que rapidez a correção de profundidade por foto se adapta. É ignorado "
       "quando use_adagrad_bilagrid_optim está ligado."),
    IT("Con quanta rapidità si adatta la correzione di profondità per foto. Viene "
       "ignorato quando use_adagrad_bilagrid_optim è attivo."),
    NL("Hoe snel de dieptecorrectie per foto zich aanpast. Wordt genegeerd als "
       "use_adagrad_bilagrid_optim aan staat."),
    RU("Насколько быстро подстраивается покадровая коррекция глубины. Игнорируется "
       "при включённом use_adagrad_bilagrid_optim."),
    TR("Fotoğraf başına derinlik düzeltmesinin ne kadar hızlı uyum sağladığı. "
       "use_adagrad_bilagrid_optim açıkken yok sayılır."));

SS_MSG(bilagrid_depth_lr_final,
    EN("Final depth correction rate"), JA("最後の深度補正の学習率"),
    ZH_HANS("最终深度校正学习率"), ZH_HANT("最終深度校正學習率"),
    KO("최종 깊이 보정 학습률"), DE("Lernrate der Tiefenkorrektur am Ende"),
    FR("Taux final de la correction de profondeur"),
    ES("Tasa final de la corrección de profundidad"),
    PT("Taxa final da correção de profundidade"),
    IT("Tasso finale della correzione di profondità"),
    NL("Eindsnelheid van de dieptecorrectie"),
    RU("Итоговая скорость коррекции глубины"),
    TR("Son derinlik düzeltme hızı"));
SS_MSG(bilagrid_depth_lr_final_help,
    EN("How fast the per-photo depth correction adapts by the end of training. "
       "Set to none to keep the rate constant."),
    JA("学習の終わりごろに、写真ごとの深度補正がどれだけ速く適応するかです。none "
       "にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时，逐张照片的深度校正适应得有多快。设为 none 可保持"
            "学习率不变。"),
    ZH_HANT("訓練接近結束時，逐張照片的深度校正適應得有多快。設為 none 可保持"
            "學習率不變。"),
    KO("학습이 끝날 무렵 사진별 깊이 보정이 얼마나 빨리 적응하는지입니다. none으"
       "로 두면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell sich die Tiefenkorrektur pro Foto gegen Ende des Trainings "
       "anpasst. Auf none setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse la correction de profondeur par photo s'adapte en fin "
       "d'entraînement. Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez se adapta la corrección de profundidad por foto al final "
       "del entrenamiento. Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez a correção de profundidade por foto se adapta no fim "
       "do treinamento. Defina como none para manter a taxa constante."),
    IT("Con quanta rapidità si adatta la correzione di profondità per foto verso "
       "la fine dell'addestramento. Impostare a none per tenere il tasso costante."),
    NL("Hoe snel de dieptecorrectie per foto zich aan het eind van de training "
       "aanpast. Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро подстраивается покадровая коррекция глубины к концу "
       "обучения. Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru fotoğraf başına derinlik düzeltmesinin ne kadar "
       "hızlı uyum sağladığı. Oranı sabit tutmak için none yapın."));

SS_MSG(bilagrid_depth_lr_warmup,
    EN("Depth correction warm-up"), JA("深度補正の立ち上がり"),
    ZH_HANS("深度校正预热"), ZH_HANT("深度校正預熱"), KO("깊이 보정 워밍업"),
    DE("Anlauf der Tiefenkorrektur"),
    FR("Montée de la correction de profondeur"),
    ES("Arranque de la corrección de profundidad"),
    PT("Aquecimento da correção de profundidade"),
    IT("Avvio della correzione di profondità"),
    NL("Opbouw van de dieptecorrectie"), RU("Разгон коррекции глубины"),
    TR("Derinlik düzeltmenin ısınması"));
SS_MSG(bilagrid_depth_lr_warmup_help,
    EN("How many steps the per-photo depth correction takes to reach full adaptation "
       "speed."),
    JA("写真ごとの深度補正が最大の適応速度に達するまでのステップ数です。"),
    ZH_HANS("逐张照片的深度校正达到最快适应速度所需的步数。"),
    ZH_HANT("逐張照片的深度校正達到最快適應速度所需的步數。"),
    KO("사진별 깊이 보정이 최대 적응 속도에 이르기까지의 스텝 수입니다."),
    DE("Wie viele Schritte die Tiefenkorrektur pro Foto braucht, um volle Anpassungsgeschwindigkeit "
       "zu erreichen."),
    FR("Combien d'étapes la correction de profondeur par photo met à atteindre "
       "sa pleine vitesse d'adaptation."),
    ES("Cuántos pasos tarda la corrección de profundidad por foto en alcanzar "
       "su máxima velocidad de adaptación."),
    PT("Quantos passos a correção de profundidade por foto leva para atingir "
       "a velocidade máxima de adaptação."),
    IT("Quanti passi impiega la correzione di profondità per foto a raggiungere "
       "la piena velocità di adattamento."),
    NL("Hoeveel stappen de dieptecorrectie per foto nodig heeft om op volle aanpassingssnelheid "
       "te komen."),
    RU("За сколько шагов покадровая коррекция глубины выходит на полную скорость "
       "подстройки."),
    TR("Fotoğraf başına derinlik düzeltmesinin tam uyum hızına ulaşması için "
       "gereken adım sayısı."));

SS_MSG(bilagrid_normal_lr,
    EN("Normal correction rate"), JA("法線補正の学習率"),
    ZH_HANS("法线校正学习率"), ZH_HANT("法線校正學習率"),
    KO("노멀 보정 학습률"), DE("Lernrate der Normalenkorrektur"),
    FR("Taux de la correction des normales"),
    ES("Tasa de la corrección de normales"),
    PT("Taxa da correção de normais"),
    IT("Tasso della correzione delle normali"),
    NL("Snelheid van de normalencorrectie"),
    RU("Скорость коррекции нормалей"), TR("Normal düzeltme hızı"));
SS_MSG(bilagrid_normal_lr_help,
    EN("How fast the per-photo normal correction adapts. Ignored when use_adagrad_bilagrid_optim "
       "is on."),
    JA("写真ごとの法線補正がどれだけ速く適応するかです。use_adagrad_bilagrid_optim "
       "が有効なときは無視されます。"),
    ZH_HANS("逐张照片的法线校正适应得有多快。当 use_adagrad_bilagrid_optim 打"
            "开时该项被忽略。"),
    ZH_HANT("逐張照片的法線校正適應得有多快。當 use_adagrad_bilagrid_optim 開"
            "啟時該項被忽略。"),
    KO("사진별 노멀 보정이 얼마나 빨리 적응하는지입니다. use_adagrad_bilagrid_optim이"
       " 켜져 있으면 무시됩니다."),
    DE("Wie schnell sich die Normalenkorrektur pro Foto anpasst. Wird ignoriert, "
       "wenn use_adagrad_bilagrid_optim an ist."),
    FR("À quelle vitesse la correction de normales par photo s'adapte. Ignoré "
       "quand use_adagrad_bilagrid_optim est activé."),
    ES("Con qué rapidez se adapta la corrección de normales por foto. Se ignora "
       "cuando use_adagrad_bilagrid_optim está activo."),
    PT("Com que rapidez a correção de normais por foto se adapta. É ignorado "
       "quando use_adagrad_bilagrid_optim está ligado."),
    IT("Con quanta rapidità si adatta la correzione delle normali per foto. Viene "
       "ignorato quando use_adagrad_bilagrid_optim è attivo."),
    NL("Hoe snel de normalencorrectie per foto zich aanpast. Wordt genegeerd "
       "als use_adagrad_bilagrid_optim aan staat."),
    RU("Насколько быстро подстраивается покадровая коррекция нормалей. Игнорируется "
       "при включённом use_adagrad_bilagrid_optim."),
    TR("Fotoğraf başına normal düzeltmesinin ne kadar hızlı uyum sağladığı. use_adagrad_bilagrid_optim "
       "açıkken yok sayılır."));

SS_MSG(bilagrid_normal_lr_final,
    EN("Final normal correction rate"), JA("最後の法線補正の学習率"),
    ZH_HANS("最终法线校正学习率"), ZH_HANT("最終法線校正學習率"),
    KO("최종 노멀 보정 학습률"),
    DE("Lernrate der Normalenkorrektur am Ende"),
    FR("Taux final de la correction des normales"),
    ES("Tasa final de la corrección de normales"),
    PT("Taxa final da correção de normais"),
    IT("Tasso finale della correzione delle normali"),
    NL("Eindsnelheid van de normalencorrectie"),
    RU("Итоговая скорость коррекции нормалей"),
    TR("Son normal düzeltme hızı"));
SS_MSG(bilagrid_normal_lr_final_help,
    EN("How fast the per-photo normal correction adapts by the end of training. "
       "Set to none to keep the rate constant."),
    JA("学習の終わりごろに、写真ごとの法線補正がどれだけ速く適応するかです。none "
       "にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时，逐张照片的法线校正适应得有多快。设为 none 可保持"
            "学习率不变。"),
    ZH_HANT("訓練接近結束時，逐張照片的法線校正適應得有多快。設為 none 可保持"
            "學習率不變。"),
    KO("학습이 끝날 무렵 사진별 노멀 보정이 얼마나 빨리 적응하는지입니다. none으"
       "로 두면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell sich die Normalenkorrektur pro Foto gegen Ende des Trainings "
       "anpasst. Auf none setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse la correction de normales par photo s'adapte en fin "
       "d'entraînement. Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez se adapta la corrección de normales por foto al final "
       "del entrenamiento. Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez a correção de normais por foto se adapta no fim do treinamento. "
       "Defina como none para manter a taxa constante."),
    IT("Con quanta rapidità si adatta la correzione delle normali per foto verso "
       "la fine dell'addestramento. Impostare a none per tenere il tasso costante."),
    NL("Hoe snel de normalencorrectie per foto zich aan het eind van de training "
       "aanpast. Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро подстраивается покадровая коррекция нормалей к концу "
       "обучения. Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru fotoğraf başına normal düzeltmesinin ne kadar hızlı "
       "uyum sağladığı. Oranı sabit tutmak için none yapın."));

SS_MSG(bilagrid_normal_lr_warmup,
    EN("Normal correction warm-up"), JA("法線補正の立ち上がり"),
    ZH_HANS("法线校正预热"), ZH_HANT("法線校正預熱"), KO("노멀 보정 워밍업"),
    DE("Anlauf der Normalenkorrektur"),
    FR("Montée de la correction des normales"),
    ES("Arranque de la corrección de normales"),
    PT("Aquecimento da correção de normais"),
    IT("Avvio della correzione delle normali"),
    NL("Opbouw van de normalencorrectie"), RU("Разгон коррекции нормалей"),
    TR("Normal düzeltmenin ısınması"));
SS_MSG(bilagrid_normal_lr_warmup_help,
    EN("How many steps the per-photo normal correction takes to reach full adaptation "
       "speed."),
    JA("写真ごとの法線補正が最大の適応速度に達するまでのステップ数です。"),
    ZH_HANS("逐张照片的法线校正达到最快适应速度所需的步数。"),
    ZH_HANT("逐張照片的法線校正達到最快適應速度所需的步數。"),
    KO("사진별 노멀 보정이 최대 적응 속도에 이르기까지의 스텝 수입니다."),
    DE("Wie viele Schritte die Normalenkorrektur pro Foto braucht, um volle Anpassungsgeschwindigkeit "
       "zu erreichen."),
    FR("Combien d'étapes la correction de normales par photo met à atteindre "
       "sa pleine vitesse d'adaptation."),
    ES("Cuántos pasos tarda la corrección de normales por foto en alcanzar su "
       "máxima velocidad de adaptación."),
    PT("Quantos passos a correção de normais por foto leva para atingir a velocidade "
       "máxima de adaptação."),
    IT("Quanti passi impiega la correzione delle normali per foto a raggiungere "
       "la piena velocità di adattamento."),
    NL("Hoeveel stappen de normalencorrectie per foto nodig heeft om op volle "
       "aanpassingssnelheid te komen."),
    RU("За сколько шагов покадровая коррекция нормалей выходит на полную скорость "
       "подстройки."),
    TR("Fotoğraf başına normal düzeltmesinin tam uyum hızına ulaşması için gereken "
       "adım sayısı."));

SS_MSG(ppisp_lr,
    EN("Camera correction rate"), JA("カメラ補正の学習率"),
    ZH_HANS("相机校正学习率"), ZH_HANT("相機校正學習率"),
    KO("카메라 보정 학습률"), DE("Lernrate der Kamerakorrektur"),
    FR("Taux de la correction caméra"),
    ES("Tasa de la corrección de cámara"), PT("Taxa da correção de câmera"),
    IT("Tasso della correzione camera"),
    NL("Snelheid van de cameracorrectie"), RU("Скорость коррекции камеры"),
    TR("Kamera düzeltme hızı"));
SS_MSG(ppisp_lr_help,
    EN("How fast the camera-effect model adapts. Ignored when use_adagrad_ppisp_optim "
       "is on."),
    JA("カメラ効果のモデルがどれだけ速く適応するかです。use_adagrad_ppisp_optim "
       "が有効なときは無視されます。"),
    ZH_HANS("相机效应模型适应得有多快。当 use_adagrad_ppisp_optim 打开时该项被"
            "忽略。"),
    ZH_HANT("相機效應模型適應得有多快。當 use_adagrad_ppisp_optim 開啟時該項被"
            "忽略。"),
    KO("카메라 효과 모델이 얼마나 빨리 적응하는지입니다. use_adagrad_ppisp_optim이"
       " 켜져 있으면 무시됩니다."),
    DE("Wie schnell sich das Kameraeffektmodell anpasst. Wird ignoriert, wenn "
       "use_adagrad_ppisp_optim an ist."),
    FR("À quelle vitesse le modèle d'effets caméra s'adapte. Ignoré quand use_adagrad_ppisp_optim "
       "est activé."),
    ES("Con qué rapidez se adapta el modelo de efectos de cámara. Se ignora cuando "
       "use_adagrad_ppisp_optim está activo."),
    PT("Com que rapidez o modelo de efeitos de câmera se adapta. É ignorado quando "
       "use_adagrad_ppisp_optim está ligado."),
    IT("Con quanta rapidità si adatta il modello degli effetti della camera. "
       "Viene ignorato quando use_adagrad_ppisp_optim è attivo."),
    NL("Hoe snel het cameraeffectmodel zich aanpast. Wordt genegeerd als use_adagrad_ppisp_optim "
       "aan staat."),
    RU("Насколько быстро подстраивается модель эффектов камеры. Игнорируется "
       "при включённом use_adagrad_ppisp_optim."),
    TR("Kamera etkisi modelinin ne kadar hızlı uyum sağladığı. use_adagrad_ppisp_optim "
       "açıkken yok sayılır."));

SS_MSG(ppisp_lr_final,
    EN("Final camera correction rate"), JA("最後のカメラ補正の学習率"),
    ZH_HANS("最终相机校正学习率"), ZH_HANT("最終相機校正學習率"),
    KO("최종 카메라 보정 학습률"),
    DE("Lernrate der Kamerakorrektur am Ende"),
    FR("Taux final de la correction caméra"),
    ES("Tasa final de la corrección de cámara"),
    PT("Taxa final da correção de câmera"),
    IT("Tasso finale della correzione camera"),
    NL("Eindsnelheid van de cameracorrectie"),
    RU("Итоговая скорость коррекции камеры"), TR("Son kamera düzeltme hızı"));
SS_MSG(ppisp_lr_final_help,
    EN("How fast the camera-effect model adapts by the end of training. Set to "
       "none to keep the rate constant."),
    JA("学習の終わりごろに、カメラ効果のモデルがどれだけ速く適応するかです。none "
       "にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时，相机效应模型适应得有多快。设为 none 可保持学习率"
            "不变。"),
    ZH_HANT("訓練接近結束時，相機效應模型適應得有多快。設為 none 可保持學習率"
            "不變。"),
    KO("학습이 끝날 무렵 카메라 효과 모델이 얼마나 빨리 적응하는지입니다. none으"
       "로 두면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell sich das Kameraeffektmodell gegen Ende des Trainings anpasst. "
       "Auf none setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse le modèle d'effets caméra s'adapte en fin d'entraînement. "
       "Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez se adapta el modelo de efectos de cámara al final del "
       "entrenamiento. Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez o modelo de efeitos de câmera se adapta no fim do treinamento. "
       "Defina como none para manter a taxa constante."),
    IT("Con quanta rapidità si adatta il modello degli effetti della camera verso "
       "la fine dell'addestramento. Impostare a none per tenere il tasso costante."),
    NL("Hoe snel het cameraeffectmodel zich aan het eind van de training aanpast. "
       "Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро подстраивается модель эффектов камеры к концу обучения. "
       "Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru kamera etkisi modelinin ne kadar hızlı uyum sağladığı. "
       "Oranı sabit tutmak için none yapın."));

SS_MSG(ppisp_lr_warmup,
    EN("Camera correction warm-up"), JA("カメラ補正の立ち上がり"),
    ZH_HANS("相机校正预热"), ZH_HANT("相機校正預熱"),
    KO("카메라 보정 워밍업"), DE("Anlauf der Kamerakorrektur"),
    FR("Montée de la correction caméra"),
    ES("Arranque de la corrección de cámara"),
    PT("Aquecimento da correção de câmera"),
    IT("Avvio della correzione camera"), NL("Opbouw van de cameracorrectie"),
    RU("Разгон коррекции камеры"), TR("Kamera düzeltmenin ısınması"));
SS_MSG(ppisp_lr_warmup_help,
    EN("How many steps the camera-effect model takes to reach full adaptation "
       "speed. Ramping in stops it from claiming brightness before the splats "
       "have any."),
    JA("カメラ効果のモデルが最大の適応速度に達するまでのステップ数です。徐々に"
       "効かせることで、スプラットがまだ明るさを持たないうちにモデルが明るさを"
       "取ってしまうのを防ぎます。"),
    ZH_HANS("相机效应模型达到最快适应速度所需的步数。逐步加力可以避免它在泼溅"
            "还没有亮度时就先把亮度占为己有。"),
    ZH_HANT("相機效應模型達到最快適應速度所需的步數。逐步加力可以避免它在潑濺"
            "還沒有亮度時就先把亮度占為己有。"),
    KO("카메라 효과 모델이 최대 적응 속도에 이르기까지의 스텝 수입니다. 서서히"
       " 올리면 스플랫이 아직 밝기를 갖기 전에 모델이 밝기를 가져가 버리는 일"
       "을 막습니다."),
    DE("Wie viele Schritte das Kameraeffektmodell braucht, um volle Anpassungsgeschwindigkeit "
       "zu erreichen. Das langsame Einblenden hindert es daran, Helligkeit zu "
       "beanspruchen, bevor die Splats welche haben."),
    FR("Combien d'étapes le modèle d'effets caméra met à atteindre sa pleine "
       "vitesse d'adaptation. Une montée progressive l'empêche de s'approprier "
       "la luminosité avant que les splats n'en aient."),
    ES("Cuántos pasos tarda el modelo de efectos de cámara en alcanzar su máxima "
       "velocidad de adaptación. Subirlo poco a poco evita que se apropie del "
       "brillo antes de que los splats tengan alguno."),
    PT("Quantos passos o modelo de efeitos de câmera leva para atingir a velocidade "
       "máxima de adaptação. Subir aos poucos evita que ele tome o brilho antes "
       "de os splats terem algum."),
    IT("Quanti passi impiega il modello degli effetti della camera a raggiungere "
       "la piena velocità di adattamento. Salire per gradi gli impedisce di prendersi "
       "la luminosità prima che gli splat ne abbiano."),
    NL("Hoeveel stappen het cameraeffectmodel nodig heeft om op volle aanpassingssnelheid "
       "te komen. Geleidelijk opvoeren belet het de helderheid op te eisen voordat "
       "de splats die hebben."),
    RU("За сколько шагов модель эффектов камеры выходит на полную скорость подстройки. "
       "Постепенное включение мешает ей забрать яркость раньше, чем та появится "
       "у сплатов."),
    TR("Kamera etkisi modelinin tam uyum hızına ulaşması için gereken adım sayısı. "
       "Kademeli devreye girmek, splat'lar henüz parlaklık kazanmadan modelin "
       "parlaklığı sahiplenmesini engeller."));


// ===========================================================================
// Color Space
// ===========================================================================

SS_MSG(image_color_is_linear,
    EN("Input images are linear"), JA("入力画像はリニア"),
    ZH_HANS("输入图像为线性"), ZH_HANT("輸入影像為線性"),
    KO("입력 이미지가 선형"), DE("Eingabebilder sind linear"),
    FR("Images d'entrée linéaires"),
    ES("Las imágenes de entrada son lineales"),
    PT("As imagens de entrada são lineares"),
    IT("Le immagini in ingresso sono lineari"),
    NL("Invoerbeelden zijn lineair"), RU("Входные изображения линейные"),
    TR("Girdi görüntüleri doğrusal"));
SS_MSG(image_color_is_linear_help,
    EN("Treat the input images as linear light rather than ordinary display-encoded "
       "photos. `auto` reads an EXR as linear, which is what that format promises, "
       "and everything else as display-encoded; set it explicitly for an EXR whose "
       "pixels are display-encoded anyway."),
    JA("入力画像を、通常の表示用に符号化された写真ではなくリニア光として扱いま"
       "す。auto では EXR をリニア（その形式の約束）として、それ以外は表示用の"
       "符号化として読み込みます。中身が表示用に符号化された EXR では明示的に"
       "設定してください。"),
    ZH_HANS("把输入图像当作线性光，而不是普通的显示编码照片。auto 会把 EXR 按"
            "线性读取（这是该格式的约定），其他格式按显示编码；若 EXR 里存的"
            "其实是显示编码的像素，请明确设置。"),
    ZH_HANT("把輸入影像當作線性光，而不是普通的顯示編碼照片。auto 會把 EXR 依"
            "線性讀取（這是該格式的約定），其他格式依顯示編碼；若 EXR 裡存的"
            "其實是顯示編碼的像素，請明確設定。"),
    KO("입력 이미지를 일반적인 디스플레이 인코딩 사진이 아니라 선형 광으로 취"
       "급합니다. auto는 EXR을 선형으로(그 형식의 약속입니다), 그 밖의 형식은 "
       "디스플레이 인코딩으로 읽습니다. 내용이 디스플레이 인코딩인 EXR이라면 "
       "직접 설정하십시오."),
    DE("Die Eingabebilder als lineares Licht behandeln statt als übliche, für "
       "die Anzeige codierte Fotos. `auto` liest eine EXR als linear -- das sagt "
       "dieses Format zu -- und alles andere als für die Anzeige codiert; bei "
       "einer EXR mit trotzdem anzeigecodierten Pixeln ausdrücklich setzen."),
    FR("Traiter les images d'entrée comme de la lumière linéaire plutôt que comme "
       "des photos encodées pour l'affichage. « auto » lit un EXR comme linéaire, "
       "ce que ce format promet, et tout le reste comme encodé pour l'affichage ; "
       "à régler explicitement pour un EXR dont les pixels sont malgré tout "
       "encodés pour l'affichage."),
    ES("Tratar las imágenes de entrada como luz lineal en vez de fotos codificadas "
       "para pantalla. «auto» lee un EXR como lineal, que es lo que ese formato "
       "promete, y todo lo demás como codificado para pantalla; indíquelo "
       "explícitamente si un EXR guarda píxeles codificados para pantalla."),
    PT("Tratar as imagens de entrada como luz linear em vez de fotos codificadas "
       "para exibição. «auto» lê um EXR como linear, que é o que esse formato "
       "promete, e todo o resto como codificado para exibição; defina "
       "explicitamente se um EXR guardar pixels codificados para exibição."),
    IT("Trattare le immagini in ingresso come luce lineare invece che come foto "
       "codificate per la visualizzazione. «auto» legge un EXR come lineare, che è "
       "ciò che quel formato promette, e tutto il resto come codificato per la "
       "visualizzazione; impostarlo esplicitamente per un EXR che contiene comunque "
       "pixel codificati per la visualizzazione."),
    NL("De invoerbeelden als lineair licht behandelen in plaats van als gewone, "
       "voor weergave gecodeerde foto's. `auto` leest een EXR als lineair -- dat "
       "belooft dat formaat -- en al het andere als voor weergave gecodeerd; stel "
       "het uitdrukkelijk in voor een EXR met toch voor weergave gecodeerde "
       "pixels."),
    RU("Считать входные изображения линейным светом, а не обычными фотографиями "
       "с кодировкой для дисплея. «auto» читает EXR как линейный -- это обещает "
       "сам формат -- а всё остальное как закодированное для дисплея; задайте "
       "явно, если в EXR всё же лежат пиксели с кодировкой для дисплея."),
    TR("Girdi görüntülerini ekran için kodlanmış sıradan fotoğraflar yerine doğrusal "
       "ışık olarak ele alır. `auto`, bir EXR'yi doğrusal okur -- o biçimin verdiği "
       "söz budur -- diğer her şeyi ekran için kodlanmış sayar; pikselleri yine de "
       "ekran için kodlanmış bir EXR'de açıkça ayarlayın."));

SS_MSG(image_color_transfer,
    EN("Input tone curve"), JA("入力のトーンカーブ"),
    ZH_HANS("输入色调曲线"), ZH_HANT("輸入色調曲線"),
    KO("입력 톤 커브"), DE("Eingabe-Tonkurve"),
    FR("Courbe de tonalité d'entrée"), ES("Curva tonal de entrada"),
    PT("Curva tonal de entrada"), IT("Curva tonale in ingresso"),
    NL("Toonkromme van de invoer"), RU("Тоновая кривая входа"),
    TR("Girdi ton eğrisi"));
SS_MSG(image_color_transfer_help,
    EN("The curve that turns the input's light values into the picture the loss "
       "is measured on. `srgb` is the plain sRGB encode and is what an ordinary "
       "photo wants. `aces`, `filmic` and `uncharted2` roll the highlights off "
       "instead of clipping them, which is for scene-linear HDR input that goes "
       "far above 1.0. Independent of whether the input is linear -- that is "
       "`--image-color-is-linear`."),
    JA("入力の光の値を、損失を測る絵に変えるカーブです。srgb は素の sRGB 符号化"
       "で、普通の写真はこれです。aces・filmic・uncharted2 はハイライトを切り"
       "捨てずになだらかに丸めるので、1.0 をはるかに超えるシーンリニアの HDR 入力"
       "向けです。入力がリニアかどうかとは独立で、それは "
       "--image-color-is-linear です。"),
    ZH_HANS("把输入的光值变成计算损失的那幅图的曲线。srgb 是普通的 sRGB 编码，"
            "一般照片用它。aces、filmic、uncharted2 会把高光平滑压下来而不是直接"
            "截断，适合远超 1.0 的场景线性 HDR 输入。这与输入是否线性无关，那由 "
            "--image-color-is-linear 决定。"),
    ZH_HANT("把輸入的光值變成計算損失的那幅圖的曲線。srgb 是普通的 sRGB 編碼，"
            "一般照片用它。aces、filmic、uncharted2 會把高光平滑壓下來而不是直接"
            "截斷，適合遠超 1.0 的場景線性 HDR 輸入。這與輸入是否線性無關，那由 "
            "--image-color-is-linear 決定。"),
    KO("입력의 빛 값을 손실을 재는 그림으로 바꾸는 곡선입니다. srgb는 평범한 "
       "sRGB 인코딩이고 보통 사진은 이것을 씁니다. aces, filmic, uncharted2는 "
       "밝은 부분을 잘라내지 않고 완만하게 눌러 주므로 1.0을 크게 넘는 장면 선형 "
       "HDR 입력에 알맞습니다. 입력이 선형인지와는 무관하며 그것은 "
       "--image-color-is-linear 입니다."),
    DE("Die Kurve, die die Lichtwerte der Eingabe in das Bild überführt, an dem "
       "der Verlust gemessen wird. `srgb` ist die schlichte sRGB-Codierung und "
       "das, was ein gewöhnliches Foto will. `aces`, `filmic` und `uncharted2` "
       "rollen die Lichter ab, statt sie abzuschneiden -- für szenenlineare "
       "HDR-Eingaben weit über 1.0. Unabhängig davon, ob die Eingabe linear ist; "
       "das ist `--image-color-is-linear`."),
    FR("La courbe qui transforme les valeurs de lumière de l'entrée en l'image "
       "sur laquelle la perte est mesurée. « srgb » est l'encodage sRGB simple, "
       "ce que veut une photo ordinaire. « aces », « filmic » et « uncharted2 » "
       "adoucissent les hautes lumières au lieu de les écrêter, pour une entrée "
       "HDR linéaire scène bien au-dessus de 1.0. Indépendant du fait que "
       "l'entrée soit linéaire : c'est « --image-color-is-linear »."),
    ES("La curva que convierte los valores de luz de la entrada en la imagen "
       "sobre la que se mide la pérdida. «srgb» es la codificación sRGB simple, "
       "lo que quiere una foto normal. «aces», «filmic» y «uncharted2» suavizan "
       "las altas luces en vez de recortarlas, para entrada HDR lineal de escena "
       "muy por encima de 1.0. Independiente de si la entrada es lineal: eso es "
       "«--image-color-is-linear»."),
    PT("A curva que converte os valores de luz da entrada na imagem sobre a qual "
       "a perda é medida. «srgb» é a codificação sRGB simples, o que uma foto "
       "normal quer. «aces», «filmic» e «uncharted2» suavizam as altas luzes em "
       "vez de as cortar, para entrada HDR linear de cena muito acima de 1.0. "
       "Independente de a entrada ser linear: isso é «--image-color-is-linear»."),
    IT("La curva che trasforma i valori di luce dell'ingresso nell'immagine su "
       "cui si misura la perdita. «srgb» è la semplice codifica sRGB, ciò che "
       "vuole una foto normale. «aces», «filmic» e «uncharted2» addolciscono le "
       "alte luci invece di troncarle, per ingressi HDR lineari di scena molto "
       "sopra 1.0. Indipendente dal fatto che l'ingresso sia lineare: quello è "
       "«--image-color-is-linear»."),
    NL("De kromme die de lichtwaarden van de invoer omzet in het beeld waarop "
       "het verlies wordt gemeten. `srgb` is de gewone sRGB-codering en is wat "
       "een normale foto wil. `aces`, `filmic` en `uncharted2` laten de hoge "
       "lichten aflopen in plaats van ze af te kappen, voor scène-lineaire "
       "HDR-invoer ver boven 1.0. Los van of de invoer lineair is; dat is "
       "`--image-color-is-linear`."),
    RU("Кривая, превращающая значения света на входе в картинку, на которой "
       "меряется потеря. «srgb» -- обычное кодирование sRGB, то, что нужно "
       "обычной фотографии. «aces», «filmic» и «uncharted2» плавно сводят света "
       "вместо обрезки, для сценарно-линейного HDR-входа сильно выше 1.0. "
       "Независимо от того, линеен ли вход: это «--image-color-is-linear»."),
    TR("Girdinin ışık değerlerini, kaybın ölçüldüğü resme çeviren eğri. `srgb` "
       "sade sRGB kodlamasıdır ve sıradan bir fotoğrafın istediği budur. `aces`, "
       "`filmic` ve `uncharted2` parlak bölgeleri kırpmak yerine yumuşatarak "
       "indirir; 1.0'ın çok üstüne çıkan sahne-doğrusal HDR girdi içindir. "
       "Girdinin doğrusal olup olmamasından bağımsızdır, o "
       "`--image-color-is-linear`'dır."));

SS_MSG(image_color_gamut,
    EN("Input color space"), JA("入力の色空間"), ZH_HANS("输入色彩空间"),
    ZH_HANT("輸入色彩空間"), KO("입력 색 공간"), DE("Farbraum der Eingabe"),
    FR("Espace colorimétrique d'entrée"), ES("Espacio de color de entrada"),
    PT("Espaço de cor de entrada"), IT("Spazio colore in ingresso"),
    NL("Kleurruimte van de invoer"), RU("Цветовое пространство входа"),
    TR("Girdi renk uzayı"));
SS_MSG(image_color_gamut_help,
    EN("Color space the input images were captured in. `none` takes it from the "
       "file for an EXR and assumes sRGB for anything else; `Rec.709` pins sRGB "
       "primaries whatever the file says. No tone mapping is applied."),
    JA("入力画像が記録された色空間です。`none` は EXR ならファイルから読み取り、"
       "それ以外は sRGB とみなします。`Rec.709` はファイルの内容にかかわらず "
       "sRGB の原色に固定します。トーンマッピングは行いません。"),
    ZH_HANS("输入图像所记录的色彩空间。`none` 对 EXR 取自文件，其他格式按 sRGB；"
            "`Rec.709` 则不论文件如何都固定为 sRGB 原色。不会做任何色调映射。"),
    ZH_HANT("輸入影像所記錄的色彩空間。`none` 對 EXR 取自檔案，其他格式依 sRGB；"
            "`Rec.709` 則不論檔案如何都固定為 sRGB 原色。不會做任何色調映射。"),
    KO("입력 이미지가 촬영된 색 공간입니다. `none`은 EXR이면 파일에서 읽고 그 "
       "밖의 형식은 sRGB로 간주하며, `Rec.709`은 파일 내용과 상관없이 sRGB "
       "원색으로 고정합니다. 톤 매핑은 적용하지 않습니다."),
    DE("Farbraum, in dem die Eingabebilder aufgenommen wurden. `none` übernimmt "
       "ihn bei einer EXR aus der Datei und nimmt sonst sRGB an; `Rec.709` legt "
       "die sRGB-Primärfarben fest, was auch immer die Datei sagt. Es wird kein "
       "Tone Mapping angewandt."),
    FR("Espace colorimétrique dans lequel les images d'entrée ont été prises. "
       "« none » le lit dans le fichier pour un EXR et suppose sRGB pour tout le "
       "reste ; « Rec.709 » impose les primaires sRGB quoi que dise le fichier. "
       "Aucun mappage tonal n'est appliqué."),
    ES("Espacio de color en el que se capturaron las imágenes de entrada. «none» "
       "lo toma del archivo si es un EXR y supone sRGB en los demás casos; "
       "«Rec.709» fija los primarios sRGB diga lo que diga el archivo. No se "
       "aplica ningún mapeo de tonos."),
    PT("Espaço de cor em que as imagens de entrada foram capturadas. «none» "
       "lê-o do arquivo se for um EXR e assume sRGB nos demais casos; «Rec.709» "
       "fixa os primários sRGB seja o que for que o arquivo diga. Nenhum "
       "mapeamento de tons é aplicado."),
    IT("Spazio colore in cui sono state acquisite le immagini in ingresso. «none» "
       "lo prende dal file per un EXR e presume sRGB per tutto il resto; "
       "«Rec.709» fissa i primari sRGB qualunque cosa dica il file. Non viene "
       "applicato alcun tone mapping."),
    NL("Kleurruimte waarin de invoerbeelden zijn opgenomen. `none` haalt hem bij "
       "een EXR uit het bestand en gaat verder uit van sRGB; `Rec.709` legt de "
       "sRGB-primaries vast, wat het bestand ook zegt. Er wordt geen tone "
       "mapping toegepast."),
    RU("Цветовое пространство, в котором сняты входные изображения. «none» берёт "
       "его из файла для EXR и считает sRGB для всего остального; «Rec.709» "
       "закрепляет основные цвета sRGB, что бы ни было в файле. Тональная "
       "компрессия не применяется."),
    TR("Girdi görüntülerinin çekildiği renk uzayı. `none`, bir EXR için bunu "
       "dosyadan alır, diğer her şeyde sRGB varsayar; `Rec.709` ise dosya ne "
       "derse desin sRGB ana renklerini sabitler. Hiçbir ton eşlemesi "
       "uygulanmaz."));

SS_MSG(splat_color_is_linear,
    EN("Train splats in linear light"), JA("スプラットをリニアで学習"),
    ZH_HANS("在线性光下训练泼溅"), ZH_HANT("在線性光下訓練潑濺"),
    KO("선형 광으로 스플랫 학습"), DE("Splats in linearem Licht trainieren"),
    FR("Entraîner les splats en lumière linéaire"),
    ES("Entrenar los splats en luz lineal"),
    PT("Treinar os splats em luz linear"),
    IT("Addestrare gli splat in luce lineare"),
    NL("Splats in lineair licht trainen"),
    RU("Обучать сплаты в линейном свете"),
    TR("Splat'ları doğrusal ışıkta eğit"));
SS_MSG(splat_color_is_linear_help,
    EN("Store splat colors as linear light rather than as display values. Leave "
       "unset to follow the input images. Linear storage is what switches the "
       "color optimizer to its square-root parameterization and trust region; "
       "which curve the render leaves through is separate "
       "(`--splat-color-transfer`)."),
    JA("スプラットの色を表示値ではなくリニア光として保持します。未設定なら入力"
       "画像に合わせます。リニアで持つことが、色の最適化を平方根パラメータ化と"
       "信頼領域に切り替えます。レンダーが出ていくカーブは別で、"
       "--splat-color-transfer です。"),
    ZH_HANS("把泼溅颜色按线性光而不是显示值存储。不设置则跟随输入图像。以线性存"
            "储会把颜色优化切换到平方根参数化和信赖域；渲染输出经过哪条曲线是另"
            "一件事，由 --splat-color-transfer 决定。"),
    ZH_HANT("把潑濺顏色依線性光而不是顯示值儲存。不設定則跟隨輸入影像。以線性儲"
            "存會把顏色最佳化切換到平方根參數化和信賴域；算繪輸出經過哪條曲線是"
            "另一件事，由 --splat-color-transfer 決定。"),
    KO("스플랫 색을 디스플레이 값이 아니라 선형 광량으로 저장합니다. 설정하지 "
       "않으면 입력 이미지를 따릅니다. 선형 저장이 색 최적화를 제곱근 매개변수화"
       "와 신뢰 영역으로 바꿉니다. 렌더가 나가는 곡선은 별개이며 "
       "--splat-color-transfer 입니다."),
    DE("Splatfarben als lineares Licht statt als Anzeigewerte speichern. Nicht "
       "gesetzt richtet es sich nach den Eingabebildern. Lineare Speicherung "
       "schaltet die Farboptimierung auf ihre Wurzel-Parametrisierung und "
       "Vertrauensregion um; welche Kurve das Rendering verlässt, ist davon "
       "getrennt (`--splat-color-transfer`)."),
    FR("Stocker les couleurs des splats en lumière linéaire plutôt qu'en valeurs "
       "d'affichage. Non défini, suit les images d'entrée. Le stockage linéaire "
       "est ce qui bascule l'optimiseur de couleur sur sa paramétrisation en "
       "racine carrée et sa région de confiance ; la courbe par laquelle sort le "
       "rendu est séparée (« --splat-color-transfer »)."),
    ES("Guardar los colores de los splats como luz lineal en vez de como valores "
       "de pantalla. Sin definir, sigue a las imágenes de entrada. El "
       "almacenamiento lineal es lo que cambia el optimizador de color a su "
       "parametrización en raíz cuadrada y su región de confianza; por qué curva "
       "sale el render es aparte («--splat-color-transfer»)."),
    PT("Guardar as cores dos splats como luz linear em vez de valores de "
       "exibição. Sem definir, segue as imagens de entrada. O armazenamento "
       "linear é o que muda o otimizador de cor para a sua parametrização em "
       "raiz quadrada e região de confiança; por que curva o render sai é à "
       "parte («--splat-color-transfer»)."),
    IT("Memorizzare i colori degli splat come luce lineare invece che come "
       "valori di visualizzazione. Se non impostato, segue le immagini in "
       "ingresso. La memorizzazione lineare è ciò che porta l'ottimizzatore di "
       "colore alla sua parametrizzazione in radice quadrata e alla regione di "
       "fiducia; per quale curva esce il render è cosa distinta "
       "(«--splat-color-transfer»)."),
    NL("Splatkleuren als lineair licht opslaan in plaats van als weergavewaarden. "
       "Niet ingesteld volgt het de invoerbeelden. Lineaire opslag is wat de "
       "kleuroptimalisatie op haar wortelparametrisatie en vertrouwensgebied "
       "zet; via welke kromme de render eruit gaat, staat daar los van "
       "(`--splat-color-transfer`)."),
    RU("Хранить цвета сплатов как линейный свет, а не как экранные значения. Без "
       "значения следует за входными изображениями. Именно линейное хранение "
       "переводит оптимизацию цвета на корневую параметризацию и доверительную "
       "область; через какую кривую выходит рендер -- отдельный вопрос "
       "(«--splat-color-transfer»)."),
    TR("Splat renklerini ekran değerleri yerine doğrusal ışık olarak saklar. "
       "Ayarlanmazsa girdi görüntülerini izler. Doğrusal saklama, renk "
       "eniyilemesini karekök parametrelemesine ve güven bölgesine geçiren "
       "şeydir; render'ın hangi eğriden çıktığı ayrıdır "
       "(`--splat-color-transfer`)."));

SS_MSG(splat_color_transfer,
    EN("Splat tone curve"), JA("スプラットのトーンカーブ"),
    ZH_HANS("泼溅色调曲线"), ZH_HANT("潑濺色調曲線"),
    KO("스플랫 톤 커브"), DE("Splat-Tonkurve"),
    FR("Courbe de tonalité des splats"), ES("Curva tonal de los splats"),
    PT("Curva tonal dos splats"), IT("Curva tonale degli splat"),
    NL("Toonkromme van de splats"), RU("Тоновая кривая сплатов"),
    TR("Splat ton eğrisi"));
SS_MSG(splat_color_transfer_help,
    EN("The curve the render goes through before the loss compares it with the "
       "photo. `none` uses the input's, which is what keeps the render matching "
       "the photographs. `aces`, `filmic` and `uncharted2` put a highlight "
       "roll-off in front of the loss instead, so a photo that is nearly white "
       "at 0.99 can be explained by a splat many times brighter than 1.0 rather "
       "than being pinned there -- dynamic range bought back from an almost-"
       "clipped capture, at the price of having to VIEW the model the same way. "
       "`overexposure-reg` pulls the other way and should stay at 0."),
    JA("損失が写真と比べる前に、レンダーが通るカーブです。none は入力のものを使"
       "い、これがレンダーを写真に一致させます。aces・filmic・uncharted2 は代わ"
       "りに損失の手前へハイライトのロールオフを置くので、0.99 でほぼ白の写真を "
       "1.0 の何倍も明るいスプラットで説明でき、そこに張り付きません。白飛び寸前"
       "の撮影からダイナミックレンジを取り戻せる代わりに、モデルは同じ設定で表示"
       "する必要があります。overexposure-reg は逆に働くので 0 のままにしてくださ"
       "い。"),
    ZH_HANS("在损失把渲染和照片相比之前，渲染要经过的曲线。none 用输入的那条，"
            "这正是让渲染与照片一致的做法。aces、filmic、uncharted2 则在损失之前"
            "加一段高光滚降，于是 0.99 这种接近纯白的照片可以由亮度远超 1.0 的泼"
            "溅来解释，而不是被钉在 1.0——从几乎过曝的素材里换回动态范围，代价是"
            "模型必须用同样的设置来观看。overexposure-reg 的作用相反，应保持为 0。"),
    ZH_HANT("在損失把算繪和照片相比之前，算繪要經過的曲線。none 用輸入的那條，"
            "這正是讓算繪與照片一致的做法。aces、filmic、uncharted2 則在損失之前"
            "加一段高光滾降，於是 0.99 這種接近純白的照片可以由亮度遠超 1.0 的潑"
            "濺來解釋，而不是被釘在 1.0——從幾乎過曝的素材裡換回動態範圍，代價是"
            "模型必須用同樣的設定來觀看。overexposure-reg 的作用相反，應保持為 0。"),
    KO("손실이 렌더를 사진과 비교하기 전에 렌더가 지나는 곡선입니다. none은 입력"
       "의 것을 써서 렌더가 사진과 일치하게 합니다. aces, filmic, uncharted2는 "
       "대신 손실 앞에 하이라이트 롤오프를 두므로 0.99처럼 거의 흰 사진을 1.0보다 "
       "몇 배 밝은 스플랫으로 설명할 수 있고 그 값에 붙박이지 않습니다. 거의 날아"
       "간 촬영에서 다이내믹 레인지를 되찾는 대신 모델도 같은 설정으로 봐야 합니"
       "다. overexposure-reg는 반대로 작용하므로 0으로 두세요."),
    DE("Die Kurve, die das Rendering durchläuft, bevor der Verlust es mit dem "
       "Foto vergleicht. `none` nimmt die der Eingabe -- so bleibt das Rendering "
       "deckungsgleich mit den Fotos. `aces`, `filmic` und `uncharted2` setzen "
       "stattdessen einen Lichter-Abfall vor den Verlust, sodass ein bei 0.99 "
       "fast weißes Foto von einem vielfach helleren Splat erklärt werden kann, "
       "statt dort festzuhängen -- Dynamikumfang aus einer fast abgeschnittenen "
       "Aufnahme, um den Preis, das Modell ebenso ANSEHEN zu müssen. "
       "`overexposure-reg` wirkt dagegen und sollte auf 0 bleiben."),
    FR("La courbe que traverse le rendu avant que la perte ne le compare à la "
       "photo. « none » reprend celle de l'entrée, ce qui garde le rendu "
       "conforme aux photographies. « aces », « filmic » et « uncharted2 » "
       "placent au contraire une atténuation des hautes lumières avant la perte, "
       "si bien qu'une photo presque blanche à 0.99 peut être expliquée par un "
       "splat bien plus lumineux que 1.0 au lieu d'y être bloquée -- de la "
       "dynamique récupérée sur une prise presque écrêtée, au prix de devoir "
       "VISUALISER le modèle de la même façon. « overexposure-reg » tire en sens "
       "inverse et devrait rester à 0."),
    ES("La curva por la que pasa el render antes de que la pérdida lo compare "
       "con la foto. «none» usa la de la entrada, que es lo que mantiene el "
       "render igual a las fotografías. «aces», «filmic» y «uncharted2» ponen en "
       "cambio una caída de altas luces delante de la pérdida, de modo que una "
       "foto casi blanca en 0.99 puede explicarse con un splat muchas veces más "
       "brillante que 1.0 en lugar de quedar clavada ahí: rango dinámico "
       "recuperado de una toma casi recortada, a cambio de tener que VER el "
       "modelo igual. «overexposure-reg» tira en sentido contrario y conviene "
       "dejarlo en 0."),
    PT("A curva por que passa o render antes de a perda o comparar com a foto. "
       "«none» usa a da entrada, que é o que mantém o render igual às "
       "fotografias. «aces», «filmic» e «uncharted2» põem em vez disso uma queda "
       "das altas luzes antes da perda, de modo que uma foto quase branca em "
       "0.99 pode ser explicada por um splat muitas vezes mais claro do que 1.0 "
       "em vez de ficar preso ali: gama dinâmica recuperada de uma captura quase "
       "cortada, ao preço de ter de VER o modelo do mesmo modo. "
       "«overexposure-reg» puxa ao contrário e deve ficar em 0."),
    IT("La curva che il render attraversa prima che la perdita lo confronti con "
       "la foto. «none» usa quella dell'ingresso, ed è ciò che tiene il render "
       "uguale alle fotografie. «aces», «filmic» e «uncharted2» mettono invece "
       "uno smorzamento delle alte luci davanti alla perdita, così una foto "
       "quasi bianca a 0.99 può essere spiegata da uno splat molte volte più "
       "luminoso di 1.0 invece di restarvi inchiodata: gamma dinamica recuperata "
       "da una ripresa quasi troncata, al prezzo di dover GUARDARE il modello "
       "allo stesso modo. «overexposure-reg» tira in senso opposto e conviene "
       "lasciarlo a 0."),
    NL("De kromme die de render doorloopt voordat het verlies hem met de foto "
       "vergelijkt. `none` neemt die van de invoer, en dat houdt de render "
       "gelijk aan de foto's. `aces`, `filmic` en `uncharted2` zetten in plaats "
       "daarvan aflopende hoge lichten vóór het verlies, zodat een foto die bij "
       "0.99 bijna wit is verklaard kan worden door een splat vele malen "
       "helderder dan 1.0 in plaats van daar vast te zitten -- dynamisch bereik "
       "terug uit een bijna afgekapte opname, tegen de prijs dat het model ook "
       "zo BEKEKEN moet worden. `overexposure-reg` trekt de andere kant op en "
       "kan het beste 0 blijven."),
    RU("Кривая, через которую проходит рендер, прежде чем потеря сравнит его с "
       "фотографией. «none» берёт кривую входа -- именно так рендер совпадает с "
       "фотографиями. «aces», «filmic» и «uncharted2» вместо этого ставят перед "
       "потерей спад светов, так что почти белую при 0.99 фотографию может "
       "объяснить сплат во много раз ярче 1.0, а не привязанный к этой границе: "
       "динамический диапазон, возвращённый из почти пересвеченной съёмки, ценой "
       "того, что и СМОТРЕТЬ модель надо так же. «overexposure-reg» тянет в "
       "другую сторону, его лучше оставить на 0."),
    TR("Kayıp, render'ı fotoğrafla karşılaştırmadan önce render'ın geçtiği eğri. "
       "`none` girdininkini kullanır; render'ı fotoğraflarla örtüşük tutan "
       "budur. `aces`, `filmic` ve `uncharted2` ise kaybın önüne bir parlaklık "
       "yumuşatması koyar; böylece 0.99'da neredeyse beyaz olan bir fotoğraf, "
       "oraya çakılmak yerine 1.0'ın kat kat üstünde bir splat ile "
       "açıklanabilir: neredeyse kırpılmış bir çekimden geri kazanılan dinamik "
       "aralık, karşılığında modelin de aynı şekilde İZLENMESİ gerekir. "
       "`overexposure-reg` ters yönde çeker, 0'da kalmalıdır."));

SS_MSG(splat_color_gamut,
    EN("Splat color space"), JA("スプラットの色空間"),
    ZH_HANS("泼溅色彩空间"), ZH_HANT("潑濺色彩空間"), KO("스플랫 색 공간"),
    DE("Farbraum der Splats"), FR("Espace colorimétrique des splats"),
    ES("Espacio de color de los splats"), PT("Espaço de cor dos splats"),
    IT("Spazio colore degli splat"), NL("Kleurruimte van de splats"),
    RU("Цветовое пространство сплатов"), TR("Splat renk uzayı"));
SS_MSG(splat_color_gamut_help,
    EN("Color space the trained splats are stored in. Leave unset to follow the "
       "input images. A wider gamut preserves saturated colors for later grading "
       "but needs a viewer that understands it. No tone mapping is applied."),
    JA("学習したスプラットを保存する色空間です。未設定なら入力画像に合わせます。"
       "広い色域は鮮やかな色を残せるので後のグレーディングに向きますが、それを"
       "理解できるビューアが必要です。トーンマッピングは行いません。"),
    ZH_HANS("训练好的泼溅所保存的色彩空间。不设置则跟随输入图像。更宽的色域能"
            "保留饱和色彩、便于后期调色，但需要能识别它的查看器。不会做任何色"
            "调映射。"),
    ZH_HANT("訓練好的潑濺所儲存的色彩空間。不設定則跟隨輸入影像。更寬的色域能"
            "保留飽和色彩、便於後期調色，但需要能辨識它的檢視器。不會做任何色"
            "調映射。"),
    KO("학습한 스플랫을 저장할 색 공간입니다. 설정하지 않으면 입력 이미지를 따"
       "릅니다. 넓은 색역은 채도 높은 색을 남겨 후반 색보정에 유리하지만, 이를"
       " 이해하는 뷰어가 필요합니다. 톤 매핑은 적용하지 않습니다."),
    DE("Farbraum, in dem die trainierten Splats gespeichert werden. Nicht gesetzt "
       "richtet es sich nach den Eingabebildern. Ein weiterer Farbraum bewahrt "
       "gesättigte Farben für spätere Farbkorrektur, braucht aber einen Betrachter, "
       "der ihn versteht. Es wird kein Tone Mapping angewandt."),
    FR("Espace colorimétrique dans lequel les splats entraînés sont enregistrés. "
       "Non défini, suit les images d'entrée. Un gamut plus large préserve les "
       "couleurs saturées pour l'étalonnage ultérieur mais exige une visionneuse "
       "qui le comprenne. Aucun mappage tonal n'est appliqué."),
    ES("Espacio de color en el que se guardan los splats entrenados. Sin definir, "
       "sigue a las imágenes de entrada. Un gamut más amplio conserva los colores "
       "saturados para el etalonaje posterior, pero exige un visor que lo entienda. "
       "No se aplica ningún mapeo de tonos."),
    PT("Espaço de cor em que os splats treinados são guardados. Sem definir, "
       "segue as imagens de entrada. Um gamut mais amplo preserva as cores saturadas "
       "para a correção de cor posterior, mas exige um visualizador que o entenda. "
       "Nenhum mapeamento de tons é aplicado."),
    IT("Spazio colore in cui vengono salvati gli splat addestrati. Se non impostato, "
       "segue le immagini in ingresso. Un gamut più ampio conserva i colori saturi "
       "per la successiva correzione colore, ma richiede un visualizzatore che "
       "lo capisca. Non viene applicato alcun tone mapping."),
    NL("Kleurruimte waarin de getrainde splats worden opgeslagen. Niet ingesteld "
       "volgt het de invoerbeelden. Een breder gamut behoudt verzadigde kleuren "
       "voor latere grading, maar vraagt een viewer die het begrijpt. Er wordt "
       "geen tone mapping toegepast."),
    RU("Цветовое пространство, в котором сохраняются обученные сплаты. Если не "
       "задано, следует за входными изображениями. Более широкий охват сохраняет "
       "насыщенные цвета для последующей цветокоррекции, но требует просмотрщика, "
       "который его понимает. Тональная компрессия не применяется."),
    TR("Eğitilmiş splat'ların saklandığı renk uzayı. Ayarlanmazsa girdi görüntülerini "
       "izler. Geniş bir gamut, sonraki renk düzenlemesi için doygun renkleri "
       "korur ama bunu anlayan bir görüntüleyici gerektirir. Hiçbir ton eşlemesi "
       "uygulanmaz."));

SS_MSG(convert_initial_point_cloud_color,
    EN("Convert seed point colors"), JA("初期点群の色を変換"),
    ZH_HANS("转换初始点云颜色"), ZH_HANT("轉換初始點雲顏色"),
    KO("초기 포인트 색 변환"), DE("Farben der Startpunktwolke umrechnen"),
    FR("Convertir les couleurs du nuage initial"),
    ES("Convertir los colores de la nube inicial"),
    PT("Converter as cores da nuvem inicial"),
    IT("Convertire i colori della nuvola iniziale"),
    NL("Kleuren van de startpuntenwolk omzetten"),
    RU("Преобразовать цвета исходного облака"),
    TR("Başlangıç nokta bulutu renklerini dönüştür"));
SS_MSG(convert_initial_point_cloud_color_help,
    EN("Read the seed point cloud's colors as ordinary sRGB and convert them "
       "into the training color space. Turn on when starting colors look wrong "
       "in a linear or wide-gamut run."),
    JA("初期点群の色を通常の sRGB として読み、学習に使う色空間へ変換します。リ"
       "ニアや広色域の学習で最初の色がおかしいときにオンにしてください。"),
    ZH_HANS("把初始点云的颜色当作普通 sRGB 读取，并转换到训练所用的色彩空间。"
            "在线性或宽色域的训练中初始颜色不对时打开它。"),
    ZH_HANT("把初始點雲的顏色當作普通 sRGB 讀取，並轉換到訓練所用的色彩空間。"
            "在線性或廣色域的訓練中初始顏色不對時開啟它。"),
    KO("초기 포인트 클라우드의 색을 일반 sRGB로 읽어 학습 색 공간으로 변환합니"
       "다. 선형이나 광색역 학습에서 시작 색이 이상해 보이면 켜십시오."),
    DE("Die Farben der Startpunktwolke als gewöhnliches sRGB lesen und in den "
       "Trainingsfarbraum umrechnen. Einschalten, wenn die Anfangsfarben in einem "
       "linearen oder weiten Farbraum falsch aussehen."),
    FR("Lire les couleurs du nuage de points initial comme du sRGB ordinaire "
       "et les convertir vers l'espace colorimétrique d'entraînement. À activer "
       "quand les couleurs de départ semblent fausses dans un entraînement linéaire "
       "ou à gamut large."),
    ES("Leer los colores de la nube de puntos inicial como sRGB corriente y convertirlos "
       "al espacio de color del entrenamiento. Actívelo cuando los colores de "
       "partida se vean mal en una ejecución lineal o de gamut amplio."),
    PT("Ler as cores da nuvem de pontos inicial como sRGB comum e convertê-las "
       "para o espaço de cor do treinamento. Ative quando as cores iniciais parecerem "
       "erradas numa execução linear ou de gamut amplo."),
    IT("Leggere i colori della nuvola di punti iniziale come sRGB normale e convertirli "
       "nello spazio colore dell'addestramento. Da attivare quando i colori di "
       "partenza sembrano sbagliati in un'esecuzione lineare o a gamut ampio."),
    NL("De kleuren van de startpuntenwolk als gewone sRGB lezen en omzetten naar "
       "de trainingskleurruimte. Zet dit aan als de beginkleuren er in een lineaire "
       "run of met breed gamut verkeerd uitzien."),
    RU("Читать цвета исходного облака точек как обычный sRGB и переводить их "
       "в цветовое пространство обучения. Включайте, когда стартовые цвета выглядят "
       "неверно в линейном или широкоохватном запуске."),
    TR("Başlangıç nokta bulutunun renklerini sıradan sRGB olarak okur ve eğitim "
       "renk uzayına dönüştürür. Doğrusal ya da geniş gamutlu bir çalıştırmada "
       "başlangıç renkleri yanlış görünüyorsa açın."));


// ===========================================================================
// Speed & Memory
// ===========================================================================

SS_MSG(cache_images,
    EN("Image cache"), JA("画像のキャッシュ先"), ZH_HANS("图像缓存位置"),
    ZH_HANT("影像快取位置"), KO("이미지 캐시 위치"),
    DE("Bildzwischenspeicher"), FR("Cache des images"),
    ES("Caché de imágenes"), PT("Cache de imagens"),
    IT("Cache delle immagini"), NL("Beeldcache"), RU("Кэш изображений"),
    TR("Görüntü önbelleği"));
SS_MSG(cache_images_help,
    EN("Where decoded training images are kept between steps. `disk` re-reads "
       "them and uses the least memory, `cpu` keeps them in RAM for faster steps. "
       "`gpu` is not supported yet."),
    JA("デコードした学習画像をステップの間どこに置くかです。`disk` は毎回読み"
       "直すのでメモリをいちばん使いません。`cpu` は RAM に保持してステップを"
       "速くします。`gpu` はまだ対応していません。"),
    ZH_HANS("解码后的训练图像在两步之间存放在哪里。`disk` 每次重新读取，占用内"
            "存最少；`cpu` 把它们留在内存里，让每步更快；`gpu` 尚未支持。"),
    ZH_HANT("解碼後的訓練影像在兩步之間存放在哪裡。`disk` 每次重新讀取，佔用記"
            "憶體最少；`cpu` 把它們留在記憶體裡，讓每步更快；`gpu` 尚未支援。"),
    KO("디코딩한 학습 이미지를 스텝 사이에 어디에 둘지입니다. `disk`는 매번 다"
       "시 읽어 메모리를 가장 적게 쓰고, `cpu`는 RAM에 담아 스텝을 빠르게 합니"
       "다. `gpu`는 아직 지원하지 않습니다."),
    DE("Wo dekodierte Trainingsbilder zwischen den Schritten liegen. `disk` liest "
       "sie neu ein und braucht am wenigsten Speicher, `cpu` hält sie im RAM "
       "für schnellere Schritte. `gpu` wird noch nicht unterstützt."),
    FR("Où sont conservées les images d'entraînement décodées entre deux étapes. "
       "`disk` les relit et consomme le moins de mémoire, `cpu` les garde en "
       "RAM pour des étapes plus rapides. `gpu` n'est pas encore pris en charge."),
    ES("Dónde se guardan las imágenes de entrenamiento decodificadas entre pasos. "
       "`disk` vuelve a leerlas y usa la menor memoria, `cpu` las mantiene en "
       "RAM para pasos más rápidos. `gpu` todavía no está admitido."),
    PT("Onde as imagens de treinamento decodificadas ficam entre os passos. `disk` "
       "volta a lê-las e usa menos memória, `cpu` mantém-nas na RAM para passos "
       "mais rápidos. `gpu` ainda não é suportado."),
    IT("Dove restano le immagini di addestramento decodificate tra un passo e "
       "l'altro. `disk` le rilegge e usa meno memoria, `cpu` le tiene in RAM "
       "per passi più rapidi. `gpu` non è ancora supportato."),
    NL("Waar gedecodeerde trainingsbeelden tussen stappen blijven staan. `disk` "
       "leest ze opnieuw in en gebruikt het minste geheugen, `cpu` houdt ze in "
       "RAM voor snellere stappen. `gpu` wordt nog niet ondersteund."),
    RU("Где между шагами хранятся раскодированные обучающие изображения. `disk` "
       "перечитывает их и расходует меньше всего памяти, `cpu` держит их в ОЗУ "
       "ради более быстрых шагов. `gpu` пока не поддерживается."),
    TR("Kodu çözülmüş eğitim görüntülerinin adımlar arasında nerede tutulacağı. "
       "`disk` onları yeniden okur ve en az belleği kullanır, `cpu` daha hızlı "
       "adımlar için RAM'de tutar. `gpu` henüz desteklenmiyor."));

SS_MSG(max_batch_per_epoch,
    EN("Steps per pass"), JA("1 巡あたりのステップ数"),
    ZH_HANS("每轮数据的步数"), ZH_HANT("每輪資料的步數"),
    KO("한 바퀴당 스텝 수"), DE("Schritte pro Durchlauf"),
    FR("Étapes par passage"), ES("Pasos por pasada"),
    PT("Passos por passagem"), IT("Passi per passaggio"),
    NL("Stappen per doorloop"), RU("Шагов за проход"),
    TR("Geçiş başına adım"));
SS_MSG(max_batch_per_epoch_help,
    EN("Target number of steps per pass over the dataset, which decides how many "
       "images each step uses. Raising it makes each step lighter and cheaper; "
       "lowering it groups more images into a step, which is steadier but slower "
       "and needs more memory. Datasets smaller than this simply use one image "
       "per step."),
    JA("データセットを一巡する間の目標ステップ数で、1 ステップが使う画像の枚数"
       "を決めます。上げるほど 1 ステップは軽く安くなり、下げるほど 1 ステップ"
       "にまとめる画像が増えて安定しますが、遅くなりメモリも多く必要になります。"
       "これより小さいデータセットでは、単に 1 ステップ 1 枚になります。"),
    ZH_HANS("遍历一遍数据集的目标步数，它决定每步使用多少张图像。调高会让每步"
            "更轻、更省；调低则把更多图像并进一步，更稳定但更慢、更占内存。数"
            "据集小于该值时，每步就只用一张图像。"),
    ZH_HANT("走完一遍資料集的目標步數，它決定每步使用多少張影像。調高會讓每步"
            "更輕、更省；調低則把更多影像併進一步，更穩定但更慢、更佔記憶體。"
            "資料集小於該值時，每步就只用一張影像。"),
    KO("데이터셋을 한 바퀴 도는 동안의 목표 스텝 수로, 한 스텝이 쓰는 이미지 "
       "수를 정합니다. 올리면 스텝이 가볍고 저렴해지고, 내리면 한 스텝에 더 많"
       "은 이미지를 묶어 안정적이지만 느려지고 메모리를 더 씁니다. 이보다 작은"
       " 데이터셋은 그냥 스텝당 한 장을 씁니다."),
    DE("Angepeilte Zahl von Schritten pro Durchlauf durch den Datensatz; sie "
       "legt fest, wie viele Bilder jeder Schritt nutzt. Erhöhen macht jeden "
       "Schritt leichter und billiger; senken fasst mehr Bilder zu einem Schritt "
       "zusammen, was ruhiger, aber langsamer ist und mehr Speicher braucht. "
       "Kleinere Datensätze nutzen einfach ein Bild pro Schritt."),
    FR("Nombre d'étapes visé pour un passage sur le jeu de données ; il détermine "
       "combien d'images chaque étape utilise. L'augmenter allège et abaisse "
       "le coût de chaque étape ; le baisser regroupe plus d'images par étape, "
       "ce qui est plus stable mais plus lent et demande plus de mémoire. Les "
       "jeux plus petits que cela utilisent simplement une image par étape."),
    ES("Número de pasos previsto por pasada sobre el conjunto de datos; decide "
       "cuántas imágenes usa cada paso. Subirlo hace cada paso más ligero y barato; "
       "bajarlo agrupa más imágenes en un paso, lo que es más estable pero más "
       "lento y consume más memoria. Los conjuntos menores que esto usan sencillamente "
       "una imagen por paso."),
    PT("Número de passos pretendido por passagem pelo conjunto de dados; decide "
       "quantas imagens cada passo usa. Aumentá-lo torna cada passo mais leve "
       "e barato; diminuí-lo agrupa mais imagens num passo, o que é mais estável "
       "mas mais lento e usa mais memória. Conjuntos menores que isso usam simplesmente "
       "uma imagem por passo."),
    IT("Numero di passi previsto per ogni passaggio sul set di dati; decide quante "
       "immagini usa ogni passo. Alzarlo rende ogni passo più leggero ed economico; "
       "abbassarlo raggruppa più immagini in un passo, il che è più stabile ma "
       "più lento e richiede più memoria. I set più piccoli di così usano semplicemente "
       "un'immagine per passo."),
    NL("Beoogd aantal stappen per doorloop van de dataset; het bepaalt hoeveel "
       "beelden elke stap gebruikt. Verhogen maakt elke stap lichter en goedkoper; "
       "verlagen bundelt meer beelden in één stap, wat rustiger is maar trager "
       "en meer geheugen vraagt. Datasets die kleiner zijn dan dit gebruiken "
       "gewoon één beeld per stap."),
    RU("Целевое число шагов за один проход по набору данных; оно определяет, "
       "сколько изображений берёт каждый шаг. Повышение делает шаг легче и дешевле; "
       "понижение объединяет больше изображений в шаг — устойчивее, но медленнее "
       "и требует больше памяти. Наборы меньше этого просто используют одно изображение "
       "на шаг."),
    TR("Veri kümesinde bir geçiş için hedeflenen adım sayısı; her adımın kaç "
       "görüntü kullanacağını belirler. Yükseltmek her adımı hafifletir ve ucuzlatır; "
       "düşürmek bir adıma daha çok görüntü toplar, bu daha durağandır ama yavaştır "
       "ve daha çok bellek ister. Bundan küçük veri kümeleri adım başına tek "
       "görüntü kullanır."));

SS_MSG(split_batch,
    EN("One image at a time"), JA("画像を 1 枚ずつ処理"),
    ZH_HANS("每次只处理一张图像"), ZH_HANT("每次只處理一張影像"),
    KO("이미지를 한 장씩 처리"), DE("Bilder einzeln verarbeiten"),
    FR("Traiter une image à la fois"), ES("Procesar una imagen a la vez"),
    PT("Processar uma imagem por vez"),
    IT("Elaborare un'immagine alla volta"),
    NL("Eén beeld tegelijk verwerken"),
    RU("Обрабатывать по одному изображению"),
    TR("Görüntüleri teker teker işle"));
SS_MSG(split_batch_help,
    EN("Process a step's images one at a time inside the training step. Cuts "
       "peak GPU memory roughly in proportion to how many images each step uses, "
       "and gives the same result as processing them together."),
    JA("学習ステップの中で、そのステップの画像を 1 枚ずつ処理します。ピーク時"
       "の GPU メモリが、1 ステップが使う画像の枚数におおよそ比例して減り、ま"
       "とめて処理した場合と同じ結果になります。"),
    ZH_HANS("在训练步内逐张处理该步要用的图像。峰值显存大致按每步使用的图像数"
            "成比例下降，结果与一起处理完全相同。"),
    ZH_HANT("在訓練步內逐張處理該步要用的影像。峰值顯示記憶體大致按每步使用的"
            "影像數成比例下降，結果與一起處理完全相同。"),
    KO("학습 스텝 안에서 그 스텝의 이미지를 한 장씩 처리합니다. 최대 GPU 메모"
       "리가 스텝당 이미지 수에 대략 비례해 줄어들며, 한꺼번에 처리한 것과 같"
       "은 결과가 나옵니다."),
    DE("Die Bilder eines Schritts innerhalb des Trainingsschritts einzeln verarbeiten. "
       "Senkt den GPU-Speicherhöchstwert etwa proportional zur Zahl der Bilder "
       "je Schritt und liefert dasselbe Ergebnis wie die gemeinsame Verarbeitung."),
    FR("Traiter une à une les images d'une étape à l'intérieur de l'étape d'entraînement. "
       "Réduit le pic de mémoire GPU à peu près proportionnellement au nombre "
       "d'images par étape, pour un résultat identique au traitement groupé."),
    ES("Procesar una a una las imágenes de un paso dentro del paso de entrenamiento. "
       "Reduce el pico de memoria de GPU casi en proporción al número de imágenes "
       "por paso, con el mismo resultado que procesarlas juntas."),
    PT("Processar uma a uma as imagens de um passo dentro do passo de treinamento. "
       "Reduz o pico de memória da GPU quase em proporção ao número de imagens "
       "por passo, com o mesmo resultado de processá-las juntas."),
    IT("Elaborare una alla volta le immagini di un passo all'interno del passo "
       "di addestramento. Riduce il picco di memoria GPU all'incirca in proporzione "
       "al numero di immagini per passo, con lo stesso risultato dell'elaborazione "
       "insieme."),
    NL("De beelden van een stap één voor één verwerken binnen de trainingsstap. "
       "Verlaagt het GPU-geheugenpiekgebruik ongeveer evenredig met het aantal "
       "beelden per stap, met hetzelfde resultaat als samen verwerken."),
    RU("Обрабатывать изображения шага по одному внутри шага обучения. Пиковая "
       "видеопамять падает примерно пропорционально числу изображений на шаг, "
       "а результат тот же, что и при совместной обработке."),
    TR("Bir adımın görüntülerini eğitim adımının içinde teker teker işler. En "
       "yüksek GPU bellek kullanımını adım başına görüntü sayısıyla yaklaşık "
       "orantılı olarak düşürür ve birlikte işlemekle aynı sonucu verir."));

SS_MSG(use_fused_proj_bwd_optim,
    EN("Fused backward and update"), JA("逆伝播と更新をまとめる"),
    ZH_HANS("合并反向传播与参数更新"), ZH_HANT("合併反向傳播與參數更新"),
    KO("역전파와 갱신 통합"),
    DE("Rückwärtsschritt und Update zusammenfassen"),
    FR("Fusionner rétropropagation et mise à jour"),
    ES("Fusionar retropropagación y actualización"),
    PT("Fundir retropropagação e atualização"),
    IT("Unire retropropagazione e aggiornamento"),
    NL("Terugwaartse stap en update samenvoegen"),
    RU("Объединить обратный проход и обновление"),
    TR("Geri yayılım ve güncellemeyi birleştir"));
SS_MSG(use_fused_proj_bwd_optim_help,
    EN("Merge the backward pass and the parameter update into one operation. "
       "Uses noticeably less memory at large splat counts. Not available with "
       "`split_batch`, where it is silently turned off."),
    JA("逆伝播とパラメータ更新を一つの処理にまとめます。スプラット数が多いとき"
       "に目に見えてメモリが減ります。`split_batch` とは併用できず、その場合は"
       "何も告げずに無効になります。"),
    ZH_HANS("把反向传播和参数更新合并成一次操作。在泼溅数量很大时能明显省下内"
            "存。它不能和 `split_batch` 一起用，那时会被静默关闭。"),
    ZH_HANT("把反向傳播和參數更新合併成一次操作。在潑濺數量很大時能明顯省下記"
            "憶體。它不能和 `split_batch` 一起用，那時會被靜默關閉。"),
    KO("역전파와 파라미터 갱신을 하나의 연산으로 합칩니다. 스플랫이 많을 때 메"
       "모리를 눈에 띄게 아낍니다. `split_batch`와는 함께 쓸 수 없으며, 그럴 "
       "때는 아무 말 없이 꺼집니다."),
    DE("Rückwärtsschritt und Parameteraktualisierung zu einer Operation "
       "zusammenfassen. Braucht bei hohen Splat-Zahlen spürbar weniger "
       "Speicher. Mit `split_batch` nicht verfügbar und dann stillschweigend "
       "abgeschaltet."),
    FR("Fusionner la rétropropagation et la mise à jour des paramètres en une "
       "seule opération. Consomme nettement moins de mémoire quand les splats "
       "sont nombreux. Incompatible avec `split_batch`, où l'option est "
       "désactivée sans avertissement."),
    ES("Fusionar la retropropagación y la actualización de parámetros en una "
       "sola operación. Usa notablemente menos memoria con muchos splats. No "
       "está disponible con `split_batch`, donde se desactiva sin avisar."),
    PT("Fundir a retropropagação e a atualização de parâmetros numa única "
       "operação. Usa bem menos memória com muitos splats. Não está "
       "disponível com `split_batch`, onde é desligada sem aviso."),
    IT("Unire la retropropagazione e l'aggiornamento dei parametri in una sola "
       "operazione. Usa nettamente meno memoria con molti splat. Non è "
       "disponibile con `split_batch`, dove viene disattivata senza avvisare."),
    NL("De terugwaartse stap en de parameterupdate samenvoegen tot één "
       "bewerking. Gebruikt merkbaar minder geheugen bij grote "
       "splataantallen. Niet beschikbaar met `split_batch`, waar de optie "
       "zonder melding wordt uitgezet."),
    RU("Объединить обратный проход и обновление параметров в одну операцию. "
       "При большом числе сплатов заметно экономит память. С `split_batch` "
       "недоступно и в этом случае молча отключается."),
    TR("Geri yayılımı ve parametre güncellemesini tek bir işlemde birleştirir. "
       "Splat sayısı yüksekken belirgin biçimde daha az bellek kullanır. "
       "`split_batch` ile birlikte kullanılamaz; o durumda sessizce kapatılır."));

SS_MSG(packed,
    EN("Compact projection storage"), JA("投影結果をコンパクトに保存"),
    ZH_HANS("紧凑存储投影结果"), ZH_HANT("緊湊儲存投影結果"),
    KO("투영 결과를 압축 저장"), DE("Projektionsergebnisse kompakt ablegen"),
    FR("Stockage compact des projections"),
    ES("Almacenamiento compacto de las proyecciones"),
    PT("Armazenamento compacto das projeções"),
    IT("Memorizzazione compatta delle proiezioni"),
    NL("Compacte opslag van de projecties"),
    RU("Компактное хранение проекций"),
    TR("Yansıtma sonuçlarını sıkışık sakla"));
SS_MSG(packed_help,
    EN("Store projection results compactly. Cuts GPU memory when many images "
       "are processed per step, sometimes at a small speed cost."),
    JA("投影の結果をコンパクトに保存します。1 ステップで多くの画像を処理すると"
       "きに GPU メモリが減りますが、わずかに遅くなることがあります。"),
    ZH_HANS("紧凑地存储投影结果。当每步处理很多图像时能省下显存，有时会略微变"
            "慢。"),
    ZH_HANT("緊湊地儲存投影結果。當每步處理很多影像時能省下顯示記憶體，有時會"
            "略微變慢。"),
    KO("투영 결과를 압축해 저장합니다. 한 스텝에서 많은 이미지를 처리할 때 GPU "
       "메모리를 아끼지만 가끔 조금 느려집니다."),
    DE("Projektionsergebnisse kompakt ablegen. Spart GPU-Speicher, wenn viele "
       "Bilder pro Schritt verarbeitet werden, manchmal zum Preis etwas geringerer "
       "Geschwindigkeit."),
    FR("Stocker les résultats de projection de façon compacte. Réduit la mémoire "
       "GPU quand beaucoup d'images sont traitées par étape, parfois au prix "
       "d'un léger ralentissement."),
    ES("Almacenar los resultados de proyección de forma compacta. Reduce la memoria "
       "de GPU cuando se procesan muchas imágenes por paso, a veces a costa de "
       "algo de velocidad."),
    PT("Armazenar os resultados de projeção de forma compacta. Reduz a memória "
       "da GPU quando muitas imagens são processadas por passo, às vezes ao custo "
       "de um pouco de velocidade."),
    IT("Memorizzare i risultati della proiezione in modo compatto. Riduce la "
       "memoria GPU quando si elaborano molte immagini per passo, a volte al "
       "costo di un po' di velocità."),
    NL("Projectieresultaten compact opslaan. Bespaart GPU-geheugen als er per "
       "stap veel beelden worden verwerkt, soms ten koste van wat snelheid."),
    RU("Хранить результаты проекции компактно. Экономит видеопамять, когда за "
       "шаг обрабатывается много изображений, иногда ценой небольшой потери скорости."),
    TR("Yansıtma sonuçlarını sıkışık saklar. Adım başına çok görüntü işlenirken "
       "GPU belleğinden tasarruf ettirir, kimi zaman az miktarda hız kaybıyla."));

SS_MSG(bin_tile_size,
    EN("Binning tile size"), JA("ビニングタイルのサイズ"),
    ZH_HANS("分块尺寸"), ZH_HANT("分塊尺寸"), KO("타일 분할 크기"),
    DE("Größe der Einteilungskachel"),
    FR("Taille des tuiles de répartition"),
    ES("Tamaño de la tesela de reparto"),
    PT("Tamanho do bloco de distribuição"),
    IT("Dimensione del riquadro di ripartizione"),
    NL("Grootte van de indelingstegel"), RU("Размер тайла разбиения"),
    TR("Bölmeleme karosu boyutu"));
SS_MSG(bin_tile_size_help,
    EN("Edge in pixels of the tiles splats are sorted into, a power of two from "
       "8 to 128. Larger tiles produce far fewer splat-tile pairs, so sorting "
       "gets faster and uses much less memory, but each tile then rasterizes a "
       "longer list. 0 picks a size from the splat sizes actually measured, and "
       "coarsens further by itself when a batch's intersections would otherwise "
       "overflow or not fit."),
    JA("スプラットを振り分けるタイルの一辺のピクセル数で、8 から 128 までの 2 "
       "の累乗です。タイルを大きくするとスプラットとタイルの組が大幅に減り、"
       "並べ替えが速くメモリも大きく節約できますが、各タイルが処理するリスト"
       "は長くなります。0 なら実測したスプラットの大きさから選び、メモリが尽"
       "れきらないときは自動でさらに粗くします。"),
    ZH_HANS("泼溅分块排序所用瓦片的边长像素数，取 8 到 128 之间的 2 的幂。瓦片"
            "越大，泼溅与瓦片的配对数越少，排序更快、内存占用小得多，但每个瓦"
            "片要光栅化的列表更长。填 0 则依据实测的泼溅大小自动选择，并在内存"
            "装不下时自行加粗。"),
    ZH_HANT("潑濺分塊排序所用磚塊的邊長像素數，取 8 到 128 之間的 2 的冪。磚塊"
            "越大，潑濺與磚塊的配對數越少，排序更快、記憶體佔用小得多，但每個"
            "磚塊要光柵化的清單更長。填 0 則依據實測的潑濺大小自動選擇，並在記"
            "憶體裝不下時自行加粗。"),
    KO("스플랫을 나눠 담는 타일의 한 변 픽셀 수로, 8에서 128 사이의 2의 거듭제"
       "곱입니다. 타일이 클수록 스플랫과 타일의 짝이 훨씬 줄어 정렬이 빨라지고"
       " 메모리도 크게 아끼지만, 타일마다 처리할 목록은 길어집니다. 0이면 실제"
       " 측정한 스플랫 크기에서 고르고, 한 배치의 교차가 넘치거나 들어가지 않으면"
       " 스스로 더 거칠게 잡습니다."),
    DE("Kantenlänge in Pixeln der Kacheln, in die Splats einsortiert werden, "
       "eine Zweierpotenz von 8 bis 128. Größere Kacheln ergeben weit weniger "
       "Splat-Kachel-Paare, das Sortieren wird schneller und braucht viel weniger "
       "Speicher, doch jede Kachel rastert dann eine längere Liste. 0 wählt eine "
       "Größe aus den tatsächlich gemessenen Splatgrößen und vergröbert von sich "
       "aus weiter, wenn die Schnittmengen eines Stapels sonst überlaufen oder "
       "nicht hineinpassen."),
    FR("Côté en pixels des tuiles dans lesquelles les splats sont répartis, une "
       "puissance de deux de 8 à 128. De plus grandes tuiles produisent bien "
       "moins de paires splat-tuile, donc le tri devient plus rapide et occupe "
       "beaucoup moins de mémoire, mais chaque tuile rastérise alors une liste "
       "plus longue. 0 choisit une taille d'après les tailles de splats réellement "
       "mesurées et grossit encore de lui-même quand les intersections d'un lot "
       "déborderaient ou ne tiendraient pas."),
    ES("Lado en píxeles de las teselas en las que se reparten los splats, una "
       "potencia de dos de 8 a 128. Las teselas grandes producen muchos menos "
       "pares splat-tesela, así que ordenar resulta más rápido y ocupa mucha menos "
       "memoria, pero cada tesela rasteriza entonces una lista más larga. 0 elige "
       "un tamaño a partir de los tamaños de splat medidos y se vuelve más grueso "
       "por su cuenta cuando las intersecciones de un lote se desbordarían o no "
       "cabrían."),
    PT("Lado em pixels dos blocos em que os splats são distribuídos, uma potência "
       "de dois de 8 a 128. Blocos maiores produzem bem menos pares splat-bloco, "
       "então a ordenação fica mais rápida e usa muito menos memória, mas cada "
       "bloco passa a rasterizar uma lista mais longa. 0 escolhe um tamanho a "
       "partir dos tamanhos de splat medidos e engrossa ainda mais por conta "
       "própria quando as interseções de um lote transbordariam ou não caberiam."),
    IT("Lato in pixel dei riquadri in cui gli splat vengono ripartiti, una potenza "
       "di due da 8 a 128. Riquadri più grandi producono molte meno coppie "
       "splat-riquadro, quindi l'ordinamento è più veloce e occupa molta meno "
       "memoria, ma ogni riquadro rasterizza poi un elenco più lungo. 0 sceglie "
       "una dimensione dalle dimensioni degli splat effettivamente misurate e si "
       "ingrossa ancora da solo quando le intersezioni di un lotto andrebbero in "
       "overflow o non ci starebbero."),
    NL("Zijde in pixels van de tegels waarover splats worden verdeeld, een macht "
       "van twee van 8 tot 128. Grotere tegels leveren veel minder splat-tegelparen "
       "op, dus sorteren gaat sneller en kost veel minder geheugen, maar elke tegel "
       "rastert dan een langere lijst. 0 kiest een maat op grond van de werkelijk "
       "gemeten splatgroottes en wordt uit zichzelf nog grover wanneer de snijdingen "
       "van een batch anders zouden overlopen of niet zouden passen."),
    RU("Сторона в пикселях тайлов, по которым распределяются сплаты, степень "
       "двойки от 8 до 128. Крупные тайлы дают намного меньше пар «сплат — тайл», "
       "поэтому сортировка идёт быстрее и занимает гораздо меньше памяти, но "
       "каждый тайл растеризует более длинный список. 0 подбирает размер по "
       "реально измеренным размерам сплатов и сам укрупняет его дальше, когда "
       "пересечения пакета иначе переполнились бы или не поместились."),
    TR("Splatların dağıtıldığı karoların piksel cinsinden kenarı, 8 ile 128 "
       "arasında ikinin kuvveti. Büyük karolar çok daha az splat-karo çifti "
       "üretir, böylece sıralama hızlanır ve çok daha az bellek kullanır, ama her "
       "karo daha uzun bir liste tarar. 0, ölçülen splat boyutlarından bir değer "
       "seçer ve bir yığının kesişimleri taşacak ya da sığmayacak olduğunda "
       "kendiliğinden daha da kabalaşır."));
SS_MSG(quantization_level,
    EN("Color storage precision"), JA("色の保存精度"),
    ZH_HANS("颜色存储精度"), ZH_HANT("顏色儲存精度"), KO("색 저장 정밀도"),
    DE("Genauigkeit der Farbspeicherung"),
    FR("Précision de stockage des couleurs"),
    ES("Precisión de almacenamiento del color"),
    PT("Precisão de armazenamento da cor"),
    IT("Precisione di memorizzazione del colore"),
    NL("Precisie van de kleuropslag"), RU("Точность хранения цвета"),
    TR("Renk saklama duyarlığı"));
SS_MSG(quantization_level_help,
    EN("How compactly splat colors are stored during training. 1 roughly halves "
       "the memory spent on view-dependent color with little visible difference; "
       "0 keeps full precision."),
    JA("学習中にスプラットの色をどれだけコンパクトに保存するかです。1 なら視点"
       "依存の色に使うメモリがおおよそ半分になり、見た目の違いはほとんどありま"
       "せん。0 なら完全な精度を保ちます。"),
    ZH_HANS("训练期间泼溅颜色存储得有多紧凑。1 大致把视角相关颜色占用的内存减"
            "半，肉眼几乎看不出差别；0 则保持完整精度。"),
    ZH_HANT("訓練期間潑濺顏色儲存得有多緊湊。1 大致把視角相關顏色佔用的記憶體"
            "減半，肉眼幾乎看不出差別；0 則保持完整精度。"),
    KO("학습 중 스플랫 색을 얼마나 압축해 저장할지입니다. 1이면 시점 의존 색에"
       " 쓰는 메모리가 대략 절반이 되고 눈에 띄는 차이는 거의 없습니다. 0이면"
       " 완전한 정밀도를 유지합니다."),
    DE("Wie kompakt Splatfarben während des Trainings gespeichert werden. 1 halbiert "
       "den Speicher für blickabhängige Farbe ungefähr, bei kaum sichtbarem Unterschied; "
       "0 behält volle Genauigkeit."),
    FR("À quel point les couleurs des splats sont stockées de façon compacte "
       "pendant l'entraînement. 1 divise à peu près par deux la mémoire consacrée "
       "à la couleur dépendante de la vue, sans différence visible ; 0 conserve "
       "la pleine précision."),
    ES("Con cuánta compacidad se almacenan los colores de los splats durante "
       "el entrenamiento. 1 reduce a la mitad, más o menos, la memoria dedicada "
       "al color dependiente de la vista, con poca diferencia visible; 0 conserva "
       "la precisión completa."),
    PT("Com quanta compactação as cores dos splats são armazenadas durante o "
       "treinamento. 1 reduz pela metade, mais ou menos, a memória gasta com "
       "a cor dependente da vista, com pouca diferença visível; 0 mantém a precisão "
       "total."),
    IT("Con quanta compattezza vengono memorizzati i colori degli splat durante "
       "l'addestramento. 1 dimezza all'incirca la memoria spesa per il colore "
       "dipendente dalla vista, con differenza appena visibile; 0 mantiene la "
       "piena precisione."),
    NL("Hoe compact splatkleuren tijdens de training worden opgeslagen. 1 halveert "
       "ruwweg het geheugen voor kijkrichtingafhankelijke kleur, met nauwelijks "
       "zichtbaar verschil; 0 behoudt volledige precisie."),
    RU("Насколько компактно хранятся цвета сплатов во время обучения. 1 примерно "
       "вдвое сокращает память под цвет, зависящий от вида, почти без видимой "
       "разницы; 0 сохраняет полную точность."),
    TR("Eğitim sırasında splat renklerinin ne kadar sıkışık saklandığı. 1, bakışa "
       "bağlı renge harcanan belleği kabaca yarıya indirir ve gözle görülür bir "
       "fark bırakmaz; 0 tam duyarlığı korur."));

SS_MSG(preallocate_splat_tensors,
    EN("Reserve memory up front"), JA("メモリを先に確保"),
    ZH_HANS("预先分配内存"), ZH_HANT("預先配置記憶體"),
    KO("메모리 미리 확보"), DE("Speicher vorab reservieren"),
    FR("Réserver la mémoire d'avance"),
    ES("Reservar la memoria por adelantado"),
    PT("Reservar a memória antecipadamente"),
    IT("Riservare la memoria in anticipo"), NL("Geheugen vooraf reserveren"),
    RU("Резервировать память заранее"), TR("Belleği önceden ayır"));
SS_MSG(preallocate_splat_tensors_help,
    EN("Reserve memory for the maximum splat count up front. Avoids running out "
       "of GPU memory partway through as splats are added, at the cost of holding "
       "that memory from the start."),
    JA("スプラット数の上限ぶんのメモリを先に確保します。スプラットが増える途中"
       "で GPU メモリが尽きるのを避けられますが、そのメモリを最初から抱え続け"
       "ることになります。"),
    ZH_HANS("预先为泼溅数量上限分配显存。可以避免在泼溅增长途中显存耗尽，代价"
            "是从一开始就一直占着这块显存。"),
    ZH_HANT("預先為潑濺數量上限配置顯示記憶體。可以避免在潑濺增長途中顯示記憶"
            "體耗盡，代價是從一開始就一直占著這塊記憶體。"),
    KO("최대 스플랫 수만큼의 메모리를 미리 확보합니다. 스플랫이 늘어나는 도중"
       " GPU 메모리가 바닥나는 일을 막지만, 그 메모리를 처음부터 계속 쥐고 있"
       "게 됩니다."),
    DE("Speicher für die maximale Splat-Zahl vorab reservieren. Vermeidet, dass "
       "mitten im Lauf beim Hinzufügen von Splats der GPU-Speicher ausgeht, hält "
       "diesen Speicher aber von Anfang an belegt."),
    FR("Réserver d'avance la mémoire pour le nombre maximal de splats. Évite "
       "de manquer de mémoire GPU en cours de route à mesure que les splats s'ajoutent, "
       "au prix d'occuper cette mémoire dès le départ."),
    ES("Reservar por adelantado la memoria para el número máximo de splats. Evita "
       "quedarse sin memoria de GPU a mitad de camino mientras se añaden splats, "
       "a costa de ocupar esa memoria desde el principio."),
    PT("Reservar antecipadamente a memória para o número máximo de splats. Evita "
       "ficar sem memória de GPU no meio do caminho à medida que os splats são "
       "acrescentados, ao custo de ocupar essa memória desde o início."),
    IT("Riservare in anticipo la memoria per il numero massimo di splat. Evita "
       "di esaurire la memoria GPU a metà strada mentre gli splat vengono aggiunti, "
       "al costo di tenere occupata quella memoria fin dall'inizio."),
    NL("Vooraf geheugen reserveren voor het maximale aantal splats. Voorkomt "
       "dat het GPU-geheugen halverwege opraakt terwijl er splats bij komen, "
       "ten koste van het vasthouden van dat geheugen vanaf de start."),
    RU("Заранее резервировать память под максимальное число сплатов. Не даёт "
       "видеопамяти закончиться на полпути, пока сплаты добавляются, но эта память "
       "занята с самого начала."),
    TR("En büyük splat sayısı için belleği önceden ayırır. Splat'lar eklenirken "
       "yol ortasında GPU belleğinin tükenmesini önler; karşılığında o bellek "
       "en baştan tutulur."));

SS_MSG(optimizer_offload,
    EN("Offload optimizer state"), JA("オプティマイザの状態を退避"),
    ZH_HANS("把优化器状态移出显存"), ZH_HANT("把最佳化器狀態移出顯示記憶體"),
    KO("옵티마이저 상태 내리기"), DE("Optimiererzustand auslagern"),
    FR("Décharger l'état de l'optimiseur"),
    ES("Descargar el estado del optimizador"),
    PT("Descarregar o estado do otimizador"),
    IT("Scaricare lo stato dell'ottimizzatore"),
    NL("Optimizerstatus uitplaatsen"),
    RU("Выгружать состояние оптимизатора"),
    TR("İyileştirici durumunu belleğe taşı"));
SS_MSG(optimizer_offload_help,
    EN("Move optimizer state to system memory to free up GPU memory. Not supported "
       "yet."),
    JA("オプティマイザの状態をシステムメモリへ移し、GPU メモリを空けます。まだ"
       "対応していません。"),
    ZH_HANS("把优化器状态移到系统内存，以腾出显存。尚未支持。"),
    ZH_HANT("把最佳化器狀態移到系統記憶體，以騰出顯示記憶體。尚未支援。"),
    KO("옵티마이저 상태를 시스템 메모리로 옮겨 GPU 메모리를 비웁니다. 아직 지"
       "원하지 않습니다."),
    DE("Den Optimiererzustand in den Systemspeicher verschieben, um GPU-Speicher "
       "frei zu machen. Noch nicht unterstützt."),
    FR("Déplacer l'état de l'optimiseur vers la mémoire système pour libérer "
       "de la mémoire GPU. Pas encore pris en charge."),
    ES("Mover el estado del optimizador a la memoria del sistema para liberar "
       "memoria de GPU. Todavía no está admitido."),
    PT("Mover o estado do otimizador para a memória do sistema para liberar memória "
       "da GPU. Ainda não é suportado."),
    IT("Spostare lo stato dell'ottimizzatore nella memoria di sistema per liberare "
       "memoria GPU. Non ancora supportato."),
    NL("De optimizerstatus naar het systeemgeheugen verplaatsen om GPU-geheugen "
       "vrij te maken. Nog niet ondersteund."),
    RU("Переносить состояние оптимизатора в системную память, чтобы освободить "
       "видеопамять. Пока не поддерживается."),
    TR("İyileştirici durumunu sistem belleğine taşıyarak GPU belleğini boşaltır. "
       "Henüz desteklenmiyor."));

SS_MSG(use_bvh,
    EN("Spatial index (BVH)"), JA("空間インデックス（BVH）"),
    ZH_HANS("空间索引（BVH）"), ZH_HANT("空間索引（BVH）"),
    KO("공간 인덱스(BVH)"), DE("Räumlicher Index (BVH)"),
    FR("Index spatial (BVH)"), ES("Índice espacial (BVH)"),
    PT("Índice espacial (BVH)"), IT("Indice spaziale (BVH)"),
    NL("Ruimtelijke index (BVH)"), RU("Пространственный индекс (BVH)"),
    TR("Uzamsal dizin (BVH)"));
SS_MSG(use_bvh_help,
    EN("Use a spatial index for splat-tile intersection, which can help when "
       "batching many small patches. Not supported yet."),
    JA("スプラットとタイルの交差判定に空間インデックスを使います。小さなパッチ"
       "を多数まとめて処理するときに効くことがあります。まだ対応していません。"),
    ZH_HANS("在泼溅与图块的相交判断中使用空间索引。当批量处理很多小图块时可能"
            "有帮助。尚未支持。"),
    ZH_HANT("在潑濺與圖塊的相交判斷中使用空間索引。當批次處理很多小圖塊時可能"
            "有幫助。尚未支援。"),
    KO("스플랫과 타일의 교차 판정에 공간 인덱스를 씁니다. 작은 패치를 많이 묶"
       "어 처리할 때 도움이 될 수 있습니다. 아직 지원하지 않습니다."),
    DE("Einen räumlichen Index für den Splat-Kachel-Schnitt verwenden, was helfen "
       "kann, wenn viele kleine Ausschnitte gebündelt verarbeitet werden. Noch "
       "nicht unterstützt."),
    FR("Utiliser un index spatial pour l'intersection splat-tuile, ce qui peut "
       "aider quand on traite par lots beaucoup de petites zones. Pas encore "
       "pris en charge."),
    ES("Usar un índice espacial para la intersección entre splats y teselas, "
       "algo que puede ayudar al procesar por lotes muchos parches pequeños. "
       "Todavía no está admitido."),
    PT("Usar um índice espacial para a interseção entre splats e blocos, o que "
       "pode ajudar ao processar em lote muitos pedaços pequenos. Ainda não é "
       "suportado."),
    IT("Usare un indice spaziale per l'intersezione tra splat e tasselli, il "
       "che può aiutare quando si elaborano a lotti molte piccole porzioni. Non "
       "ancora supportato."),
    NL("Een ruimtelijke index gebruiken voor de doorsnede van splats en tegels, "
       "wat kan helpen bij het in batches verwerken van veel kleine stukjes. "
       "Nog niet ondersteund."),
    RU("Использовать пространственный индекс для пересечения сплатов с тайлами "
       "— это может помочь при пакетной обработке множества мелких фрагментов. "
       "Пока не поддерживается."),
    TR("Splat ile karo kesişimi için uzamsal bir dizin kullanır; çok sayıda küçük "
       "parça toplu işlenirken yardımcı olabilir. Henüz desteklenmiyor."));


// ===========================================================================
// Learning Rates
// ===========================================================================

SS_MSG(means_lr,
    EN("Movement rate"), JA("移動の学習率"), ZH_HANS("移动学习率"),
    ZH_HANT("移動學習率"), KO("이동 학습률"), DE("Lernrate der Bewegung"),
    FR("Taux de déplacement"), ES("Tasa de movimiento"),
    PT("Taxa de movimento"), IT("Tasso di movimento"),
    NL("Bewegingssnelheid"), RU("Скорость перемещения"), TR("Hareket oranı"));
SS_MSG(means_lr_help,
    EN("How fast splats move through space. Higher rearranges geometry quickly "
       "but can jitter and blur; lower stays closer to the starting point cloud."),
    JA("スプラットが空間をどれだけ速く動くかです。高いほどジオメトリが早く組み"
       "替わりますが、ぶれてぼやけることがあります。低いほど初期の点群に近いま"
       "ま残ります。"),
    ZH_HANS("泼溅在空间中移动的快慢。数值越高，几何重排越快，但可能抖动和发糊；"
            "越低则更贴近初始点云。"),
    ZH_HANT("潑濺在空間中移動的快慢。數值越高，幾何重排越快，但可能抖動和發糊；"
            "越低則更貼近初始點雲。"),
    KO("스플랫이 공간에서 얼마나 빨리 움직이는지입니다. 값이 크면 지오메트리가"
       " 빨리 재배치되지만 흔들리고 뭉개질 수 있고, 작으면 초기 포인트 클라우"
       "드에 가깝게 남습니다."),
    DE("Wie schnell sich Splats durch den Raum bewegen. Höher ordnet die Geometrie "
       "rasch um, kann aber zittern und verwischen; niedriger bleibt näher an "
       "der Startpunktwolke."),
    FR("À quelle vitesse les splats se déplacent dans l'espace. Plus haut réorganise "
       "vite la géométrie mais peut trembler et flouter ; plus bas reste proche "
       "du nuage de points de départ."),
    ES("Con qué rapidez se mueven los splats por el espacio. Más alto reordena "
       "la geometría deprisa pero puede temblar y emborronar; más bajo se queda "
       "cerca de la nube de puntos inicial."),
    PT("Com que rapidez os splats se movem pelo espaço. Mais alto reorganiza "
       "a geometria depressa, mas pode tremer e borrar; mais baixo fica perto "
       "da nuvem de pontos inicial."),
    IT("Con quanta rapidità gli splat si muovono nello spazio. Più alto riorganizza "
       "in fretta la geometria ma può tremolare e sfocare; più basso resta vicino "
       "alla nuvola di punti di partenza."),
    NL("Hoe snel splats zich door de ruimte verplaatsen. Hoger herschikt de geometrie "
       "snel maar kan trillen en vervagen; lager blijft dichter bij de beginpuntenwolk."),
    RU("Насколько быстро сплаты перемещаются в пространстве. Больше — геометрия "
       "перестраивается быстро, но может дрожать и мылить; меньше — остаётся "
       "ближе к исходному облаку точек."),
    TR("Splat'ların uzayda ne kadar hızlı hareket ettiği. Yüksek değerler geometriyi "
       "çabuk yeniden düzenler ama titreyip bulanıklaşabilir; düşük değerler "
       "başlangıç nokta bulutuna yakın kalır."));

SS_MSG(means_lr_final,
    EN("Final movement rate"), JA("最後の移動の学習率"),
    ZH_HANS("最终移动学习率"), ZH_HANT("最終移動學習率"),
    KO("최종 이동 학습률"), DE("Lernrate der Bewegung am Ende"),
    FR("Taux de déplacement final"), ES("Tasa de movimiento final"),
    PT("Taxa de movimento final"), IT("Tasso di movimento finale"),
    NL("Bewegingssnelheid aan het eind"),
    RU("Итоговая скорость перемещения"), TR("Son hareket oranı"));
SS_MSG(means_lr_final_help,
    EN("How fast splats move by the end of training. Positions ease to a stop "
       "so detail can settle. Set to none to keep the rate constant."),
    JA("学習の終わりごろにスプラットがどれだけ速く動くかです。位置がゆっくり止"
       "まっていき、細部が落ち着きます。none にすると学習率は一定のままになり"
       "ます。"),
    ZH_HANS("训练接近结束时泼溅移动的快慢。位置会缓缓停下，让细节稳定下来。设"
            "为 none 可保持学习率不变。"),
    ZH_HANT("訓練接近結束時潑濺移動的快慢。位置會緩緩停下，讓細節穩定下來。設"
            "為 none 可保持學習率不變。"),
    KO("학습이 끝날 무렵 스플랫이 얼마나 빨리 움직이는지입니다. 위치가 서서히"
       " 멈춰 디테일이 자리 잡습니다. none으로 두면 학습률이 일정하게 유지됩니"
       "다."),
    DE("Wie schnell sich Splats gegen Ende des Trainings bewegen. Die Positionen "
       "laufen sanft aus, damit sich Detail setzen kann. Auf none setzen, um "
       "die Rate konstant zu halten."),
    FR("À quelle vitesse les splats se déplacent en fin d'entraînement. Les positions "
       "s'immobilisent en douceur pour que le détail se fixe. Mettre à none pour "
       "garder un taux constant."),
    ES("Con qué rapidez se mueven los splats al final del entrenamiento. Las "
       "posiciones se detienen con suavidad para que el detalle se asiente. Póngalo "
       "en none para mantener la tasa constante."),
    PT("Com que rapidez os splats se movem no fim do treinamento. As posições "
       "param suavemente para o detalhe assentar. Defina como none para manter "
       "a taxa constante."),
    IT("Con quanta rapidità gli splat si muovono verso la fine dell'addestramento. "
       "Le posizioni si fermano dolcemente così il dettaglio si assesta. Impostare "
       "a none per tenere il tasso costante."),
    NL("Hoe snel splats aan het eind van de training bewegen. De posities komen "
       "zachtjes tot stilstand zodat detail kan bezinken. Op none zetten om de "
       "snelheid constant te houden."),
    RU("Насколько быстро сплаты движутся к концу обучения. Позиции плавно останавливаются, "
       "чтобы детали устоялись. Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru splat'ların ne kadar hızlı hareket ettiği. Konumlar "
       "yumuşakça durur ve ayrıntı oturur. Oranı sabit tutmak için none yapın."));

SS_MSG(scales_lr,
    EN("Size rate"), JA("大きさの学習率"), ZH_HANS("尺寸学习率"),
    ZH_HANT("尺寸學習率"), KO("크기 학습률"), DE("Lernrate der Größe"),
    FR("Taux de taille"), ES("Tasa de tamaño"), PT("Taxa de tamanho"),
    IT("Tasso di dimensione"), NL("Groottesnelheid"),
    RU("Скорость изменения размера"), TR("Boyut oranı"));
SS_MSG(scales_lr_help,
    EN("How fast splats change size. Higher adapts coverage quickly; lower keeps "
       "sizes near where they started."),
    JA("スプラットの大きさがどれだけ速く変わるかです。高いほど覆う範囲が素早く"
       "適応し、低いほど最初の大きさに近いまま残ります。"),
    ZH_HANS("泼溅尺寸变化的快慢。数值越高，覆盖范围适应得越快；越低则尺寸更接"
            "近起始值。"),
    ZH_HANT("潑濺尺寸變化的快慢。數值越高，覆蓋範圍適應得越快；越低則尺寸更接"
            "近起始值。"),
    KO("스플랫 크기가 얼마나 빨리 변하는지입니다. 값이 크면 덮는 범위가 빠르게"
       " 맞춰지고, 작으면 처음 크기 근처에 머무릅니다."),
    DE("Wie schnell Splats ihre Größe ändern. Höher passt die Abdeckung rasch "
       "an; niedriger hält die Größen nahe am Ausgangswert."),
    FR("À quelle vitesse les splats changent de taille. Plus haut adapte vite "
       "la couverture ; plus bas garde les tailles proches du départ."),
    ES("Con qué rapidez cambian de tamaño los splats. Más alto adapta la cobertura "
       "deprisa; más bajo mantiene los tamaños cerca de los iniciales."),
    PT("Com que rapidez os splats mudam de tamanho. Mais alto adapta a cobertura "
       "depressa; mais baixo mantém os tamanhos perto dos iniciais."),
    IT("Con quanta rapidità gli splat cambiano dimensione. Più alto adatta in "
       "fretta la copertura; più basso tiene le dimensioni vicine a quelle di "
       "partenza."),
    NL("Hoe snel splats van grootte veranderen. Hoger past de dekking snel aan; "
       "lager houdt de groottes dicht bij het beginpunt."),
    RU("Насколько быстро сплаты меняют размер. Больше — покрытие подстраивается "
       "быстро; меньше — размеры остаются близки к исходным."),
    TR("Splat'ların boyutunu ne kadar hızlı değiştirdiği. Yüksek değerler kaplamayı "
       "çabuk uyarlar; düşük değerler boyutları başlangıçtakine yakın tutar."));

SS_MSG(scales_lr_final,
    EN("Final size rate"), JA("最後の大きさの学習率"),
    ZH_HANS("最终尺寸学习率"), ZH_HANT("最終尺寸學習率"),
    KO("최종 크기 학습률"), DE("Lernrate der Größe am Ende"),
    FR("Taux de taille final"), ES("Tasa de tamaño final"),
    PT("Taxa de tamanho final"), IT("Tasso di dimensione finale"),
    NL("Groottesnelheid aan het eind"),
    RU("Итоговая скорость изменения размера"), TR("Son boyut oranı"));
SS_MSG(scales_lr_final_help,
    EN("How fast splats change size by the end of training. Set to none to keep "
       "the rate constant."),
    JA("学習の終わりごろにスプラットの大きさがどれだけ速く変わるかです。none "
       "にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时泼溅尺寸变化的快慢。设为 none 可保持学习率不变。"),
    ZH_HANT("訓練接近結束時潑濺尺寸變化的快慢。設為 none 可保持學習率不變。"),
    KO("학습이 끝날 무렵 스플랫 크기가 얼마나 빨리 변하는지입니다. none으로 두"
       "면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell Splats gegen Ende des Trainings ihre Größe ändern. Auf none "
       "setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse les splats changent de taille en fin d'entraînement. "
       "Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez cambian de tamaño los splats al final del entrenamiento. "
       "Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez os splats mudam de tamanho no fim do treinamento. Defina "
       "como none para manter a taxa constante."),
    IT("Con quanta rapidità gli splat cambiano dimensione verso la fine dell'addestramento. "
       "Impostare a none per tenere il tasso costante."),
    NL("Hoe snel splats aan het eind van de training van grootte veranderen. "
       "Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро сплаты меняют размер к концу обучения. Установите none, "
       "чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru splat'ların boyutunu ne kadar hızlı değiştirdiği. "
       "Oranı sabit tutmak için none yapın."));

SS_MSG(quats_lr,
    EN("Rotation rate"), JA("回転の学習率"), ZH_HANS("旋转学习率"),
    ZH_HANT("旋轉學習率"), KO("회전 학습률"), DE("Lernrate der Rotation"),
    FR("Taux de rotation"), ES("Tasa de rotación"), PT("Taxa de rotação"),
    IT("Tasso di rotazione"), NL("Rotatiesnelheid"), RU("Скорость вращения"),
    TR("Dönme oranı"));
SS_MSG(quats_lr_help,
    EN("How fast splats rotate, which sets how quickly they align themselves "
       "to surfaces."),
    JA("スプラットがどれだけ速く回るかで、面に沿う向きにそろうまでの速さを決め"
       "ます。"),
    ZH_HANS("泼溅旋转的快慢，它决定它们对齐到表面的速度。"),
    ZH_HANT("潑濺旋轉的快慢，它決定它們對齊到表面的速度。"),
    KO("스플랫이 얼마나 빨리 회전하는지이며, 표면에 정렬되는 속도를 좌우합니다"
       "."),
    DE("Wie schnell Splats rotieren, was bestimmt, wie rasch sie sich an Flächen "
       "ausrichten."),
    FR("À quelle vitesse les splats tournent, ce qui fixe la rapidité avec laquelle "
       "ils s'alignent sur les surfaces."),
    ES("Con qué rapidez giran los splats, lo que fija lo deprisa que se alinean "
       "con las superficies."),
    PT("Com que rapidez os splats giram, o que define a rapidez com que se alinham "
       "às superfícies."),
    IT("Con quanta rapidità gli splat ruotano, il che stabilisce quanto in fretta "
       "si allineano alle superfici."),
    NL("Hoe snel splats draaien, wat bepaalt hoe vlot ze zich naar oppervlakken "
       "richten."),
    RU("Насколько быстро сплаты вращаются — это задаёт скорость их выравнивания "
       "по поверхностям."),
    TR("Splat'ların ne kadar hızlı döndüğü; bu, yüzeylere ne kadar çabuk hizalandıklarını "
       "belirler."));

SS_MSG(opacities_lr,
    EN("Opacity rate"), JA("不透明度の学習率"), ZH_HANS("不透明度学习率"),
    ZH_HANT("不透明度學習率"), KO("불투명도 학습률"),
    DE("Lernrate der Deckkraft"), FR("Taux d'opacité"),
    ES("Tasa de opacidad"), PT("Taxa de opacidade"), IT("Tasso di opacità"),
    NL("Dekkingssnelheid"), RU("Скорость изменения непрозрачности"),
    TR("Saydamsızlık oranı"));
SS_MSG(opacities_lr_help,
    EN("How fast splat transparency changes. Higher clears haze and prunes faint "
       "splats sooner; lower gives weak structure more time to prove itself."),
    JA("スプラットの透明度がどれだけ速く変わるかです。高いほどもやが早く消え、"
       "薄いスプラットも早く整理されます。低いほど弱い構造に自分を証明する時間"
       "が与えられます。"),
    ZH_HANS("泼溅透明度变化的快慢。数值越高，雾感消散和淡泼溅的清除越早；越低"
            "则给弱结构更多时间证明自己。"),
    ZH_HANT("潑濺透明度變化的快慢。數值越高，霧感消散和淡潑濺的清除越早；越低"
            "則給弱結構更多時間證明自己。"),
    KO("스플랫 투명도가 얼마나 빨리 변하는지입니다. 값이 크면 안개가 빨리 걷히"
       "고 흐린 스플랫이 일찍 정리되며, 작으면 약한 구조에 스스로를 증명할 시"
       "간이 더 주어집니다."),
    DE("Wie schnell sich die Transparenz der Splats ändert. Höher räumt Schleier "
       "weg und entfernt blasse Splats früher; niedriger gibt schwacher Struktur "
       "mehr Zeit, sich zu bewähren."),
    FR("À quelle vitesse la transparence des splats change. Plus haut dissipe "
       "le voile et élague plus tôt les splats pâles ; plus bas laisse aux structures "
       "faibles le temps de faire leurs preuves."),
    ES("Con qué rapidez cambia la transparencia de los splats. Más alto disipa "
       "la neblina y poda antes los splats tenues; más bajo da a la estructura "
       "débil más tiempo para demostrar su valía."),
    PT("Com que rapidez a transparência dos splats muda. Mais alto dissipa a "
       "névoa e poda mais cedo os splats fracos; mais baixo dá à estrutura frágil "
       "mais tempo para se provar."),
    IT("Con quanta rapidità cambia la trasparenza degli splat. Più alto dissolve "
       "la foschia e pota prima gli splat deboli; più basso lascia alle strutture "
       "fragili più tempo per dimostrare il proprio valore."),
    NL("Hoe snel de doorzichtigheid van splats verandert. Hoger ruimt waas op "
       "en snoeit vage splats eerder weg; lager geeft zwakke structuur meer tijd "
       "om zich te bewijzen."),
    RU("Насколько быстро меняется прозрачность сплатов. Больше — дымка уходит "
       "и бледные сплаты вычищаются раньше; меньше — слабой структуре даётся "
       "больше времени показать себя."),
    TR("Splat saydamlığının ne kadar hızlı değiştiği. Yüksek değerler pusu giderir "
       "ve soluk splat'ları erken budar; düşük değerler zayıf yapıya kendini "
       "kanıtlaması için daha çok süre tanır."));

SS_MSG(features_dc_lr,
    EN("Base color rate"), JA("基本色の学習率"), ZH_HANS("基础颜色学习率"),
    ZH_HANT("基礎顏色學習率"), KO("기본 색 학습률"),
    DE("Lernrate der Grundfarbe"), FR("Taux de la couleur de base"),
    ES("Tasa del color base"), PT("Taxa da cor base"),
    IT("Tasso del colore di base"), NL("Snelheid van de basiskleur"),
    RU("Скорость изменения базового цвета"), TR("Temel renk oranı"));
SS_MSG(features_dc_lr_help,
    EN("How fast the base color of a splat changes."),
    JA("スプラットの基本色がどれだけ速く変わるかです。"),
    ZH_HANS("泼溅基础颜色变化的快慢。"), ZH_HANT("潑濺基礎顏色變化的快慢。"),
    KO("스플랫의 기본 색이 얼마나 빨리 변하는지입니다."),
    DE("Wie schnell sich die Grundfarbe eines Splats ändert."),
    FR("À quelle vitesse la couleur de base d'un splat change."),
    ES("Con qué rapidez cambia el color base de un splat."),
    PT("Com que rapidez a cor base de um splat muda."),
    IT("Con quanta rapidità cambia il colore di base di uno splat."),
    NL("Hoe snel de basiskleur van een splat verandert."),
    RU("Насколько быстро меняется базовый цвет сплата."),
    TR("Bir splat'ın temel renginin ne kadar hızlı değiştiği."));

SS_MSG(features_sh_lr,
    EN("View-dependent color rate"), JA("視点依存色の学習率"),
    ZH_HANS("视角相关颜色学习率"), ZH_HANT("視角相關顏色學習率"),
    KO("시점 의존 색 학습률"), DE("Lernrate der blickabhängigen Farbe"),
    FR("Taux de la couleur dépendante de la vue"),
    ES("Tasa del color dependiente de la vista"),
    PT("Taxa da cor dependente da vista"),
    IT("Tasso del colore dipendente dalla vista"),
    NL("Snelheid van de kijkrichtingafhankelijke kleur"),
    RU("Скорость изменения цвета, зависящего от вида"),
    TR("Bakışa bağlı renk oranı"));
SS_MSG(features_sh_lr_help,
    EN("How fast view-dependent color changes. Kept well below the base color "
       "rate so reflections do not run away with the color."),
    JA("視点によって変わる色がどれだけ速く変わるかです。反射が色を持ち去らない"
       "よう、基本色の学習率よりかなり低く保たれます。"),
    ZH_HANS("视角相关颜色变化的快慢。它被保持在远低于基础颜色的学习率，以免反"
            "射把颜色带跑。"),
    ZH_HANT("視角相關顏色變化的快慢。它被保持在遠低於基礎顏色的學習率，以免反"
            "射把顏色帶跑。"),
    KO("시점에 따라 달라지는 색이 얼마나 빨리 변하는지입니다. 반사가 색을 끌고"
       " 가지 않도록 기본 색 학습률보다 훨씬 낮게 유지합니다."),
    DE("Wie schnell sich die blickabhängige Farbe ändert. Wird deutlich unter "
       "der Rate der Grundfarbe gehalten, damit Reflexe die Farbe nicht davontragen."),
    FR("À quelle vitesse la couleur dépendante de la vue change. Maintenue bien "
       "en dessous du taux de la couleur de base pour que les reflets n'emportent "
       "pas la couleur."),
    ES("Con qué rapidez cambia el color dependiente de la vista. Se mantiene "
       "muy por debajo de la tasa del color base para que los reflejos no se "
       "lleven el color."),
    PT("Com que rapidez a cor dependente da vista muda. Mantida bem abaixo da "
       "taxa da cor base para que os reflexos não levem a cor embora."),
    IT("Con quanta rapidità cambia il colore dipendente dalla vista. Tenuto ben "
       "al di sotto del tasso del colore di base perché i riflessi non portino "
       "via il colore."),
    NL("Hoe snel de kijkrichtingafhankelijke kleur verandert. Wordt ruim onder "
       "de snelheid van de basiskleur gehouden zodat reflecties er niet met de "
       "kleur vandoor gaan."),
    RU("Насколько быстро меняется цвет, зависящий от вида. Держится заметно ниже "
       "скорости базового цвета, чтобы блики не уносили цвет с собой."),
    TR("Bakışa bağlı rengin ne kadar hızlı değiştiği. Yansımaların rengi alıp "
       "götürmemesi için temel renk oranının epey altında tutulur."));

SS_MSG(background_dc_lr,
    EN("Skybox base color rate"), JA("スカイボックス基本色の学習率"),
    ZH_HANS("天空盒基础颜色学习率"), ZH_HANT("天空盒基礎顏色學習率"),
    KO("스카이박스 기본 색 학습률"), DE("Lernrate der Skybox-Grundfarbe"),
    FR("Taux de la couleur de base du skybox"),
    ES("Tasa del color base del skybox"), PT("Taxa da cor base do skybox"),
    IT("Tasso del colore di base dello skybox"),
    NL("Snelheid van de basiskleur van de skybox"),
    RU("Скорость базового цвета скайбокса"),
    TR("Gökyüzü kutusu temel renk oranı"));
SS_MSG(background_dc_lr_help,
    EN("How fast the skybox base color changes. Only used with the `sh` background."),
    JA("スカイボックスの基本色がどれだけ速く変わるかです。背景が `sh` のときだ"
       "け使われます。"),
    ZH_HANS("天空盒基础颜色变化的快慢。仅在背景为 `sh` 时使用。"),
    ZH_HANT("天空盒基礎顏色變化的快慢。僅在背景為 `sh` 時使用。"),
    KO("스카이박스의 기본 색이 얼마나 빨리 변하는지입니다. 배경이 `sh`일 때만"
       " 쓰입니다."),
    DE("Wie schnell sich die Grundfarbe der Skybox ändert. Wird nur mit dem Hintergrund "
       "`sh` verwendet."),
    FR("À quelle vitesse la couleur de base du skybox change. Utilisé uniquement "
       "avec l'arrière-plan `sh`."),
    ES("Con qué rapidez cambia el color base del skybox. Solo se usa con el fondo "
       "`sh`."),
    PT("Com que rapidez a cor base do skybox muda. Só é usado com o fundo `sh`."),
    IT("Con quanta rapidità cambia il colore di base dello skybox. Usato solo "
       "con lo sfondo `sh`."),
    NL("Hoe snel de basiskleur van de skybox verandert. Wordt alleen bij achtergrond "
       "`sh` gebruikt."),
    RU("Насколько быстро меняется базовый цвет скайбокса. Используется только "
       "с фоном `sh`."),
    TR("Gökyüzü kutusunun temel renginin ne kadar hızlı değiştiği. Yalnızca `sh` "
       "arka planıyla kullanılır."));

SS_MSG(background_sh_lr,
    EN("Skybox detail rate"), JA("スカイボックスの細部の学習率"),
    ZH_HANS("天空盒细节学习率"), ZH_HANT("天空盒細節學習率"),
    KO("스카이박스 디테일 학습률"), DE("Lernrate des Skybox-Details"),
    FR("Taux du détail du skybox"), ES("Tasa del detalle del skybox"),
    PT("Taxa do detalhe do skybox"), IT("Tasso del dettaglio dello skybox"),
    NL("Snelheid van het skyboxdetail"),
    RU("Скорость детализации скайбокса"), TR("Gökyüzü kutusu ayrıntı oranı"));
SS_MSG(background_sh_lr_help,
    EN("How fast skybox detail changes. Only used with the `sh` background."),
    JA("スカイボックスの細部がどれだけ速く変わるかです。背景が `sh` のときだけ"
       "使われます。"),
    ZH_HANS("天空盒细节变化的快慢。仅在背景为 `sh` 时使用。"),
    ZH_HANT("天空盒細節變化的快慢。僅在背景為 `sh` 時使用。"),
    KO("스카이박스의 디테일이 얼마나 빨리 변하는지입니다. 배경이 `sh`일 때만 "
       "쓰입니다."),
    DE("Wie schnell sich das Detail der Skybox ändert. Wird nur mit dem Hintergrund "
       "`sh` verwendet."),
    FR("À quelle vitesse le détail du skybox change. Utilisé uniquement avec "
       "l'arrière-plan `sh`."),
    ES("Con qué rapidez cambia el detalle del skybox. Solo se usa con el fondo "
       "`sh`."),
    PT("Com que rapidez o detalhe do skybox muda. Só é usado com o fundo `sh`."),
    IT("Con quanta rapidità cambia il dettaglio dello skybox. Usato solo con "
       "lo sfondo `sh`."),
    NL("Hoe snel het detail van de skybox verandert. Wordt alleen bij achtergrond "
       "`sh` gebruikt."),
    RU("Насколько быстро меняется детализация скайбокса. Используется только "
       "с фоном `sh`."),
    TR("Gökyüzü kutusu ayrıntısının ne kadar hızlı değiştiği. Yalnızca `sh` arka "
       "planıyla kullanılır."));

SS_MSG(max_steps,
    EN("Learning-rate schedule length"), JA("学習率スケジュールの長さ"),
    ZH_HANS("学习率调度长度"), ZH_HANT("學習率排程長度"),
    KO("학습률 스케줄 길이"), DE("Länge des Lernratenplans"),
    FR("Longueur du calendrier des taux"),
    ES("Longitud del calendario de tasas"),
    PT("Duração do cronograma de taxas"),
    IT("Durata della pianificazione dei tassi"),
    NL("Lengte van het leersnelheidsschema"),
    RU("Длина расписания скоростей обучения"),
    TR("Öğrenme oranı çizelgesinin uzunluğu"));
SS_MSG(max_steps_help,
    EN("Length of the learning-rate schedule, in steps. Leave unset to match "
       "num_iterations. Set it to keep the rates on their usual curve when training "
       "longer or shorter than the schedule was designed for."),
    JA("学習率スケジュールの長さをステップ数で指定します。未設定なら num_iterations "
       "に合わせます。スケジュールが想定した長さより長く、または短く学習すると"
       "きに、学習率をいつもの曲線に沿わせたい場合に設定してください。"),
    ZH_HANS("学习率调度的长度，以步数计。不设置则与 num_iterations 一致。当训"
            "练时长比调度设计的更长或更短、而你仍希望学习率沿着常规曲线变化时，"
            "可以设置它。"),
    ZH_HANT("學習率排程的長度，以步數計。不設定則與 num_iterations 一致。當訓"
            "練時長比排程設計的更長或更短、而你仍希望學習率沿著常規曲線變化時，"
            "可以設定它。"),
    KO("학습률 스케줄의 길이를 스텝 수로 지정합니다. 설정하지 않으면 num_iterations를"
       " 따릅니다. 스케줄이 상정한 것보다 길거나 짧게 학습하면서도 학습률을 원"
       "래 곡선대로 두고 싶을 때 설정하십시오."),
    DE("Länge des Lernratenplans in Schritten. Nicht gesetzt entspricht er num_iterations. "
       "Setzen, um die Raten auf ihrer gewohnten Kurve zu halten, wenn länger "
       "oder kürzer trainiert wird, als der Plan vorsah."),
    FR("Longueur du calendrier des taux d'apprentissage, en étapes. Non défini, "
       "il suit num_iterations. À définir pour garder les taux sur leur courbe "
       "habituelle quand l'entraînement dure plus ou moins longtemps que prévu "
       "par le calendrier."),
    ES("Longitud del calendario de tasas de aprendizaje, en pasos. Sin definir, "
       "coincide con num_iterations. Defínalo para mantener las tasas en su curva "
       "habitual cuando se entrena más o menos tiempo del previsto por el calendario."),
    PT("Duração do cronograma de taxas de aprendizado, em passos. Sem definir, "
       "acompanha num_iterations. Defina-o para manter as taxas na curva habitual "
       "quando o treinamento for mais longo ou mais curto do que o cronograma "
       "previa."),
    IT("Durata della pianificazione dei tassi di apprendimento, in passi. Se "
       "non impostata, segue num_iterations. Da impostare per tenere i tassi "
       "sulla curva abituale quando si addestra più a lungo o più brevemente "
       "di quanto la pianificazione prevedeva."),
    NL("Lengte van het leersnelheidsschema, in stappen. Niet ingesteld volgt "
       "het num_iterations. Stel het in om de snelheden op hun gebruikelijke "
       "curve te houden als er langer of korter wordt getraind dan het schema "
       "bedoeld was."),
    RU("Длина расписания скоростей обучения в шагах. Если не задана, совпадает "
       "с num_iterations. Задайте, чтобы скорости шли по своей обычной кривой, "
       "когда обучение длиннее или короче, чем предполагало расписание."),
    TR("Öğrenme oranı çizelgesinin adım cinsinden uzunluğu. Ayarlanmazsa num_iterations "
       "ile eşleşir. Çizelgenin öngördüğünden daha uzun ya da kısa eğitirken "
       "oranları alışılmış eğrisinde tutmak için ayarlayın."));

SS_MSG(use_scale_agnostic_mean,
    EN("Scene-scale independent movement"),
    JA("シーンの大きさに依らない移動速度"),
    ZH_HANS("移动速度不受场景尺度影响"), ZH_HANT("移動速度不受場景尺度影響"),
    KO("장면 크기와 무관한 이동 속도"),
    DE("Bewegung unabhängig von der Szenengröße"),
    FR("Déplacement indépendant de l'échelle de la scène"),
    ES("Movimiento independiente de la escala de la escena"),
    PT("Movimento independente da escala da cena"),
    IT("Movimento indipendente dalla scala della scena"),
    NL("Beweging onafhankelijk van de schaal van de scène"),
    RU("Перемещение независимо от масштаба сцены"),
    TR("Sahne ölçeğinden bağımsız hareket"));
SS_MSG(use_scale_agnostic_mean_help,
    EN("Make how fast splats move independent of how large the scene is, so one "
       "setting works across datasets. Turn off to have it scale with the scene, "
       "matching the original 3DGS behavior."),
    JA("スプラットが動く速さを、シーンの大きさに左右されないようにします。ひと"
       "つの設定がどのデータセットでも通用するようになります。オフにするとシー"
       "ンの大きさに比例し、もとの 3DGS の振る舞いに揃います。"),
    ZH_HANS("让泼溅移动的快慢与场景大小无关，这样同一套设置就能通用于各个数据"
            "集。关闭后速度会随场景尺度变化，与原始 3DGS 的行为一致。"),
    ZH_HANT("讓潑濺移動的快慢與場景大小無關，這樣同一套設定就能通用於各個資料"
            "集。關閉後速度會隨場景尺度變化，與原始 3DGS 的行為一致。"),
    KO("스플랫이 움직이는 속도를 장면 크기와 무관하게 만듭니다. 그러면 한 설정"
       "이 어느 데이터셋에서나 통합니다. 끄면 장면 크기에 비례해 원래 3DGS의 "
       "동작과 같아집니다."),
    DE("Die Bewegungsgeschwindigkeit der Splats unabhängig von der Szenengröße "
       "machen, sodass eine Einstellung über Datensätze hinweg passt. Abgeschaltet "
       "skaliert sie mit der Szene und entspricht dem ursprünglichen 3DGS-Verhalten."),
    FR("Rendre la vitesse de déplacement des splats indépendante de la taille "
       "de la scène, pour qu'un même réglage vaille pour tous les jeux de données. "
       "Décoché, elle suit l'échelle de la scène, comme le 3DGS d'origine."),
    ES("Hacer que la rapidez con que se mueven los splats no dependa del tamaño "
       "de la escena, para que un mismo ajuste sirva en todos los conjuntos. "
       "Sin marcar, escala con la escena, igual que el 3DGS original."),
    PT("Fazer com que a rapidez com que os splats se movem não dependa do tamanho "
       "da cena, para que um mesmo ajuste sirva em todos os conjuntos. Desmarcado, "
       "escala com a cena, como o 3DGS original."),
    IT("Rendere la velocità di spostamento degli splat indipendente dalla dimensione "
       "della scena, così una sola impostazione vale per tutti i set di dati. "
       "Deselezionato scala con la scena, come il 3DGS originale."),
    NL("De snelheid waarmee splats bewegen losmaken van de omvang van de scène, "
       "zodat één instelling voor alle datasets werkt. Uitgevinkt schaalt ze "
       "mee met de scène, net als het oorspronkelijke 3DGS."),
    RU("Сделать скорость перемещения сплатов независимой от размера сцены, чтобы "
       "одна настройка подходила для любых наборов. Без флажка она масштабируется "
       "со сценой, как в исходном 3DGS."),
    TR("Splat'ların hareket hızını sahne boyutundan bağımsız kılar; böylece tek "
       "bir ayar tüm veri kümelerinde işe yarar. Kapatılırsa hız sahneyle ölçeklenir "
       "ve özgün 3DGS davranışına uyar."));

SS_MSG(use_per_splat_bias_correction,
    EN("Fresh start for new splats"), JA("新しいスプラットを初期状態から"),
    ZH_HANS("让新泼溅从零开始"), ZH_HANT("讓新潑濺從零開始"),
    KO("새 스플랫은 처음부터"), DE("Neustart für neue Splats"),
    FR("Départ à neuf pour les nouveaux splats"),
    ES("Comienzo limpio para los splats nuevos"),
    PT("Recomeço para os novos splats"),
    IT("Partenza da zero per i nuovi splat"),
    NL("Frisse start voor nieuwe splats"),
    RU("Новый старт для новых сплатов"),
    TR("Yeni splat'lara temiz başlangıç"));
SS_MSG(use_per_splat_bias_correction_help,
    EN("Give newly created splats a fresh start in the optimizer. They then move "
       "at full speed instead of inheriting the momentum of the splat they came "
       "from, so new detail sharpens up faster."),
    JA("新しく作られたスプラットに、オプティマイザ上で新しい出発点を与えます。"
       "元になったスプラットの勢いを引き継がず最初から全速で動けるので、新しい"
       "細部が早くくっきりします。"),
    ZH_HANS("让新创建的泼溅在优化器中从零开始。它们不会继承来源泼溅的动量，而"
            "是一开始就全速移动，因此新的细节能更快变清晰。"),
    ZH_HANT("讓新建立的潑濺在最佳化器中從零開始。它們不會繼承來源潑濺的動量，"
            "而是一開始就全速移動，因此新的細節能更快變清晰。"),
    KO("새로 만들어진 스플랫에 옵티마이저 상의 새 출발점을 줍니다. 원래 스플랫"
       "의 관성을 물려받지 않고 처음부터 제 속도로 움직이므로 새 디테일이 더 "
       "빨리 또렷해집니다."),
    DE("Neu erzeugten Splats im Optimierer einen frischen Start geben. Sie bewegen "
       "sich dann mit voller Geschwindigkeit, statt den Schwung des Splats zu "
       "erben, aus dem sie hervorgingen, sodass neues Detail schneller scharf "
       "wird."),
    FR("Donner aux splats nouvellement créés un départ à neuf dans l'optimiseur. "
       "Ils se déplacent alors à pleine vitesse au lieu d'hériter de l'élan du "
       "splat dont ils sont issus, si bien que le détail nouveau se précise plus "
       "vite."),
    ES("Dar a los splats recién creados un comienzo limpio en el optimizador. "
       "Así se mueven a plena velocidad en vez de heredar el impulso del splat "
       "del que salieron, y el detalle nuevo se afila antes."),
    PT("Dar aos splats recém-criados um recomeço no otimizador. Assim eles se "
       "movem a toda a velocidade em vez de herdar o impulso do splat de onde "
       "saíram, e o detalhe novo ganha nitidez mais depressa."),
    IT("Dare agli splat appena creati una partenza da zero nell'ottimizzatore. "
       "Si muovono così a piena velocità invece di ereditare lo slancio dello "
       "splat da cui provengono, e il dettaglio nuovo si affina prima."),
    NL("Nieuw gemaakte splats een frisse start geven in de optimizer. Ze bewegen "
       "dan op volle snelheid in plaats van het momentum te erven van de splat "
       "waaruit ze voortkwamen, zodat nieuw detail sneller scherp wordt."),
    RU("Дать вновь созданным сплатам чистый старт в оптимизаторе. Тогда они сразу "
       "движутся на полной скорости, а не наследуют инерцию сплата-родителя, "
       "и новые детали резчают быстрее."),
    TR("Yeni oluşturulan splat'lara iyileştiricide temiz bir başlangıç verir. "
       "Böylece türedikleri splat'ın momentumunu devralmak yerine tam hızda hareket "
       "ederler ve yeni ayrıntı daha çabuk keskinleşir."));


// ===========================================================================
// Dropdown values
// ===========================================================================
//
// A `choices` entry lists the literal strings the flag accepts: they go into
// config.json, they are what `--eval-mode interval` means, and the help text
// refers to them in backticks. So they are NOT replaced by a translation --
// the GUI shows the translation next to the value, "按间隔 (interval)", and
// an English reader sees the value alone because the two are the same word.
//
// Only flags whose values are ordinary WORDS get an entry. `3dgs`, `Rec.709`,
// `ppisp` and `robust_edge_aware` are names and stay as they are, all of them
// together: a list that translated half its entries would read as a bug.

SS_MSG(choice_off,
    EN("off"),           JA("オフ"),          ZH_HANS("关闭"),     ZH_HANT("關閉"),
    KO("끄기"),           DE("aus"),          FR("désactivé"),    ES("desactivado"),
    PT("desativado"),    IT("disattivato"),  NL("uit"),          RU("выкл."),
    TR("kapalı"));

SS_MSG(choice_mild,
    EN("mild"),          JA("弱め"),          ZH_HANS("轻度"),     ZH_HANT("輕度"),
    KO("약하게"),         DE("leicht"),       FR("modéré"),       ES("suave"),
    PT("suave"),         IT("leggero"),      NL("licht"),        RU("слабое"),
    TR("hafif"));

SS_MSG(choice_strong,
    EN("strong"),        JA("強め"),          ZH_HANS("强力"),     ZH_HANT("強力"),
    KO("강하게"),         DE("stark"),        FR("fort"),         ES("fuerte"),
    PT("forte"),         IT("forte"),        NL("sterk"),        RU("сильное"),
    TR("güçlü"));

SS_MSG(choice_low,
    EN("low"),           JA("低"),            ZH_HANS("低"),       ZH_HANT("低"),
    KO("낮음"),           DE("niedrig"),      FR("faible"),       ES("baja"),
    PT("baixa"),         IT("bassa"),        NL("laag"),         RU("низкое"),
    TR("düşük"));

SS_MSG(choice_medium,
    EN("medium"),        JA("中"),            ZH_HANS("中"),       ZH_HANT("中"),
    KO("보통"),           DE("mittel"),       FR("moyen"),        ES("media"),
    PT("média"),         IT("media"),        NL("gemiddeld"),    RU("среднее"),
    TR("orta"));

SS_MSG(choice_high,
    EN("high"),          JA("高"),            ZH_HANS("高"),       ZH_HANT("高"),
    KO("높음"),           DE("hoch"),         FR("élevé"),        ES("alta"),
    PT("alta"),          IT("alta"),         NL("hoog"),         RU("высокое"),
    TR("yüksek"));

SS_MSG(choice_ultra,
    EN("ultra"),         JA("最高"),          ZH_HANS("极高"),     ZH_HANT("極高"),
    KO("최고"),           DE("ultra"),        FR("ultra"),        ES("ultra"),
    PT("ultra"),         IT("ultra"),        NL("ultra"),        RU("максимальное"),
    TR("ultra"));

SS_MSG(choice_all,
    EN("all"),           JA("すべて"),        ZH_HANS("全部"),     ZH_HANT("全部"),
    KO("전체"),           DE("alle"),         FR("toutes"),       ES("todas"),
    PT("todas"),         IT("tutte"),        NL("alle"),         RU("все"),
    TR("tümü"));

SS_MSG(choice_interval,
    EN("interval"),      JA("一定間隔"),      ZH_HANS("按间隔"),   ZH_HANT("按間隔"),
    KO("일정 간격"),      DE("Intervall"),    FR("intervalle"),   ES("intervalo"),
    PT("intervalo"),     IT("intervallo"),   NL("interval"),     RU("интервал"),
    TR("aralık"));

SS_MSG(choice_fraction,
    EN("fraction"),      JA("割合"),          ZH_HANS("按比例"),   ZH_HANT("按比例"),
    KO("비율"),           DE("Anteil"),       FR("proportion"),   ES("proporción"),
    PT("proporção"),     IT("quota"),        NL("aandeel"),      RU("доля"),
    TR("oran"));

SS_MSG(choice_filename,
    EN("filename"),      JA("ファイル名"),    ZH_HANS("按文件名"), ZH_HANT("按檔名"),
    KO("파일 이름"),      DE("Dateiname"),    FR("nom de fichier"), ES("nombre de archivo"),
    PT("nome do arquivo"), IT("nome file"),  NL("bestandsnaam"), RU("имя файла"),
    TR("dosya adı"));

SS_MSG(choice_floor,
    EN("floor"),         JA("切り捨て"),      ZH_HANS("向下取整"), ZH_HANT("向下取整"),
    KO("내림"),           DE("abrunden"),     FR("arrondi inférieur"), ES("hacia abajo"),
    PT("para baixo"),    IT("per difetto"),  NL("naar beneden"), RU("вниз"),
    TR("aşağı"));

SS_MSG(choice_ceil,
    EN("ceil"),          JA("切り上げ"),      ZH_HANS("向上取整"), ZH_HANT("向上取整"),
    KO("올림"),           DE("aufrunden"),    FR("arrondi supérieur"), ES("hacia arriba"),
    PT("para cima"),     IT("per eccesso"),  NL("naar boven"),   RU("вверх"),
    TR("yukarı"));

SS_MSG(choice_round,
    EN("round"),         JA("四捨五入"),      ZH_HANS("四舍五入"), ZH_HANT("四捨五入"),
    KO("반올림"),         DE("runden"),       FR("arrondi"),      ES("al más cercano"),
    PT("arredondar"),    IT("arrotonda"),    NL("afronden"),     RU("к ближайшему"),
    TR("yuvarla"));

SS_MSG(choice_normalized,
    EN("normalized"),    JA("正規化"),        ZH_HANS("归一化"),   ZH_HANT("正規化"),
    KO("정규화"),         DE("normalisiert"), FR("normalisé"),    ES("normalizado"),
    PT("normalizado"),   IT("normalizzato"), NL("genormaliseerd"), RU("нормализованная"),
    TR("normalleştirilmiş"));

SS_MSG(choice_camera,
    EN("camera"),        JA("カメラ"),        ZH_HANS("相机"),     ZH_HANT("相機"),
    KO("카메라"),         DE("Kamera"),       FR("caméra"),       ES("cámara"),
    PT("câmera"),        IT("camera"),       NL("camera"),       RU("камера"),
    TR("kamera"));

SS_MSG(choice_points,
    EN("points"),        JA("点群"),          ZH_HANS("点云"),     ZH_HANT("點雲"),
    KO("포인트"),         DE("Punkte"),       FR("points"),       ES("puntos"),
    PT("pontos"),        IT("punti"),        NL("punten"),       RU("точки"),
    TR("noktalar"));

SS_MSG(choice_mean,
    EN("mean"),          JA("平均"),          ZH_HANS("平均"),     ZH_HANT("平均"),
    KO("평균"),           DE("Mittelwert"),   FR("moyenne"),      ES("media"),
    PT("média"),         IT("media"),        NL("gemiddelde"),   RU("среднее"),
    TR("ortalama"));

SS_MSG(choice_max,
    EN("max"),           JA("最大"),          ZH_HANS("最大"),     ZH_HANT("最大"),
    KO("최대"),           DE("Maximum"),      FR("maximum"),      ES("máximo"),
    PT("máximo"),        IT("massimo"),      NL("maximum"),      RU("максимум"),
    TR("en büyük"));

SS_MSG(choice_sum,
    EN("sum"),           JA("合計"),          ZH_HANS("求和"),     ZH_HANT("求和"),
    KO("합계"),           DE("Summe"),        FR("somme"),        ES("suma"),
    PT("soma"),          IT("somma"),        NL("som"),          RU("сумма"),
    TR("toplam"));

SS_MSG(choice_median,
    EN("median"),        JA("中央値"),        ZH_HANS("中位数"),   ZH_HANT("中位數"),
    KO("중앙값"),         DE("Median"),       FR("médiane"),      ES("mediana"),
    PT("mediana"),       IT("mediana"),      NL("mediaan"),      RU("медиана"),
    TR("ortanca"));

SS_MSG(choice_cpu,
    EN("RAM"),           JA("メモリ"),        ZH_HANS("内存"),     ZH_HANT("記憶體"),
    KO("메모리"),         DE("RAM"),          FR("RAM"),          ES("RAM"),
    PT("RAM"),           IT("RAM"),          NL("RAM"),          RU("ОЗУ"),
    TR("RAM"));

SS_MSG(choice_gpu,
    EN("GPU"),           JA("GPU"),          ZH_HANS("GPU"),     ZH_HANT("GPU"),
    KO("GPU"),           DE("GPU"),          FR("GPU"),          ES("GPU"),
    PT("GPU"),           IT("GPU"),          NL("GPU"),          RU("видеопамять"),
    TR("GPU"));

SS_MSG(choice_disk,
    EN("disk"),          JA("ディスク"),      ZH_HANS("磁盘"),     ZH_HANT("磁碟"),
    KO("디스크"),         DE("Festplatte"),   FR("disque"),       ES("disco"),
    PT("disco"),         IT("disco"),        NL("schijf"),       RU("диск"),
    TR("disk"));

SS_MSG(choice_from_the_file,
    EN("from the file"),  JA("ファイルから"),  ZH_HANS("取自文件"),  ZH_HANT("取自檔案"),
    KO("파일에서"),        DE("aus der Datei"), FR("d'après le fichier"),
    ES("según el archivo"), PT("conforme o arquivo"), IT("dal file"),
    NL("uit het bestand"), RU("из файла"),     TR("dosyadan"));

SS_MSG(choice_srgb_default,
    EN("plain sRGB"), JA("素の sRGB"), ZH_HANS("普通 sRGB"), ZH_HANT("普通 sRGB"),
    KO("일반 sRGB"), DE("schlichtes sRGB"), FR("sRGB simple"), ES("sRGB simple"),
    PT("sRGB simples"), IT("sRGB semplice"), NL("gewoon sRGB"),
    RU("обычный sRGB"), TR("sade sRGB"));

SS_MSG(choice_same_as_input,
    EN("same as the input"), JA("入力と同じ"),  ZH_HANS("与输入相同"), ZH_HANT("與輸入相同"),
    KO("입력과 동일"),       DE("wie die Eingabe"), FR("comme l'entrée"),
    ES("igual que la entrada"), PT("igual à entrada"), IT("come l'ingresso"),
    NL("zoals de invoer"),  RU("как у входа"),  TR("girdiyle aynı"));

// ---------------------------------------------------------------------------
// flag + value -> label. Null for anything not listed, which is most of them.
// ---------------------------------------------------------------------------

struct ChoiceText {
    const char* flag;
    const char* value;
    const Msg* label;
};

inline constexpr ChoiceText kChoiceText[] = {
    {"quality", "low",    &choice_low},
    {"quality", "medium", &choice_medium},
    {"quality", "high",   &choice_high},
    {"quality", "ultra",  &choice_ultra},

    {"floater_suppression", "off",    &choice_off},
    {"floater_suppression", "mild",   &choice_mild},
    {"floater_suppression", "strong", &choice_strong},

    {"distraction_robustness", "off",    &choice_off},
    {"distraction_robustness", "mild",   &choice_mild},
    {"distraction_robustness", "strong", &choice_strong},

    {"eval_mode", "all",      &choice_all},
    {"eval_mode", "interval", &choice_interval},
    {"eval_mode", "fraction", &choice_fraction},
    {"eval_mode", "filename", &choice_filename},

    {"downscale_rounding_mode", "floor", &choice_floor},
    {"downscale_rounding_mode", "ceil",  &choice_ceil},
    {"downscale_rounding_mode", "round", &choice_round},

    {"train_frame", "normalized", &choice_normalized},
    {"train_frame", "camera",     &choice_camera},
    {"train_frame", "points",     &choice_points},

    {"densify_score_mode", "mean",   &choice_mean},
    {"densify_score_mode", "max",    &choice_max},
    {"densify_score_mode", "median", &choice_median},

    {"densify_accum_mode", "max", &choice_max},
    {"densify_accum_mode", "sum", &choice_sum},
    {"densify_accum_mode", "avg", &choice_mean},

    {"cache_images", "cpu",  &choice_cpu},
    {"cache_images", "gpu",  &choice_gpu},
    {"cache_images", "disk", &choice_disk},

    // `none` is the UNSET value for these four, not a colour space -- Rec.709
    // and `srgb` are the explicit ones. Labelled so the dropdown cannot read
    // as "no gamut" / "no transfer".
    {"image_color_gamut", "none", &choice_from_the_file},
    {"splat_color_gamut", "none", &choice_same_as_input},
    {"image_color_transfer", "none", &choice_srgb_default},
    {"splat_color_transfer", "none", &choice_same_as_input},
};
inline constexpr size_t kNumChoiceText =
    sizeof(kChoiceText) / sizeof(kChoiceText[0]);

inline const Msg* choice_label(const char* flag, const char* value) {
    for (const ChoiceText& c : kChoiceText)
        if (std::strcmp(c.flag, flag) == 0 && std::strcmp(c.value, value) == 0)
            return c.label;
    return nullptr;
}

}  // namespace field
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
