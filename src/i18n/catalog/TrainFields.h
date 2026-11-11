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
    EN("Continue from a resumable checkpoint.\n- Pass a run folder for its newest checkpoint or a specific step-*.ckpt folder.\n- Restores model and dataset settings. CLI still controls run length, save cadence and viewer settings."),
    JA("再開可能なチェックポイントから続行します。\n- 実行フォルダなら最新、step-*.ckpt フォルダならそのチェックポイントを使います。\n- モデルとデータセット設定を復元します。長さ、保存間隔、ビューアは CLI の設定です。"),
    ZH_HANS("从可恢复的检查点继续训练。\n- 指定运行文件夹以使用最新检查点，或指定 step-*.ckpt 文件夹。\n- 恢复模型与数据集设置。训练长度、保存间隔与查看器仍由 CLI 指定。"),
    ZH_HANT("從可恢復的檢查點繼續訓練。\n- 指定執行資料夾以使用最新檢查點，或指定 step-*.ckpt 資料夾。\n- 還原模型與資料集設定。訓練長度、儲存間隔與檢視器仍由 CLI 指定。"),
    KO("재개 가능한 체크포인트에서 계속합니다.\n- 실행 폴더의 최신 체크포인트 또는 특정 step-*.ckpt 폴더를 사용합니다.\n- 모델과 데이터셋 설정을 복원합니다. 길이, 저장 간격, 뷰어는 CLI 설정을 따릅니다."),
    DE("Von einem fortsetzbaren Checkpoint weitertrainieren.\n- Laufordner für den neuesten Checkpoint oder einen bestimmten step-*.ckpt-Ordner wählen.\n- Modell und Daten bleiben aus dem Lauf. Dauer, Speichertakt und Viewer kommen aus der CLI."),
    FR("Reprendre depuis un point de contrôle complet.\n- Dossier d’exécution pour le dernier point, ou dossier step-*.ckpt précis.\n- Restaure modèle et données. Durée, sauvegardes et visionneuse suivent la ligne de commande."),
    ES("Continúa desde un punto de control reanudable.\n- Indica una carpeta de ejecución para usar el más reciente o una carpeta step-*.ckpt concreta.\n- Restaura modelo y datos. La CLI controla duración, frecuencia de guardado y visor."),
    PT("Retoma de um checkpoint que permite continuar.\n- Indique a pasta da execução para o mais recente ou uma pasta step-*.ckpt específica.\n- Restaura modelo e dados. A CLI define duração, frequência de gravação e visualizador."),
    IT("Riprende da un checkpoint ripristinabile.\n- Usa la cartella di esecuzione per l’ultimo checkpoint o una cartella step-*.ckpt specifica.\n- Ripristina modello e dati. Durata, cadenza dei salvataggi e visualizzatore seguono la CLI."),
    NL("Hervat vanaf een hervatbaar checkpoint.\n- Geef de run-map voor het nieuwste checkpoint of een specifieke step-*.ckpt-map.\n- Herstelt model en dataset. Duur, opslagschema en viewer volgen de CLI."),
    RU("Продолжает с контрольной точки, пригодной для возобновления.\n- Укажите папку запуска для последней точки или конкретную папку step-*.ckpt.\n- Восстанавливает модель и данные. Длительность, сохранение и просмотр задаёт CLI."),
    TR("Sürdürülebilir bir kontrol noktasından devam eder.\n- Son nokta için çalışma klasörünü veya belirli bir step-*.ckpt klasörünü verin.\n- Model ve veri ayarlarını geri yükler. Süre, kayıt aralığı ve görüntüleyiciyi CLI belirler."));

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
    EN("How often a checkpoint is written, in steps.\n- Use -1 to save only when training finishes, or 0 to never save.\n- Frequent saves cost disk space and a little time."),
    JA("チェックポイントを書き出す間隔をステップ数で指定します。\n- -1 なら学習の終わりにだけ保存し、0 なら保存しません。\n- 頻繁に保存するとディスク容量と少しの時間を使います。"),
    ZH_HANS("多少步写一次检查点。\n- -1 表示只在训练结束时保存，0 表示从不保存。\n- 频繁保存会占用磁盘空间和少量时间。"),
    ZH_HANT("多少步寫一次檢查點。\n- -1 表示只在訓練結束時儲存，0 表示從不儲存。\n- 頻繁儲存會佔用磁碟空間和少量時間。"),
    KO("체크포인트를 쓰는 간격을 스텝 수로 지정합니다.\n- -1이면 학습이 끝날 때만 저장하고 0이면 저장하지 않습니다.\n- 자주 저장하면 디스크 공간과 약간의 시간이 듭니다."),
    DE("Wie oft ein Checkpoint geschrieben wird, in Schritten.\n- -1 speichert nur am Ende des Trainings, 0 gar nicht.\n- Häufiges Speichern kostet Plattenplatz und ein wenig Zeit."),
    FR("À quelle fréquence une sauvegarde est écrite, en étapes.\n- -1 n'enregistre qu'à la fin de l'entraînement, 0 jamais.\n- Des sauvegardes fréquentes coûtent de l'espace disque et un peu de temps."),
    ES("Cada cuántos pasos se escribe un punto de control.\n- -1 guarda solo al terminar el entrenamiento y 0 no guarda nunca.\n- Guardar a menudo cuesta espacio en disco y algo de tiempo."),
    PT("De quantos em quantos passos um checkpoint é gravado.\n- -1 salva apenas ao terminar o treinamento e 0 nunca salva.\n- Gravar com frequência custa espaço em disco e um pouco de tempo."),
    IT("Ogni quanti passi viene scritto un checkpoint.\n- -1 salva solo alla fine dell'addestramento, 0 non salva mai.\n- Salvare spesso costa spazio su disco e un po' di tempo."),
    NL("Om de hoeveel stappen een checkpoint wordt geschreven.\n- -1 slaat alleen aan het eind van de training op, 0 nooit.\n- Vaak opslaan kost schijfruimte en een beetje tijd."),
    RU("Как часто пишется контрольная точка, в шагах.\n- -1 сохраняет только в конце обучения, 0 — никогда.\n- Частые сохранения стоят места на диске и немного времени."),
    TR("Bir denetim noktasının kaç adımda bir yazılacağı.\n- -1 yalnızca eğitim bitince kaydeder, 0 hiç kaydetmez.\n- Sık kaydetmek disk alanına ve biraz zamana mal olur."));

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
    EN("Keep only the newest checkpoint and delete older ones as training goes.\nTurn off to keep the whole history, which uses considerably more disk space."),
    JA("いちばん新しいチェックポイントだけを残し、古いものは学習の進行に合わせて削除します。\nオフにすると履歴をすべて残しますが、ディスク容量をかなり多く使います。"),
    ZH_HANS("只保留最新的检查点，随着训练推进删除较旧的。\n关闭后会保留全部历史，但会占用多得多的磁盘空间。"),
    ZH_HANT("只保留最新的檢查點，隨著訓練推進刪除較舊的。\n關閉後會保留全部歷史，但會佔用多得多的磁碟空間。"),
    KO("가장 최근 체크포인트만 남기고 학습이 진행되는 동안 오래된 것은 지웁니다.\n끄면 전체 이력을 남기지만 디스크를 훨씬 많이 씁니다."),
    DE("Nur den neuesten Checkpoint behalten und ältere im Verlauf des Trainings löschen.\nAbgeschaltet bleibt die ganze Historie erhalten, was erheblich mehr Plattenplatz braucht."),
    FR("Ne garder que la sauvegarde la plus récente et supprimer les plus anciennes au fil de l'entraînement.\nDécoché, tout l'historique est conservé, ce qui occupe bien plus d'espace disque."),
    ES("Conservar solo el punto de control más reciente y borrar los anteriores según avanza el entrenamiento.\nSin marcar se guarda todo el historial, lo que ocupa bastante más disco."),
    PT("Manter apenas o checkpoint mais recente e apagar os antigos conforme o treinamento avança.\nDesmarcado, todo o histórico é mantido, o que ocupa bem mais disco."),
    IT("Conservare solo il checkpoint più recente ed eliminare i precedenti man mano che l'addestramento procede.\nDeselezionato mantiene tutta la storia, il che occupa molto più disco."),
    NL("Alleen het nieuwste checkpoint bewaren en oudere tijdens de training verwijderen.\nUitgevinkt blijft de hele geschiedenis staan, wat aanzienlijk meer schijfruimte kost."),
    RU("Хранить только самую свежую контрольную точку и удалять старые по ходу обучения.\nБез флажка сохраняется вся история, что занимает заметно больше места на диске."),
    TR("Yalnızca en yeni denetim noktasını tutar ve eğitim ilerledikçe eskileri siler.\nKapatılırsa tüm geçmiş saklanır ve bu belirgin biçimde daha çok disk yeri kaplar."));

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
    EN("Saves resumable state in all checkpoints, including final and older ones.\n- Off: the newest checkpoint can still resume while only the latest is kept.\n- Full checkpoints are much larger: all splat slots and optimizer state."),
    JA("最終・過去分を含む全チェックポイントに再開用の状態を保存します。\n- 無効でも最新だけを保持する場合、その最新分から再開できます。\n- 全スプラット枠と最適化状態を含むため、容量が大きくなります。"),
    ZH_HANS("在所有检查点（包括最终和较旧的）中保存可续训状态。\n- 关闭时，若仅保留最新检查点，仍可从最新检查点续训。\n- 完整检查点含全部泼溅槽和优化器状态，占用空间大得多。"),
    ZH_HANT("在所有檢查點（包括最終和較舊的）中儲存可續訓狀態。\n- 關閉時，若僅保留最新檢查點，仍可從最新檢查點續訓。\n- 完整檢查點含全部潑濺槽和最佳化器狀態，占用空間大得多。"),
    KO("최종 및 이전 체크포인트 모두에 재개 상태를 저장합니다.\n- 꺼도 최신 파일만 유지하면 그 체크포인트로 재개할 수 있습니다.\n- 모든 스플랫 슬롯과 최적화기 상태를 담아 용량이 훨씬 커집니다."),
    DE("Speichert fortsetzbaren Zustand in allen Checkpoints, auch finalen und älteren.\n- Aus: Der neueste bleibt fortsetzbar, solange nur der neueste behalten wird.\n- Viel größer: alle Splat-Slots und der Optimiererzustand."),
    FR("Sauve l’état de reprise dans tous les checkpoints, finaux et anciens inclus.\n- Désactivé : le plus récent reste reprenable si seul le dernier est conservé.\n- Bien plus gros : tous les emplacements de splats et l’état de l’optimiseur."),
    ES("Guarda estado reanudable en todos los checkpoints, finales y antiguos.\n- Desactivado: el último permite reanudar si solo se conserva el más reciente.\n- Ocupa mucho más: todos los huecos de splats y el estado del optimizador."),
    PT("Salva estado retomável em todos os checkpoints, finais e antigos.\n- Desativado: o último ainda retoma se apenas o mais recente for mantido.\n- Muito maior: todos os slots de splats e o estado do otimizador."),
    IT("Salva stato ripristinabile in tutti i checkpoint, finali e precedenti.\n- Disattivato: l’ultimo resta ripristinabile se si conserva solo il più recente.\n- Molto più grande: tutti gli slot di splat e lo stato dell’ottimizzatore."),
    NL("Bewaart hervatstatus in alle checkpoints, ook de laatste en oudere.\n- Uit: het nieuwste blijft hervatbaar als alleen het nieuwste wordt behouden.\n- Veel groter: alle splatplaatsen en de optimalisatiestatus."),
    RU("Сохраняет состояние для продолжения во всех контрольных точках.\n- Выкл.: новейшая точка позволяет продолжить, если сохраняется только она.\n- Намного больше: все слоты сплатов и состояние оптимизатора."),
    TR("Son ve eski olanlar dahil tüm kontrol noktalarına sürdürme durumu kaydeder.\n- Kapalı: yalnız en yenisi tutuluyorsa ondan sürdürmek yine mümkündür.\n- Çok daha büyük: tüm splat yuvaları ve eniyileyici durumu."));

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
    EN("Write rendered and reference images for the held-out views when training finishes.\nUseful for judging quality, at the cost of a little disk space."),
    JA("学習が終わったとき、評価用に取り分けた視点のレンダリング画像と参照画像を書き出します。\n品質を確かめるのに便利ですが、少しディスクを使います。"),
    ZH_HANS("训练结束时，为留出的评估视角写出渲染图和参考图。\n便于判断质量，代价是占用少量磁盘空间。"),
    ZH_HANT("訓練結束時，為留出的評估視角寫出算圖與參考影像。\n便於判斷品質，代價是佔用少量磁碟空間。"),
    KO("학습이 끝나면 평가용으로 떼어 둔 시점의 렌더 이미지와 원본 이미지를 저장합니다.\n품질을 판단하기 좋지만 디스크를 조금 씁니다."),
    DE("Am Ende des Trainings gerenderte und Referenzbilder für die zurückgehaltenen Ansichten schreiben.\nNützlich, um die Qualität zu beurteilen, kostet etwas Plattenplatz."),
    FR("Écrire, à la fin de l'entraînement, les images rendues et de référence pour les vues mises de côté.\nUtile pour juger de la qualité, au prix d'un peu d'espace disque."),
    ES("Escribir, al terminar el entrenamiento, las imágenes renderizadas y de referencia de las vistas apartadas.\nÚtil para juzgar la calidad, a costa de algo de espacio en disco."),
    PT("Gravar, ao terminar o treinamento, as imagens renderizadas e de referência das vistas reservadas.\nÚtil para julgar a qualidade, ao custo de um pouco de espaço em disco."),
    IT("Scrivere, alla fine dell'addestramento, le immagini renderizzate e di riferimento per le viste messe da parte.\nUtile per giudicare la qualità, al costo di un po' di spazio su disco."),
    NL("Schrijft na training render- en referentiebeelden voor evaluatiebeelden.\nHelpt de kwaliteit te beoordelen, maar kost wat schijfruimte."),
    RU("По окончании обучения записывать отрисованные и эталонные изображения для отложенных видов.\nУдобно для оценки качества ценой небольшого места на диске."),
    TR("Eğitim bitince, ayrılmış görünümler için çizilen ve kaynak görüntüleri yazar.\nKaliteyi değerlendirmek için işe yarar, karşılığında biraz disk yeri harcar."));

SS_MSG(eval_lpips,
    EN("Score eval views with LPIPS"),
    JA("評価用の視点を LPIPS で採点"),
    ZH_HANS("用 LPIPS 为评估视角评分"),
    ZH_HANT("用 LPIPS 為評估視角評分"),
    KO("평가 시점을 LPIPS로 채점"),
    DE("Auswertungsansichten mit LPIPS bewerten"),
    FR("Noter les vues d'évaluation avec LPIPS"),
    ES("Puntuar las vistas de evaluación con LPIPS"),
    PT("Pontuar as vistas de avaliação com LPIPS"),
    IT("Valutare le viste con LPIPS"),
    NL("Evaluatieaanzichten scoren met LPIPS"),
    RU("Оценивать виды по LPIPS"),
    TR("Değerlendirme görünümlerini LPIPS ile puanla"));
SS_MSG(eval_lpips_help,
    EN("Scores held-out views with LPIPS AlexNet/VGG in metrics.json.\n- Downloads four checkpoints (~800 MB); download failure stops the run before training.\n- Without SS_BUILD_SAM, use reference/python/eval_lpips.py."),
    JA("評価用ビューをLPIPS AlexNet/VGGで採点し、metrics.jsonへ記録します。\n- チェックポイント4点（約800 MB）を取得。失敗すると学習前に終了します。\n- SS_BUILD_SAMなしでは reference/python/eval_lpips.py を使います。"),
    ZH_HANS("用 LPIPS AlexNet/VGG 评估留出视图，写入 metrics.json。\n- 下载四个检查点（约 800 MB）；下载失败会在训练前停止。\n- 未启用 SS_BUILD_SAM 时，使用 reference/python/eval_lpips.py。"),
    ZH_HANT("用 LPIPS AlexNet/VGG 評估保留視圖，寫入 metrics.json。\n- 下載四個檢查點（約 800 MB）；下載失敗會在訓練前停止。\n- 未啟用 SS_BUILD_SAM 時，使用 reference/python/eval_lpips.py。"),
    KO("평가용 뷰를 LPIPS AlexNet/VGG로 채점해 metrics.json에 기록합니다.\n- 체크포인트 네 개(약 800 MB)를 다운로드합니다. 실패하면 학습 전에 중단됩니다.\n- SS_BUILD_SAM 없이는 reference/python/eval_lpips.py를 쓰세요."),
    DE("Bewertet zurückgehaltene Ansichten mit LPIPS AlexNet/VGG in metrics.json.\n- Lädt vier Checkpoints (~800 MB); Downloadfehler stoppen den Lauf vor dem Training.\n- Ohne SS_BUILD_SAM: reference/python/eval_lpips.py verwenden."),
    FR("Évalue les vues réservées avec LPIPS AlexNet/VGG dans metrics.json.\n- Télécharge quatre checkpoints (~800 MB) ; échec du téléchargement : arrêt avant entraînement.\n- Sans SS_BUILD_SAM, utiliser reference/python/eval_lpips.py."),
    ES("Evalúa vistas reservadas con LPIPS AlexNet/VGG en metrics.json.\n- Descarga cuatro checkpoints (~800 MB); si falla, se detiene antes de entrenar.\n- Sin SS_BUILD_SAM, usa reference/python/eval_lpips.py."),
    PT("Avalia vistas reservadas com LPIPS AlexNet/VGG em metrics.json.\n- Baixa quatro checkpoints (~800 MB); falha na transferência para antes do treino.\n- Sem SS_BUILD_SAM, use reference/python/eval_lpips.py."),
    IT("Valuta le viste riservate con LPIPS AlexNet/VGG in metrics.json.\n- Scarica quattro checkpoint (~800 MB); se fallisce si ferma prima del training.\n- Senza SS_BUILD_SAM, usare reference/python/eval_lpips.py."),
    NL("Beoordeelt apart gehouden beelden met LPIPS AlexNet/VGG in metrics.json.\n- Downloadt vier checkpoints (~800 MB); een fout stopt de run vóór training.\n- Zonder SS_BUILD_SAM: gebruik reference/python/eval_lpips.py."),
    RU("Оценивает отложенные виды с LPIPS AlexNet/VGG в metrics.json.\n- Загружает четыре точки (~800 MB); сбой загрузки останавливает запуск до обучения.\n- Без SS_BUILD_SAM используйте reference/python/eval_lpips.py."),
    TR("Ayrılan görünümleri LPIPS AlexNet/VGG ile metrics.json’a puanlar.\n- Dört kontrol noktası (~800 MB) indirir; indirme hatası eğitime başlamadan durdurur.\n- SS_BUILD_SAM olmadan reference/python/eval_lpips.py kullanın."));

SS_MSG(eval_align,
    EN("Align eval views before scoring"), JA("採点の前に評価用の視点を位置合わせ"),
    ZH_HANS("评分前先对齐评估视角"), ZH_HANT("評分前先對齊評估視角"),
    KO("채점 전에 평가 시점 정렬"),
    DE("Auswertungsansichten vor der Bewertung ausrichten"),
    FR("Aligner les vues d'évaluation avant de les noter"),
    ES("Alinear las vistas de evaluación antes de puntuar"),
    PT("Alinhar as vistas de avaliação antes de pontuar"),
    IT("Allineare le viste prima della valutazione"),
    NL("Evaluatieaanzichten uitlijnen vóór het scoren"),
    RU("Совмещать виды перед оценкой"),
    TR("Puanlamadan önce değerlendirme görünümlerini hizala"));
SS_MSG(eval_align_help,
    EN("Score held-out views after fitting a smooth warp from each render to its photo.\n- metrics.json uses al_ for aligned and alcc_ for aligned and color-corrected. Masks add masked_al_ and masked_alcc_.\n- Separates appearance quality from calibration error.\n- auto enables this with calibration correction."),
    JA("評価用に視点を取り分けたとき、各描画を滑らかに当てはめたフローで写真に重ねてからも採点し、al_（位置合わせ後）と alcc_（位置合わせして色補正した後）の指標として、マスクがあれば masked_al_ と masked_alcc_ としても metrics.json に書きます。\n取り分けたカメラには、学習したワープでは知りようのない固有のキャリブレーション誤差があるため、これらの指標は再構成がどれだけ揃っているかではなく、どれだけ良く見えるかを表します。 `auto` ではキャリブレーション補正が有効なときに採点します。"),
    ZH_HANS("留出评估视角时，先用拟合出的平滑光流把每幅渲染对到照片上再评分一次，作为 al_（对齐后）和 alcc_（对齐并校色后）指标写入 metrics.json；有遮罩时还写入 masked_al_ 和 masked_alcc_。\n- 留出的相机有自己的标定误差，任何训练出的形变都无从得知；因此这些指标衡量的是重建看起来有多好，而不是它对得有多准。\n- `auto` 在开启标定校正时评分。"),
    ZH_HANT("留出評估視角時，先用擬合出的平滑光流把每幅算繪對到照片上再評分一次，作為 al_（對齊後）與 alcc_（對齊並校色後）指標寫入 metrics.json；有遮罩時還寫入 masked_al_ 與 masked_alcc_。\n- 留出的相機有自己的校準誤差，任何訓練出的形變都無從得知；因此這些指標衡量的是重建看起來有多好，而不是它對得有多準。\n- `auto` 在開啟校準修正時評分。"),
    KO("평가용으로 시점을 떼어 두면, 각 렌더링을 맞춰 구한 부드러운 흐름으로 사진에 겹친 뒤에도 채점해 al_(정렬 후)와 alcc_(정렬하고 색 보정한 후) 지표로, 마스크가 있으면 masked_al_과 masked_alcc_로도 metrics.json에 씁니다.\n- 떼어 둔 카메라에는 학습된 워프가 알 수 없는 고유한 캘리브레이션 오차가 있으므로, 이 지표들은 재구성이 얼마나 잘 맞는지가 아니라 얼마나 좋아 보이는지를 나타냅니다.\n- `auto`는 캘리브레이션 보정이 켜져 있을 때 채점합니다."),
    DE("Werden Ansichten zurückgehalten, sie zusätzlich bewerten, nachdem jedes Rendering mit einem glatten, angepassten Fluss auf sein Foto verzerrt wurde, als Metriken al_ (ausgerichtet) und alcc_ (ausgerichtet, dann farbkorrigiert) in metrics.json, und als masked_al_ und masked_alcc_, wo es Masken gibt. Eine zurückgehaltene Kamera hat einen eigenen Kalibrierfehler, den keine trainierte Verzerrung kennen kann; diese Werte sagen also, wie gut die Rekonstruktion aussieht, nicht wie genau sie passt.\n`auto` bewertet sie, wenn die Kalibrierkorrektur an ist."),
    FR("Quand des vues sont mises de côté, les noter aussi après avoir déformé chaque rendu sur sa photo par un flot lisse ajusté, comme métriques al_ (aligné) et alcc_ (aligné, puis corrigé en couleur) dans metrics.json, et comme masked_al_ et masked_alcc_ là où il y a des masques.\n- Une caméra mise de côté a sa propre erreur d'étalonnage, qu'aucune déformation entraînée ne peut connaître ;\n- ces valeurs disent donc à quel point la reconstruction est belle, non à quel point elle tombe juste. `auto` les note quand la correction d'étalonnage est activée."),
    ES("Cuando se apartan vistas, puntuarlas también tras deformar cada renderizado sobre su foto con un flujo suave ajustado, como métricas al_ (alineado) y alcc_ (alineado y luego con el color corregido) en metrics.json, y como masked_al_ y masked_alcc_ donde hay máscaras.\n- Una cámara apartada tiene su propio error de calibración, que ninguna deformación entrenada puede conocer;\n- estas métricas dicen, pues, lo bien que se ve la reconstrucción, no lo bien que encaja. `auto` las puntúa cuando la corrección de calibración está activada."),
    PT("Quando vistas são separadas, pontuá-las também depois de deformar cada renderização sobre sua foto com um fluxo suave ajustado, como métricas al_ (alinhado) e alcc_ (alinhado e depois com a cor corrigida) em metrics.json, e como masked_al_ e masked_alcc_ onde há máscaras.\n- Uma câmera separada tem seu próprio erro de calibração, que nenhuma deformação treinada pode conhecer;\n- essas métricas dizem, então, quão boa a reconstrução parece, e não quão bem ela se encaixa. `auto` as pontua quando a correção de calibração está ativada."),
    IT("Quando delle viste sono tenute da parte, valutarle anche dopo aver deformato ogni rendering sulla sua foto con un flusso liscio adattato, come metriche al_ (allineato) e alcc_ (allineato, poi con colore corretto) in metrics.json, e come masked_al_ e masked_alcc_ dove ci sono maschere.\nUna camera tenuta da parte ha un proprio errore di calibrazione che nessuna deformazione addestrata può conoscere; queste metriche dicono quindi quanto la ricostruzione appare buona, non quanto combacia. `auto` le calcola quando la correzione della calibrazione è attiva."),
    NL("Worden aanzichten achtergehouden, ze dan ook scoren nadat elke render met een gladde, aangepaste stroming op zijn foto is vervormd, als metrieken al_ (uitgelijnd) en alcc_ (uitgelijnd, daarna kleurgecorrigeerd) in metrics.json, en als masked_al_ en masked_alcc_ waar er maskers zijn.\n- Een achtergehouden camera heeft een eigen kalibratiefout die geen getrainde vervorming kan kennen;\n- deze waarden zeggen dus hoe goed de reconstructie eruitziet, niet hoe precies hij aansluit.\n- `auto` scoort ze wanneer de kalibratiecorrectie aan staat."),
    RU("Когда виды отложены для оценки, оценивать их также после того, как каждый рендер наложен на свой снимок подобранным гладким потоком, — как метрики al_ (совмещено) и alcc_ (совмещено, затем скорректирован цвет) в metrics.json, а при наличии масок ещё и как masked_al_ и masked_alcc_.\n- У отложенной камеры своя ошибка калибровки, которую не может знать ни одна обученная деформация, поэтому эти метрики говорят о том, насколько хорошо выглядит реконструкция, а не насколько точно она совпадает.\n- `auto` вычисляет их, когда включена коррекция калибровки."),
    TR("Görünümler ayrıldığında, her işlemeyi uydurulmuş yumuşak bir akışla fotoğrafının üzerine büktükten sonra da puanlar;\n- metrics.json'a al_ (hizalı) ve alcc_ (hizalı, sonra renk düzeltilmiş) metrikleri olarak, maske olan yerlerde de masked_al_ ve masked_alcc_ olarak yazar.\n- Ayrılan bir kameranın, hiçbir eğitilmiş bükmenin bilemeyeceği kendi kalibrasyon hatası vardır;\n- bu yüzden bu metrikler yeniden yapılandırmanın ne kadar iyi göründüğünü söyler, ne kadar iyi oturduğunu değil.\n- `auto`, kalibrasyon düzeltmesi açıkken bunları puanlar."));

SS_MSG(lpips_checkpoints,
    EN("LPIPS checkpoints folder"),
    JA("LPIPS チェックポイントのフォルダー"),
    ZH_HANS("LPIPS 检查点文件夹"),
    ZH_HANT("LPIPS 檢查點資料夾"),
    KO("LPIPS 체크포인트 폴더"),
    DE("Ordner der LPIPS-Checkpoints"),
    FR("Dossier des points de contrôle LPIPS"),
    ES("Carpeta de puntos de control de LPIPS"),
    PT("Pasta dos checkpoints do LPIPS"),
    IT("Cartella dei checkpoint LPIPS"),
    NL("Map met LPIPS-checkpoints"),
    RU("Папка контрольных точек LPIPS"),
    TR("LPIPS denetim noktası klasörü"));
SS_MSG(lpips_checkpoints_help,
    EN("A folder that already holds the four LPIPS checkpoints, used instead of downloading them: alexnet-owt-7be5be79.pth, vgg16-397923af.pth, and LPIPS v0.1's alex.pth and vgg.pth.\n- Torch's hub cache (~/.cache/torch/hub/checkpoints) is such a folder.\n- Empty: the model cache, downloading what is missing."),
    JA("4 つの LPIPS チェックポイントがすでに入っているフォルダーで、ダウンロードの代わりに使います：alexnet-owt-7be5be79.pth、vgg16-397923af.pth、LPIPS v0.1 の alex.pth と vgg.pth。\n- torch のハブキャッシュ（~/.cache/torch/hub/checkpoints）がそうしたフォルダーです。\n- 空のときはモデルキャッシュを使い、足りないものをダウンロードします。"),
    ZH_HANS("已经存有四个 LPIPS 检查点的文件夹，用它代替下载：alexnet-owt-7be5be79.pth、vgg16-397923af.pth，以及 LPIPS v0.1 的 alex.pth 和 vgg.pth。\ntorch 的 hub 缓存（~/.cache/torch/hub/checkpoints）就是这样的文件夹。 留空则使用模型缓存，并下载缺少的文件。"),
    ZH_HANT("已經存有四個 LPIPS 檢查點的資料夾，用它代替下載：alexnet-owt-7be5be79.pth、vgg16-397923af.pth，以及 LPIPS v0.1 的 alex.pth 與 vgg.pth。\ntorch 的 hub 快取（~/.cache/torch/hub/checkpoints）就是這樣的資料夾。 留空則使用模型快取，並下載缺少的檔案。"),
    KO("LPIPS 체크포인트 4개가 이미 들어 있는 폴더로, 내려받는 대신 씁니다: alexnet-owt-7be5be79.pth, vgg16-397923af.pth, 그리고 LPIPS v0.1의 alex.pth와 vgg.pth.\n- torch의 허브 캐시(~/.cache/torch/hub/checkpoints)가 그런 폴더입니다.\n- 비워 두면 모델 캐시를 쓰고 없는 파일을 내려받습니다."),
    DE("Ordner mit vorhandenen LPIPS-Checkpoints.\n- alexnet-owt-7be5be79.pth, vgg16-397923af.pth und LPIPS v0.1 alex.pth, vgg.pth.\n- Torch-Cache: ~/.cache/torch/hub/checkpoints.\n- Leer: Modell-Cache nutzen und fehlende Dateien laden."),
    FR("Dossier contenant les quatre poids LPIPS.\n- alexnet-owt-7be5be79.pth, vgg16-397923af.pth, et alex.pth et vgg.pth de LPIPS v0.1.\n- Cache torch : ~/.cache/torch/hub/checkpoints.\n- Vide : utilise le cache des modèles et télécharge les fichiers manquants."),
    ES("Carpeta con los cuatro pesos LPIPS.\n- alexnet-owt-7be5be79.pth, vgg16-397923af.pth, alex.pth y vgg.pth de LPIPS v0.1.\n- Caché torch: ~/.cache/torch/hub/checkpoints.\n- Vacía: usa la caché de modelos y descarga lo que falte."),
    PT("Uma pasta que já contém os quatro checkpoints do LPIPS, usada em vez de baixá-los: alexnet-owt-7be5be79.pth, vgg16-397923af.pth, e alex.pth e vgg.pth do LPIPS v0.1.\n- O cache hub do torch (~/.cache/torch/hub/checkpoints) é uma delas.\n- Vazia: o cache de modelos, baixando o que faltar."),
    IT("Una cartella che contiene già i quattro checkpoint LPIPS, usata al posto del download: alexnet-owt-7be5be79.pth, vgg16-397923af.pth, e alex.pth e vgg.pth di LPIPS v0.1.\n- La cache hub di torch (~/.cache/torch/hub/checkpoints) è una di queste.\n- Vuota: la cache dei modelli, scaricando ciò che manca."),
    NL("Een map die de vier LPIPS-checkpoints al bevat, gebruikt in plaats van downloaden: alexnet-owt-7be5be79.pth, vgg16-397923af.pth, en alex.pth en vgg.pth van LPIPS v0.1.\n- De hub-cache van torch (~/.cache/torch/hub/checkpoints) is zo'n map.\n- Leeg: de modelcache, met een download van wat ontbreekt."),
    RU("Папка с четырьмя файлами весов LPIPS.\n- alexnet-owt-7be5be79.pth, vgg16-397923af.pth и alex.pth, vgg.pth из LPIPS v0.1.\n- Кэш torch: ~/.cache/torch/hub/checkpoints.\n- Пусто: используется кэш моделей, недостающие файлы скачиваются."),
    TR("Dört LPIPS ağırlık dosyasının klasörü.\n- alexnet-owt-7be5be79.pth, vgg16-397923af.pth ve LPIPS v0.1 alex.pth, vgg.pth.\n- Torch önbelleği: ~/.cache/torch/hub/checkpoints.\n- Boşsa model önbelleği kullanılır ve eksik dosyalar indirilir."));

SS_MSG(log_performance,
    EN("Log performance stats"),
    JA("性能の統計を記録"),
    ZH_HANS("记录性能统计"),
    ZH_HANT("記錄效能統計"),
    KO("성능 통계 기록"),
    DE("Leistungsdaten protokollieren"),
    FR("Journaliser les statistiques de performance"),
    ES("Registrar estadísticas de rendimiento"),
    PT("Registrar estatísticas de desempenho"),
    IT("Registra le statistiche delle prestazioni"),
    NL("Prestatiestatistieken vastleggen"),
    RU("Записывать статистику производительности"),
    TR("Performans istatistiklerini kaydet"));
SS_MSG(log_performance_help,
    EN("Sample CPU, RAM, disk, GPU, clocks, temperature and step timings once per second.\n- Logs wait time for images and GPU time in the run’s perf folder.\n- Logging costs a small fraction of one CPU core."),
    JA("学習中、1 秒ごとにコンピューターと実行の状態を記録します。コアごとの CPU 使用率、メモリ、ディスク、GPU 使用率、クロック、温度、そして各ステップが画像を待った時間と GPU で動いた時間です。ファイルは実行フォルダーの perf に保存されます。\nCPU コア 1 つのごく一部しか使いません。"),
    ZH_HANS("训练时每秒记录一次计算机和本次运行的状态：每个核心的 CPU 占用、内存、磁盘、GPU 占用、频率和温度，以及每一步等待图像与在 GPU 上运行各花了多长时间。文件保存在运行文件夹的 perf 中。\n- 只占用一个 CPU 核心的很小一部分。"),
    ZH_HANT("訓練時每秒記錄一次電腦與本次執行的狀態：每個核心的 CPU 使用率、記憶體、磁碟、GPU 使用率、時脈與溫度，以及每一步等待影像與在 GPU 上執行各花了多久。檔案存放在執行資料夾的 perf 中。\n- 只佔用一個 CPU 核心的很小一部分。"),
    KO("학습하는 동안 1초마다 컴퓨터와 실행 상태를 기록합니다. 코어별 CPU 사용률, 메모리, 디스크, GPU 사용률, 클럭과 온도, 그리고 각 스텝이 이미지를 기다린 시간과 GPU에서 실행된 시간입니다. 파일은 실행 폴더의 perf에 저장됩니다.\n- CPU 코어 하나의 아주 작은 일부만 씁니다."),
    DE("Zeichnet während des Trainings einmal pro Sekunde Rechner und Lauf auf: CPU-Last je Kern, Arbeitsspeicher, Datenträger, GPU-Last, Takt und Temperatur sowie, wie lange jeder Schritt auf Bilder gewartet und wie lange er auf der GPU gerechnet hat.\n- Die Dateien landen im Ordner perf des Laufs.\n- Kostet einen kleinen Bruchteil eines CPU-Kerns."),
    FR("Enregistre l'ordinateur et l'exécution une fois par seconde pendant l'entraînement : charge CPU par cœur, mémoire, disque, charge GPU, fréquences et température, et le temps que chaque étape a passé à attendre les images ou à calculer sur le GPU.\n- Les fichiers vont dans le dossier perf de l'exécution.\n- Coûte une petite fraction d'un cœur CPU."),
    ES("Registra el equipo y la ejecución una vez por segundo mientras entrena: uso de CPU por núcleo, memoria, disco, uso de GPU, frecuencias y temperatura, y cuánto tiempo cada paso esperó imágenes frente a cuánto se ejecutó en la GPU.\n- Los archivos van a la carpeta perf de la ejecución.\n- Cuesta una pequeña fracción de un núcleo de CPU."),
    PT("Registra o computador e a execução uma vez por segundo durante o treinamento: uso de CPU por núcleo, memória, disco, uso de GPU, frequências e temperatura, e quanto tempo cada passo esperou por imagens e quanto rodou na GPU.\n- Os arquivos vão para a pasta perf da execução.\n- Custa uma pequena fração de um núcleo de CPU."),
    IT("Registra il computer e l'esecuzione una volta al secondo durante l'addestramento: uso della CPU per core, memoria, disco, uso della GPU, frequenze e temperatura, e quanto tempo ogni passo ha atteso le immagini rispetto a quanto ha lavorato sulla GPU.\n- I file vanno nella cartella perf dell'esecuzione.\n- Costa una piccola frazione di un core della CPU."),
    NL("Legt tijdens de training elke seconde de computer en de run vast: CPU-gebruik per kern, geheugen, schijf, GPU-gebruik, kloksnelheden en temperatuur, en hoe lang elke stap op beelden wachtte tegenover hoe lang hij op de GPU rekende.\n- De bestanden komen in de map perf van de run.\n- Kost een klein deel van één CPU-kern."),
    RU("Во время обучения раз в секунду записывает состояние компьютера и запуска: загрузку CPU по ядрам, память, диск, загрузку GPU, частоты и температуру, а также сколько времени каждый шаг ждал изображений и сколько работал на GPU.\n- Файлы сохраняются в папку perf запуска.\n- Занимает малую долю одного ядра CPU."),
    TR("Eğitim sürerken bilgisayarı ve çalıştırmayı saniyede bir kaydeder: çekirdek başına CPU kullanımı, bellek, disk, GPU kullanımı, saat hızları ve sıcaklık, ayrıca her adımın görüntü beklediği süre ile GPU'da çalıştığı süre.\n- Dosyalar çalıştırmanın perf klasörüne yazılır.\n- Tek bir CPU çekirdeğinin küçük bir kısmını harcar."));

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
    EN("Keep the program running after training finishes so the result stays open in the viewer.\n- Press Ctrl-C to exit.\n- Has no effect when the viewer is disabled."),
    JA("学習が終わってもプログラムを終了させず、結果をビューアで開いたままにします。\n- 終了するには Ctrl-C を押してください。\n- ビューアを無効にしているときは効果がありません。"),
    ZH_HANS("训练结束后不退出程序，让结果继续在查看器中打开。\n- 按 Ctrl-C 退出。\n- 查看器被禁用时该项无效。"),
    ZH_HANT("訓練結束後不結束程式，讓結果繼續在檢視器中開啟。\n- 按 Ctrl-C 結束。\n- 檢視器被停用時該項無效。"),
    KO("학습이 끝나도 프로그램을 종료하지 않고 결과를 뷰어에 열어 둡니다.\n- 끝내려면 Ctrl-C를 누르십시오.\n- 뷰어를 껐을 때는 효과가 없습니다."),
    DE("Das Programm nach dem Training weiterlaufen lassen, damit das Ergebnis im Viewer offen bleibt.\n- Zum Beenden Strg-C drücken.\n- Ohne Wirkung, wenn der Viewer abgeschaltet ist."),
    FR("Garder le programme en fonctionnement après l'entraînement pour que le résultat reste ouvert dans la visionneuse.\n- Appuyez sur Ctrl-C pour quitter.\n- Sans effet si la visionneuse est désactivée."),
    ES("Mantiene el visor abierto tras el entrenamiento.\n- Pulsa Ctrl-C para salir.\n- Sin efecto si el visor está desactivado."),
    PT("Manter o programa em execução depois do treinamento para que o resultado continue aberto no visualizador.\n- Pressione Ctrl-C para sair.\n- Sem efeito se o visualizador estiver desativado."),
    IT("Tenere il programma in esecuzione dopo l'addestramento perché il risultato resti aperto nel visualizzatore.\n- Premere Ctrl-C per uscire.\n- Senza effetto se il visualizzatore è disattivato."),
    NL("Het programma na de training laten doorlopen zodat het resultaat in de viewer open blijft.\n- Druk op Ctrl-C om af te sluiten.\n- Zonder effect als de viewer uit staat."),
    RU("Не завершать программу после обучения, чтобы результат оставался открытым в просмотрщике.\n- Для выхода нажмите Ctrl-C.\n- Без действия, когда просмотрщик отключён."),
    TR("Eğitimden sonra programı ve sonucu görüntüleyicide açık tutar.\n- Çıkmak için Ctrl-C’ye basın.\n- Görüntüleyici kapalıysa etkisi yoktur."));


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
    EN("Use the dataset's masks when they exist, and the alpha channel of images that have one.\n- What they then mean is set by apply_loss_for_mask\n- turn off to train as if the dataset had none."),
    JA("既存のマスクと画像のアルファチャンネルを使います。\n- マスクの意味は apply_loss_for_mask で決まります。\n- 無効ならマスクなしで学習します。"),
    ZH_HANS("数据集里有蒙版时就使用它们，带 Alpha 通道的图像也用该通道作蒙版。\n- 它们的含义由 apply_loss_for_mask 决定\n- 关掉就当作数据集没有蒙版来训练。"),
    ZH_HANT("資料集裡有遮罩時就使用它們，帶 Alpha 通道的影像也用該通道作遮罩。\n- 它們的含意由 apply_loss_for_mask 決定\n- 關掉就當作資料集沒有遮罩來訓練。"),
    KO("데이터셋에 마스크가 있으면 사용하며, 알파 채널이 있는 이미지는 그 채널도 마스크로 씁니다.\n그 의미는 apply_loss_for_mask가 정하며, 끄면 마스크가 없는 데이터셋처럼 학습합니다."),
    DE("Die Masken des Datensatzes verwenden, sofern vorhanden, dazu den Alphakanal von Bildern, die einen haben.\n- Was sie bedeuten, legt apply_loss_for_mask fest\n- abschalten trainiert wie ohne Masken."),
    FR("Utilise les masques existants et le canal alpha des images.\n- Leur sens dépend de apply_loss_for_mask.\n- Désactiver pour entraîner sans masque."),
    ES("Usar las máscaras del conjunto de datos cuando existan, y el canal alfa de las imágenes que lo tengan.\n- Lo que significan lo fija apply_loss_for_mask\n- desactive para entrenar como si no hubiera."),
    PT("Usar as máscaras do conjunto de dados quando existirem, e o canal alfa das imagens que tiverem um.\n- O que elas significam é definido por apply_loss_for_mask\n- desligue para treinar como se não houvesse nenhuma."),
    IT("Usare le maschere del set di dati quando ci sono, e il canale alfa delle immagini che ne hanno uno.\n- Che cosa significhino lo stabilisce apply_loss_for_mask\n- disattivare addestra come se non ce ne fossero."),
    NL("De maskers van de dataset gebruiken als die er zijn, en het alfakanaal van beelden die er een hebben.\n- Wat ze betekenen, bepaalt apply_loss_for_mask\n- zet uit om te trainen alsof er geen zijn."),
    RU("Использовать маски набора, если они есть, а также альфа-канал изображений, у которых он есть.\n- Что они означают, задаёт apply_loss_for_mask\n- выключите, чтобы обучать как без масок."),
    TR("Veri kümesinde maskeler varsa onları, alfa kanalı olan görüntülerde de bu kanalı kullanır.\n- Ne anlama geldiklerini apply_loss_for_mask belirler\n- maskesiz eğitmek için kapatın."));

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
    EN("When unset, on for alpha-only masks and off otherwise.\n- Off — skip masked pixels in the loss.\n- On — train masked pixels as empty space."),
    JA("未設定なら、アルファマスクだけの場合はオン、それ以外はオフです。\n- オフ — マスク画素を損失から除外。\n- オン — マスク画素を空として学習。"),
    ZH_HANS("未设置时，仅有透明通道蒙版则开启，其他情况关闭。\n- 关闭 — 损失忽略蒙版像素。\n- 开启 — 将蒙版像素训练为空。"),
    ZH_HANT("未設定時，僅有透明通道遮罩則開啟，其他情況關閉。\n- 關閉 — 損失忽略遮罩像素。\n- 開啟 — 將遮罩像素訓練為空。"),
    KO("미설정이면 알파 마스크만 있을 때 켜고 그 외에는 끕니다.\n- 끔 — 마스크 픽셀을 손실에서 제외합니다.\n- 켬 — 마스크 픽셀을 빈 공간으로 학습합니다."),
    DE("Ohne Einstellung bei reinen Alpha-Masken an, sonst aus.\n- Aus — maskierte Pixel im Loss auslassen.\n- An — maskierte Pixel als leer trainieren."),
    FR("Par défaut, activé si les masques viennent uniquement de l’alpha.\n- Désactivé — exclut les pixels masqués de la perte.\n- Activé — entraîne les pixels masqués comme vides."),
    ES("Sin ajuste, se activa solo si las máscaras proceden del alfa.\n- Desactivado — omite los píxeles enmascarados en la pérdida.\n- Activado — entrena los píxeles enmascarados como vacíos."),
    PT("Sem definição, ativa apenas se as máscaras vierem do alfa.\n- Desativado — omite píxeis mascarados na perda.\n- Ativado — treina píxeis mascarados como vazios."),
    IT("Senza impostazione, attivo solo con maschere dal canale alfa.\n- Disattivo — esclude i pixel mascherati dalla perdita.\n- Attivo — addestra i pixel mascherati come vuoti."),
    NL("Oningesteld alleen aan bij uitsluitend alfamaskers.\n- Uit — gemaskeerde pixels tellen niet mee in het verlies.\n- Aan — leert gemaskeerde pixels als lege ruimte."),
    RU("Если не задано, включено только для масок из альфа-канала.\n- Выкл. — исключает пиксели маски из потерь.\n- Вкл. — обучает пиксели маски как пустые."),
    TR("Ayarlanmazsa yalnız alfa maskeleri için açıktır.\n- Kapalı — maskeli pikselleri kayıptan çıkarır.\n- Açık — maskeli pikselleri boş alan olarak eğitir."));

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
    EN("Inverts mask files before edge adjustment. White normally means keep. Image alpha channels are never inverted."),
    JA("縁の調整前にマスクファイルを反転します。通常は白が保持領域です。画像のアルファチャンネルは反転しません。"),
    ZH_HANS("在边缘调整前反转蒙版文件。白色通常表示保留。不会反转图像的 alpha 通道。"),
    ZH_HANT("在邊緣調整前反轉遮罩檔案。白色通常表示保留。不會反轉影像的 alpha 通道。"),
    KO("경계 조정 전에 마스크 파일을 반전합니다. 보통 흰색은 유지 영역입니다. 이미지 알파 채널은 반전하지 않습니다."),
    DE("Invertiert Maskendateien vor der Randanpassung. Weiß bedeutet normalerweise behalten. Bild-Alphakanäle werden nie invertiert."),
    FR("Inverse les fichiers de masque avant d’ajuster les bords. Le blanc signifie normalement conserver. Le canal alpha reste inchangé."),
    ES("Invierte archivos de máscara antes de ajustar bordes. Blanco suele significar conservar. No invierte el canal alfa de las imágenes."),
    PT("Inverte os arquivos de máscara antes de ajustar bordas. Branco normalmente significa manter. O canal alfa das imagens não é invertido."),
    IT("Inverte i file maschera prima di regolare i bordi. Il bianco indica normalmente cosa mantenere. Non inverte i canali alfa delle immagini."),
    NL("Keert maskerbestanden om vóór randaanpassing. Wit betekent normaal behouden. Alfakanalen van beelden blijven ongewijzigd."),
    RU("Инвертирует файлы масок до коррекции краёв. Белый обычно означает сохранить. Альфа-каналы изображений не инвертируются."),
    TR("Kenar ayarından önce maske dosyalarını ters çevirir. Beyaz normalde korunan alandır. Görüntü alfa kanalları ters çevrilmez."));

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
    EN("Adjusts mask edges by a fraction of image size. Negative shrinks the mask to trim halos; positive expands it."),
    JA("画像サイズに対する割合でマスクの縁を調整します。負で縮めて縁のにじみを除き、正で広げます。"),
    ZH_HANS("按图像尺寸的比例调整蒙版边缘。负值向内收缩以去除光晕；正值向外扩张。"),
    ZH_HANT("按影像尺寸的比例調整遮罩邊緣。負值向內收縮以去除光暈；正值向外擴張。"),
    KO("이미지 크기의 비율로 마스크 경계를 조정합니다. 음수는 줄여 후광을 제거하고, 양수는 넓힙니다."),
    DE("Verschiebt Maskenränder um einen Anteil der Bildgröße. Negative Werte verkleinern gegen Halos, positive vergrößern die Maske."),
    FR("Décale les bords du masque selon une fraction de la taille de l’image. Une valeur négative réduit les halos ; une valeur positive agrandit le masque."),
    ES("Ajusta los bordes según una fracción del tamaño de imagen. Un valor negativo contrae la máscara para quitar halos; uno positivo la amplía."),
    PT("Ajusta as bordas por uma fração do tamanho da imagem. Valores negativos reduzem a máscara para retirar halos; positivos ampliam."),
    IT("Regola i bordi in proporzione alla dimensione dell’immagine. Valori negativi riducono la maschera per togliere aloni; positivi la espandono."),
    NL("Verschuift maskerranden met een fractie van de beeldgrootte. Negatief verkleint tegen halo’s; positief vergroot het masker."),
    RU("Сдвигает края маски на долю размера изображения. Отрицательное значение сужает маску, убирая ореолы; положительное расширяет."),
    TR("Maske kenarını görüntü boyutunun bir oranı kadar ayarlar. Negatif değer haleleri kırparak maskeyi küçültür; pozitif değer genişletir."));

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
    EN("Which COLMAP reconstruction to read, relative to the dataset folder, such as sparse/0.\nLeave empty to pick automatically: the reconstruction with the most registered images wins."),
    JA("読み込む COLMAP の再構成を、データセットフォルダからの相対パスで指定します（例：sparse/0）。\n空にすると自動で選ばれ、登録画像がいちばん多い再構成が使われます。"),
    ZH_HANS("要读取的 COLMAP 重建结果，相对数据集文件夹给出，例如 sparse/0。\n留空则自动选择：注册图像最多的那个重建胜出。"),
    ZH_HANT("要讀取的 COLMAP 重建結果，相對資料集資料夾給出，例如 sparse/0。\n留空則自動選擇：註冊影像最多的那個重建勝出。"),
    KO("읽어들일 COLMAP 재구성을 데이터셋 폴더 기준 상대 경로로 지정합니다(예: sparse/0).\n비워 두면 자동으로 고르며, 등록된 이미지가 가장 많은 재구성이 선택됩니다."),
    DE("Welche COLMAP-Rekonstruktion gelesen wird, relativ zum Datensatzordner, etwa sparse/0.\nLeer lassen für die automatische Wahl: Es gewinnt die Rekonstruktion mit den meisten registrierten Bildern."),
    FR("Reconstruction COLMAP à lire, relative au dossier du jeu de données, par exemple sparse/0.\nLaisser vide pour un choix automatique : celle qui contient le plus d'images enregistrées l'emporte."),
    ES("Reconstrucción de COLMAP que se leerá, relativa a la carpeta del conjunto de datos, por ejemplo sparse/0.\nDéjelo vacío para elegirla automáticamente: gana la que tenga más imágenes registradas."),
    PT("Reconstrução do COLMAP a ler, relativa à pasta do conjunto de dados, por exemplo sparse/0.\nDeixe vazio para escolher automaticamente: vence a que tiver mais imagens registradas."),
    IT("Ricostruzione COLMAP da leggere, relativa alla cartella del set di dati, ad esempio sparse/0.\nLasciare vuoto per la scelta automatica: vince quella con più immagini registrate."),
    NL("Welke COLMAP-reconstructie wordt gelezen, relatief aan de datasetmap, bijvoorbeeld sparse/0.\nLaat leeg voor een automatische keuze: die met de meeste geregistreerde beelden wint."),
    RU("Путь к модели COLMAP относительно папки набора, например sparse/0.\nПусто: выбирается модель с наибольшим числом зарегистрированных снимков."),
    TR("Okunacak COLMAP yeniden oluşturması\n- veri kümesi klasörüne göre, örneğin sparse/0.\n- Otomatik seçim için boş bırakın: en çok kayıtlı görüntüye sahip olan kazanır."));

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

SS_MSG(sampling_seed,
    EN("Camera sampling seed"), JA("カメラ選択の乱数シード"), ZH_HANS("相机采样随机种子"),
    ZH_HANT("相機取樣隨機種子"), KO("카메라 샘플링 시드"), DE("Startwert der Kameraauswahl"),
    FR("Graine de sélection des caméras"), ES("Semilla de muestreo de cámaras"),
    PT("Semente de amostragem de câmeras"), IT("Seme di campionamento delle fotocamere"),
    NL("Startwaarde voor cameraselectie"), RU("Зерно выборки камер"), TR("Kamera örnekleme tohumu"));
SS_MSG(sampling_seed_help,
    EN("Random seed for camera sampling. 0 chooses a new seed for each run."),
    JA("カメラ選択に使う乱数シードです。0 は実行ごとに新しい値を選びます。"),
    ZH_HANS("相机采样的随机种子。0 表示每次运行选择新种子。"),
    ZH_HANT("相機取樣的隨機種子。0 表示每次執行選擇新種子。"),
    KO("카메라 샘플링의 난수 시드입니다. 0은 실행마다 새 시드를 선택합니다."),
    DE("Zufallsstartwert für die Kameraauswahl. 0 wählt für jeden Lauf einen neuen Wert."),
    FR("Graine aléatoire de sélection des caméras. 0 choisit une nouvelle graine à chaque exécution."),
    ES("Semilla aleatoria para el muestreo de cámaras. 0 elige una nueva en cada ejecución."),
    PT("Semente aleatória para amostragem de câmeras. 0 escolhe uma nova a cada execução."),
    IT("Seme casuale per il campionamento delle fotocamere. 0 sceglie un nuovo seme a ogni esecuzione."),
    NL("Willekeurige startwaarde voor cameraselectie. 0 kiest bij elke uitvoering een nieuwe waarde."),
    RU("Зерно случайной выборки камер. При 0 для каждого запуска выбирается новое зерно."),
    TR("Kamera örneklemesi için rastgele tohum. 0, her çalıştırmada yeni bir tohum seçer."));

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
    EN("Divides each image side by this value: 2 halves it; 0 or 1 keeps its size. Already downscaled image folders need no extra divisor."),
    JA("画像の各辺をこの値で割ります。2で半分、0または1で元のサイズです。縮小済み画像フォルダには追加の除数は不要です。"),
    ZH_HANS("将图像各边除以此值：2 表示减半，0 或 1 保持原尺寸。已缩小的图像文件夹无需额外除数。"),
    ZH_HANT("將影像各邊除以此值：2 表示減半，0 或 1 保持原尺寸。已縮小的影像資料夾無需額外除數。"),
    KO("각 이미지 변을 이 값으로 나눕니다. 2는 절반, 0이나 1은 원래 크기입니다. 이미 축소된 이미지 폴더에는 추가 나눗값이 필요 없습니다."),
    DE("Teilt jede Bildkante durch diesen Wert: 2 halbiert, 0 oder 1 behält die Größe. Bereits verkleinerte Bildordner brauchen keinen weiteren Teiler."),
    FR("Divise chaque côté par cette valeur : 2 le réduit de moitié, 0 ou 1 garde sa taille. Un dossier d’images déjà réduites n’a pas besoin de diviseur."),
    ES("Divide cada lado por este valor: 2 lo reduce a la mitad; 0 o 1 conserva el tamaño. Las imágenes ya reducidas no necesitan otro divisor."),
    PT("Divide cada lado por este valor: 2 reduz à metade; 0 ou 1 mantém o tamanho. Imagens já reduzidas não precisam de outro divisor."),
    IT("Divide ogni lato per questo valore: 2 dimezza, 0 o 1 mantiene la dimensione. Le immagini già ridotte non richiedono un altro divisore."),
    NL("Deelt elke beeldzijde door deze waarde: 2 halveert, 0 of 1 behoudt het formaat. Al verkleinde beeldmappen hebben geen extra deler nodig."),
    RU("Делит стороны изображения на это значение: 2 — вдвое, 0 или 1 — без изменения. Уже уменьшенным изображениям дополнительный делитель не нужен."),
    TR("Her görüntü kenarını bu değere böler: 2 yarıya indirir, 0 veya 1 boyutu korur. Zaten küçültülmüş görüntülere ek bölen gerekmez."));

SS_MSG(progressive_resolution,
    EN("Progressive resolution"),
    JA("段階的な解像度"),
    ZH_HANS("渐进分辨率"),
    ZH_HANT("漸進解析度"),
    KO("점진적 해상도"),
    DE("Schrittweise Auflösung"),
    FR("Résolution progressive"),
    ES("Resolución progresiva"),
    PT("Resolução progressiva"),
    IT("Risoluzione progressiva"),
    NL("Oplopende resolutie"),
    RU("Постепенное разрешение"),
    TR("Kademeli çözünürlük"));
SS_MSG(progressive_resolution_help,
    EN("Starts with smaller images and increases resolution between dataset passes. Each image gets equal training at each size. Off uses full resolution."),
    JA("小さい画像から始め、データセットを一巡するごとに解像度を上げます。各サイズで全画像を同じ回数学習します。無効時は常にフル解像度です。"),
    ZH_HANS("从小图像开始，在数据集轮次之间提高分辨率。每种尺寸下所有图像的训练次数相同。关闭后始终使用完整分辨率。"),
    ZH_HANT("從小影像開始，在資料集輪次之間提高解析度。每種尺寸下所有影像的訓練次數相同。關閉後始終使用完整解析度。"),
    KO("작은 이미지로 시작해 데이터셋 순회 사이에 해상도를 높입니다. 각 크기에서 모든 이미지를 같은 횟수로 학습합니다. 끄면 전체 해상도를 씁니다."),
    DE("Beginnt mit kleinen Bildern und erhöht die Auflösung zwischen Datensatzdurchläufen. Alle Bilder werden je Größe gleich oft trainiert. Aus: volle Auflösung."),
    FR("Augmente la résolution entre les passes, à partir d’images réduites. Toutes les images sont entraînées autant à chaque taille. Désactivé : pleine résolution."),
    ES("Empieza con imágenes pequeñas y sube la resolución entre pasadas. Todas se entrenan igual en cada tamaño. Desactivado: resolución completa."),
    PT("Começa com imagens menores e aumenta a resolução entre passagens. Todas recebem o mesmo treino por tamanho. Desativado: resolução completa."),
    IT("Inizia con immagini ridotte e aumenta la risoluzione tra i passaggi. Ogni immagine riceve lo stesso addestramento per dimensione. Disattivato: piena risoluzione."),
    NL("Begint met kleine beelden en verhoogt de resolutie tussen rondes. Elk beeld wordt per formaat even vaak getraind. Uit: volledige resolutie."),
    RU("Начинает с малых изображений, повышая разрешение между проходами. На каждом размере все кадры обучаются поровну. Выкл.: полное разрешение."),
    TR("Küçük görüntülerle başlar; veri kümesi turları arasında çözünürlüğü artırır. Her boyutta görüntüler eşit eğitilir. Kapalı: tam çözünürlük."));
SS_MSG(progressive_resolution_start,
    EN("Starting resolution divisor"),
    JA("開始時の解像度の分母"),
    ZH_HANS("起始分辨率缩小倍数"),
    ZH_HANT("起始解析度縮小倍數"),
    KO("시작 해상도 축소 배수"),
    DE("Anfänglicher Auflösungsteiler"),
    FR("Diviseur de résolution de départ"),
    ES("Divisor de resolución inicial"),
    PT("Divisor de resolução inicial"),
    IT("Divisore di risoluzione iniziale"),
    NL("Begindeler van de resolutie"),
    RU("Начальный делитель разрешения"),
    TR("Başlangıç çözünürlük böleni"));
SS_MSG(progressive_resolution_start_help,
    EN("Initial stage divisor, multiplied by the training divisor. Power of two, at least 2; 4 gives stages 4, 2, 1."),
    JA("学習用除数に掛ける初期段階の除数。2以上の2の累乗を指定します。4なら段階ごとの除数は4、2、1です。"),
    ZH_HANS("初始阶段除数，与训练分辨率除数相乘。使用不小于 2 的 2 的幂；4 对应各阶段的除数为 4、2、1。"),
    ZH_HANT("初始階段除數，與訓練解析度除數相乘。使用不小於 2 的 2 的冪；4 對應各階段的除數為 4、2、1。"),
    KO("학습 나눗값에 곱할 초기 단계 나눗값입니다. 2 이상의 2의 거듭제곱을 쓰세요. 4는 단계별로 4, 2, 1을 적용합니다."),
    DE("Anfänglicher Stufenteiler, mit dem Trainingsteiler multipliziert. Zweierpotenz ab 2; bei 4 folgen die Stufen 4, 2, 1."),
    FR("Diviseur initial, multiplié par le diviseur d’entraînement. Puissance de deux, au moins 2 ; avec 4, les étapes sont 4, 2, 1."),
    ES("Divisor inicial, multiplicado por el divisor de entrenamiento. Potencia de dos, al menos 2; con 4 las etapas son 4, 2, 1."),
    PT("Divisor inicial, multiplicado pelo divisor de treino. Potência de dois, no mínimo 2; com 4 as etapas são 4, 2, 1."),
    IT("Divisore iniziale, moltiplicato per il divisore di addestramento. Potenza di due, almeno 2; con 4 le fasi sono 4, 2, 1."),
    NL("Eerste fasedeler, vermenigvuldigd met de trainingsdeler. Een macht van twee vanaf 2; bij 4 volgen de fases 4, 2, 1."),
    RU("Начальный делитель этапа умножается на делитель обучения. Степень двойки от 2; при 4 этапы идут с делителями 4, 2, 1."),
    TR("Eğitim böleniyle çarpılan ilk aşama böleni. En az 2 olan bir ikinin kuvveti; 4 için aşamalar 4, 2, 1 olur."));
SS_MSG(progressive_resolution_full_at,
    EN("Full resolution from"),
    JA("元の解像度に達する時点"),
    ZH_HANS("达到完整分辨率的时机"),
    ZH_HANT("達到完整解析度的時機"),
    KO("원래 해상도 도달 시점"),
    DE("Volle Auflösung ab"),
    FR("Pleine résolution à partir de"),
    ES("Resolución completa desde"),
    PT("Resolução completa a partir de"),
    IT("Risoluzione piena da"),
    NL("Volle resolutie vanaf"),
    RU("Полное разрешение с"),
    TR("Tam çözünürlük başlangıcı"));
SS_MSG(progressive_resolution_full_at_help,
    EN("Run fraction for reaching full resolution (0.3 = 30%). Earlier stages are successively half as long. Switches align to whole dataset passes."),
    JA("フル解像度に達する時点を実行の割合で指定します（0.3 = 30%）。前の段階ほど期間は半分です。切り替えはデータセットの一巡に合わせます。"),
    ZH_HANS("达到完整分辨率时的训练进度比例（0.3 = 30%）。越早的阶段时长依次减半。切换对齐到完整的数据集轮次。"),
    ZH_HANT("達到完整解析度時的訓練進度比例（0.3 = 30%）。越早的階段時長依次減半。切換對齊到完整的資料集輪次。"),
    KO("전체 해상도에 도달할 실행 비율입니다(0.3 = 30%). 앞선 단계일수록 기간이 절반입니다. 전환은 전체 데이터셋 순회에 맞춥니다."),
    DE("Laufanteil bis zur vollen Auflösung (0.3 = 30%). Frühere Stufen dauern jeweils halb so lang. Wechsel erfolgen an ganzen Datensatzdurchläufen."),
    FR("Fraction du parcours pour atteindre la pleine résolution (0.3 = 30%). Chaque étape précédente dure moitié moins. Les transitions suivent les passes complètes."),
    ES("Fracción del proceso hasta la resolución completa (0.3 = 30%). Cada etapa previa dura la mitad. Los cambios coinciden con pasadas completas."),
    PT("Fração da execução até a resolução completa (0.3 = 30%). Cada etapa anterior dura metade. As mudanças seguem passagens completas."),
    IT("Frazione della sessione per raggiungere la piena risoluzione (0.3 = 30%). Ogni fase precedente dura la metà. I cambi seguono passaggi completi."),
    NL("Deel van de run tot volledige resolutie (0.3 = 30%). Elke eerdere fase duurt half zo lang. Wissels vallen op volledige dataset-rondes."),
    RU("Доля обучения до полного разрешения (0.3 = 30%). Каждый предыдущий этап вдвое короче. Переключения привязаны к полным проходам."),
    TR("Tam çözünürlüğe ulaşma oranı (0.3 = %30). Her önceki aşama yarı uzunluktadır. Geçişler tam veri kümesi turlarına hizalanır."));
SS_MSG(progressive_resolution_schedule,
    EN("Manual resolution schedule"),
    JA("手動の解像度スケジュール"),
    ZH_HANS("手动分辨率计划"),
    ZH_HANT("手動解析度計畫"),
    KO("수동 해상도 일정"),
    DE("Manueller Auflösungsplan"),
    FR("Calendrier de résolution manuel"),
    ES("Calendario de resolución manual"),
    PT("Cronograma de resolução manual"),
    IT("Calendario di risoluzione manuale"),
    NL("Handmatig resolutieschema"),
    RU("Ручное расписание разрешения"),
    TR("Elle çözünürlük planı"));
SS_MSG(progressive_resolution_schedule_help,
    EN("Custom step:divisor pairs, e.g. 0:4, 3000:2, 9000:1.\n- Divisors may only decrease; switches snap to whole dataset passes.\n- Empty uses the automatic schedule."),
    JA("独自の step:divisor の組。例：0:4, 3000:2, 9000:1。\n- 除数は減少のみ。切り替えをデータセットの一巡に合わせます。\n- 空欄は自動スケジュールを使います。"),
    ZH_HANS("自定义 step:divisor 对，例如 0:4, 3000:2, 9000:1。\n- 除数只能减小；切换对齐到完整的数据集轮次。\n- 留空使用自动计划。"),
    ZH_HANT("自訂 step:divisor 對，例如 0:4, 3000:2, 9000:1。\n- 除數只能減小；切換對齊到完整的資料集輪次。\n- 留空使用自動排程。"),
    KO("사용자 step:divisor 쌍입니다. 예: 0:4, 3000:2, 9000:1.\n- 나눗값은 줄어들어야 하며, 전환은 전체 데이터셋 순회에 맞춥니다.\n- 비우면 자동 일정을 사용합니다."),
    DE("Eigene Schritt:Teiler-Paare, z.B. 0:4, 3000:2, 9000:1.\n- Teiler dürfen nur sinken; Wechsel folgen ganzen Datensatzdurchläufen.\n- Leer verwendet den automatischen Plan."),
    FR("Paires pas:diviseur, par ex. 0:4, 3000:2, 9000:1.\n- Diviseurs uniquement décroissants ; transitions aux passes complètes.\n- Vide utilise le programme automatique."),
    ES("Pares paso:divisor, p. ej. 0:4, 3000:2, 9000:1.\n- Los divisores solo bajan; cambios en pasadas completas.\n- Vacío usa el plan automático."),
    PT("Pares passo:divisor, ex. 0:4, 3000:2, 9000:1.\n- Os divisores só diminuem; mudanças em passagens completas.\n- Vazio usa o plano automático."),
    IT("Coppie passo:divisore, ad es. 0:4, 3000:2, 9000:1.\n- I divisori possono solo diminuire; cambi ai passaggi completi.\n- Vuoto usa il piano automatico."),
    NL("Eigen stap:deler-paren, bijv. 0:4, 3000:2, 9000:1.\n- Delers mogen alleen dalen; wissels vallen op volledige rondes.\n- Leeg gebruikt het automatische schema."),
    RU("Свои пары шаг:делитель, например 0:4, 3000:2, 9000:1.\n- Делители только уменьшаются; переходы на полных проходах.\n- Пусто: автоматический график."),
    TR("Özel adım:bölen çiftleri, ör. 0:4, 3000:2, 9000:1.\n- Bölenler yalnız azalabilir; geçişler tam veri turlarına hizalanır.\n- Boşsa otomatik takvim kullanılır."));
SS_MSG(progressive_splat_budget,
    EN("Grow splats with resolution"),
    JA("解像度に合わせてスプラットを増やす"),
    ZH_HANS("随分辨率增加泼溅"),
    ZH_HANT("隨解析度增加潑濺"),
    KO("해상도에 맞춰 스플랫 늘리기"),
    DE("Splats mit der Auflösung wachsen lassen"),
    FR("Faire croître les splats avec la résolution"),
    ES("Aumentar los splats con la resolución"),
    PT("Aumentar os splats com a resolução"),
    IT("Far crescere gli splat con la risoluzione"),
    NL("Splats laten groeien met de resolutie"),
    RU("Наращивать сплаты вместе с разрешением"),
    TR("Splat sayısını çözünürlükle birlikte artır"));
SS_MSG(progressive_splat_budget_help,
    EN("- Auto: raises the cap by equal factors per resolution stage, from initial points to maximum.\n- Manual: set each stage’s cap.\n- Off: normal growth to maximum, without stage caps."),
    JA("- 自動：初期点数から最大数へ、解像度段階ごとに同じ倍率で上限を上げます。\n- 手動：各段階の上限を指定します。\n- 無効：段階の上限なしで、通常どおり最大数まで増やします。"),
    ZH_HANS("- 自动：从初始点数到最大值，每个分辨率阶段按相同倍数提高上限。\n- 手动：设置每个阶段的上限。\n- 关闭：不设阶段上限，按通常速度增长到最大值。"),
    ZH_HANT("- 自動：從初始點數到最大值，每個解析度階段按相同倍數提高上限。\n- 手動：設定每個階段的上限。\n- 關閉：不設階段上限，按通常速度增長到最大值。"),
    KO("- 자동: 초기 점 수부터 최댓값까지 해상도 단계마다 같은 비율로 상한을 높입니다.\n- 수동: 각 단계의 상한을 설정합니다.\n- 끄기: 단계 상한 없이 평소처럼 최댓값까지 늘립니다."),
    DE("- Auto: erhöht die Grenze je Auflösungsstufe um gleiche Faktoren vom Startwert zum Maximum.\n- Manuell: Grenze pro Stufe setzen.\n- Aus: wächst ohne Stufengrenzen wie üblich zum Maximum."),
    FR("- Auto : relève le plafond par facteurs égaux, des points initiaux au maximum, à chaque résolution.\n- Manuel : définir le plafond de chaque étape.\n- Désactivé : croissance habituelle jusqu’au maximum sans plafond par étape."),
    ES("- Auto: sube el límite por factores iguales en cada resolución, de los puntos iniciales al máximo.\n- Manual: fija el límite de cada etapa.\n- Desactivado: crece al máximo como siempre, sin límites por etapa."),
    PT("- Auto: aumenta o limite por fatores iguais a cada resolução, dos pontos iniciais ao máximo.\n- Manual: define o limite por etapa.\n- Desativado: cresce ao máximo normalmente, sem limites por etapa."),
    IT("- Auto: aumenta il limite con fattori uguali per risoluzione, dai punti iniziali al massimo.\n- Manuale: imposta il limite per fase.\n- Disattivato: cresce al massimo come di consueto, senza limiti per fase."),
    NL("- Auto: verhoogt de limiet per resolutiefase met dezelfde factor, van startpunten tot maximum.\n- Handmatig: stel per fase de limiet in.\n- Uit: groeit normaal naar het maximum, zonder faselimieten."),
    RU("- Авто: повышает лимит равными множителями по этапам, от начальных точек до максимума.\n- Вручную: задайте лимит каждого этапа.\n- Выкл.: обычный рост до максимума без лимитов этапов."),
    TR("- Otomatik: başlangıçtan en çoğa, her çözünürlük aşamasında üst sınırı eşit katla artırır.\n- Elle: her aşamanın sınırını ayarlayın.\n- Kapalı: aşama sınırı olmadan normal biçimde en çoğa büyür."));
SS_MSG(progressive_splat_budget_schedule,
    EN("Manual splat budget"),
    JA("手動のスプラット予算"),
    ZH_HANS("手动泼溅预算"),
    ZH_HANT("手動潑濺預算"),
    KO("수동 스플랫 예산"),
    DE("Manuelles Splat-Budget"),
    FR("Budget de splats manuel"),
    ES("Presupuesto de splats manual"),
    PT("Orçamento de splats manual"),
    IT("Budget di splat manuale"),
    NL("Handmatig splatbudget"),
    RU("Ручной бюджет сплатов"),
    TR("Elle splat bütçesi"));
SS_MSG(progressive_splat_budget_schedule_help,
    EN("Stage caps: counts (500000), fractions of maximum (0.25), or percentages (25%).\n- Caps may only grow; blank boxes use automatic values.\n- CLI: one comma-separated value per stage."),
    JA("段階の上限：個数（500000）、最大値の割合（0.25）、百分率（25%）。\n- 上限は増加のみ。空欄は自動値です。\n- CLI：段階ごとに1値、カンマ区切り。"),
    ZH_HANS("阶段上限：数量（500000）、最大值的比例（0.25）或百分比（25%）。\n- 上限只能增加；空框使用自动值。\n- 命令行：每阶段一个值，用逗号分隔。"),
    ZH_HANT("階段上限：數量（500000）、最大值的比例（0.25）或百分比（25%）。\n- 上限只能增加；空框使用自動值。\n- 命令列：每階段一個值，用逗號分隔。"),
    KO("단계 상한: 개수(500000), 최댓값의 비율(0.25) 또는 백분율(25%).\n- 상한은 증가만 가능하며, 빈칸은 자동값입니다.\n- CLI: 단계별 값을 쉼표로 구분합니다."),
    DE("Stufengrenzen: Anzahl (500000), Anteil des Maximums (0.25) oder Prozent (25%).\n- Grenzen dürfen nur wachsen; leere Felder nutzen automatische Werte.\n- CLI: ein kommagetrennter Wert pro Stufe."),
    FR("Plafonds : nombre (500000), fraction du maximum (0.25) ou pourcentage (25%).\n- Uniquement croissants ; champs vides : valeurs automatiques.\n- CLI : une valeur par étape, séparées par des virgules."),
    ES("Límites: cantidad (500000), fracción del máximo (0.25) o porcentaje (25%).\n- Solo pueden crecer; campos vacíos usan valores automáticos.\n- CLI: un valor por etapa, separado por comas."),
    PT("Limites: quantidade (500000), fração do máximo (0.25) ou percentual (25%).\n- Só podem crescer; campos vazios usam valores automáticos.\n- CLI: um valor por etapa, separado por vírgulas."),
    IT("Limiti: numero (500000), frazione del massimo (0.25) o percentuale (25%).\n- Possono solo crescere; campi vuoti: valori automatici.\n- CLI: un valore per fase, separato da virgole."),
    NL("Faselimieten: aantal (500000), fractie van maximum (0.25) of percentage (25%).\n- Mogen alleen stijgen; lege velden gebruiken automatische waarden.\n- CLI: één waarde per fase, gescheiden door komma’s."),
    RU("Лимиты этапов: число (500000), доля максимума (0.25) или процент (25%).\n- Только растут; пустые поля используют автоматические значения.\n- CLI: по значению на этап, через запятую."),
    TR("Aşama sınırları: sayı (500000), en çoğun oranı (0.25) veya yüzde (25%).\n- Yalnız artabilir; boş kutular otomatik değer kullanır.\n- CLI: aşama başına bir değer, virgülle ayrılır."));
SS_MSG(progressive_splat_budget_mode,
    EN("Splat budget"),
    JA("スプラット予算"),
    ZH_HANS("泼溅预算"),
    ZH_HANT("潑濺預算"),
    KO("스플랫 예산"),
    DE("Splat-Budget"),
    FR("Budget de splats"),
    ES("Presupuesto de splats"),
    PT("Orçamento de splats"),
    IT("Budget di splat"),
    NL("Splatbudget"),
    RU("Бюджет сплатов"),
    TR("Splat bütçesi"));
SS_MSG(progressive_splat_budget_auto,
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
SS_MSG(progressive_splat_budget_manual,
    EN("Manual, per stage"),
    JA("手動（段階ごと）"),
    ZH_HANS("手动（按阶段）"),
    ZH_HANT("手動（按階段）"),
    KO("수동 (단계별)"),
    DE("Manuell, je Stufe"),
    FR("Manuel, par étape"),
    ES("Manual, por etapa"),
    PT("Manual, por etapa"),
    IT("Manuale, per fase"),
    NL("Handmatig, per fase"),
    RU("Вручную, по этапам"),
    TR("Elle, aşama başına"));
SS_MSG(progressive_splat_budget_stage,
    EN("Up to, at 1/{0} resolution"),
    JA("1/{0} 解像度での上限"),
    ZH_HANS("1/{0} 分辨率时的上限"),
    ZH_HANT("1/{0} 解析度時的上限"),
    KO("1/{0} 해상도에서의 한도"),
    DE("Höchstens, bei 1/{0} Auflösung"),
    FR("Au plus, à la résolution 1/{0}"),
    ES("Hasta, a resolución 1/{0}"),
    PT("Até, na resolução 1/{0}"),
    IT("Fino a, a risoluzione 1/{0}"),
    NL("Tot, bij resolutie 1/{0}"),
    RU("Не больше, при разрешении 1/{0}"),
    TR("En çok, 1/{0} çözünürlükte"));
SS_MSG(progressive_splat_budget_stage_full,
    EN("Up to, at full resolution"),
    JA("元の解像度での上限"),
    ZH_HANS("完整分辨率时的上限"),
    ZH_HANT("完整解析度時的上限"),
    KO("원래 해상도에서의 한도"),
    DE("Höchstens, bei voller Auflösung"),
    FR("Au plus, en pleine résolution"),
    ES("Hasta, a resolución completa"),
    PT("Até, na resolução completa"),
    IT("Fino a, a piena risoluzione"),
    NL("Tot, bij volledige resolutie"),
    RU("Не больше, при полном разрешении"),
    TR("En çok, tam çözünürlükte"));
SS_MSG(progressive_splat_budget_hint,
    EN("automatic"),
    JA("自動"),
    ZH_HANS("自动"),
    ZH_HANT("自動"),
    KO("자동"),
    DE("automatisch"),
    FR("automatique"),
    ES("automático"),
    PT("automático"),
    IT("automatico"),
    NL("automatisch"),
    RU("автоматически"),
    TR("otomatik"));
SS_MSG(progressive_splat_budget_plan,
    EN("Limits: {0}"),
    JA("上限: {0}"),
    ZH_HANS("上限：{0}"),
    ZH_HANT("上限：{0}"),
    KO("한도: {0}"),
    DE("Grenzen: {0}"),
    FR("Limites : {0}"),
    ES("Límites: {0}"),
    PT("Limites: {0}"),
    IT("Limiti: {0}"),
    NL("Grenzen: {0}"),
    RU("Пределы: {0}"),
    TR("Sınırlar: {0}"));
SS_MSG(progressive_splat_budget_seed,
    EN("From {0} starting points to the maximum of {1}."),
    JA("開始時の {0} 点から最大の {1} まで。"),
    ZH_HANS("从 {0} 个初始点到最大值 {1}。"),
    ZH_HANT("從 {0} 個初始點到最大值 {1}。"),
    KO("시작 점 {0}개에서 최대치 {1}까지."),
    DE("Von {0} Startpunkten bis zum Maximum von {1}."),
    FR("De {0} points de départ jusqu'au maximum de {1}."),
    ES("De {0} puntos iniciales al máximo de {1}."),
    PT("De {0} pontos iniciais até o máximo de {1}."),
    IT("Da {0} punti iniziali al massimo di {1}."),
    NL("Van {0} startpunten tot het maximum van {1}."),
    RU("От {0} начальных точек до максимума {1}."),
    TR("{0} başlangıç noktasından en yüksek değer olan {1} değerine."));
SS_MSG(progressive_splat_budget_seed_unknown,
    EN("Open the dataset to see the starting point count; until then this assumes none, and training uses the real count."),
    JA("開始時の点数はデータセットを開くと表示されます。それまではゼロとして計算し、学習では実際の点数を使います。"),
    ZH_HANS("打开数据集后会显示初始点数；在此之前按零计算，训练时使用实际点数。"),
    ZH_HANT("開啟資料集後會顯示初始點數；在此之前按零計算，訓練時使用實際點數。"),
    KO("데이터셋을 열면 시작 점 수가 표시됩니다. 그 전에는 0으로 계산하며 학습에는 실제 점 수를 씁니다."),
    DE("Die Startpunktzahl erscheint, wenn der Datensatz geöffnet ist; bis dahin wird mit null gerechnet, das Training nimmt die echte Zahl."),
    FR("Le nombre de points de départ s'affiche une fois le jeu de données ouvert ; d'ici là le calcul suppose zéro, et l'entraînement utilise le vrai nombre."),
    ES("El número de puntos iniciales aparece al abrir el conjunto de datos; hasta entonces se supone cero, y el entrenamiento usa el número real."),
    PT("O número de pontos iniciais aparece ao abrir o conjunto de dados; até lá supõe-se zero, e o treinamento usa o número real."),
    IT("Il numero di punti iniziali compare aprendo il set di dati; fino ad allora si assume zero, e l'addestramento usa il numero reale."),
    NL("Het aantal startpunten verschijnt zodra de dataset geopend is; tot dan wordt nul aangenomen, en de training gebruikt het echte aantal."),
    RU("Число начальных точек появится после открытия набора данных; до этого считается ноль, а обучение использует настоящее число."),
    TR("Başlangıç nokta sayısı veri kümesi açılınca görünür; o zamana dek sıfır varsayılır, eğitim gerçek sayıyı kullanır."));
SS_MSG(downscale_rounding_mode,
    EN("Downscale rounding"), JA("縮小時の丸め方"),
    ZH_HANS("缩小时的取整方式"), ZH_HANT("縮小時的取整方式"),
    KO("축소 시 반올림 방식"), DE("Rundung beim Verkleinern"),
    FR("Arrondi lors de la réduction"), ES("Redondeo al reducir"),
    PT("Arredondamento ao reduzir"), IT("Arrotondamento nella riduzione"),
    NL("Afronding bij verkleinen"), RU("Округление при уменьшении"),
    TR("Küçültmede yuvarlama"));
SS_MSG(downscale_rounding_mode_help,
    EN("Rounding image dimensions after division by train_resolution_divisor: floor rounds down, round to nearest, ceil up."),
    JA("画像の各辺を train_resolution_divisor で割った後の丸め方。floor は切り捨て、round は四捨五入、ceil は切り上げです。"),
    ZH_HANS("图像各边除以 train_resolution_divisor 后的取整方式：floor 向下，round 就近，ceil 向上。"),
    ZH_HANT("影像各邊除以 train_resolution_divisor 後的取整方式：floor 向下，round 就近，ceil 向上。"),
    KO("각 변을 train_resolution_divisor로 나눈 뒤 반올림하는 방식입니다. floor는 내림, round는 가장 가까운 값, ceil은 올림입니다."),
    DE("Rundung der Bildmaße nach Teilung durch train_resolution_divisor: floor abwärts, round zum nächsten Wert, ceil aufwärts."),
    FR("Arrondi des dimensions après division par train_resolution_divisor : floor vers le bas, round au plus proche, ceil vers le haut."),
    ES("Redondeo de dimensiones tras dividir por train_resolution_divisor: floor hacia abajo, round al más cercano, ceil hacia arriba."),
    PT("Arredonda dimensões após dividir por train_resolution_divisor: floor para baixo, round ao mais próximo, ceil para cima."),
    IT("Arrotonda le dimensioni divise per train_resolution_divisor: floor per difetto, round al più vicino, ceil per eccesso."),
    NL("Afronding van beeldmaten gedeeld door train_resolution_divisor: floor omlaag, round naar de dichtstbijzijnde waarde, ceil omhoog."),
    RU("Округление размеров после деления на train_resolution_divisor: floor вниз, round к ближайшему, ceil вверх."),
    TR("Boyutları train_resolution_divisor ile böldükten sonra yuvarlar: floor aşağı, round en yakına, ceil yukarı."));

SS_MSG(cache_downscaled_images,
    EN("Cache downscaled images"), JA("縮小画像をキャッシュ"),
    ZH_HANS("缓存缩小后的图像"), ZH_HANT("快取縮小後的影像"),
    KO("축소 이미지 캐시"),
    DE("Verkleinerte Bilder zwischenspeichern"),
    FR("Mettre en cache les images réduites"),
    ES("Guardar en caché las imágenes reducidas"),
    PT("Guardar em cache as imagens reduzidas"),
    IT("Memorizza le immagini ridotte"),
    NL("Verkleinde beelden cachen"),
    RU("Кэшировать уменьшенные изображения"),
    TR("Küçültülmüş görüntüleri önbelleğe al"));
SS_MSG(cache_downscaled_images_help,
    EN("Caches reduced JPEG/PNG files for integer training divisors in images_2, images_4, etc.\n- Later runs reuse matching-size files to avoid decoding large images.\n- Existing files are never overwritten."),
    JA("整数の学習用除数で縮小したJPEG/PNGを images_2、images_4 などに保存します。\n- 次回以降はサイズが一致するファイルを再利用し、大きな画像の復号を省きます。\n- 既存ファイルは上書きしません。"),
    ZH_HANS("整数训练除数下，将缩小的 JPEG/PNG 缓存到 images_2、images_4 等目录。\n- 后续运行复用尺寸匹配的文件，避免解码大图像。\n- 从不覆盖现有文件。"),
    ZH_HANT("整數訓練除數下，將縮小的 JPEG/PNG 快取到 images_2、images_4 等目錄。\n- 後續執行重用尺寸匹配的檔案，避免解碼大影像。\n- 從不覆寫現有檔案。"),
    KO("정수 학습 나눗값의 축소 JPEG/PNG를 images_2, images_4 등에 저장합니다.\n- 이후 실행은 크기가 맞는 파일을 재사용해 큰 이미지 디코딩을 피합니다.\n- 기존 파일은 덮어쓰지 않습니다."),
    DE("Speichert verkleinerte JPEG/PNG bei ganzzahligen Teilern in images_2, images_4 usw.\n- Folgeläufe nutzen passende Dateien statt große Bilder neu zu dekodieren.\n- Bestehende Dateien werden nie überschrieben."),
    FR("Cache les JPEG/PNG réduits pour diviseurs entiers dans images_2, images_4, etc.\n- Réutilise ensuite les fichiers de taille adaptée au lieu de décoder de grandes images.\n- N’écrase jamais les fichiers existants."),
    ES("Guarda JPEG/PNG reducidos para divisores enteros en images_2, images_4, etc.\n- Las siguientes ejecuciones reutilizan tamaños coincidentes para evitar decodificar imágenes grandes.\n- Nunca sobrescribe archivos existentes."),
    PT("Guarda JPEG/PNG reduzidos para divisores inteiros em images_2, images_4 etc.\n- Próximas execuções reusam arquivos de tamanho igual, evitando decodificar imagens grandes.\n- Nunca sobrescreve arquivos existentes."),
    IT("Salva JPEG/PNG ridotti per divisori interi in images_2, images_4 ecc.\n- Le sessioni successive riusano file di dimensioni adatte, evitando di decodificare immagini grandi.\n- Non sovrascrive mai i file esistenti."),
    NL("Bewaart verkleinde JPEG/PNG voor gehele delers in images_2, images_4 enz.\n- Latere runs hergebruiken passende bestanden om grote beelden niet opnieuw te decoderen.\n- Bestaande bestanden worden nooit overschreven."),
    RU("Кэширует уменьшенные JPEG/PNG при целых делителях в images_2, images_4 и т.д.\n- Следующие запуски используют файлы нужного размера без декодирования больших кадров.\n- Существующие файлы не перезаписываются."),
    TR("Tam sayı bölenlerde küçük JPEG/PNG’leri images_2, images_4 vb. içine önbellekler.\n- Sonraki çalıştırmalar büyük görüntüleri çözmek yerine uygun boyutlu dosyaları kullanır.\n- Var olan dosyaları asla üzerine yazmaz."));

SS_MSG(eval_mode,
    EN("Evaluation split"), JA("評価用の分け方"), ZH_HANS("评估集划分方式"),
    ZH_HANT("評估集劃分方式"), KO("평가용 분할 방식"),
    DE("Aufteilung für die Auswertung"), FR("Répartition pour l'évaluation"),
    ES("División para la evaluación"), PT("Divisão para a avaliação"),
    IT("Suddivisione per la valutazione"), NL("Verdeling voor de evaluatie"),
    RU("Разделение для оценки"), TR("Değerlendirme ayrımı"));
SS_MSG(eval_mode_help,
    EN("- all: train and score on the same images.\n- interval: hold out every Nth image; fraction: hold out a share.\n- filename: use train/eval in names. Held-out views give honest quality scores."),
    JA("- all：同じ画像で学習と評価。\n- interval：N枚ごとに評価用に確保。fraction：一定割合を確保。\n- filename：名前のtrain/evalで分割。未学習ビューで品質を正しく評価します。"),
    ZH_HANS("- all：用相同图像训练和评分。\n- interval：每 N 张留出一张；fraction：按比例留出。\n- filename：按名称中的 train/eval 分组。留出视图可真实评估质量。"),
    ZH_HANT("- all：用相同影像訓練和評分。\n- interval：每 N 張保留一張；fraction：按比例保留。\n- filename：按名稱中的 train/eval 分組。保留視圖可真實評估品質。"),
    KO("- all: 같은 이미지로 학습과 평가를 합니다.\n- interval: N번째마다 제외, fraction: 일정 비율을 평가용으로 분리합니다.\n- filename: 이름의 train/eval로 나눕니다. 분리된 뷰로 품질을 공정하게 평가합니다."),
    DE("- all: auf denselben Bildern trainieren und bewerten.\n- interval: jedes N-te Bild zurückhalten; fraction: einen Anteil.\n- filename: train/eval im Namen. Zurückgehaltene Ansichten geben ehrliche Qualitätswerte."),
    FR("- all : entraîne et évalue les mêmes images.\n- interval : réserve chaque Nième image ; fraction : une part.\n- filename : train/eval dans les noms. Les vues réservées donnent un score fiable."),
    ES("- all: entrena y evalúa las mismas imágenes.\n- interval: reserva cada N imágenes; fraction: una proporción.\n- filename: train/eval en nombres. Las vistas reservadas dan puntuaciones honestas."),
    PT("- all: treina e avalia nas mesmas imagens.\n- interval: reserva cada enésima imagem; fraction: uma fração.\n- filename: train/eval nos nomes. Vistas reservadas dão pontuações honestas."),
    IT("- all: addestra e valuta sulle stesse immagini.\n- interval: riserva ogni ennesima immagine; fraction: una quota.\n- filename: train/eval nei nomi. Le viste riservate danno punteggi attendibili."),
    NL("- all: traint en beoordeelt dezelfde beelden.\n- interval: houdt elk N-de beeld apart; fraction: een aandeel.\n- filename: train/eval in namen. Apart gehouden beelden geven eerlijke scores."),
    RU("- all: обучение и оценка на одних снимках.\n- interval: отложить каждый N-й кадр; fraction: долю кадров.\n- filename: train/eval в именах. Отложенные виды дают честную оценку качества."),
    TR("- all: aynı görüntülerde eğitim ve puanlama.\n- interval: her N. görüntüyü ayırır; fraction: bir oranını ayırır.\n- filename: adlardaki train/eval. Ayrılan görünümler dürüst kalite puanı verir."));

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
    EN("Share of the training images set aside to watch for overfitting.\n- Training can then stop before quality starts to drop.\n- Early stopping is not active yet: the images are held out but the run always goes to the end."),
    JA("過学習の監視用に取り分ける学習画像の割合です。\n早期終了は未対応です。画像は取り分けますが、実行は最後まで進みます。"),
    ZH_HANS("为监视过拟合而从训练图像中留出的比例。\n- 这样可以在质量开始下降前停止训练。\n- 早停目前尚未启用：图像会被留出，但训练始终会跑到最后。"),
    ZH_HANT("為監視過擬合而從訓練影像中留出的比例。\n- 這樣可以在品質開始下降前停止訓練。\n- 早停目前尚未啟用：影像會被留出，但訓練始終會跑到最後。"),
    KO("과적합 감시용으로 따로 둘 학습 이미지의 비율입니다.\n조기 종료는 미지원입니다. 이미지는 따로 두지만 실행은 끝까지 진행됩니다."),
    DE("Anteil der Trainingsbilder, der zurückgelegt wird, um Überanpassung zu beobachten.\n- Das Training kann dann enden, bevor die Qualität abzufallen beginnt.\n- Frühes Stoppen ist noch nicht aktiv: Die Bilder werden zurückgehalten, aber der Lauf geht immer bis zum Ende."),
    FR("Part des images d'entraînement mise de côté pour surveiller le surapprentissage.\n- L'entraînement peut alors s'arrêter avant que la qualité ne baisse.\n- L'arrêt anticipé n'est pas encore actif : les images sont mises de côté, mais l'exécution va toujours jusqu'au bout."),
    ES("Proporción de las imágenes de entrenamiento apartada para vigilar el sobreajuste.\n- El entrenamiento puede así detenerse antes de que la calidad empiece a caer.\n- La parada temprana todavía no está activa: las imágenes se apartan, pero la ejecución siempre llega al final."),
    PT("Proporção das imagens de treinamento reservada para vigiar o sobreajuste.\n- O treinamento pode então parar antes de a qualidade começar a cair.\n- A parada antecipada ainda não está ativa: as imagens são reservadas, mas a execução vai sempre até o fim."),
    IT("Quota di immagini riservate al controllo del sovradattamento.\nArresto anticipato non ancora attivo: la sessione prosegue fino alla fine."),
    NL("Aandeel van de trainingsbeelden dat apart wordt gezet om overfitting in de gaten te houden.\n- De training kan dan stoppen voordat de kwaliteit begint te dalen.\n- Vroegtijdig stoppen werkt nog niet: de beelden worden apart gezet, maar de run loopt altijd tot het eind."),
    RU("Доля обучающих изображений, отложенная, чтобы следить за переобучением.\n- Тогда обучение можно остановить до того, как качество начнёт падать.\n- Ранняя остановка пока не работает: снимки откладываются, но запуск всегда идёт до конца."),
    TR("Aşırı öğrenmeyi izlemek için eğitim görüntülerinden ayrılan oran.\n- Böylece kalite düşmeye başlamadan eğitim durdurulabilir.\n- Erken durdurma henüz etkin değil: görüntüler ayrılır ama çalıştırma hep sonuna kadar gider."));

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
    EN("Splits each fisheye into five perspective views for better quality and compatibility with fisheye/360 captures, at the cost of processing more images."),
    JA("魚眼画像を5つの透視投影ビューに分割します。魚眼・360撮影の品質と互換性が向上しますが、処理する画像数が増えます。"),
    ZH_HANS("将每张鱼眼图像拆成五个透视视图，提高鱼眼和 360 拍摄的质量与兼容性，但需处理更多图像。"),
    ZH_HANT("將每張魚眼影像拆成五個透視視圖，提高魚眼和 360 拍攝的品質與相容性，但需處理更多影像。"),
    KO("각 어안 이미지를 투시 뷰 다섯 개로 나눕니다. 어안/360 촬영의 품질과 호환성이 좋아지지만 처리할 이미지가 늘어납니다."),
    DE("Teilt jedes Fischaugenbild in fünf Perspektiven. Verbessert Qualität und Kompatibilität für Fischauge/360°, verarbeitet aber mehr Bilder."),
    FR("Divise chaque image fisheye en cinq vues perspectives. Améliore qualité et compatibilité fisheye/360, mais traite plus d’images."),
    ES("Divide cada imagen ojo de pez en cinco vistas en perspectiva. Mejora calidad y compatibilidad ojo de pez/360, pero procesa más imágenes."),
    PT("Divide cada imagem olho de peixe em cinco vistas em perspectiva. Melhora qualidade e compatibilidade olho de peixe/360, mas processa mais imagens."),
    IT("Divide ogni fisheye in cinque viste prospettiche. Migliora qualità e compatibilità fisheye/360, ma richiede più immagini da elaborare."),
    NL("Splitst elk fisheye-beeld in vijf perspectiefbeelden. Betere kwaliteit en compatibiliteit voor fisheye/360, maar meer beelden te verwerken."),
    RU("Делит каждый кадр «рыбий глаз» на пять перспективных видов. Улучшает качество и совместимость для fisheye/360, но обрабатывает больше кадров."),
    TR("Her balıkgözü görüntüyü beş perspektif görünüme ayırır. Balıkgözü/360 kalite ve uyumluluğu artar, ancak daha çok görüntü işlenir."));

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
    EN("Face sizes for wide lenses split into pinhole views.\n- uniform — one render pass and fused optimizer support. Faster.\n- per-face — crops to lens coverage (at least half a frame). Draws 15–40% fewer pixels to save VRAM.\n- per-face uses one pass per size and disables the fused optimizer."),
    JA("広角レンズをピンホール視点に分割したときの面サイズです。\n- uniform — 1パスで高速。融合オプティマイザーに対応。\n- per-face — レンズの範囲に切り詰め（最低半フレーム）、画素を15–40%減らしてVRAMを節約。\n- per-face はサイズごとに描画し、融合オプティマイザーを無効にします。"),
    ZH_HANS("广角镜头分割成针孔视图后的面尺寸。\n- uniform — 单次渲染，更快，支持融合优化器。\n- per-face — 裁到镜头覆盖区（至少半帧），少绘制 15–40% 像素以节省显存。\n- per-face 每种尺寸单独渲染，禁用融合优化器。"),
    ZH_HANT("廣角鏡頭分割成針孔視圖後的面尺寸。\n- uniform — 單次算繪，更快，支援融合最佳化器。\n- per-face — 裁到鏡頭涵蓋區（至少半影格），少繪製 15–40% 像素以節省顯存。\n- per-face 每種尺寸單獨算繪，停用融合最佳化器。"),
    KO("광각 렌즈를 핀홀 뷰로 나눈 면의 크기입니다.\n- uniform — 한 번에 렌더링해 빠르며 융합 옵티마이저를 지원합니다.\n- per-face — 렌즈 범위로 자릅니다(최소 반 프레임). 픽셀 15–40%와 VRAM을 절약합니다.\n- per-face는 크기별로 렌더링하고 융합 옵티마이저를 끕니다."),
    DE("Flächengrößen für in Lochkamera-Ansichten geteilte Weitwinkelbilder.\n- uniform — ein Renderpass, schneller, unterstützt den fusionierten Optimierer.\n- per-face — Zuschnitt auf Linsenbereich (mindestens halbes Bild). 15–40% weniger Pixel und weniger VRAM.\n- per-face rendert je Größe und schaltet den fusionierten Optimierer ab."),
    FR("Taille des faces sténopé issues d’un grand-angle.\n- uniform — une passe, plus rapide, compatible avec l’optimiseur fusionné.\n- per-face — rogne à la zone utile (demi-image minimum). 15–40% de pixels en moins et moins de VRAM.\n- per-face utilise une passe par taille et désactive l’optimiseur fusionné."),
    ES("Tamaño de caras estenopeicas de lentes amplias.\n- uniform — una pasada rápida, compatible con el optimizador fusionado.\n- per-face — recorta a la zona útil (mínimo medio cuadro). 15–40% menos píxeles y menos VRAM.\n- per-face usa una pasada por tamaño y desactiva el optimizador fusionado."),
    PT("Tamanho das faces pinhole de lentes amplas.\n- uniform — uma passagem rápida, com suporte ao otimizador fundido.\n- per-face — recorta a zona útil (mínimo meio quadro). 15–40% menos píxeis e menos VRAM.\n- per-face usa uma passagem por tamanho e desativa o otimizador fundido."),
    IT("Dimensione delle facce pinhole da un grandangolo.\n- uniform — un passaggio rapido, supporta l’ottimizzatore fuso.\n- per-face — ritaglia l’area utile (almeno mezzo fotogramma). 15–40% di pixel in meno e meno VRAM.\n- per-face usa un passaggio per dimensione e disattiva l’ottimizzatore fuso."),
    NL("Vlakgrootte bij splitsing van groothoek in pinholebeelden.\n- uniform — één snelle renderpass met ondersteuning voor de gefuseerde optimizer.\n- per-face — snijdt bij op lensdekking (minstens half beeld). 15–40% minder pixels en minder VRAM.\n- per-face rendert per grootte en schakelt de gefuseerde optimizer uit."),
    RU("Размер граней при разбиении широкого кадра на виды.\n- uniform — один быстрый проход с поддержкой объединённого оптимизатора.\n- per-face — обрезает до области объектива (минимум полкадра). На 15–40% меньше пикселей и меньше VRAM.\n- per-face рендерит каждый размер отдельно и отключает объединённый оптимизатор."),
    TR("Geniş lens iğne deliği görünümlerine bölündüğünde yüz boyutları.\n- uniform — tek hızlı geçiş, birleşik iyileştirici desteği.\n- per-face — lens kapsamına kırpar (en az yarım kare). %15–40 daha az piksel ve VRAM.\n- per-face her boyutu ayrı işler ve birleşik iyileştiriciyi kapatır."));

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
    EN("Add a backward pinhole face for lenses reaching past 135°.\n- Enable when the lens sees behind itself. Fisheyes often have little usable coverage there.\n- Adds work and one appearance slot per image, about 20% more bilateral grid memory.\n- Panoramas always use six faces."),
    JA("135 度を超えて写るレンズに、後ろ向きのピンホール面をもう 1 枚加えるかどうか。実際の魚眼ではその方向に写るのはレンズの折り返し像がほとんどで、たいていマスクで除かれます。\n- それでも 1 ステップごとの処理は遅くなり、画像ごとの外観スロットを 1 つ余計に使います（バイラテラルグリッドのメモリが約 20% 増）。\n- 本当に後方まで写るレンズのときだけ有効にしてください。パノラマは常に 6 面すべてを使います。"),
    ZH_HANS("视场超过 135 度的镜头是否再加一张朝后的针孔面。真实鱼眼在这个方向上几乎只有镜头折返的像，通常本来就会被遮罩掉，却会拖慢每一步，并让每张图像多占一个外观槽（双边网格显存约多 20%）。\n只有镜头确实能看到身后时才打开。全景图始终使用全部六个面。"),
    ZH_HANT("視場超過 135 度的鏡頭是否再加一張朝後的針孔面。真實魚眼在這個方向上幾乎只有鏡頭折返的影像，通常本來就會被遮罩掉，卻會拖慢每一步，並讓每張影像多佔一個外觀槽（雙邊網格顯示記憶體約多 20%）。只有鏡頭確實能看到身後時才開啟。\n全景影像一律使用全部六個面。"),
    KO("135도를 넘겨 보이는 렌즈에 뒤를 향하는 핀홀 면을 하나 더 둘지 여부입니다. 실제 어안에서 그 방향에는 렌즈가 접혀 생긴 상뿐이라 보통 마스크로 지워지지만, 면이 늘면 스텝마다 느려지고 이미지당 외관 슬롯을 하나 더 씁니다(양방향 그리드 메모리 약 20% 증가). 정말로 뒤까지 보는 렌즈에서만 켜세요.\n파노라마는 항상 여섯 면을 모두 씁니다."),
    DE("Ob ein über 135 Grad hinaus sehendes Objektiv zusätzlich eine nach hinten gerichtete Lochkamera-Fläche bekommt. Bei einem echten Fisheye steht dort fast nur das umgeklappte Bild des Objektivs, das ohnehin meist maskiert ist;\n- die Fläche bremst jeden Schritt und kostet je Bild einen weiteren Erscheinungs-Slot (rund 20% mehr Speicher für das bilaterale Gitter).\n- Nur einschalten, wenn das Objektiv wirklich nach hinten sieht. Ein Panorama nutzt immer alle sechs."),
    FR("Si un objectif vu au-delà de 135 degrés reçoit en plus une face sténopé tournée vers l'arrière.\n- Sur un vrai fisheye, cette direction ne contient guère que l'image repliée de l'objectif, le plus souvent masquée ;\n- la face ralentit chaque étape et coûte un emplacement d'apparence de plus par image (environ 20% de mémoire en plus pour la grille bilatérale).\n- À activer seulement pour un objectif qui voit vraiment derrière lui. Un panorama utilise toujours les six."),
    ES("Si un objetivo que ve más allá de 135 grados recibe además una cara estenopeica orientada hacia atrás.\n- En un ojo de pez real esa dirección solo contiene la imagen plegada del objetivo, casi siempre enmascarada;\n- la cara ralentiza cada paso y ocupa una ranura de apariencia más por imagen (alrededor de un 20% más de memoria para la rejilla bilateral).\n- Actívala solo con un objetivo que vea de verdad hacia atrás. Una panorámica siempre usa las seis."),
    PT("Se uma lente vista para além de 135 graus recebe também uma face estenopeica virada para trás.\n- Numa olho de peixe real essa direção contém pouco mais do que a imagem dobrada da lente, quase sempre mascarada;\n- a face atrasa cada passo e ocupa mais um espaço de aparência por imagem (cerca de 20% mais memória para a grelha bilateral). Ligue apenas com uma lente que veja mesmo para trás. Um panorama usa sempre as seis."),
    IT("Se un obiettivo che vede oltre i 135 gradi riceve anche una faccia stenopeica rivolta all'indietro.\n- In un fisheye reale quella direzione contiene quasi solo l'immagine ripiegata dell'obiettivo, di norma mascherata;\n- la faccia rallenta ogni passo e occupa uno slot di aspetto in più per immagine (circa il 20% di memoria in più per la griglia bilaterale).\n- Attivala solo con un obiettivo che veda davvero dietro di sé. Un panorama usa sempre tutte e sei."),
    NL("Of een lens die verder dan 135 graden kijkt er een naar achteren gericht pinhole-vlak bij krijgt. Bij een echte fisheye staat in die richting bijna alleen het omgevouwen beeld van de lens, dat meestal toch gemaskeerd is;\n- het vlak vertraagt elke stap en kost per beeld een extra uiterlijk-slot (ongeveer 20% meer geheugen voor het bilaterale raster).\n- Zet dit alleen aan voor een lens die echt achter zich kijkt. Een panorama gebruikt altijd alle zes."),
    RU("Получает ли объектив, видящий дальше 135 градусов, дополнительную пинхол-грань, направленную назад.\n- У настоящего фишая в этом направлении почти всегда лишь завёрнутое изображение самого объектива, которое обычно и так замаскировано; грань замедляет каждый шаг и занимает ещё один слот внешнего вида на изображение (примерно на 20% больше памяти под билатеральную сетку).\n- Включайте только для объектива, который действительно видит назад. Панорама всегда использует все шесть."),
    TR("135 dereceden geniş gören bir merceğe ayrıca arkaya bakan bir iğne deliği yüzü eklenip eklenmeyeceği. Gerçek bir balıkgözünde o yönde neredeyse yalnızca merceğin katlanmış görüntüsü bulunur ve zaten çoğunlukla maskelenir; bu yüz her adımı yavaşlatır ve görüntü başına bir görünüm yuvası daha harcar (bilateral ızgara belleğinde yaklaşık %20 artış).\nYalnızca gerçekten arkasını gören bir mercek için açın. Panorama her zaman altı yüzü de kullanır."));


// ===========================================================================
// Scene Placement
// ===========================================================================

SS_MSG(exif_orientation,
    EN("EXIF orientation"), JA("EXIF の回転情報"), ZH_HANS("EXIF 方向"),
    ZH_HANT("EXIF 方向"), KO("EXIF 방향"), DE("EXIF-Ausrichtung"),
    FR("Orientation EXIF"), ES("Orientación EXIF"),
    PT("Orientação EXIF"), IT("Orientamento EXIF"),
    NL("EXIF-oriëntatie"), RU("Ориентация EXIF"),
    TR("EXIF yönlendirmesi"));
SS_MSG(exif_orientation_help,
    EN("How training handles photo orientation from EXIF.\n- orient — use the tag to set scene up. Leave pixels unchanged.\n- apply — rotate pixels on load. Requires a reconstruction made the same way."),
    JA("学習時に写真の EXIF の向きを扱う方法です。\n- orient — タグでシーンの上方向を決めます。画素は変更しません。\n- apply — 読み込み時に画素を回転。同じ方法で再構成が必要です。"),
    ZH_HANS("设置训练如何处理照片的 EXIF 方向。\n- orient — 用标签设置场景朝上方向，不改变像素。\n- apply — 加载时旋转像素。重建必须使用相同方法。"),
    ZH_HANT("設定訓練如何處理照片的 EXIF 方向。\n- orient — 用標籤設定場景朝上方向，不改變像素。\n- apply — 載入時旋轉像素。重建必須使用相同方法。"),
    KO("학습에서 사진의 EXIF 방향을 처리하는 방법입니다.\n- orient — 태그로 장면의 위쪽을 정하고 픽셀은 유지합니다.\n- apply — 읽을 때 픽셀을 회전합니다. 재구성도 같은 방식이어야 합니다."),
    DE("EXIF-Ausrichtung der Fotos beim Training.\n- orient — Tag für Szenen-Oben nutzen. Pixel bleiben unverändert.\n- apply — Pixel beim Laden drehen. Rekonstruktion muss ebenso erstellt sein."),
    FR("Orientation EXIF des photos à l’entraînement.\n- orient — définit le haut de la scène et conserve les pixels.\n- apply — tourne les pixels au chargement. La reconstruction doit faire de même."),
    ES("Orientación EXIF de las fotos durante el entrenamiento.\n- orient — fija la vertical de la escena y conserva los píxeles.\n- apply — gira los píxeles al cargar. La reconstrucción debe hacer lo mismo."),
    PT("Orientação EXIF das fotos no treino.\n- orient — define a vertical da cena e mantém os píxeis.\n- apply — roda os píxeis ao carregar. A reconstrução deve fazer o mesmo."),
    IT("Orientamento EXIF delle foto nell’addestramento.\n- orient — imposta l’alto della scena e conserva i pixel.\n- apply — ruota i pixel al caricamento. La ricostruzione deve fare lo stesso."),
    NL("EXIF-oriëntatie van foto’s tijdens training.\n- orient — bepaalt de bovenrichting. Pixels blijven gelijk.\n- apply — draait pixels bij laden. De reconstructie moet hetzelfde doen."),
    RU("Как обучение использует ориентацию EXIF снимков.\n- orient — определяет верх сцены. Пиксели сохраняются.\n- apply — поворачивает пиксели при чтении. Реконструкция должна делать так же."),
    TR("Eğitim fotoğrafların EXIF yönünü böyle işler.\n- orient — sahnenin üstünü belirler. Pikseller değişmez.\n- apply — yüklerken pikselleri döndürür. Yeniden yapım da aynı yöntemi kullanmalı."));

SS_MSG(orientation_method,
    EN("Upright method"), JA("上向きの決め方"), ZH_HANS("摆正方式"),
    ZH_HANT("擺正方式"), KO("수직 정렬 방식"), DE("Aufrichtungsverfahren"),
    FR("Méthode de redressement"), ES("Método de enderezado"),
    PT("Método de nivelamento"), IT("Metodo di raddrizzamento"),
    NL("Methode voor rechtzetten"), RU("Способ выравнивания по вертикали"),
    TR("Dikleştirme yöntemi"));
SS_MSG(orientation_method_help,
    EN("How the scene is rotated to stand upright.\n- This only affects how the result is framed for viewing, not the splats themselves.\n- Anything other than `up` is approximated for now."),
    JA("シーンをどう回して上向きに立たせるかです。\n- 結果を見るときの見え方だけに関わり、スプラットそのものは変わりません。\n- `up` 以外は今のところ近似です。"),
    ZH_HANS("如何旋转场景使其摆正。\n- 它只影响观看时的取景方式，不改变泼溅本身。\n- 目前 `up` 以外的选项都是近似实现。"),
    ZH_HANT("如何旋轉場景使其擺正。\n- 它只影響觀看時的取景方式，不改變潑濺本身。\n- 目前 `up` 以外的選項都是近似實作。"),
    KO("장면을 어떻게 돌려 똑바로 세울지입니다.\n- 결과를 볼 때의 화면 구성에만 영향을 주고 스플랫 자체는 바뀌지 않습니다.\n- `up` 이외는 아직 근사 구현입니다."),
    DE("Wie die Szene gedreht wird, damit sie aufrecht steht.\n- Betrifft nur, wie das Ergebnis zur Ansicht gerahmt wird, nicht die Splats selbst.\n- Alles außer `up` ist derzeit eine Näherung."),
    FR("Comment la scène est tournée pour se tenir droite.\n- N'agit que sur le cadrage du résultat à l'affichage, pas sur les splats eux-mêmes.\n- Tout ce qui n'est pas `up` reste approximatif pour l'instant."),
    ES("Cómo se gira la escena para que quede derecha.\n- Solo afecta al encuadre del resultado al verlo, no a los splats en sí.\n- Todo lo que no sea `up` es por ahora una aproximación."),
    PT("Como a cena é girada para ficar em pé.\n- Só afeta o enquadramento do resultado ao visualizar, não os splats em si.\n- Tudo o que não for `up` é por enquanto aproximado."),
    IT("Come la scena viene ruotata per stare dritta.\n- Riguarda solo l'inquadratura del risultato in visualizzazione, non gli splat stessi.\n- Tutto ciò che non è `up` è per ora approssimato."),
    NL("Hoe de scène wordt gedraaid om rechtop te staan.\n- Raakt alleen de kadrering van het resultaat bij het bekijken, niet de splats zelf.\n- Alles behalve `up` is voorlopig een benadering."),
    RU("Как сцена поворачивается, чтобы стоять вертикально.\n- Влияет только на кадрирование результата при просмотре, а не на сами сплаты.\n- Всё, кроме `up`, пока приближение."),
    TR("Sahnenin dik durması için nasıl döndürüleceği.\n- Yalnızca sonucun görüntülenirken nasıl çerçevelendiğini etkiler, splat'ların kendisini değil.\n- `up` dışındaki her şey şimdilik yaklaşıktır."));

SS_MSG(center_method,
    EN("Centering method"), JA("中心の決め方"), ZH_HANS("居中方式"),
    ZH_HANT("置中方式"), KO("중심 정하는 방식"), DE("Zentrierverfahren"),
    FR("Méthode de centrage"), ES("Método de centrado"),
    PT("Método de centralização"), IT("Metodo di centratura"),
    NL("Centreermethode"), RU("Способ центрирования"), TR("Ortalama yöntemi"));
SS_MSG(center_method_help,
    EN("How the scene's origin is chosen.\n- Like the orientation setting, this affects framing rather than the splats themselves.\n- Anything other than `poses` is approximated for now."),
    JA("シーンの原点をどう決めるかです。\n- 向きの設定と同じく、スプラットそのものではなく見え方に関わります。\n- `poses` 以外は今のところ近似です。"),
    ZH_HANS("如何确定场景的原点。\n- 与朝向设置一样，它影响的是取景而不是泼溅本身。\n- 目前 `poses` 以外的选项都是近似实现。"),
    ZH_HANT("如何確定場景的原點。\n- 與朝向設定一樣，它影響的是取景而不是潑濺本身。\n- 目前 `poses` 以外的選項都是近似實作。"),
    KO("장면의 원점을 어떻게 정할지입니다.\n- 방향 설정과 마찬가지로 스플랫 자체가 아니라 화면 구성에 영향을 줍니다.\n- `poses` 이외는 아직 근사 구현입니다."),
    DE("Wie der Ursprung der Szene gewählt wird.\n- Wie bei der Ausrichtung betrifft das die Rahmung, nicht die Splats selbst.\n- Alles außer `poses` ist derzeit eine Näherung."),
    FR("Comment l'origine de la scène est choisie.\n- Comme pour l'orientation, cela concerne le cadrage et non les splats eux-mêmes.\n- Tout ce qui n'est pas `poses` reste approximatif pour l'instant."),
    ES("Cómo se elige el origen de la escena.\n- Igual que con la orientación, afecta al encuadre y no a los splats en sí.\n- Todo lo que no sea `poses` es por ahora una aproximación."),
    PT("Como a origem da cena é escolhida.\n- Tal como na orientação, isso afeta o enquadramento e não os splats em si.\n- Tudo o que não for `poses` é por enquanto aproximado."),
    IT("Come viene scelta l'origine della scena.\n- Come per l'orientamento, riguarda l'inquadratura e non gli splat stessi.\n- Tutto ciò che non è `poses` è per ora approssimato."),
    NL("Hoe de oorsprong van de scène wordt gekozen.\n- Net als bij de oriëntatie raakt dit de kadrering en niet de splats zelf.\n- Alles behalve `poses` is voorlopig een benadering."),
    RU("Как выбирается начало координат сцены.\n- Как и с ориентацией, это влияет на кадрирование, а не на сами сплаты.\n- Всё, кроме `poses`, пока приближение."),
    TR("Sahnenin başlangıç noktasının nasıl seçileceği.\n- Yönelim ayarında olduğu gibi, bu splat'ları değil çerçevelemeyi etkiler.\n- `poses` dışındaki her şey şimdilik yaklaşıktır."));

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
    EN("Scales cameras into a unit box so learning rates and penalties suit any scene size. Disable only for an already scaled dataset."),
    JA("カメラが単位サイズの箱に収まるよう拡縮し、学習率と正則化をシーンの大きさに合わせます。調整済みのデータセットでのみ無効にします。"),
    ZH_HANS("将相机缩放到单位大小的包围盒内，让学习率和正则项适用于不同尺寸的场景。仅对已缩放好的数据集关闭。"),
    ZH_HANT("將相機縮放到單位大小的包圍盒內，讓學習率與正則項適用於不同尺寸的場景。僅對已縮放好的資料集關閉。"),
    KO("카메라가 단위 상자에 들어가도록 배율을 조정해 학습률과 규제를 장면 크기에 맞춥니다. 이미 배율이 맞는 데이터셋에서만 끄세요."),
    DE("Skaliert Kameras in eine Einheitsbox, damit Lernraten und Strafen zur Szene passen. Nur bei bereits passend skalierten Datensätzen ausschalten."),
    FR("Ramène les caméras dans une boîte unitaire pour adapter taux et pénalités à la taille de scène. Désactiver seulement pour un jeu déjà mis à l’échelle."),
    ES("Escala las cámaras a una caja unitaria para adaptar tasas y penalizaciones al tamaño de escena. Desactívalo solo si el conjunto ya está escalado."),
    PT("Escala as câmeras para uma caixa unitária para adequar taxas e penalidades à cena. Desative apenas se o conjunto já estiver na escala desejada."),
    IT("Scala le camere in un volume unitario per adattare tassi e penalità alla scena. Disattivare solo per dati già alla scala desiderata."),
    NL("Schaalt camera’s naar een eenheidsvak zodat leersnelheden en straffen bij de scène passen. Alleen uitzetten als de dataset al goed geschaald is."),
    RU("Масштабирует камеры в единичный объём, согласуя шаги обучения и штрафы с размером сцены. Отключайте лишь для уже масштабированных данных."),
    TR("Öğrenme hızları ve cezalar sahneye uysun diye kameraları birim kutuya ölçekler. Yalnız ölçeği zaten uygun veri kümelerinde kapatın."));

SS_MSG(outlier_threshold,
    EN("Camera outlier threshold"), JA("外れカメラのしきい値"),
    ZH_HANS("离群相机阈值"), ZH_HANT("離群相機閾值"),
    KO("이상치 카메라 임계값"), DE("Schwelle für Ausreißerkameras"),
    FR("Seuil des caméras aberrantes"), ES("Umbral de cámaras atípicas"),
    PT("Limiar de câmeras discrepantes"), IT("Soglia delle camere anomale"),
    NL("Drempel voor uitschietercamera's"), RU("Порог выбросов среди камер"),
    TR("Aykırı kamera eşiği"));
SS_MSG(outlier_threshold_help,
    EN("Rejects cameras far from the capture. Lower values reject more bad poses that distort scene scale. Infinity keeps every camera."),
    JA("撮影範囲から遠く離れたカメラを除きます。小さい値ほど、シーンの尺度を乱す不正な姿勢を多く除きます。無限大はすべて保持します。"),
    ZH_HANS("剔除远离拍摄区域的相机。值越小，剔除的异常姿态越多，可避免场景尺度失真。无穷大保留所有相机。"),
    ZH_HANT("剔除遠離拍攝區域的相機。值越小，剔除的異常姿態越多，可避免場景尺度失真。無窮大保留所有相機。"),
    KO("촬영 영역에서 먼 카메라를 제외합니다. 낮출수록 장면 배율을 왜곡하는 잘못된 자세를 더 많이 제외합니다. 무한대는 모두 유지합니다."),
    DE("Entfernt weit entfernte Kameras. Kleinere Werte verwerfen mehr Fehlposen, die den Szenenmaßstab verzerren. Unendlich behält alle Kameras."),
    FR("Écarte les caméras loin de la capture. Une valeur faible rejette plus de poses fausses qui faussent l’échelle. L’infini conserve toutes les caméras."),
    ES("Descarta cámaras alejadas de la captura. Un valor menor quita más poses erróneas que alteran la escala. Infinito conserva todas las cámaras."),
    PT("Descarta câmeras longe da captura. Valores menores rejeitam mais poses incorretas que distorcem a escala. Infinito mantém todas as câmeras."),
    IT("Esclude le camere lontane dalla ripresa. Valori minori scartano più pose errate che alterano la scala. Infinito conserva tutte le camere."),
    NL("Verwijdert camera’s ver buiten de opname. Lager verwerpt meer foute poses die de schaal verstoren. Oneindig behoudt alle camera’s."),
    RU("Отбрасывает камеры вдали от съёмки. Меньше — строже отбор ошибочных поз, искажающих масштаб. Бесконечность сохраняет все камеры."),
    TR("Çekimden uzak kameraları eler. Düşük değerler sahne ölçeğini bozan daha fazla hatalı pozu eler. Sonsuz, tüm kameraları korur."));

SS_MSG(scene_center,
    EN("Scene centering"), JA("シーンの中心合わせ"), ZH_HANS("场景居中"),
    ZH_HANT("場景置中"), KO("장면 중심 맞추기"), DE("Szene zentrieren"),
    FR("Centrage de la scène"), ES("Centrado de la escena"),
    PT("Centralização da cena"), IT("Centratura della scena"),
    NL("Scène centreren"), RU("Центрирование сцены"), TR("Sahne ortalama"));
SS_MSG(scene_center_help,
    EN("Set the training origin. The shift is saved in scene_transform.json.\n- Useful for georeferenced scenes far from zero, where single precision loses detail.\n- auto uses the point cloud median when the origin is far relative to scene size. Otherwise keeps the frame.\n- none keeps the dataset frame."),
    JA("学習前にデータセットを移動し、この点を原点にします。移動量は出力フォルダの scene_transform.json に記録され、スプラットは移動後の座標系で学習されます。\n- 座標が原点から数百万単位も離れた地理参照付きの再構成では、単精度では細部が失われるため必要です。\n- `auto` はシーンが自身の大きさに比べて原点から遠いときだけ点群の中央値に合わせ、それ以外は座標系を変えません。\n- `none` は常にデータセット自身の座標系を使います。"),
    ZH_HANS("训练前平移数据集，使该点成为原点，并把平移量记录到输出文件夹的 scene_transform.json 中；泼溅在平移后的坐标系中训练。\n- 坐标距原点数百万单位的地理参考重建需要这样做，否则单精度会丢失细节。\n- `auto` 仅在场景相对自身大小离原点很远时以点云中位数居中，否则不改变坐标系； `none` 始终保留数据集自身的坐标系。"),
    ZH_HANT("訓練前平移資料集，使該點成為原點，並把平移量記錄到輸出資料夾的 scene_transform.json 中；潑濺在平移後的座標系中訓練。\n- 座標距原點數百萬單位的地理參考重建需要這樣做，否則單精度會遺失細節。\n- `auto` 僅在場景相對自身大小離原點很遠時以點雲中位數置中，否則不改變座標系； `none` 始終保留資料集自身的座標系。"),
    KO("학습 전에 데이터셋을 옮겨 이 점을 원점으로 삼고, 그 이동량을 출력 폴더의 scene_transform.json에 기록합니다. 스플랫은 옮겨진 좌표계에서 학습됩니다.\n- 좌표가 원점에서 수백만 단위 떨어진 지리 참조 복원에서는 단정밀도로 세부가 사라지므로 필요합니다.\n- `auto`는 장면이 자체 크기에 비해 원점에서 멀리 있을 때만 점군 중앙값으로 맞추고, 그렇지 않으면 좌표계를 그대로 둡니다. `none`은 항상 데이터셋 자체의 좌표계를 유지합니다."),
    DE("Verschiebt den Datensatz vor dem Training so, dass dieser Punkt zum Ursprung wird, und hält die Verschiebung in scene_transform.json im Ausgabeordner fest.\n- Die Splats werden im verschobenen Bezugssystem trainiert.\n- Nötig für georeferenzierte Rekonstruktionen, deren Koordinaten Millionen Einheiten vom Ursprung entfernt liegen, wo einfache Genauigkeit Details verliert. `auto` zentriert nur dann auf den Median der Punktwolke, wenn die Szene gemessen an ihrer Größe weit vom Ursprung entfernt liegt, und lässt das Bezugssystem sonst unverändert;\n- `none` behält immer das Bezugssystem des Datensatzes."),
    FR("Déplace le jeu de données avant l'entraînement pour que ce point devienne l'origine, et note le décalage dans scene_transform.json dans le dossier de sortie.\n- Les splats sont alors entraînés dans le repère décalé.\n- Nécessaire pour les reconstructions géoréférencées dont les coordonnées sont à des millions d'unités de l'origine, où la simple précision perd des détails.\n- `auto` centre sur la médiane du nuage de points seulement quand la scène est loin de l'origine au regard de sa taille, et garde le repère sinon ;\n- `none` garde toujours le repère du jeu de données."),
    ES("Desplaza el conjunto de datos antes de entrenar para que este punto sea el origen, y anota el desplazamiento en scene_transform.json en la carpeta de salida.\n- Los splats se entrenan entonces en el sistema desplazado.\n- Necesario para reconstrucciones georreferenciadas cuyas coordenadas están a millones de unidades del origen, donde la precisión simple pierde detalle.\n- `auto` centra en la mediana de la nube de puntos solo cuando la escena está lejos del origen en relación con su tamaño, y si no conserva el sistema; `none` conserva siempre el sistema propio del conjunto de datos."),
    PT("Desloca o conjunto de dados antes do treino para que este ponto vire a origem, e registra o deslocamento em scene_transform.json na pasta de saída.\n- Os splats são então treinados no referencial deslocado.\n- Necessário para reconstruções georreferenciadas cujas coordenadas ficam a milhões de unidades da origem, onde a precisão simples perde detalhe.\n- `auto` centraliza na mediana da nuvem de pontos só quando a cena está longe da origem em relação ao próprio tamanho, e caso contrário mantém o referencial; `none` mantém sempre o referencial do próprio conjunto de dados."),
    IT("Sposta il dataset prima dell'addestramento in modo che questo punto diventi l'origine, e annota lo spostamento in scene_transform.json nella cartella di output.\n- Gli splat vengono quindi addestrati nel sistema spostato.\n- Necessario per ricostruzioni georeferenziate le cui coordinate distano milioni di unità dall'origine, dove la precisione singola perde dettaglio.\n- `auto` centra sulla mediana della nuvola di punti solo quando la scena è lontana dall'origine rispetto alle sue dimensioni, altrimenti mantiene il sistema;\n- `none` mantiene sempre il sistema proprio del dataset."),
    NL("Verschuift de dataset vóór het trainen zodat dit punt de oorsprong wordt, en legt de verschuiving vast in scene_transform.json in de uitvoermap.\n- De splats worden dan in het verschoven stelsel getraind.\n- Nodig voor gegeorefereerde reconstructies waarvan de coördinaten miljoenen eenheden van de oorsprong liggen, waar enkele precisie detail verliest.\n- `auto` centreert alleen op de mediaan van de puntenwolk als de scène ver van de oorsprong ligt in verhouding tot haar grootte, en behoudt het stelsel anders;\n- `none` behoudt altijd het eigen stelsel van de dataset."),
    RU("Сдвигает набор данных перед обучением так, чтобы эта точка стала началом координат, и записывает сдвиг в scene_transform.json в папке вывода.\n- Сплаты обучаются в сдвинутой системе координат.\n- Необходимо для геопривязанных реконструкций, координаты которых отстоят от начала на миллионы единиц, где одинарная точность теряет детали.\n- `auto` центрирует по медиане облака точек, только если сцена далеко от начала координат по сравнению со своим размером, а иначе сохраняет систему;\n- `none` всегда сохраняет собственную систему набора данных."),
    TR("Eğitimden önce veri kümesini bu nokta başlangıç olacak şekilde kaydırır ve kaydırmayı çıktı klasöründeki scene_transform.json dosyasına yazar.\n- Splatlar kaydırılmış çerçevede eğitilir.\n- Koordinatları başlangıçtan milyonlarca birim uzakta olan coğrafi referanslı yeniden kurmalar için gereklidir;\n- tek duyarlık orada ayrıntı kaybeder. `auto` yalnızca sahne kendi boyutuna göre başlangıçtan uzaktaysa nokta bulutu ortancasına ortalar, değilse çerçeveyi korur;\n- `none` her zaman veri kümesinin kendi çerçevesini korur."));

SS_MSG(scene_center_threshold,
    EN("Auto-centering distance"), JA("自動中心合わせの距離"),
    ZH_HANS("自动居中距离"), ZH_HANT("自動置中距離"), KO("자동 중심 맞추기 거리"),
    DE("Abstand für automatisches Zentrieren"),
    FR("Distance du centrage automatique"), ES("Distancia del centrado automático"),
    PT("Distância da centralização automática"),
    IT("Distanza della centratura automatica"),
    NL("Afstand voor automatisch centreren"), RU("Расстояние автоцентрирования"),
    TR("Otomatik ortalama mesafesi"));
SS_MSG(scene_center_threshold_help,
    EN("Auto-centering threshold in scene radii (median distance to the median). Both the point cloud and cameras must be this far from the origin."),
    JA("自動中心合わせの閾値。シーン半径（中央値からの距離の中央値）を単位とし、点群とカメラの両方が原点からこの距離以上離れる必要があります。"),
    ZH_HANS("自动居中的阈值，以场景半径（到中位点距离的中位数）为单位。点云和相机都须离原点至少这么远。"),
    ZH_HANT("自動置中的閾值，以場景半徑（到中位點距離的中位數）為單位。點雲與相機都須離原點至少這麼遠。"),
    KO("자동 중심 이동 임계값으로, 장면 반경(중앙값까지 거리의 중앙값) 단위입니다. 점군과 카메라 모두 원점에서 이만큼 떨어져야 합니다."),
    DE("Auto-Zentrierschwelle in Szenenradien (Medianabstand zum Median). Punktwolke und Kameras müssen so weit vom Ursprung entfernt sein."),
    FR("Seuil de centrage auto en rayons de scène (distance médiane à la médiane). Nuage et caméras doivent être aussi loin de l’origine."),
    ES("Umbral de centrado auto en radios de escena (distancia mediana a la mediana). Nube y cámaras deben estar así de lejos del origen."),
    PT("Limiar de centralização automática em raios da cena (distância mediana à mediana). Nuvem e câmeras devem estar tão longe da origem."),
    IT("Soglia di centratura auto in raggi della scena (distanza mediana dalla mediana). Nuvola e camere devono distare tanto dall’origine."),
    NL("Grens voor automatisch centreren in scèneradii (mediaanafstand tot de mediaan). Puntenwolk en camera’s moeten zo ver van de oorsprong zijn."),
    RU("Порог автоцентрирования в радиусах сцены (медианное расстояние до медианы). Облако и камеры должны быть настолько далеко от начала координат."),
    TR("Otomatik merkezleme eşiği, sahne yarıçapı (ortancaya uzaklıkların ortancası) biriminde. Bulut ve kameralar orijinden bu kadar uzak olmalı."));

SS_MSG(relative_scale,
    EN("Scene scale multiplier"), JA("シーンの倍率"),
    ZH_HANS("场景缩放倍数"), ZH_HANT("場景縮放倍數"), KO("장면 배율"),
    DE("Skalierungsfaktor der Szene"), FR("Facteur d'échelle de la scène"),
    ES("Factor de escala de la escena"), PT("Fator de escala da cena"),
    IT("Fattore di scala della scena"), NL("Schaalfactor van de scène"),
    RU("Множитель масштаба сцены"), TR("Sahne ölçek çarpanı"));
SS_MSG(relative_scale_help,
    EN("Multiplies scene size before training. Increase if a large capture loses detail at a small scale. Unset lets the optimizer handle scale."),
    JA("学習前にシーンの大きさを乗算します。大規模な撮影が小さすぎて細部を失う場合に上げます。未指定なら最適化処理に任せます。"),
    ZH_HANS("训练前按此倍数缩放场景。大场景因尺度过小而丢失细节时可增大。留空则由优化器处理尺度。"),
    ZH_HANT("訓練前按此倍數縮放場景。大場景因尺度過小而丟失細節時可增大。留空則由最佳化器處理尺度。"),
    KO("학습 전 장면 크기에 곱합니다. 큰 촬영이 작은 배율로 세부를 잃으면 높이세요. 미설정 시 최적화기가 배율을 처리합니다."),
    DE("Multipliziert die Szenengröße vor dem Training. Bei Detailverlust durch zu kleinen Maßstab erhöhen. Unbelegt überlässt den Maßstab dem Optimierer."),
    FR("Multiplie la taille de scène avant l’entraînement. Augmenter si une grande capture perd du détail à petite échelle. Vide : gestion par l’optimiseur."),
    ES("Multiplica el tamaño de escena antes de entrenar. Auméntalo si una captura grande pierde detalle a escala pequeña. Sin definir, lo gestiona el optimizador."),
    PT("Multiplica o tamanho da cena antes do treino. Aumente se uma captura grande perde detalhe em escala pequena. Vazio deixa o otimizador gerir a escala."),
    IT("Moltiplica la dimensione della scena prima del training. Aumentare se una ripresa grande perde dettaglio a scala ridotta. Vuoto: gestisce l’ottimizzatore."),
    NL("Vermenigvuldigt de scènegrootte vóór training. Verhoog bij detailverlies doordat een grote opname te klein wordt. Leeg laat de optimalisatie de schaal regelen."),
    RU("Множитель размера сцены перед обучением. Увеличьте, если крупная съёмка теряет детали из-за малого масштаба. Не задано: масштабом управляет оптимизатор."),
    TR("Eğitimden önce sahne boyutunu çarpar. Büyük çekim küçük ölçekte ayrıntı kaybediyorsa artırın. Boşsa ölçeği eniyileyici yönetir."));


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
    EN("- 3dgs: standard Gaussians, widest viewer compatibility.\n- mip: less aliasing across different distances or resolutions.\n- 3dgut: more accurate wide/distorted lenses and cleaner mesh geometry."),
    JA("- 3dgs：標準ガウス。ビューアとの互換性が最も高い方式。\n- mip：距離や解像度の差によるエイリアシングを軽減。\n- 3dgut：広角・歪みのあるレンズを正確に扱い、メッシュ形状を改善。"),
    ZH_HANS("- 3dgs：标准高斯，查看器兼容性最广。\n- mip：减少不同距离或分辨率之间的锯齿。\n- 3dgut：更准确处理广角及畸变镜头，生成更干净的网格几何。"),
    ZH_HANT("- 3dgs：標準高斯，檢視器相容性最廣。\n- mip：減少不同距離或解析度之間的鋸齒。\n- 3dgut：更準確處理廣角及畸變鏡頭，產生更乾淨的網格幾何。"),
    KO("- 3dgs: 표준 가우시안으로 뷰어 호환성이 가장 넓습니다.\n- mip: 거리나 해상도 차이에 따른 앨리어싱을 줄입니다.\n- 3dgut: 광각/왜곡 렌즈를 정확하게 다루고 메시 형상을 개선합니다."),
    DE("- 3dgs: Standardgauß, breiteste Viewer-Kompatibilität.\n- mip: weniger Aliasing bei verschiedenen Entfernungen oder Auflösungen.\n- 3dgut: genauere Weitwinkel-/Verzerrungslinsen und sauberere Mesh-Geometrie."),
    FR("- 3dgs : gaussiennes standard, compatibilité maximale avec les visionneuses.\n- mip : moins d’aliasing selon la distance ou la résolution.\n- 3dgut : grand-angle et distorsion plus précis, géométrie de maillage plus propre."),
    ES("- 3dgs: gaussianas estándar, máxima compatibilidad con visores.\n- mip: menos aliasing al cambiar distancia o resolución.\n- 3dgut: lentes anchas/distorsionadas más precisas y geometría de malla más limpia."),
    PT("- 3dgs: gaussianas padrão, maior compatibilidade com visores.\n- mip: menos aliasing em distâncias ou resoluções diferentes.\n- 3dgut: lentes amplas/distorcidas mais precisas e geometria de malha mais limpa."),
    IT("- 3dgs: gaussiane standard, massima compatibilità con i visualizzatori.\n- mip: meno aliasing a distanze o risoluzioni diverse.\n- 3dgut: più preciso per lenti larghe/distorte e geometria mesh più pulita."),
    NL("- 3dgs: standaard Gauss, breedste viewercompatibiliteit.\n- mip: minder aliasing bij andere afstanden of resoluties.\n- 3dgut: nauwkeuriger voor groothoek/vervormde lenzen en schonere mesh-geometrie."),
    RU("- 3dgs: стандартные гауссианы, самая широкая совместимость с просмотрщиками.\n- mip: меньше алиасинга при смене расстояния или разрешения.\n- 3dgut: точнее для широких/искажённых линз, чище геометрия сетки."),
    TR("- 3dgs: standart Gauss, en geniş görüntüleyici uyumluluğu.\n- mip: farklı uzaklık veya çözünürlükte daha az örtüşme.\n- 3dgut: geniş/bozuk lenslerde daha doğru, ağ geometrisi daha temiz."));

SS_MSG(sh_degree,
    EN("Color detail (SH)"), JA("色の細かさ（SH）"),
    ZH_HANS("颜色细节（SH）"), ZH_HANT("顏色細節（SH）"),
    KO("색 디테일(SH)"), DE("Farbdetail (SH)"),
    FR("Détail des couleurs (SH)"), ES("Detalle de color (SH)"),
    PT("Detalhe de cor (SH)"), IT("Dettaglio del colore (SH)"),
    NL("Kleurdetail (SH)"), RU("Детальность цвета (SH)"),
    TR("Renk ayrıntısı (SH)"));
SS_MSG(sh_degree_help,
    EN("Higher degrees give sharper view-dependent reflections and shading.\n- Higher uses more memory and file space; 0 gives view-independent color.\n- Degrees 4 and above have limited support in common viewers."),
    JA("高い次数ほど、視点による反射や陰影が鮮明になります。\n- メモリとファイル容量も増えます。0は視点によらない色です。\n- 一般的なビューアでは4次以上の対応が限られます。"),
    ZH_HANS("阶数越高，视角相关的反射和明暗越清晰。\n- 更高阶数占用更多内存和文件空间；0 表示颜色与视角无关。\n- 常见查看器对 4 阶及以上的支持有限。"),
    ZH_HANT("階數越高，視角相關的反射與明暗越清晰。\n- 更高階數占用更多記憶體和檔案空間；0 表示顏色與視角無關。\n- 常見檢視器對 4 階及以上的支援有限。"),
    KO("높을수록 시점에 따른 반사와 음영이 선명합니다.\n- 높을수록 메모리와 파일 공간이 증가합니다. 0은 시점과 무관한 색입니다.\n- 일반 뷰어는 4차 이상 지원이 제한됩니다."),
    DE("Höhere Grade zeigen schärfere blickabhängige Reflexe und Schattierung.\n- Höher braucht mehr Speicher und Dateiplatz; 0 gibt blickunabhängige Farbe.\n- Grad 4 und höher wird von gängigen Viewern nur begrenzt unterstützt."),
    FR("Un degré élevé affine reflets et ombres selon la vue.\n- Il augmente mémoire et taille de fichier ; 0 donne une couleur sans effet de vue.\n- Les degrés 4 et plus sont peu pris en charge par les visionneuses courantes."),
    ES("Un grado mayor afina reflejos y sombras según la vista.\n- Usa más memoria y archivo; 0 da color independiente de la vista.\n- Los grados 4 y superiores tienen soporte limitado en visores comunes."),
    PT("Grau maior dá reflexos e sombras mais nítidos conforme a vista.\n- Usa mais memória e arquivo; 0 dá cor independente da vista.\n- Graus 4 e superiores têm suporte limitado nos visores comuns."),
    IT("Gradi maggiori danno riflessi e ombre più nitidi secondo la vista.\n- Usano più memoria e spazio; 0 dà colore indipendente dalla vista.\n- I gradi da 4 in su hanno supporto limitato nei visualizzatori comuni."),
    NL("Hogere graden geven scherpere kijkafhankelijke reflecties en schaduw.\n- Hoger kost meer geheugen en bestandsruimte; 0 geeft kijkonafhankelijke kleur.\n- Graden vanaf 4 worden beperkt ondersteund door gangbare viewers."),
    RU("Более высокая степень даёт чётче отражения и тени по ракурсу.\n- Требует больше памяти и места; 0 даёт цвет без зависимости от ракурса.\n- Степени от 4 ограниченно поддерживаются обычными просмотрщиками."),
    TR("Yüksek derece görüşe bağlı yansıma ve gölgeleri keskinleştirir.\n- Daha çok bellek ve dosya alanı kullanır; 0 görüşten bağımsız renk verir.\n- 4 ve üstü dereceler yaygın görüntüleyicilerde sınırlı desteklenir."));

SS_MSG(sb_lobes,
    EN("Highlight lobes (SB)"), JA("ハイライトのローブ（SB）"),
    ZH_HANS("高光波瓣（SB）"), ZH_HANT("高光波瓣（SB）"),
    KO("하이라이트 로브(SB)"), DE("Glanzlicht-Keulen (SB)"),
    FR("Lobes de reflets (SB)"), ES("Lóbulos de brillo (SB)"),
    PT("Lóbulos de brilho (SB)"), IT("Lobi di riflesso (SB)"),
    NL("Glanslobben (SB)"), RU("Лепестки бликов (SB)"),
    TR("Parlama lobları (SB)"));
SS_MSG(sb_lobes_help,
    EN("Spherical-beta lobes per splat, from Deformable Beta Splatting: each adds a sharp highlight that follows the camera, for six numbers.\n- 2 is the published setting. Unless the SH degree is set as well it drops to 0, so the lobes replace the SH bands.\n- Vulkan builds only; viewers that do not know the lobes show the base color."),
    JA("Deformable Beta Splatting の球面ベータのローブを、スプラットごとにいくつ持たせるかです。\n- ローブ1つは6つの数値で、カメラに合わせて動く鋭いハイライトを加えます。 論文の設定は 2 です。\n- SH の次数も指定しない限り次数は 0 になり、ローブが SH の帯を置き換えます。 Vulkan 版のみ。\n- ローブに対応しないビューアでは基本色だけが表示されます。"),
    ZH_HANS("每个泼溅带多少个球面 Beta 波瓣（来自 Deformable Beta Splatting）：每个波瓣用六个数，加上一处随相机移动的锐利高光。\n- 论文的设置是 2。 除非同时设置了 SH 阶数，否则它会降为 0，由波瓣取代 SH 频带。 仅限 Vulkan 版本；\n- 不认识波瓣的查看器只显示基础颜色。"),
    ZH_HANT("每個潑濺帶多少個球面 Beta 波瓣（來自 Deformable Beta Splatting）：每個波瓣用六個數，加上一處隨相機移動的銳利高光。\n- 論文的設定是 2。 除非同時設定了 SH 階數，否則它會降為 0，由波瓣取代 SH 頻帶。 僅限 Vulkan 版本；\n- 不認得波瓣的檢視器只顯示基礎顏色。"),
    KO("스플랫마다 둘 구면 베타 로브의 수입니다(Deformable Beta Splatting).\n- 로브 하나는 숫자 여섯 개로 카메라를 따라 움직이는 날카로운 하이라이트를 더합니다. 논문의 설정은 2입니다.\n- SH 차수도 지정하지 않으면 차수는 0이 되어 로브가 SH 대역을 대신합니다.\n- Vulkan 빌드 전용이며, 로브를 모르는 뷰어는 기본 색만 보여 줍니다."),
    DE("Sphärische Beta-Keulen je Splat, aus Deformable Beta Splatting: jede fügt für sechs Zahlen ein scharfes Glanzlicht hinzu, das der Kamera folgt.\n- 2 ist die veröffentlichte Einstellung.\n- Sofern der SH-Grad nicht ebenfalls gesetzt ist, sinkt er auf 0, und die Keulen ersetzen die SH-Bänder.\n- Nur Vulkan-Builds; Betrachter, die die Keulen nicht kennen, zeigen die Grundfarbe."),
    FR("Lobes bêta sphériques par splat, issus de Deformable Beta Splatting : chacun ajoute, pour six nombres, un reflet net qui suit la caméra.\n- 2 est le réglage publié. Sauf si le degré SH est aussi fixé, il passe à 0 et les lobes remplacent les bandes SH.\n- Builds Vulkan uniquement ; les visionneuses qui ignorent les lobes montrent la couleur de base."),
    ES("Lóbulos beta esféricos por splat, de Deformable Beta Splatting: cada uno añade, por seis números, un brillo nítido que sigue a la cámara.\n- 2 es el ajuste publicado.\n- Salvo que también se fije el grado SH, este baja a 0 y los lóbulos sustituyen a las bandas SH.\n- Solo en las compilaciones Vulkan; los visores que no conocen los lóbulos muestran el color base."),
    PT("Lóbulos beta esféricos por splat, de Deformable Beta Splatting: cada um acrescenta, por seis números, um brilho nítido que acompanha a câmera.\n- 2 é a configuração publicada.\n- A menos que o grau SH também seja definido, ele cai para 0 e os lóbulos substituem as bandas SH.\n- Apenas nas compilações Vulkan; visualizadores que não conhecem os lóbulos mostram a cor base."),
    IT("Lobi beta sferici per splat, da Deformable Beta Splatting: ognuno aggiunge, per sei numeri, un riflesso netto che segue la fotocamera.\n- 2 è l'impostazione pubblicata.\n- Se non si imposta anche il grado SH, questo scende a 0 e i lobi sostituiscono le bande SH. Solo nelle build Vulkan;\n- i visualizzatori che non conoscono i lobi mostrano il colore base."),
    NL("Sferische bètalobben per splat, uit Deformable Beta Splatting: elke lob voegt voor zes getallen een scherpe glans toe die de camera volgt.\n- 2 is de gepubliceerde instelling.\n- Tenzij de SH-graad ook is ingesteld, zakt die naar 0 en vervangen de lobben de SH-banden. Alleen in Vulkan-builds;\n- viewers die de lobben niet kennen tonen de basiskleur."),
    RU("Сферические бета-лепестки на сплат из Deformable Beta Splatting: каждый за шесть чисел добавляет резкий блик, следующий за камерой.\n- В статье используется 2. Если порядок SH не задан явно, он падает до 0, и лепестки заменяют полосы SH.\n- Только в сборках Vulkan; просмотрщики, не знающие лепестков, показывают базовый цвет."),
    TR("Deformable Beta Splatting'ten splat başına küresel beta lobları: her biri altı sayı karşılığında kamerayı izleyen keskin bir parlama ekler.\n- Yayımlanan ayar 2'dir. SH derecesi de ayarlanmadıkça 0'a düşer ve loblar SH bantlarının yerini alır.\n- Yalnızca Vulkan derlemeleri; lobları tanımayan görüntüleyiciler temel rengi gösterir."));
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
    EN("Steps between increases in view-dependent color detail. Gradual increases stabilize early training; small values reach full detail sooner."),
    JA("視点依存の色の細かさを上げる間隔（ステップ数）。徐々に上げると初期学習が安定し、小さい値では早く最大の細かさに達します。"),
    ZH_HANS("视角相关颜色细节逐级增加的步数间隔。逐步增加可稳定早期训练；值越小越早达到完整细节。"),
    ZH_HANT("視角相關色彩細節逐級增加的步數間隔。逐步增加可穩定早期訓練；值越小越早達到完整細節。"),
    KO("시점에 따른 색 세부를 높이는 스텝 간격입니다. 점진적으로 높이면 초기 학습이 안정되며, 작을수록 최대 세부에 빨리 도달합니다."),
    DE("Schritte zwischen Stufen des blickabhängigen Farbdetails. Allmähliche Steigerung stabilisiert den Start; kleine Werte erreichen volle Details früher."),
    FR("Pas entre les hausses du détail de couleur selon la vue. Une hausse progressive stabilise le début ; de petites valeurs atteignent le maximum plus tôt."),
    ES("Pasos entre aumentos del detalle de color según la vista. La subida gradual estabiliza el inicio; valores pequeños alcanzan antes el detalle completo."),
    PT("Passos entre aumentos do detalhe de cor por vista. A subida gradual estabiliza o início; valores menores chegam ao detalhe completo mais cedo."),
    IT("Passi tra gli aumenti del dettaglio cromatico dipendente dalla vista. La crescita graduale stabilizza l’inizio; valori piccoli arrivano prima al massimo."),
    NL("Stappen tussen verhogingen van kijkafhankelijk kleurdetail. Geleidelijk verhogen stabiliseert de start; kleine waarden bereiken eerder het volle detail."),
    RU("Шаги между повышениями детализации цвета по ракурсу. Постепенный рост стабилизирует начало; малые значения быстрее дают полную детализацию."),
    TR("Görüşe bağlı renk ayrıntısındaki artışlar arasındaki adım sayısı. Kademeli artış başlangıcı dengeler; küçük değerler tam ayrıntıya erken ulaşır."));

SS_MSG(background_mode,
    EN("Background"), JA("背景"), ZH_HANS("背景"), ZH_HANT("背景"),
    KO("배경"), DE("Hintergrund"), FR("Arrière-plan"), ES("Fondo"),
    PT("Fundo"), IT("Sfondo"), NL("Achtergrond"), RU("Фон"), TR("Arka plan"));
SS_MSG(background_mode_help,
    EN("Fill pixels that splats do not cover.\n- color uses one chosen color. Black adds no cost.\n- noise, pseudorandom and random change pixels, tiles or the whole frame each step to discourage translucency.\n- sh learns a skybox for distant background."),
    JA("スプラットが覆わない画素を何で埋めるかです。 `color` が通常の選択で、選んだ一つの色を使います。\n- 黒なら何の処理もしません。 `noise` と `pseudorandom` と `random` はどれも、覆われていない画素の色が毎ステップ変わるようにして半透明な面を抑えます。色のまとまりの大きさが違い、画素ごと、鮮やかなタイル、画面全体と大きくなります。\n- 大きいほど損失に平均化されず効き目が強くなります。 `sh` は空を学習し、遠くの背景を無視せず表現します。"),
    ZH_HANS("用什么填充没有泼溅覆盖的像素。 `color` 是通常的选择，用你指定的一种颜色；黑色则完全不做处理。\n- `noise`、`pseudorandom` 和 `random` 都通过让未覆盖像素的颜色每步都变来抑制半透明表面；\n- 区别在于色块的大小，从逐像素到鲜艳的色块再到整帧一色，越大损失越难把它平均掉，压得也越狠。\n- `sh` 会学习一个天空盒，让远处背景被表示而不是被忽略。"),
    ZH_HANT("用什麼填充沒有潑濺覆蓋的像素。 `color` 是通常的選擇，用你指定的一種顏色；黑色則完全不做處理。\n- `noise`、`pseudorandom` 和 `random` 都透過讓未覆蓋像素的顏色每步都變來抑制半透明表面；\n- 區別在於色塊的大小，從逐像素到鮮豔的色塊再到整幀一色，越大損失越難把它平均掉，壓得也越狠。\n- `sh` 會學習一個天空盒，讓遠處背景被表示而不是被忽略。"),
    KO("스플랫이 덮지 않은 픽셀을 무엇으로 채울지입니다. `color`가 보통의 선택으로, 직접 고른 한 가지 색을 씁니다. 검정이면 아무 처리도 하지 않습니다.\n- `noise`와 `pseudorandom`과 `random`은 모두 덮이지 않은 픽셀의 색이 매 스텝 바뀌게 해서 반투명한 표면을 억제합니다.\n- 색 덩어리의 크기가 다른데, 픽셀 단위에서 선명한 타일, 화면 전체 한 색으로 커집니다.\n- 클수록 손실이 평균으로 지워 버리지 못해 더 세게 누릅니다.\n- `sh`는 스카이박스를 학습해 먼 배경을 무시하지 않고 표현합니다."),
    DE("Was Pixel füllt, die kein Splat bedeckt. `color` ist die übliche Wahl: eine Farbe nach Wahl, und Schwarz kostet gar nichts. `noise`, `pseudorandom` und `random` entmutigen alle halbdurchsichtige Flächen, indem ein unbedecktes Pixel auf einer Farbe landet, die sich jeden Schritt ändert;\n- sie unterscheiden sich in der Fleckengröße -- pro Pixel, kräftige Kacheln oder eine Farbe für das ganze Bild -- und je größer, desto weniger kann der Verlust sie wegmitteln.\n- `sh` lernt eine Skybox, damit ferner Hintergrund dargestellt statt ignoriert wird."),
    FR("Ce qui remplit les pixels qu'aucun splat ne couvre. `color` est le choix habituel : une couleur de votre choix, et le noir ne coûte rien.\n- `noise`, `pseudorandom` et `random` découragent tous les surfaces à demi transparentes en faisant tomber un pixel non couvert sur une couleur qui change à chaque étape ;\n- ils diffèrent par la taille des taches -- par pixel, tuiles vives, ou une seule couleur pour toute l'image -- et plus elles sont grandes, moins la perte peut les moyenner.\n- `sh` apprend une skybox pour que l'arrière-plan lointain soit représenté au lieu d'être ignoré."),
    ES("Qué rellena los píxeles que ningún splat cubre. `color` es la elección habitual: un color que eliges, y el negro no cuesta nada.\n- `noise`, `pseudorandom` y `random` desincentivan las superficies semitransparentes haciendo que un píxel sin cubrir caiga sobre un color que cambia en cada paso;\n- se diferencian en el tamaño de las manchas -- por píxel, baldosas vivas o un solo color para todo el fotograma -- y cuanto mayores, menos puede promediarlas la pérdida.\n- `sh` aprende un cielo para que el fondo lejano quede representado en vez de ignorado."),
    PT("O que preenche os pixels que nenhum splat cobre. `color` é a escolha habitual: uma cor à sua escolha, e o preto não custa nada. `noise`, `pseudorandom` e `random` desencorajam superfícies semitransparentes fazendo um pixel descoberto cair sobre uma cor que muda a cada passo;\n- diferem no tamanho das manchas -- por pixel, ladrilhos vivos ou uma só cor para o quadro inteiro -- e quanto maiores, menos a perda consegue mediá-las.\n- `sh` aprende um céu para que o fundo distante seja representado em vez de ignorado."),
    IT("Che cosa riempie i pixel che nessuno splat copre. `color` è la scelta abituale: un colore a tua scelta, e il nero non costa nulla. `noise`, `pseudorandom` e `random` scoraggiano le superfici semitrasparenti facendo cadere un pixel scoperto su un colore che cambia a ogni passo;\n- differiscono per la dimensione delle chiazze -- per pixel, piastrelle vivaci o un solo colore per l'intero fotogramma -- e più sono grandi, meno la perdita può mediarle.\n- `sh` impara un cielo perché lo sfondo lontano sia rappresentato invece che ignorato."),
    NL("Wat pixels vult die geen splat bedekt. `color` is de gebruikelijke keuze: een kleur naar keuze, en zwart kost helemaal niets.\n- `noise`, `pseudorandom` en `random` ontmoedigen allemaal halfdoorzichtige oppervlakken doordat een onbedekte pixel op een kleur valt die elke stap verandert;\n- ze verschillen in de grootte van de vlekken -- per pixel, felle tegels of één kleur voor het hele beeld -- en hoe groter, hoe minder het verlies ze kan wegmiddelen.\n- `sh` leert een skybox zodat verre achtergrond wordt weergegeven in plaats van genegeerd."),
    RU("Чем заполняются пиксели, которые не покрыл ни один сплат. `color` — обычный выбор: любой заданный вами цвет, а чёрный не стоит ничего. `noise`, `pseudorandom` и `random` мешают полупрозрачным поверхностям: непокрытый пиксель попадает на цвет, меняющийся каждый шаг; они отличаются размером пятен — на пиксель, яркие плитки или один цвет на весь кадр — и чем крупнее, тем хуже функция потерь их усредняет.\n`sh` обучает скайбокс, чтобы дальний фон был представлен, а не проигнорирован."),
    TR("Hiçbir splat'ın kaplamadığı pikselleri neyin dolduracağı. `color` alışılmış seçimdir: seçtiğiniz bir renk, siyah ise hiçbir şeye mal olmaz. `noise`, `pseudorandom` ve `random` yarı saydam yüzeyleri caydırır:\n- kaplanmamış bir piksel her adımda değişen bir renge düşer;\n- benek boyutlarıyla ayrılırlar -- piksel başına, canlı karolar ya da tüm kare için tek renk -- ve büyüdükçe kayıp bunları ortalamayla silemez. `sh` bir gökyüzü öğrenir, böylece uzak arka plan yok sayılmak yerine temsil edilir."));

SS_MSG(background_color,
    EN("Background color"), JA("背景色"), ZH_HANS("背景颜色"),
    ZH_HANT("背景顏色"), KO("배경색"), DE("Hintergrundfarbe"),
    FR("Couleur de l'arrière-plan"), ES("Color del fondo"),
    PT("Cor do fundo"), IT("Colore dello sfondo"), NL("Achtergrondkleur"),
    RU("Цвет фона"), TR("Arka plan rengi"));
SS_MSG(background_color_help,
    EN("The color that fills pixels no splat covers. Black is free -- it skips "
       "the blend entirely. Only used with the `color` background."),
    JA("スプラットが覆わない画素を埋める色です。黒なら合成そのものを省くので費"
       "用はかかりません。背景が `color` のときだけ使われます。"),
    ZH_HANS("用来填充没有泼溅覆盖的像素的颜色。黑色是免费的，会整个跳过混合。"
            "仅在背景为 `color` 时使用。"),
    ZH_HANT("用來填充沒有潑濺覆蓋的像素的顏色。黑色是免費的，會整個跳過混合。"
            "僅在背景為 `color` 時使用。"),
    KO("스플랫이 덮지 않은 픽셀을 채우는 색입니다. 검정은 합성 자체를 건너뛰므"
       "로 비용이 없습니다. 배경이 `color`일 때만 쓰입니다."),
    DE("Die Farbe, die Pixel füllt, die kein Splat bedeckt. Schwarz ist "
       "kostenlos -- es überspringt die Überblendung ganz. Wird nur mit dem "
       "Hintergrund `color` verwendet."),
    FR("La couleur qui remplit les pixels qu'aucun splat ne couvre. Le noir "
       "est gratuit : il saute entièrement le mélange. Utilisée "
       "uniquement avec l'arrière-plan `color`."),
    ES("El color que rellena los píxeles que ningún splat cubre. El negro es "
       "gratis: se salta la mezcla por completo. Solo se usa "
       "con el fondo `color`."),
    PT("A cor que preenche os pixels que nenhum splat cobre. O preto é de "
       "graça: ignora a mistura por completo. Só é usada com o "
       "fundo `color`."),
    IT("Il colore che riempie i pixel che nessuno splat copre. Il nero è "
       "gratis: salta del tutto la fusione. Usato solo con "
       "lo sfondo `color`."),
    NL("De kleur die pixels vult die geen splat bedekt. Zwart is gratis: het "
       "slaat het mengen helemaal over. Wordt alleen bij "
       "achtergrond `color` gebruikt."),
    RU("Цвет, которым заполняются пиксели, не покрытые ни одним сплатом. "
       "Чёрный бесплатен: смешивание пропускается целиком. "
       "Используется только с фоном `color`."),
    TR("Hiçbir splat'ın kaplamadığı pikselleri dolduran renk. Siyah "
       "bedavadır: harmanlamayı tümüyle atlar. Yalnızca "
       "`color` arka planıyla kullanılır."));

SS_MSG(background_export_height,
    EN("Background export height"), JA("背景の出力高さ"), ZH_HANS("背景导出高度"),
    ZH_HANT("背景匯出高度"), KO("배경 내보내기 높이"), DE("Höhe des Hintergrundexports"),
    FR("Hauteur d'export de l'arrière-plan"), ES("Altura de exportación del fondo"),
    PT("Altura de exportação do fundo"), IT("Altezza di esportazione dello sfondo"),
    NL("Hoogte van de achtergrondexport"), RU("Высота экспорта фона"), TR("Arka plan dışa aktarma yüksekliği"));
SS_MSG(background_export_height_help,
    EN("Height in pixels of background.webp saved with SH checkpoints (1–8192). Width is twice the height."),
    JA("SHチェックポイントと保存する background.webp の高さ（ピクセル、1–8192）。幅は高さの2倍です。"),
    ZH_HANS("随 SH 检查点保存的 background.webp 的像素高度（1–8192）。宽度为高度的两倍。"),
    ZH_HANT("隨 SH 檢查點儲存的 background.webp 的像素高度（1–8192）。寬度為高度的兩倍。"),
    KO("SH 체크포인트와 함께 저장되는 background.webp의 픽셀 높이(1–8192). 너비는 높이의 두 배입니다."),
    DE("Höhe der mit SH-Checkpoints gespeicherten background.webp in Pixeln (1–8192). Die Breite ist doppelt so groß."),
    FR("Hauteur en pixels de background.webp enregistré avec les checkpoints SH (1–8192). La largeur est le double."),
    ES("Altura en píxeles de background.webp guardado con los checkpoints SH (1–8192). El ancho es el doble."),
    PT("Altura em pixels de background.webp salvo com os checkpoints SH (1–8192). A largura é o dobro."),
    IT("Altezza in pixel di background.webp salvato con i checkpoint SH (1–8192). La larghezza è il doppio."),
    NL("Hoogte in pixels van background.webp bij SH-checkpoints (1–8192). De breedte is tweemaal de hoogte."),
    RU("Высота background.webp, сохраняемого с контрольными точками SH, в пикселях (1–8192). Ширина вдвое больше высоты."),
    TR("SH kontrol noktalarıyla kaydedilen background.webp dosyasının piksel yüksekliği (1–8192). Genişlik yüksekliğin iki katıdır."));

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
       "used with the `noise` / `pseudorandom` / `random` backgrounds."),
    JA("背景ノイズが最大の強さになるまでのステップ数です。背景が `noise` / "
       "`pseudorandom` / `random` のときだけ使われます。"),
    ZH_HANS("背景噪声达到最大强度所需的步数。仅在背景为 `noise` / "
            "`pseudorandom` / `random` 时使用。"),
    ZH_HANT("背景雜訊達到最大強度所需的步數。僅在背景為 `noise` / "
            "`pseudorandom` / `random` 時使用。"),
    KO("배경 노이즈가 최대 세기에 이르기까지의 스텝 수입니다. 배경이 `noise` /"
       " `pseudorandom` / `random`일 때만 쓰입니다."),
    DE("Wie viele Schritte das Hintergrundrauschen braucht, um volle Stärke zu "
       "erreichen. Wird nur mit den Hintergründen `noise` / `pseudorandom` / "
       "`random` verwendet."),
    FR("Combien d'étapes le bruit de fond met à atteindre sa pleine force. Utilisé "
       "uniquement avec les arrière-plans `noise` / `pseudorandom` / `random`."),
    ES("Cuántos pasos tarda el ruido de fondo en alcanzar toda su fuerza. Solo "
       "se usa con los fondos `noise` / `pseudorandom` / `random`."),
    PT("Quantos passos o ruído de fundo leva para atingir força total. Só é usado "
       "com os fundos `noise` / `pseudorandom` / `random`."),
    IT("Quanti passi impiega il rumore di fondo a raggiungere la piena forza. "
       "Usato solo con gli sfondi `noise` / `pseudorandom` / `random`."),
    NL("Hoeveel stappen de achtergrondruis nodig heeft om op volle sterkte te "
       "komen. Wordt alleen bij de achtergronden `noise` / `pseudorandom` / "
       "`random` gebruikt."),
    RU("За сколько шагов фоновый шум набирает полную силу. Используется только "
       "с фонами `noise` / `pseudorandom` / `random`."),
    TR("Arka plan gürültüsünün tam güce ulaşması için gereken adım sayısı. Yalnızca "
       "`noise` / `pseudorandom` / `random` arka planlarıyla kullanılır."));

SS_MSG(background_match_luminance,
    EN("Match background to image brightness"), JA("背景を画像の明るさに合わせる"),
    ZH_HANS("背景亮度匹配图像"), ZH_HANT("背景亮度匹配圖像"),
    KO("배경을 이미지 밝기에 맞춤"), DE("Hintergrund an Bildhelligkeit anpassen"),
    FR("Adapter le fond à la luminosité de l'image"),
    ES("Ajustar el fondo al brillo de la imagen"),
    PT("Ajustar o fundo ao brilho da imagem"),
    IT("Adatta lo sfondo alla luminosità dell'immagine"),
    NL("Achtergrond aan beeldhelderheid aanpassen"),
    RU("Подогнать фон под яркость снимка"),
    TR("Arka planı görüntü parlaklığına uydur"));
SS_MSG(background_match_luminance_help,
    EN("Matches random background brightness to each photo’s exposure-corrected mean, reducing haze. Applies only to noise, pseudorandom and random backgrounds."),
    JA("ランダム背景の明るさを写真ごとの露出補正済み平均に合わせ、かすみを抑えます。noise、pseudorandom、random の背景だけに適用します。"),
    ZH_HANS("将随机背景亮度匹配到各照片经曝光校正后的平均值，减少雾状伪影。仅用于 noise、pseudorandom 和 random 背景。"),
    ZH_HANT("將隨機背景亮度匹配到各照片經曝光校正後的平均值，減少霧狀偽影。僅用於 noise、pseudorandom 和 random 背景。"),
    KO("무작위 배경 밝기를 각 사진의 노출 보정된 평균에 맞춰 안개를 줄입니다. noise, pseudorandom, random 배경에만 적용됩니다."),
    DE("Passt die Helligkeit zufälliger Hintergründe an den belichtungskorrigierten Fotomittelwert an, gegen Dunst. Nur für noise, pseudorandom und random."),
    FR("Adapte le fond aléatoire à la luminosité moyenne corrigée d’exposition de chaque photo pour réduire la brume. Fonds noise, pseudorandom et random seulement."),
    ES("Ajusta el brillo del fondo aleatorio a la media corregida por exposición de cada foto para reducir neblina. Solo noise, pseudorandom y random."),
    PT("Ajusta o brilho do fundo aleatório à média corrigida pela exposição de cada foto para reduzir névoa. Só noise, pseudorandom e random."),
    IT("Adatta il fondo casuale alla luminosità media corretta per esposizione di ogni foto, riducendo la foschia. Solo noise, pseudorandom e random."),
    NL("Stemt willekeurig achtergrondlicht af op de belichtingsgecorrigeerde fotogemiddelden, tegen nevel. Alleen bij noise, pseudorandom en random."),
    RU("Согласует яркость случайного фона со средней яркостью фото с учётом экспозиции, уменьшая дымку. Только для noise, pseudorandom и random."),
    TR("Rastgele arka plan parlaklığını fotoğrafın pozlama düzeltilmiş ortalamasına uydurarak sisi azaltır. Yalnız noise, pseudorandom ve random için."));
SS_MSG(noise_gate_peak_weight,
    EN("Hidden splats explore"), JA("隠れたスプラットも探索"),
    ZH_HANS("被遮挡的泼溅也探索"), ZH_HANT("被遮擋的潑濺也探索"),
    KO("가려진 스플랫도 탐색"), DE("Verdeckte Splats suchen weiter"),
    FR("Les splats cachés explorent"), ES("Los splats ocultos exploran"),
    PT("Splats ocultos exploram"), IT("Gli splat nascosti esplorano"),
    NL("Verborgen splats zoeken verder"), RU("Скрытые сплаты тоже ищут место"),
    TR("Gizli splat'lar da yer arar"));
SS_MSG(noise_gate_peak_weight_help,
    EN("Adds exploration noise based on peak opacity × transmittance instead of opacity alone, so hidden opaque splats can move like faint ones."),
    JA("不透明度だけでなく、最大の不透明度×透過率に基づいて探索ノイズを加えます。隠れた不透明スプラットも薄いものと同様に動けます。"),
    ZH_HANS("按峰值不透明度 × 透射率添加探索噪声，而非只看不透明度，使被遮挡的不透明点也能像淡点一样移动。"),
    ZH_HANT("按峰值不透明度 × 透射率加入探索雜訊，而非只看不透明度，使被遮擋的不透明點也能像淡點一樣移動。"),
    KO("불투명도만 보지 않고 최대 불투명도 × 투과율로 탐색 노이즈를 적용해, 가려진 불투명 스플랫도 희미한 것처럼 움직이게 합니다."),
    DE("Nutzt maximale Opazität × Transmission statt nur Opazität für Erkundungsrauschen. So bewegen sich verdeckte, dichte Splats wie schwache."),
    FR("Base le bruit d’exploration sur opacité maximale × transmission plutôt que sur l’opacité seule, pour déplacer aussi les splats opaques cachés."),
    ES("Aplica ruido de exploración según opacidad máxima × transmitancia, no solo opacidad, para que los splats opacos ocultos se muevan como los tenues."),
    PT("Aplica ruído de exploração por opacidade máxima × transmitância, não só opacidade, para mover splats opacos ocultos como os fracos."),
    IT("Usa opacità massima × trasmittanza, non la sola opacità, per il rumore esplorativo: gli splat opachi nascosti si muovono come quelli deboli."),
    NL("Gebruikt piekopaciteit × transmissie voor verkenningsruis, in plaats van alleen opaciteit, zodat verborgen dichte splats als zwakke kunnen bewegen."),
    RU("Шум поиска зависит от пика непрозрачность × пропускание, а не лишь непрозрачности: скрытые плотные сплаты двигаются как слабые."),
    TR("Keşif gürültüsünde yalnız opaklık yerine tepe opaklık × geçirgenlik kullanır; gizli opak splatlar da soluk olanlar gibi hareket edebilir."));
SS_MSG(viewer_kernel_fraction,
    EN("Web viewer kernel share"), JA("Webビューアのカーネルの割合"),
    ZH_HANS("网页查看器核函数比例"), ZH_HANT("網頁檢視器核函數比例"),
    KO("웹 뷰어 커널 비율"), DE("Anteil des Web-Viewer-Kernels"),
    FR("Part du noyau des visionneuses web"),
    ES("Proporción del núcleo de visores web"),
    PT("Proporção do núcleo de visualizadores web"),
    IT("Quota del kernel dei visualizzatori web"),
    NL("Aandeel van de webviewer-kernel"), RU("Доля ядра веб-просмотрщиков"),
    TR("Web görüntüleyici çekirdeğinin payı"));
SS_MSG(viewer_kernel_fraction_help,
    EN("Share of steps using the PlayCanvas/SuperSplat cutoff (~2.8 sigma, falloff rescaled to zero). 0 uses full Gaussians throughout."),
    JA("PlayCanvas/SuperSplat方式（約2.8シグマで打ち切り、減衰をゼロに再調整）で学習するステップの割合。0なら常に完全なガウス分布を使います。"),
    ZH_HANS("使用 PlayCanvas/SuperSplat 截断方式的步数比例（约 2.8 sigma，衰减重缩放至零）。0 表示始终使用完整高斯。"),
    ZH_HANT("使用 PlayCanvas/SuperSplat 截斷方式的步數比例（約 2.8 sigma，衰減重縮放至零）。0 表示始終使用完整高斯。"),
    KO("PlayCanvas/SuperSplat 방식(약 2.8 시그마에서 절단, 감쇠를 0으로 재조정)을 쓰는 스텝 비율입니다. 0은 항상 전체 가우시안을 씁니다."),
    DE("Anteil der Schritte mit PlayCanvas-/SuperSplat-Grenze (~2.8 Sigma, Abfall auf null skaliert). 0 verwendet stets volle Gaußfunktionen."),
    FR("Part des pas avec la coupure PlayCanvas/SuperSplat (~2.8 sigma, décroissance ramenée à zéro). 0 utilise toujours les gaussiennes complètes."),
    ES("Fracción de pasos con el corte PlayCanvas/SuperSplat (~2.8 sigma, caída reescalada a cero). 0 usa gaussianas completas siempre."),
    PT("Fração de passos com corte PlayCanvas/SuperSplat (~2.8 sigma, queda reescalada a zero). 0 usa sempre gaussianas completas."),
    IT("Quota di passi con taglio PlayCanvas/SuperSplat (~2.8 sigma, decadimento riscalato a zero). 0 usa sempre gaussiane complete."),
    NL("Aandeel stappen met PlayCanvas/SuperSplat-afkap (~2.8 sigma, afval naar nul geschaald). 0 gebruikt altijd volledige Gaussfuncties."),
    RU("Доля шагов с обрезкой PlayCanvas/SuperSplat (~2.8 сигма, спад масштабирован до нуля). 0 всегда использует полные гауссианы."),
    TR("PlayCanvas/SuperSplat kesimi kullanan adım oranı (~2.8 sigma, düşüş sıfıra ölçeklenir). 0 her zaman tam Gauss kullanır."));
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

SS_MSG(seed_pointcloud,
    EN("Seed point cloud PLY"), JA("初期点群 PLY"),
    ZH_HANS("初始化点云 PLY"), ZH_HANT("初始化點雲 PLY"), KO("초기 점 구름 PLY"),
    DE("Startpunktwolke PLY"), FR("Nuage de points initial PLY"),
    ES("Nube de puntos inicial PLY"), PT("Nuvem de pontos inicial PLY"),
    IT("Nuvola di punti iniziale PLY"), NL("Startpuntenwolk PLY"),
    RU("Начальное облако точек PLY"), TR("Başlangıç nokta bulutu PLY"));
SS_MSG(seed_pointcloud_help,
    EN("Choose the point cloud used for training seeds.\n- Empty uses dense/roma.ply if available, otherwise dataset points. sparse always uses dataset points.\n- A path selects an XYZ + RGB point cloud PLY in the same source frame as the cameras. Relative paths start at the dataset folder.\n- Centering and scaling apply. random_init=always overrides the cloud. init_ply uses it with init_ply_add_points.\n- Resume restores checkpoint splats."),
    JA("空欄では、データセットに完成した高密度点群（dense/roma.ply）があればそれを、なければ自身の点群を使います。 `sparse` は常に自身の点群です。パスを指定した場合：カメラと同じ元の座標系の XYZ + RGB PLY で初期点群を置き換えます。\n- 相対パスはデータセット基準です。中心移動とスケールは両方に適用されます。学習済みスプラットではなく点群です。\n- random_init=always はこれを上書きし、init_ply は init_ply_add_points の場合のみ使用します。\n- 再開時はチェックポイントを復元します。"),
    ZH_HANS("留空时，若数据集有已完成的稠密点云（dense/roma.ply）则使用它，否则使用自带点云； `sparse` 始终使用自带点云。\n- 指定路径时：用与相机处于同一原始坐标系的 XYZ + RGB PLY 替换数据集的初始化点云。相对路径以数据集目录为基准，居中和缩放同时作用于点云与相机。\n- 这是普通点云，不是训练好的高斯 PLY。 random_init=always 会覆盖它； init_ply 仅在 init_ply_add_points 开启时使用它。\n- 恢复训练使用检查点中的高斯。"),
    ZH_HANT("留空時，若資料集有已完成的稠密點雲（dense/roma.ply）則使用它，否則使用自帶點雲； `sparse` 始終使用自帶點雲。\n- 指定路徑時：用與相機處於同一原始座標系的 XYZ + RGB PLY 取代資料集的初始化點雲。相對路徑以資料集目錄為基準，置中和縮放同時作用於點雲與相機。\n- 這是普通點雲，不是訓練好的高斯 PLY。 random_init=always 會覆蓋它； init_ply 僅在 init_ply_add_points 開啟時使用它。\n- 恢復訓練使用檢查點中的高斯。"),
    KO("비워 두면 데이터셋에 완성된 고밀도 점 구름(dense/roma.ply)이 있을 때 그것을, 없으면 자체 점 구름을 씁니다. `sparse`는 항상 자체 점 구름입니다. 경로를 주면: 카메라와 같은 원본 좌표계의 XYZ + RGB PLY로 초기 점 구름을 교체합니다. 상대 경로는 데이터셋 기준이며 중심 이동과 배율은 둘 다에 적용됩니다.\n- 학습된 스플랫이 아닌 점 구름입니다. random_init=always가 덮어쓰며 init_ply는 init_ply_add_points일 때만 사용합니다.\n- 재개 시 체크포인트를 복원합니다."),
    DE("Leer nimmt die fertige dichte Punktwolke des Datensatzes (dense/roma.ply), sonst dessen eigene Punkte; `sparse` nimmt immer die eigenen Punkte.\n- Ein Pfad: Ersetzt die Startpunktwolke durch ein XYZ + RGB PLY im Quellkoordinatensystem der Kameras.\n- Relative Pfade beginnen im Datensatzordner.\n- Zentrierung und Skalierung gelten für beide. Kein trainiertes Splat-PLY. random_init=always überschreibt es;\n- init_ply nutzt es nur mit init_ply_add_points. Fortsetzen stellt Checkpoint-Splats wieder her."),
    FR("Vide : le nuage dense terminé du jeu de données (dense/roma.ply) s'il existe, sinon ses propres points ; `sparse` prend toujours ses propres points.\n- Un chemin : Remplace le nuage initial par un PLY XYZ + RGB dans le repère source des caméras.\n- Les chemins relatifs partent du dossier du jeu de données.\n- Centrage et échelle s'appliquent aux deux. Ce n'est pas un PLY de splats entraîné. random_init=always le remplace ;\n- init_ply l'utilise uniquement avec init_ply_add_points. La reprise restaure les splats du checkpoint."),
    ES("Vacío usa la nube densa terminada del conjunto de datos (dense/roma.ply) si existe, si no sus propios puntos; `sparse` usa siempre sus propios puntos.\n- Una ruta: Sustituye la nube inicial por un PLY XYZ + RGB en el sistema de origen de las cámaras.\n- Las rutas relativas parten del conjunto de datos. Centrado y escala se aplican a ambos.\n- No es un PLY de splats entrenado. random_init=always lo sustituye; init_ply solo lo usa con init_ply_add_points.\n- Reanudar restaura los splats del checkpoint."),
    PT("Vazio usa a nuvem densa concluída do conjunto de dados (dense/roma.ply) quando existe, senão os próprios pontos; `sparse` usa sempre os próprios pontos.\n- Um caminho: Substitui a nuvem inicial por um PLY XYZ + RGB no sistema de origem das câmeras.\n- Caminhos relativos partem da pasta do conjunto de dados. Centralização e escala se aplicam a ambos.\n- Não é um PLY de splats treinado. random_init=always o substitui; init_ply só o usa com init_ply_add_points.\n- Retomar restaura os splats do checkpoint."),
    IT("Vuoto usa la nuvola densa completata del set di dati (dense/roma.ply) se c'è, altrimenti i suoi punti; `sparse` usa sempre i suoi punti.\n- Un percorso: Sostituisce la nuvola iniziale con un PLY XYZ + RGB nel sistema originale delle camere.\n- I percorsi relativi partono dalla cartella del set di dati. Centratura e scala si applicano a entrambi.\n- Non è un PLY di splat addestrati. random_init=always lo sostituisce; init_ply lo usa solo con init_ply_add_points.\n- La ripresa ripristina gli splat del checkpoint."),
    NL("Leeg gebruikt de voltooide dichte puntenwolk van de dataset (dense/roma.ply) als die er is, anders de eigen punten; `sparse` gebruikt altijd de eigen punten.\n- Een pad: Vervangt de startpuntenwolk door een XYZ + RGB PLY in het bronstelsel van de camera's.\n- Relatieve paden beginnen bij de datasetmap. Centrering en schaal gelden voor beide.\n- Geen getraind splat-PLY. random_init=always vervangt het; init_ply gebruikt het alleen met init_ply_add_points.\n- Hervatten herstelt de checkpoint-splats."),
    RU("Пусто — готовое плотное облако набора данных (dense/roma.ply), если оно есть, иначе его собственные точки; `sparse` — всегда собственные точки.\n- Путь: Заменяет начальное облако файлом XYZ + RGB PLY в исходной системе координат камер.\n- Относительные пути идут от каталога данных. Центрирование и масштаб применяются к обоим.\n- Это не обученный PLY сплатов. random_init=always заменяет его; init_ply использует его только с init_ply_add_points.\n- Возобновление восстанавливает сплаты контрольной точки."),
    TR("Boş bırakılırsa veri kümesinin tamamlanmış yoğun bulutu (dense/roma.ply) varsa o, yoksa kendi noktaları kullanılır; `sparse` her zaman kendi noktalarını kullanır.\n- Bir yol: Başlangıç bulutunu kameralarla aynı kaynak koordinat sistemindeki XYZ + RGB PLY ile değiştirir.\n- Göreli yollar veri kümesi klasöründen başlar.\n- Merkezleme ve ölçek ikisine de uygulanır. Eğitilmiş splat PLY değildir. random_init=always bunun yerine geçer;\n- init_ply yalnızca init_ply_add_points ile kullanır. Devam etme kontrol noktası splatlarını geri yükler."));

SS_MSG(init_ply,
    EN("Initial splat PLY"), JA("初期スプラットの PLY"),
    ZH_HANS("初始泼溅 PLY"), ZH_HANT("初始潑濺 PLY"), KO("초기 스플랫 PLY"),
    DE("Start-PLY der Splats"), FR("PLY de splats initial"),
    ES("PLY de splats inicial"), PT("PLY de splats inicial"),
    IT("PLY di splat iniziale"), NL("Start-PLY met splats"),
    RU("Начальный PLY сплатов"), TR("Başlangıç splat PLY'si"));
SS_MSG(init_ply_help,
    EN("Starts from a trained splat PLY with a fresh optimizer at step 0. It must use the same photos and scene settings to align correctly."),
    JA("学習済みスプラットPLYから、ステップ0と新しい最適化状態で開始します。位置を合わせるには同じ写真とシーン設定が必要です。"),
    ZH_HANS("从已训练的泼溅 PLY 开始，使用新优化器并从第 0 步训练。必须使用相同照片和场景设置才能对齐。"),
    ZH_HANT("從已訓練的潑濺 PLY 開始，使用新最佳化器並從第 0 步訓練。必須使用相同照片和場景設定才能對齊。"),
    KO("학습된 스플랫 PLY에서 새 최적화기로 0스텝부터 시작합니다. 올바르게 정렬하려면 같은 사진과 장면 설정을 사용해야 합니다."),
    DE("Startet aus einer trainierten Splat-PLY mit neuem Optimierer bei Schritt 0. Zur Ausrichtung sind dieselben Fotos und Szeneneinstellungen nötig."),
    FR("Part d’un PLY de splats entraîné avec un nouvel optimiseur au pas 0. Les mêmes photos et réglages de scène sont nécessaires à l’alignement."),
    ES("Parte de un PLY de splats entrenado con optimizador nuevo en el paso 0. Necesita las mismas fotos y ajustes de escena para alinearse."),
    PT("Parte de um PLY de splats treinado com novo otimizador no passo 0. Precisa das mesmas fotos e configurações de cena para alinhar."),
    IT("Parte da un PLY di splat addestrato con un nuovo ottimizzatore al passo 0. Per allinearsi servono le stesse foto e impostazioni della scena."),
    NL("Start vanuit een getrainde splat-PLY met een nieuwe optimalisatie bij stap 0. Dezelfde foto’s en scène-instellingen zijn nodig voor uitlijning."),
    RU("Стартует с обученного PLY сплатов, с новым оптимизатором и шага 0. Для совмещения нужны те же фотографии и настройки сцены."),
    TR("Eğitilmiş splat PLY’den yeni eniyileyiciyle 0. adımdan başlar. Hizalama için aynı fotoğraflar ve sahne ayarları gerekir."));

SS_MSG(init_ply_add_points,
    EN("Add the dataset's points"), JA("データセットの点も足す"),
    ZH_HANS("同时加上数据集的点"), ZH_HANT("同時加上資料集的點"),
    KO("데이터셋의 점도 더하기"),
    DE("Punkte des Datensatzes ergänzen"),
    FR("Ajouter les points du jeu de données"),
    ES("Añadir los puntos del conjunto de datos"),
    PT("Somar os pontos do conjunto de dados"),
    IT("Aggiungere i punti del set di dati"),
    NL("Punten van de dataset erbij"),
    RU("Добавить точки набора данных"),
    TR("Veri kümesinin noktalarını da ekle"));
SS_MSG(init_ply_add_points_help,
    EN("Loads the PLY first, then adds dataset seed points up to the splat cap. Useful after adding photos; has no effect without init_ply."),
    JA("PLYを先に読み込み、上限までデータセットの初期点を加えます。写真を追加した場合に有効です。init_plyなしでは効果がありません。"),
    ZH_HANS("先加载 PLY，再添加数据集初始点直至数量上限。适合新增照片后使用；未设置 init_ply 时无效。"),
    ZH_HANT("先載入 PLY，再加入資料集初始點直至數量上限。適合新增照片後使用；未設定 init_ply 時無效。"),
    KO("PLY를 먼저 불러온 뒤 상한까지 데이터셋 초기 점을 추가합니다. 사진을 추가한 경우 유용하며 init_ply 없이는 효과가 없습니다."),
    DE("Lädt zuerst die PLY und ergänzt Startpunkte bis zur Splat-Grenze. Nützlich nach neuen Fotos; ohne init_ply wirkungslos."),
    FR("Charge le PLY puis ajoute des points initiaux jusqu’au plafond. Utile après ajout de photos ; sans effet sans init_ply."),
    ES("Carga primero el PLY y añade puntos iniciales hasta el límite. Útil tras añadir fotos; sin efecto sin init_ply."),
    PT("Carrega o PLY e adiciona pontos iniciais até o limite. Útil após adicionar fotos; sem efeito sem init_ply."),
    IT("Carica prima il PLY, poi aggiunge punti iniziali fino al limite. Utile dopo aver aggiunto foto; nessun effetto senza init_ply."),
    NL("Laadt eerst de PLY, daarna startpunten tot de splatlimiet. Nuttig na toevoegen van foto’s; zonder init_ply geen effect."),
    RU("Сначала загружает PLY, затем добавляет начальные точки до лимита сплатов. Полезно после добавления фото; без init_ply не действует."),
    TR("Önce PLY’yi yükler, sonra splat sınırına kadar başlangıç noktaları ekler. Fotoğraf ekledikten sonra yararlıdır; init_ply yoksa etkisizdir."));

SS_MSG(partition,
    EN("Partition file"), JA("分割ファイル"), ZH_HANS("分区文件"), ZH_HANT("分區檔案"),
    KO("분할 파일"), DE("Partitionsdatei"), FR("Fichier de partition"),
    ES("Archivo de partición"), PT("Ficheiro de partição"), IT("File di partizione"),
    NL("Partitiebestand"), RU("Файл разбиения"), TR("Bölümleme dosyası"));
SS_MSG(partition_help,
    EN("partition.json from the Partition panel or spirula partition. Trains one part using its cameras, overlap cameras and visible seeds; parts are merged later."),
    JA("Partitionパネルまたは spirula partition の partition.json。担当カメラ、重複領域のカメラ、可視の初期点で1区画を学習し、後で統合します。"),
    ZH_HANS("Partition 面板或 spirula partition 生成的 partition.json。使用分区相机、重叠相机及可见初始点训练一个分区，之后合并各分区。"),
    ZH_HANT("Partition 面板或 spirula partition 產生的 partition.json。使用分區相機、重疊相機及可見初始點訓練一個分區，之後合併各分區。"),
    KO("Partition 패널 또는 spirula partition의 partition.json입니다. 구역 카메라, 겹침 카메라와 보이는 초기 점으로 한 구역을 학습한 뒤 병합합니다."),
    DE("partition.json vom Partition-Panel oder spirula partition.\nTrainiert einen Teil mit eigenen und überlappenden Kameras plus sichtbaren Punkten; danach werden Teile vereint."),
    FR("partition.json du panneau Partition ou de spirula partition. Entraîne une partie avec ses caméras, celles du recouvrement et les points visibles ; fusion ensuite."),
    ES("partition.json del panel Partition o spirula partition. Entrena una parte con sus cámaras, las de solapamiento y puntos visibles; luego fusiona las partes."),
    PT("partition.json do painel Partition ou spirula partition. Treina uma parte com suas câmeras, as de sobreposição e pontos visíveis; depois une as partes."),
    IT("partition.json del pannello Partition o di spirula partition. Addestra una parte con le sue camere, quelle sovrapposte e i punti visibili; poi unisce le parti."),
    NL("partition.json van het Partition-paneel of spirula partition.\nTraint één deel met eigen en overlappende camera’s plus zichtbare punten; daarna samenvoegen."),
    RU("partition.json из панели Partition или spirula partition. Обучает часть по её камерам, перекрывающим камерам и видимым точкам; затем части объединяются."),
    TR("Partition paneli ya da spirula partition kaynaklı partition.json.\nBölümü kendi ve örtüşen kameralarıyla, görünür noktalarla eğitir; sonra bölümleri birleştirir."));
SS_MSG(partition_part,
    EN("Part to train"), JA("学習するパート"), ZH_HANS("要训练的分区"),
    ZH_HANT("要訓練的分區"), KO("학습할 파트"), DE("Zu trainierender Teil"),
    FR("Partie à entraîner"), ES("Parte a entrenar"), PT("Parte a treinar"),
    IT("Parte da addestrare"), NL("Te trainen deel"), RU("Обучаемая часть"),
    TR("Eğitilecek parça"));
SS_MSG(partition_part_help,
    EN("Which part of the partition file this run trains, counting from 0. "
       "Required with --partition."),
    JA("分割ファイルのどのパートを学習するかを 0 から数えて指定します。"
       "--partition と一緒に必須です。"),
    ZH_HANS("本次训练分区文件中的第几个分区，从 0 起算。与 --partition 一起时必填。"),
    ZH_HANT("本次訓練分區檔案中的第幾個分區，從 0 起算。與 --partition 一起時必填。"),
    KO("이 실행이 분할 파일의 어느 파트를 학습할지, 0부터 셉니다. --partition과 "
       "함께 필수입니다."),
    DE("Welchen Teil der Partitionsdatei dieser Lauf trainiert, ab 0 gezählt. "
       "Mit --partition erforderlich."),
    FR("Quelle partie du fichier de partition cet entraînement traite, à partir "
       "de 0. Obligatoire avec --partition."),
    ES("Qué parte del archivo de partición entrena esta ejecución, contando desde "
       "0. Obligatorio con --partition."),
    PT("Qual parte do ficheiro de partição este treino usa, a contar de 0. "
       "Obrigatório com --partition."),
    IT("Quale parte del file di partizione addestra questa esecuzione, contando "
       "da 0. Obbligatorio con --partition."),
    NL("Welk deel van het partitiebestand deze run traint, geteld vanaf 0. "
       "Verplicht met --partition."),
    RU("Какую часть файла разбиения обучает этот запуск, считая с 0. Обязательно "
       "вместе с --partition."),
    TR("Bu çalıştırmanın bölümleme dosyasının hangi parçasını eğittiği, 0'dan "
       "sayılır. --partition ile zorunludur."));

SS_MSG(roi_region,
    EN("Region of interest file"), JA("関心領域ファイル"), ZH_HANS("感兴趣区域文件"),
    ZH_HANT("感興趣區域檔案"), KO("관심 영역 파일"), DE("Datei des Interessenbereichs"),
    FR("Fichier de région d'intérêt"), ES("Archivo de región de interés"),
    PT("Ficheiro da região de interesse"), IT("File della regione di interesse"),
    NL("Bestand met interessegebied"), RU("Файл области интереса"),
    TR("İlgi bölgesi dosyası"));
SS_MSG(roi_region_help,
    EN("Region JSON from the ROI editor or shapes combined with Boolean operations.\n- Unset uses the first file in <dataset>/roi. A bare name picks a file there. off trains the whole scene.\n- Partitioned training uses the overlap of the selected part and the region."),
    JA("ROI エディターで保存した領域 JSON、または論理演算で組み合わせた形状です。\n- 未指定なら <dataset>/roi の最初のファイルを使います。名前だけなら同じフォルダから選び、off は全体を学習します。\n- 分割学習は選んだパートと領域の重なりだけを使います。"),
    ZH_HANS("使用 ROI 编辑器保存的区域 JSON，或由布尔运算组合的形状。\n- 未设置时使用 <dataset>/roi 中首个文件。只写名称则从该目录选择。off 训练整个场景。\n- 分区训练使用所选分区与区域的交集。"),
    ZH_HANT("使用 ROI 編輯器儲存的區域 JSON，或由布林運算組合的形狀。\n- 未設定時使用 <dataset>/roi 中首個檔案。只寫名稱則從該目錄選擇。off 訓練整個場景。\n- 分區訓練使用所選分區與區域的交集。"),
    KO("ROI 편집기의 영역 JSON이나 불리언 연산으로 결합한 모양을 사용합니다.\n- 미설정이면 <dataset>/roi의 첫 파일을 씁니다. 이름만 주면 거기서 고르고, off는 전체 장면을 학습합니다.\n- 분할 학습은 선택한 부분과 영역이 겹치는 곳을 사용합니다."),
    DE("Regions-JSON aus dem ROI-Editor oder boolesch kombinierte Formen.\n- Ohne Angabe erste Datei in <dataset>/roi. Ein Name wählt dort eine Datei. off trainiert die ganze Szene.\n- Partitionstraining nutzt die Schnittmenge von Teil und Region."),
    FR("JSON de région de l’éditeur ROI ou formes combinées par opérations booléennes.\n- Vide utilise le premier fichier de <dataset>/roi. Un nom y choisit un fichier. off entraîne toute la scène.\n- L’entraînement par partition utilise l’intersection de la partie et de la région."),
    ES("JSON de región del editor ROI o formas combinadas con operaciones booleanas.\n- Vacío usa el primer archivo en <dataset>/roi. Un nombre elige uno allí. off entrena toda la escena.\n- El entrenamiento por particiones usa la intersección de la parte elegida y la región."),
    PT("JSON de região do editor ROI ou formas combinadas por operações booleanas.\n- Vazio usa o primeiro ficheiro em <dataset>/roi. Um nome escolhe um ficheiro ali. off treina toda a cena.\n- O treino por partições usa a interseção da parte escolhida com a região."),
    IT("JSON della regione dall’editor ROI o forme combinate con operazioni booleane.\n- Vuoto usa il primo file in <dataset>/roi. Un nome sceglie un file lì. off addestra l’intera scena.\n- L’addestramento per partizioni usa l’intersezione tra parte e regione."),
    NL("Regio-JSON uit de ROI-editor of vormen gecombineerd met booleaanse bewerkingen.\n- Leeg gebruikt het eerste bestand in <dataset>/roi. Een naam kiest daar een bestand. off traint de hele scène.\n- Training per partitie gebruikt de overlap van het gekozen deel en de regio."),
    RU("JSON области из редактора ROI или фигуры с булевыми операциями.\n- Без значения берётся первый файл в <dataset>/roi. Имя выбирает файл там. off обучает всю сцену.\n- Обучение по частям использует пересечение выбранной части и области."),
    TR("ROI düzenleyicisinden bölge JSON’u veya Boole işlemleriyle birleştirilen şekiller.\n- Boşsa <dataset>/roi içindeki ilk dosyayı kullanır. Yalnız ad oradan dosya seçer. off tüm sahneyi eğitir.\n- Bölümlü eğitim, seçilen parça ile bölgenin kesişimini kullanır."));
SS_MSG(roi_outside_weight,
    EN("Draw weight outside the region"), JA("領域外の抽選重み"),
    ZH_HANS("区域外的抽样权重"), ZH_HANT("區域外的抽樣權重"), KO("영역 밖 추첨 가중치"),
    DE("Ziehgewicht außerhalb des Bereichs"), FR("Poids de tirage hors région"),
    ES("Peso de sorteo fuera de la región"), PT("Peso de sorteio fora da região"),
    IT("Peso di estrazione fuori dalla regione"), NL("Lotgewicht buiten het gebied"),
    RU("Вес выборки вне области"), TR("Bölge dışı çekiliş ağırlığı"));
SS_MSG(roi_mask_pixels,
    EN("Train only on what shows the region"), JA("領域が写る部分だけで学習"),
    ZH_HANS("只用拍到区域的像素训练"), ZH_HANT("只用拍到區域的像素訓練"),
    KO("영역이 보이는 픽셀로만 학습"), DE("Nur auf Pixeln des Bereichs trainieren"),
    FR("N'entraîner que sur ce qui montre la région"), ES("Entrenar solo con lo que muestra la región"),
    PT("Treinar só com o que mostra a região"), IT("Addestrare solo su ciò che mostra la regione"),
    NL("Alleen trainen op wat het gebied toont"), RU("Обучать только на пикселях области"),
    TR("Yalnızca bölgeyi gösteren piksellerle eğit"));
SS_MSG(roi_mask_pixels_help,
    EN("Masks each image to pixels whose nearest seed is inside the ROI, plus a margin. Prevents outside content from creating splats that survive merging."),
    JA("最近傍の初期点がROI内にあるピクセルと余白だけを使います。外側の内容から、統合後も残るスプラットが生じるのを防ぎます。"),
    ZH_HANS("仅保留最近初始点位于 ROI 内的像素及边距，防止外部内容生成合并后仍残留的泼溅。"),
    ZH_HANT("僅保留最近初始點位於 ROI 內的像素及邊距，防止外部內容產生合併後仍殘留的潑濺。"),
    KO("가장 가까운 초기 점이 ROI 안에 있는 픽셀과 여백만 사용합니다. 외부 내용이 병합 후에도 남는 스플랫을 만드는 것을 막습니다."),
    DE("Maskiert Bilder auf Pixel mit nächstem Startpunkt im ROI, plus Rand. Verhindert Splats für Außeninhalte, die das Zusammenführen überleben."),
    FR("Garde les pixels dont le point initial le plus proche est dans la ROI, plus une marge. Évite des splats extérieurs qui subsistent à la fusion."),
    ES("Conserva píxeles cuya semilla más cercana esté en la ROI, más un margen. Evita splats de contenido externo que sobrevivan a la fusión."),
    PT("Mantém pixels cujo ponto inicial mais próximo esteja na ROI, mais uma margem. Evita splats de conteúdo externo que sobrevivam à união."),
    IT("Mantiene i pixel col punto iniziale più vicino nella ROI, più un margine. Evita splat di contenuti esterni che sopravvivono alla fusione."),
    NL("Behoudt pixels waarvan het dichtstbijzijnde startpunt in de ROI ligt, plus een marge. Voorkomt externe splats die na samenvoegen blijven."),
    RU("Оставляет пиксели с ближайшей начальной точкой внутри ROI плюс отступ. Не даёт внешнему содержимому создавать сплаты, остающиеся после слияния."),
    TR("En yakın başlangıç noktası ROI içinde olan pikselleri ve bir kenar payını tutar. Birleştirmeden sonra kalacak dış içerik splatlarını önler."));
SS_MSG(roi_outside_opacity_decay,
    EN("Opacity kept outside the region"), JA("領域外で残す不透明度"),
    ZH_HANS("区域外保留的不透明度"), ZH_HANT("區域外保留的不透明度"), KO("영역 밖에 남기는 불투명도"),
    DE("Deckkraft, die außerhalb bleibt"), FR("Opacité conservée hors région"),
    ES("Opacidad conservada fuera de la región"), PT("Opacidade mantida fora da região"),
    IT("Opacità mantenuta fuori dalla regione"), NL("Dekking die buiten het gebied blijft"),
    RU("Сохраняемая непрозрачность вне области"), TR("Bölge dışında kalan opaklık"));
SS_MSG(roi_outside_opacity_decay_help,
    EN("At every densification step, splats outside the region of interest keep this share of their opacity.\n- The ones the images still need win it back\n- the rest fade until they are relocated inside. 1 turns it off."),
    JA("密度化のたびに、関心領域の外のスプラットは不透明度のこの割合だけを残します。\n- 画像がまだ必要とするものは取り戻し、残りは薄れて領域内へ再配置されます。\n- 1 で無効。"),
    ZH_HANS("每次致密化时，感兴趣区域外的泼溅只保留这一比例的不透明度。\n- 图像仍需要的会恢复，其余逐渐变淡，直到被重定位到区域内。\n- 取 1 关闭。"),
    ZH_HANT("每次緻密化時，感興趣區域外的潑濺只保留這一比例的不透明度。\n- 影像仍需要的會恢復，其餘逐漸變淡，直到被重定位到區域內。\n- 取 1 關閉。"),
    KO("밀집화할 때마다 관심 영역 밖의 스플랫은 불투명도의 이 비율만 남깁니다.\n이미지가 여전히 필요로 하는 것은 되찾고, 나머지는 흐려져 영역 안으로 재배치됩니다. 1이면 끕니다."),
    DE("Bei jedem Verdichtungsschritt behalten Splats außerhalb des Interessenbereichs diesen Anteil ihrer Deckkraft.\n- Was die Bilder noch brauchen, holt ihn zurück\n- der Rest verblasst, bis er nach innen verlagert wird. 1 schaltet es ab."),
    FR("À chaque densification, les splats hors de la région d'intérêt gardent cette part de leur opacité.\n- Ceux dont les images ont encore besoin la regagnent\n- les autres s'effacent jusqu'à être relocalisés à l'intérieur. 1 désactive."),
    ES("En cada densificación, los splats fuera de la región de interés conservan esta parte de su opacidad.\n- Los que las imágenes aún necesitan la recuperan\n- el resto se desvanece hasta reubicarse dentro. 1 lo desactiva."),
    PT("Em cada densificação, os splats fora da região de interesse mantêm esta parte da sua opacidade.\n- Os que as imagens ainda precisam recuperam-na\n- os restantes desvanecem até serem realocados para dentro. 1 desliga."),
    IT("A ogni densificazione, gli splat fuori dalla regione di interesse mantengono questa quota della loro opacità.\n- Quelli che servono ancora alle immagini la recuperano\n- gli altri svaniscono finché non vengono ricollocati dentro. 1 la disattiva."),
    NL("Bij elke verdichting houden splats buiten het interessegebied dit deel van hun dekking.\n- Wat de beelden nog nodig hebben, wint het terug\n- de rest vervaagt tot het naar binnen verplaatst wordt. 1 zet het uit."),
    RU("На каждом шаге уплотнения сплаты вне области интереса сохраняют эту долю непрозрачности.\nНужные изображениям восстанавливают её, остальные тускнеют, пока их не переместят внутрь. 1 отключает."),
    TR("Her yoğunlaştırma adımında ilgi bölgesi dışındaki splat'ler opaklıklarının bu payını korur.\n- Görüntülerin hâlâ ihtiyaç duyduğu geri kazanır\n- kalanlar içeri taşınana dek solar. 1 kapatır."));
SS_MSG(roi_outside_weight_help,
    EN("What a splat outside the region of interest counts for when "
       "densification picks where to relocate or add splats, relative to 1 "
       "inside. 1 turns the region off."),
    JA("密度化が再配置や追加先を選ぶとき、関心領域の外のスプラットを内側の 1 に対"
       "してどれだけに数えるか。1 で領域を無効にします。"),
    ZH_HANS("致密化选择重定位或新增位置时，感兴趣区域外的泼溅相对区域内的 1 算多"
            "少。取 1 即关闭区域。"),
    ZH_HANT("緻密化選擇重定位或新增位置時，感興趣區域外的潑濺相對區域內的 1 算多"
            "少。取 1 即關閉區域。"),
    KO("밀집화가 재배치나 추가 위치를 고를 때 관심 영역 밖의 스플랫을 안쪽의 1에 "
       "비해 얼마로 칠지. 1이면 영역을 끕니다."),
    DE("Wie viel ein Splat außerhalb des Interessenbereichs zählt, wenn die "
       "Verdichtung wählt, wohin sie verlagert oder ergänzt, gegenüber 1 innen. "
       "1 schaltet den Bereich ab."),
    FR("Ce que vaut un splat hors de la région d'intérêt quand la densification "
       "choisit où relocaliser ou ajouter, par rapport à 1 à l'intérieur. 1 "
       "désactive la région."),
    ES("Cuánto cuenta un splat fuera de la región de interés cuando la "
       "densificación elige dónde reubicar o añadir, frente a 1 dentro. 1 "
       "desactiva la región."),
    PT("Quanto vale um splat fora da região de interesse quando a densificação "
       "escolhe onde realocar ou acrescentar, face a 1 dentro. 1 desliga a "
       "região."),
    IT("Quanto conta uno splat fuori dalla regione di interesse quando la "
       "densificazione sceglie dove ricollocare o aggiungere, rispetto a 1 "
       "dentro. 1 disattiva la regione."),
    NL("Hoeveel een splat buiten het interessegebied telt als de verdichting "
       "kiest waar ze verplaatst of toevoegt, tegenover 1 erbinnen. 1 zet het "
       "gebied uit."),
    RU("Сколько весит сплат вне области интереса, когда уплотнение выбирает, "
       "куда перемещать или добавлять, при 1 внутри. 1 отключает область."),
    TR("Yoğunlaştırma nereye taşıyacağını ya da ekleyeceğini seçerken ilgi "
       "bölgesi dışındaki bir splatın içerideki 1'e göre kaça sayıldığı. 1 "
       "bölgeyi kapatır."));
SS_MSG(roi_splat_samples,
    EN("Points per splat tested against the region"), JA("スプラットごとの領域判定点数"),
    ZH_HANS("每个泼溅的区域判定点数"), ZH_HANT("每個潑濺的區域判定點數"),
    KO("스플랫당 영역 판정 점 수"), DE("Geprüfte Punkte je Splat"),
    FR("Points testés par splat"), ES("Puntos probados por splat"),
    PT("Pontos testados por splat"), IT("Punti verificati per splat"),
    NL("Geteste punten per splat"), RU("Проверяемых точек на сплат"),
    TR("Splat başına denenen nokta"));
SS_MSG(roi_splat_samples_help,
    EN("ROI tests per splat at densification: center, then randomly rotated Gaussian samples.\n- Any sample inside makes the splat count as inside; 1 tests only the center.\n- At 8, nearly all splats with 20% of their mass inside pass."),
    JA("増殖時のROIテスト数。中心の後、ランダム回転したガウス分布内の点を使います。\n- 1点でも内側ならスプラットは内側扱い。1は中心のみです。\n- 8なら質量の20%が内側にあるスプラットのほぼすべてが通ります。"),
    ZH_HANS("增密时每个泼溅的 ROI 测试数：先测中心，再测随机旋转的高斯采样点。\n- 任一点在内部即视为内部；1 仅测中心。\n- 取 8 时，质量有 20% 位于内部的泼溅几乎都能通过。"),
    ZH_HANT("增密時每個潑濺的 ROI 測試數：先測中心，再測隨機旋轉的高斯取樣點。\n- 任一點在內部即視為內部；1 僅測中心。\n- 取 8 時，質量有 20% 位於內部的潑濺幾乎都能通過。"),
    KO("증밀 시 스플랫별 ROI 검사 수입니다. 중심 다음 무작위 회전 가우시안 표본을 씁니다.\n- 하나라도 안쪽이면 내부로 처리합니다. 1은 중심만 검사합니다.\n- 8이면 질량의 20%가 내부인 스플랫이 거의 모두 통과합니다."),
    DE("ROI-Tests pro Splat bei Verdichtung: Zentrum, dann zufällig gedrehte Gauß-Stichproben.\n- Eine Probe innen genügt; 1 prüft nur das Zentrum.\n- Bei 8 bestehen fast alle Splats mit 20% ihrer Masse innen."),
    FR("Tests ROI par splat à la densification : centre, puis échantillons gaussiens tournés au hasard.\n- Un échantillon dedans suffit ; 1 teste le centre seul.\n- À 8, presque tous les splats ayant 20% de leur masse dedans passent."),
    ES("Pruebas ROI por splat al densificar: centro y muestras gaussianas giradas al azar.\n- Basta una muestra dentro; 1 prueba solo el centro.\n- Con 8, pasan casi todos los splats con 20% de masa dentro."),
    PT("Testes ROI por splat na densificação: centro e amostras gaussianas giradas ao acaso.\n- Basta uma amostra dentro; 1 testa só o centro.\n- Com 8, passam quase todos os splats com 20% da massa dentro."),
    IT("Test ROI per splat alla densificazione: centro, poi campioni gaussiani ruotati a caso.\n- Basta un campione interno; 1 testa solo il centro.\n- Con 8, passano quasi tutti gli splat con il 20% della massa dentro."),
    NL("ROI-tests per splat bij verdichting: centrum, dan willekeurig gedraaide Gauss-monsters.\n- Eén monster binnen is genoeg; 1 test alleen het centrum.\n- Bij 8 slagen bijna alle splats met 20% van hun massa binnen."),
    RU("Проверки ROI на сплат при уплотнении: центр, затем случайно повёрнутые гауссовы точки.\n- Достаточно одной точки внутри; 1 проверяет лишь центр.\n- При 8 проходят почти все сплаты с 20% массы внутри."),
    TR("Yoğunlaştırmada splat başına ROI testi: merkez, sonra rastgele dönen Gauss örnekleri.\n- İçeride tek örnek yeter; 1 yalnız merkezi test eder.\n- 8’de, kütlesinin %20’si içeride olan splatların neredeyse tümü geçer."));

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
    EN("How solid each splat starts out.\n- Leave unset to choose automatically\n- lower makes early training more forgiving, higher locks geometry in sooner."),
    JA("各スプラットの初期の濃さです。\n- 未設定なら自動で選ばれます。\n- 低いほど学習の序盤が寛容になり、高いほど早くジオメトリが固まります。"),
    ZH_HANS("每个泼溅的初始实心程度。\n- 留空自动选择。\n- 值越低，训练初期越宽容；越高，几何越早定形。"),
    ZH_HANT("每個潑濺的初始實心程度。\n- 留空自動選擇。\n- 值越低，訓練初期越寬容；越高，幾何越早定形。"),
    KO("각 스플랫이 처음에 얼마나 진한지입니다.\n- 설정하지 않으면 자동으로 고릅니다.\n- 낮으면 학습 초반이 너그러워지고, 높으면 지오메트리가 더 일찍 굳습니다."),
    DE("Wie fest jeder Splat anfangs ist.\n- Nicht gesetzt wird automatisch gewählt\n- niedriger macht das frühe Training nachsichtiger, höher legt die Geometrie früher fest."),
    FR("À quel point chaque splat est plein au départ.\n- Non défini, la valeur est choisie automatiquement\n- plus bas rend le début de l'entraînement plus indulgent, plus haut fige la géométrie plus tôt."),
    ES("Cuán sólido empieza cada splat.\n- Sin definir, se elige automáticamente\n- más bajo hace el inicio del entrenamiento más indulgente, más alto fija la geometría antes."),
    PT("Quão sólido cada splat começa.\n- Sem definir, é escolhido automaticamente\n- mais baixo torna o início do treinamento mais tolerante, mais alto fixa a geometria mais cedo."),
    IT("Quanto è pieno ogni splat all'inizio.\n- Se non impostato, viene scelto automaticamente\n- più basso rende l'inizio dell'addestramento più indulgente, più alto fissa prima la geometria."),
    NL("Hoe massief elke splat begint.\n- Niet ingesteld wordt het automatisch gekozen\n- lager maakt het begin van de training vergevingsgezinder, hoger legt de geometrie eerder vast."),
    RU("Насколько плотным начинается каждый сплат.\n- Если не задано, выбирается автоматически\n- меньше — начало обучения снисходительнее, больше — геометрия закрепляется раньше."),
    TR("Her splat'ın başlangıçta ne kadar dolu olduğu.\n- Ayarlanmazsa kendiliğinden seçilir\n- düşük değerler eğitimin başını daha bağışlayıcı yapar, yüksek değerler geometriyi daha erken sabitler."));

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

SS_MSG(random_init,
    EN("Random seed points"),
    JA("ランダムな初期点"),
    ZH_HANS("随机初始点"),
    ZH_HANT("隨機初始點"),
    KO("무작위 초기 점"),
    DE("Zufällige Startpunkte"),
    FR("Points de départ aléatoires"),
    ES("Puntos iniciales aleatorios"),
    PT("Pontos iniciais aleatórios"),
    IT("Punti iniziali casuali"),
    NL("Willekeurige beginpunten"),
    RU("Случайные начальные точки"),
    TR("Rastgele başlangıç noktaları"));

SS_MSG(random_init_help,
    EN("- auto: random seed points when the dataset cloud is missing or empty.\n- always: random points even when a dataset cloud exists.\n- never: requires a nonempty dataset cloud; fails if missing or empty."),
    JA("- auto：点群がない、または空ならランダムな初期点を作ります。\n- always：データセットに点群があってもランダム点を使います。\n- never：空でないデータセット点群が必要。なければエラーにします。"),
    ZH_HANS("- auto：数据集点云缺失或为空时生成随机初始点。\n- always：即使有数据集点云也使用随机点。\n- never：要求数据集点云存在且非空，否则报错。"),
    ZH_HANT("- auto：資料集點雲缺失或為空時產生隨機初始點。\n- always：即使有資料集點雲也使用隨機點。\n- never：要求資料集點雲存在且非空，否則報錯。"),
    KO("- auto: 점군이 없거나 비어 있으면 무작위 초기 점을 만듭니다.\n- always: 데이터셋 점군이 있어도 무작위 점을 씁니다.\n- never: 비어 있지 않은 데이터셋 점군이 필요하며, 없거나 비면 오류입니다."),
    DE("- auto: Zufallspunkte, wenn die Datenpunktwolke fehlt oder leer ist.\n- always: Zufallspunkte auch bei vorhandener Datenpunktwolke.\n- never: benötigt eine nichtleere Datenpunktwolke; sonst Fehler."),
    FR("- auto : points aléatoires si le nuage du jeu est absent ou vide.\n- always : points aléatoires même si le nuage existe.\n- never : exige un nuage du jeu non vide ; sinon erreur."),
    ES("- auto: puntos aleatorios si falta la nube o está vacía.\n- always: puntos aleatorios aunque exista la nube del conjunto.\n- never: requiere una nube del conjunto no vacía; si falta o está vacía, error."),
    PT("- auto: pontos aleatórios se a nuvem estiver ausente ou vazia.\n- always: pontos aleatórios mesmo com nuvem existente.\n- never: exige nuvem do conjunto não vazia; ausente ou vazia causa erro."),
    IT("- auto: punti casuali se la nuvola manca o è vuota.\n- always: punti casuali anche se la nuvola dei dati esiste.\n- never: richiede una nuvola dei dati non vuota; altrimenti errore."),
    NL("- auto: willekeurige punten als de datasetwolk ontbreekt of leeg is.\n- always: willekeurige punten, ook met bestaande datasetwolk.\n- never: vereist een niet-lege datasetwolk; anders een fout."),
    RU("- auto: случайные точки, если облако набора отсутствует или пусто.\n- always: случайные точки даже при наличии облака набора.\n- never: требуется непустое облако набора; иначе ошибка."),
    TR("- auto: veri bulutu yoksa veya boşsa rastgele başlangıç noktaları.\n- always: veri bulutu varken bile rastgele noktalar.\n- never: boş olmayan veri bulutu gerekir; yoksa veya boşsa hata verir."));

SS_MSG(random_init_fraction,
    EN("Random seed count"),
    JA("ランダム初期点の数"),
    ZH_HANS("随机初始点数量"),
    ZH_HANT("隨機初始點數量"),
    KO("무작위 초기 점 수"),
    DE("Anzahl zufälliger Startpunkte"),
    FR("Nombre de points de départ aléatoires"),
    ES("Número de puntos iniciales aleatorios"),
    PT("Número de pontos iniciais aleatórios"),
    IT("Numero di punti iniziali casuali"),
    NL("Aantal willekeurige beginpunten"),
    RU("Число случайных начальных точек"),
    TR("Rastgele başlangıç noktası sayısı"));

SS_MSG(random_init_fraction_help,
    EN("How many points random_init draws, as a share of cap_max: 0.1 of a "
       "million is a hundred thousand."),
    JA("random_init が置く点の数を cap_max に対する割合で指定します。100 万の"
       " 0.1 なら 10 万点です。"),
    ZH_HANS("random_init 抽取的点数，按 cap_max 的比例计：一百万的 0.1 就是十"
            "万个。"),
    ZH_HANT("random_init 抽取的點數，按 cap_max 的比例計：一百萬的 0.1 就是十"
            "萬個。"),
    KO("random_init이 뽑는 점의 수를 cap_max에 대한 비율로 정합니다. 100만의 "
       "0.1이면 10만 개입니다."),
    DE("Wie viele Punkte random_init zieht, als Anteil von cap_max: 0.1 von "
       "einer Million sind hunderttausend."),
    FR("Nombre de points que tire random_init, en part de cap_max : 0.1 d'un "
       "million font cent mille."),
    ES("Cuántos puntos sortea random_init, como fracción de cap_max: 0.1 de un "
       "millón son cien mil."),
    PT("Quantos pontos random_init sorteia, como fração de cap_max: 0.1 de um "
       "milhão são cem mil."),
    IT("Quanti punti estrae random_init, come quota di cap_max: 0.1 di un "
       "milione fa centomila."),
    NL("Hoeveel punten random_init trekt, als aandeel van cap_max: 0.1 van een "
       "miljoen is honderdduizend."),
    RU("Сколько точек создаёт random_init, в долях cap_max: 0.1 от миллиона — "
       "сто тысяч."),
    TR("random_init'in çektiği nokta sayısı, cap_max'in payı olarak: bir "
       "milyonun 0.1'i yüz bindir."));

SS_MSG(random_init_distribution,
    EN("Random seed distribution"),
    JA("ランダム初期点の分布"),
    ZH_HANS("随机初始点分布"),
    ZH_HANT("隨機初始點分布"),
    KO("무작위 초기 점 분포"),
    DE("Verteilung der zufälligen Startpunkte"),
    FR("Répartition des points de départ aléatoires"),
    ES("Distribución de los puntos iniciales aleatorios"),
    PT("Distribuição dos pontos iniciais aleatórios"),
    IT("Distribuzione dei punti iniziali casuali"),
    NL("Verdeling van de willekeurige beginpunten"),
    RU("Распределение случайных начальных точек"),
    TR("Rastgele başlangıç noktalarının dağılımı"));

SS_MSG(random_init_distribution_help,
    EN("Distribution of random seed points.\n- isotropic-gaussian is round. anisotropic-gaussian follows camera spread on its principal axes.\n- ellipsoid and box fill those shapes evenly, sized to camera spread.\n- Cameras at one height flatten every shape except isotropic-gaussian."),
    JA("ランダム初期点をどの分布から取るかです。 isotropic-gaussian は等方的な球状、anisotropic-gaussian はカメラの広がりの主軸ごとの大きさに従います。\n- ellipsoid と box は同じ広がりに合わせた楕円体または向き付きの箱の中に一様に置きます。\n- カメラがすべて同じ高さにあると上下の広がりがなく、等方的なもの以外は平たくなります。"),
    ZH_HANS("随机初始点从什么分布中抽取。 isotropic-gaussian 是各向同性的圆形分布； anisotropic-gaussian 沿相机分布的各个主轴跟随其范围；\n- ellipsoid 和 box 在按同样范围确定大小的实心椭球或定向长方体内均匀填充。\n- 相机都在同一高度时没有竖直方向的范围，除各向同性以外的形状都会被压扁。"),
    ZH_HANT("隨機初始點從什麼分布中抽取。 isotropic-gaussian 是各向同性的圓形分布； anisotropic-gaussian 沿相機分布的各個主軸跟隨其範圍；\n- ellipsoid 和 box 在按同樣範圍決定大小的實心橢球或定向長方體內均勻填充。\n- 相機都在同一高度時沒有垂直方向的範圍，除各向同性以外的形狀都會被壓扁。"),
    KO("무작위 초기 점을 어떤 분포에서 뽑을지 정합니다. isotropic-gaussian은 둥근 분포이고, anisotropic-gaussian은 카메라 분포의 주축마다 그 퍼짐을 따릅니다.\n- ellipsoid와 box는 같은 퍼짐에 맞춘 속이 찬 타원체나 방향이 있는 상자를 고르게 채웁니다.\n- 카메라가 모두 같은 높이에 있으면 수직 퍼짐이 없어 등방성 분포 말고는 모두 납작해집니다."),
    DE("Verteilung zufälliger Startpunkte.\n- isotropic-gaussian ist rund. anisotropic-gaussian folgt der Kamerastreuung auf ihren Hauptachsen.\n- ellipsoid und box füllen die Form gleichmäßig, bemessen nach der Kamerastreuung.\n- Gleiche Kamerahöhe flacht alle Formen außer isotropic-gaussian ab."),
    FR("Distribution des points de départ aléatoires.\n- isotropic-gaussian est rond. anisotropic-gaussian suit les axes principaux de dispersion des caméras.\n- ellipsoid et box remplissent uniformément leur forme, selon cette dispersion.\n- Des caméras à même hauteur aplatissent tout sauf isotropic-gaussian."),
    ES("Distribución de puntos iniciales aleatorios.\n- isotropic-gaussian es redonda. anisotropic-gaussian sigue los ejes principales de dispersión de cámaras.\n- ellipsoid y box llenan la forma uniformemente según esa dispersión.\n- Cámaras a una altura aplanan todo salvo isotropic-gaussian."),
    PT("De onde os pontos iniciais aleatórios são sorteados. isotropic-gaussian é redonda;\n- anisotropic-gaussian segue a dispersão das câmeras ao longo de cada um dos seus eixos principais;\n- ellipsoid e box preenchem por igual um elipsoide sólido ou uma caixa orientada, dimensionados com a mesma dispersão.\n- Câmeras todas na mesma altura não têm dispersão vertical, o que achata toda forma exceto a isotrópica."),
    IT("Distribuzione dei punti iniziali casuali.\n- isotropic-gaussian è rotonda. anisotropic-gaussian segue gli assi principali delle camere.\n- ellipsoid e box riempiono la forma in modo uniforme, secondo la dispersione delle camere.\n- Camere alla stessa altezza appiattiscono tutto tranne isotropic-gaussian."),
    NL("Verdeling van willekeurige startpunten.\n- isotropic-gaussian is rond. anisotropic-gaussian volgt cameraspreiding langs hoofdassen.\n- ellipsoid en box vullen de vorm gelijkmatig, geschaald naar cameraspreiding.\n- Camera’s op één hoogte vlakken alles behalve isotropic-gaussian af."),
    RU("Из какого распределения берутся случайные начальные точки. isotropic-gaussian — круглое; anisotropic-gaussian следует разбросу камер вдоль каждой из его главных осей;\n- ellipsoid и box равномерно заполняют сплошной эллипсоид или ориентированный параллелепипед того же разброса.\n- Если все камеры на одной высоте, вертикального разброса нет, и все формы, кроме изотропной, становятся плоскими."),
    TR("Rastgele başlangıç noktalarının hangi dağılımdan çekileceği. isotropic-gaussian yuvarlaktır; anisotropic-gaussian kameraların yayılımını her bir ana ekseni boyunca izler;\n- ellipsoid ve box aynı yayılıma göre boyutlanan dolu bir elipsoidi ya da yönlü bir kutuyu eşit biçimde doldurur.\n- Kameraların hepsi aynı yükseklikteyse dikey yayılım olmaz ve izotropik olan dışında her şekil yassılaşır."));

SS_MSG(random_init_center,
    EN("Random seed center"),
    JA("ランダム初期点の中心"),
    ZH_HANS("随机初始点中心"),
    ZH_HANT("隨機初始點中心"),
    KO("무작위 초기 점 중심"),
    DE("Mitte der zufälligen Startpunkte"),
    FR("Centre des points de départ aléatoires"),
    ES("Centro de los puntos iniciales aleatorios"),
    PT("Centro dos pontos iniciais aleatórios"),
    IT("Centro dei punti iniziali casuali"),
    NL("Midden van de willekeurige beginpunten"),
    RU("Центр случайных начальных точек"),
    TR("Rastgele başlangıç noktalarının merkezi"));

SS_MSG(random_init_center_help,
    EN("Random seed center: camera median, mean, optical-axis focus, or dataset origin. camera-focus suits an object encircled by cameras."),
    JA("初期点の中心：カメラ位置の中央値、平均、光軸の交点、データセットの原点。camera-focus は物体を囲む撮影向けです。"),
    ZH_HANS("随机初始点中心：相机位置的中位数、均值、光轴交汇点或数据集原点。camera-focus 适合环绕物体拍摄。"),
    ZH_HANT("隨機初始點中心：相機位置的中位數、均值、光軸交匯點或資料集原點。camera-focus 適合環繞物體拍攝。"),
    KO("무작위 초기 점의 중심: 카메라 중앙값, 평균, 광축 교점 또는 데이터셋 원점. camera-focus는 물체를 둘러싼 촬영에 적합합니다."),
    DE("Zentrum der Zufallspunkte: Kameramedian, Mittelwert, Schnittpunkt der Blickachsen oder Datenursprung. camera-focus für umkreiste Objekte."),
    FR("Centre initial aléatoire : médiane des caméras, moyenne, foyer des axes optiques ou origine. camera-focus convient aux objets entourés de caméras."),
    ES("Centro inicial aleatorio: mediana de cámaras, media, foco de ejes ópticos u origen. camera-focus sirve para objetos rodeados de cámaras."),
    PT("Centro inicial aleatório: mediana das câmeras, média, foco dos eixos ópticos ou origem. camera-focus serve para objetos cercados por câmeras."),
    IT("Centro iniziale casuale: mediana delle camere, media, fuoco degli assi ottici o origine. camera-focus per oggetti circondati da camere."),
    NL("Centrum van willekeurige startpunten: cameramediaan, gemiddelde, snijpunt van kijkassen of oorsprong. camera-focus voor omcirkelde objecten."),
    RU("Центр случайных точек: медиана камер, среднее, фокус оптических осей или начало координат. camera-focus — для съёмки вокруг объекта."),
    TR("Rastgele nokta merkezi: kamera ortancası, ortalama, optik eksen odağı veya veri orijini. camera-focus, çevresi çekilen nesnelere uygundur."));

SS_MSG(random_init_spread,
    EN("Random seed spread measure"),
    JA("ランダム初期点の広がりの測り方"),
    ZH_HANS("随机初始点范围的度量"),
    ZH_HANT("隨機初始點範圍的度量"),
    KO("무작위 초기 점 퍼짐 측정"),
    DE("Maß der Streuung der Startpunkte"),
    FR("Mesure de l'étalement des points de départ"),
    ES("Medida de la dispersión de los puntos iniciales"),
    PT("Medida da dispersão dos pontos iniciais"),
    IT("Misura della dispersione dei punti iniziali"),
    NL("Maat voor de spreiding van de beginpunten"),
    RU("Мера разброса начальных точек"),
    TR("Başlangıç noktası yayılımının ölçüsü"));

SS_MSG(random_init_spread_help,
    EN("Measures camera spread using the median or mean squared distance along each axis. The median resists distant outlier cameras."),
    JA("各軸の距離の二乗の中央値または平均でカメラの広がりを測ります。中央値は遠く離れた外れカメラの影響を抑えます。"),
    ZH_HANS("用各轴距离平方的中位数或均值衡量相机分布范围。中位数可抵抗远处异常相机的影响。"),
    ZH_HANT("用各軸距離平方的中位數或均值衡量相機分布範圍。中位數可抵抗遠處異常相機的影響。"),
    KO("축별 거리 제곱의 중앙값 또는 평균으로 카메라 분포 폭을 측정합니다. 중앙값은 멀리 떨어진 이상치 카메라에 강합니다."),
    DE("Misst die Kamerastreuung über Median oder Mittel der quadrierten Achsabstände. Der Median widersteht weit entfernten Ausreißerkameras."),
    FR("Mesure la dispersion des caméras par médiane ou moyenne des distances au carré par axe. La médiane résiste aux caméras aberrantes éloignées."),
    ES("Mide la dispersión con la mediana o media de distancias al cuadrado por eje. La mediana resiste cámaras atípicas lejanas."),
    PT("Mede a dispersão pela mediana ou média das distâncias ao quadrado por eixo. A mediana resiste a câmeras atípicas distantes."),
    IT("Misura la dispersione con mediana o media delle distanze al quadrato per asse. La mediana resiste alle camere anomale lontane."),
    NL("Meet cameraspreiding met de mediaan of het gemiddelde van gekwadrateerde asafstanden. De mediaan weerstaat verre uitschieters."),
    RU("Разброс камер по осям: медиана или среднее квадратов расстояний. Медиана устойчива к далёким камерам-выбросам."),
    TR("Kamera yayılımını eksen başına uzaklık karelerinin ortancası veya ortalamasıyla ölçer. Ortanca uzak aykırı kameralara dayanıklıdır."));

SS_MSG(random_init_std,
    EN("Random seed standard deviation"),
    JA("ランダム初期点の標準偏差"),
    ZH_HANS("随机初始点标准差"),
    ZH_HANT("隨機初始點標準差"),
    KO("무작위 초기 점 표준편차"),
    DE("Standardabweichung der Startpunkte"),
    FR("Écart type des points de départ aléatoires"),
    ES("Desviación típica de los puntos iniciales aleatorios"),
    PT("Desvio padrão dos pontos iniciais aleatórios"),
    IT("Deviazione standard dei punti iniziali casuali"),
    NL("Standaardafwijking van de willekeurige beginpunten"),
    RU("Стандартное отклонение начальных точек"),
    TR("Rastgele başlangıç noktalarının standart sapması"));

SS_MSG(random_init_std_help,
    EN("Seed standard deviation in units of camera spread. Below 1 suits objects viewed from outside; above 1 suits scenes viewed from inside."),
    JA("カメラの広がりを単位とした初期点の標準偏差。1未満は外から見る物体向け、1超は内側から見るシーン向けです。"),
    ZH_HANS("初始点的标准差，以相机分布范围为单位。小于 1 适合从外部拍摄的物体；大于 1 适合从内部拍摄的场景。"),
    ZH_HANT("初始點的標準差，以相機分布範圍為單位。小於 1 適合從外部拍攝的物體；大於 1 適合從內部拍攝的場景。"),
    KO("카메라 분포 폭 단위의 초기 점 표준편차입니다. 1 미만은 바깥에서 촬영한 물체, 1 초과는 안에서 촬영한 장면에 적합합니다."),
    DE("Standardabweichung der Startpunkte relativ zur Kamerastreuung. Unter 1 für von außen betrachtete Objekte; über 1 für von innen betrachtete Szenen."),
    FR("Écart type des points initiaux en unités de dispersion des caméras. Sous 1 pour un objet vu de l’extérieur ; au-dessus pour une scène vue de l’intérieur."),
    ES("Desviación estándar de puntos iniciales en unidades de dispersión de cámaras. Menos de 1 para objetos vistos desde fuera; más de 1 para escenas desde dentro."),
    PT("Desvio padrão dos pontos iniciais em unidades da dispersão das câmeras. Abaixo de 1 para objetos vistos de fora; acima de 1 para cenas vistas de dentro."),
    IT("Deviazione standard dei punti iniziali rispetto alla dispersione delle camere. Sotto 1 per oggetti visti da fuori; sopra 1 per scene viste da dentro."),
    NL("Standaardafwijking van startpunten in eenheden van cameraspreiding. Onder 1 voor objecten van buitenaf; boven 1 voor scènes van binnenuit."),
    RU("Стандартное отклонение начальных точек в единицах разброса камер. Меньше 1 — для объектов снаружи; больше 1 — для сцен изнутри."),
    TR("Kamera yayılımı biriminde başlangıç noktalarının standart sapması. 1 altı dışarıdan görülen nesnelere, 1 üstü içeriden görülen sahnelere uygundur."));


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
    EN("Overall detail level, setting the splat budget and how long training runs.\nUse a higher level for larger datasets, or for datasets with more detail in them."),
    JA("全体の精細さです。\n- スプラット数の上限と学習の長さをまとめて決めます。\n- データセットが大きいときや、細部の多いデータセットでは高い段階を使ってください。"),
    ZH_HANS("整体细节水平，同时决定泼溅数量上限和训练时长。\n数据集较大、或者细节较多时，请选更高的档位。"),
    ZH_HANT("整體細節水準，同時決定潑濺數量上限和訓練長度。\n資料集較大、或者細節較多時，請選更高的檔位。"),
    KO("전체적인 디테일 수준으로, 스플랫 예산과 학습 길이를 함께 정합니다.\n데이터셋이 크거나 세부가 많다면 더 높은 단계를 쓰세요."),
    DE("Gesamtdetailgrad; legt das Splat-Budget und die Trainingsdauer fest.\nFür größere Datensätze oder solche mit mehr Details eine höhere Stufe wählen."),
    FR("Niveau de détail global : fixe le budget de splats et la durée de l'entraînement.\nPrenez un niveau plus élevé pour les grands jeux de données, ou pour ceux qui contiennent plus de détails."),
    ES("Nivel de detalle general: fija el presupuesto de splats y la duración del entrenamiento.\nUse un nivel más alto con conjuntos de datos grandes, o con más detalle dentro."),
    PT("Nível de detalhe geral: define o orçamento de splats e a duração do treinamento.\nUse um nível mais alto para conjuntos de dados maiores, ou com mais detalhe dentro."),
    IT("Livello di dettaglio complessivo: fissa il budget di splat e la durata dell'addestramento.\nUsi un livello più alto per set di dati grandi, o con più dettaglio dentro."),
    NL("Algemeen detailniveau: bepaalt het splatbudget en hoe lang er wordt getraind.\nNeem een hoger niveau bij grotere datasets, of datasets met meer detail erin."),
    RU("Общий уровень детализации: задаёт бюджет сплатов и длительность обучения.\nДля больших наборов данных или наборов с большим количеством деталей берите уровень выше."),
    TR("Genel ayrıntı düzeyi; splat bütçesini ve eğitimin ne kadar süreceğini birlikte belirler.\nDaha büyük veri kümelerinde ya da içinde daha çok ayrıntı olanlarda daha yüksek bir düzey kullanın."));

SS_MSG(cap_max,
    EN("Maximum splats"), JA("スプラット数の上限"), ZH_HANS("泼溅数量上限"),
    ZH_HANT("潑濺數量上限"), KO("최대 스플랫 수"),
    DE("Höchstzahl der Splats"), FR("Nombre maximal de splats"),
    ES("Número máximo de splats"), PT("Número máximo de splats"),
    IT("Numero massimo di splat"), NL("Maximum aantal splats"),
    RU("Максимум сплатов"), TR("En fazla splat sayısı"));
SS_MSG(cap_max_help,
    EN("Maximum splat count. More splats capture finer detail but produce larger files and slower rendering; tune per scene."),
    JA("スプラット数の上限。増やすと細部が増しますが、ファイルが大きく描画が遅くなります。シーンごとに調整します。"),
    ZH_HANS("泼溅数量上限。更多点可呈现更细的细节，但文件更大、渲染更慢；请按场景调整。"),
    ZH_HANT("潑濺數量上限。更多點可呈現更細的細節，但檔案更大、算繪更慢；請按場景調整。"),
    KO("최대 스플랫 수입니다. 많을수록 세밀하지만 파일이 커지고 렌더가 느려집니다. 장면에 맞춰 조정하세요."),
    DE("Maximale Splat-Anzahl. Mehr Splats bringen feinere Details, größere Dateien und langsameres Rendern. Pro Szene abstimmen."),
    FR("Nombre maximal de splats. Plus de splats donnent plus de détail, mais des fichiers plus gros et un rendu plus lent. À régler par scène."),
    ES("Máximo de splats. Más splats dan más detalle, pero archivos mayores y render más lento. Ajústalo por escena."),
    PT("Máximo de splats. Mais splats dão mais detalhes, mas arquivos maiores e render mais lento. Ajuste por cena."),
    IT("Numero massimo di splat. Più splat danno maggior dettaglio, file più grandi e rendering più lento. Regolare per scena."),
    NL("Maximaal aantal splats. Meer splats geven fijnere details, grotere bestanden en langzamer renderen. Stem af per scène."),
    RU("Максимум сплатов. Больше сплатов — тоньше детали, крупнее файлы и медленнее рендер. Подбирайте для каждой сцены."),
    TR("En fazla splat sayısı. Fazlası daha ince ayrıntı, daha büyük dosya ve daha yavaş render demektir; sahneye göre ayarlayın."));

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
    EN("Ignore people, cars and anything else that moves between photos.\nSplats are no longer spent on them, at the cost of some sensitivity to real detail."),
    JA("写真ごとに動く人や車などを無視します。\nそうしたものにスプラットを使わなくなりますが、本物の細部への感度は少し下がります。"),
    ZH_HANS("忽略在照片之间移动的行人、车辆等。\n不再为它们分配泼溅，代价是对真实细节的敏感度略有下降。"),
    ZH_HANT("忽略在照片之間移動的行人、車輛等。\n不再為它們分配潑濺，代價是對真實細節的敏感度略有下降。"),
    KO("사진마다 움직이는 사람, 자동차 등을 무시합니다.\n그런 것에 스플랫을 쓰지 않게 되지만, 실제 디테일에 대한 민감도는 조금 떨어집니다."),
    DE("Personen, Autos und alles andere ignorieren, was sich zwischen den Fotos bewegt.\nAuf sie werden keine Splats mehr verwendet, um den Preis etwas geringerer Empfindlichkeit für echte Details."),
    FR("Ignorer les passants, les voitures et tout ce qui bouge d'une photo à l'autre.\nPlus aucun splat n'y est consacré, au prix d'une sensibilité un peu moindre aux vrais détails."),
    ES("Ignorar personas, coches y cualquier otra cosa que se mueva entre fotos.\nYa no se gastan splats en ellos, a costa de algo de sensibilidad al detalle real."),
    PT("Ignorar pessoas, carros e qualquer outra coisa que se mova entre as fotos.\nDeixam de ser gastos splats com eles, ao custo de alguma sensibilidade ao detalhe real."),
    IT("Ignorare persone, automobili e tutto ciò che si muove da una foto all'altra.\nNon vengono più spesi splat su di loro, al costo di un po' di sensibilità al dettaglio reale."),
    NL("Mensen, auto's en al het andere dat tussen de foto's beweegt negeren.\nDaar gaan geen splats meer heen, ten koste van iets minder gevoel voor echt detail."),
    RU("Игнорировать людей, машины и всё остальное, что меняется от снимка к снимку.\nСплаты на них больше не тратятся ценой некоторой потери чувствительности к настоящим деталям."),
    TR("Fotoğraflar arasında yer değiştiren insanları, arabaları ve benzerlerini yok sayar.\nOnlara artık splat harcanmaz; karşılığında gerçek ayrıntıya duyarlılık biraz azalır."));

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
    EN("Minimum starting splats as a fraction of cap_max. Increase for sparse initial clouds, including synthetic scenes."),
    JA("初期スプラット数の下限を cap_max の割合で指定します。合成シーンなど、初期点群が疎な場合に増やします。"),
    ZH_HANS("初始泼溅数量下限，以 cap_max 的比例表示。初始点云稀疏时（如合成场景）可增大。"),
    ZH_HANT("初始潑濺數量下限，以 cap_max 的比例表示。初始點雲稀疏時（如合成場景）可增大。"),
    KO("cap_max 대비 최소 초기 스플랫 비율입니다. 합성 장면처럼 초기 점군이 드물면 높이세요."),
    DE("Minimale Startanzahl als Anteil von cap_max. Für dünne Startpunktwolken, etwa synthetische Szenen, erhöhen."),
    FR("Minimum de splats initiaux en fraction de cap_max. Augmenter pour les nuages initiaux clairsemés, notamment synthétiques."),
    ES("Mínimo inicial de splats como fracción de cap_max. Auméntalo si la nube inicial es escasa, por ejemplo en escenas sintéticas."),
    PT("Mínimo inicial de splats como fração de cap_max. Aumente para nuvens iniciais esparsas, incluindo cenas sintéticas."),
    IT("Minimo di splat iniziali come frazione di cap_max. Aumentare per nuvole iniziali rade, anche nelle scene sintetiche."),
    NL("Minimum aan startsplats als fractie van cap_max. Verhoog bij dunne startpuntenwolken, zoals bij synthetische scènes."),
    RU("Минимум начальных сплатов как доля cap_max. Увеличьте для редких начальных облаков, в том числе синтетических сцен."),
    TR("cap_max oranı olarak en az başlangıç splatı. Sentetik sahneler dahil seyrek başlangıç bulutlarında artırın."));

SS_MSG(growth_factor,
    EN("Growth per round"), JA("1 回ごとの増加率"), ZH_HANS("每轮增长倍数"),
    ZH_HANT("每輪增長倍數"), KO("회차당 증가 배수"),
    DE("Wachstum pro Runde"), FR("Croissance par cycle"),
    ES("Crecimiento por ronda"), PT("Crescimento por rodada"),
    IT("Crescita per ciclo"), NL("Groei per ronde"), RU("Прирост за раунд"),
    TR("Tur başına büyüme"));
SS_MSG(growth_factor_help,
    EN("How fast the splat count grows at each round, as a multiplier.\n- Higher reaches cap_max sooner\n- lower grows gradually, which tends to place splats more carefully."),
    JA("1 回の増加ラウンドでスプラット数がどれだけ増えるかを倍率で指定します。\n高いほど早く cap_max に届き、低いほど少しずつ増えるので、スプラットの置き方が丁寧になりがちです。"),
    ZH_HANS("每一轮泼溅数量增长的倍数。\n- 数值越高越快达到 cap_max\n- 越低则逐步增长，泼溅的摆放往往更讲究。"),
    ZH_HANT("每一輪潑濺數量增長的倍數。\n- 數值越高越快達到 cap_max\n- 越低則逐步增長，潑濺的擺放往往更講究。"),
    KO("한 회차마다 스플랫 수가 얼마나 늘어나는지를 배수로 지정합니다.\n크면 cap_max에 빨리 닿고, 작으면 조금씩 늘어나 스플랫 배치가 더 신중해지는 편입니다."),
    DE("Wie stark die Splat-Zahl in jeder Runde wächst, als Multiplikator.\n- Höher erreicht cap_max eher\n- niedriger wächst allmählich, was Splats meist sorgfältiger platziert."),
    FR("De combien le nombre de splats croît à chaque cycle, en multiplicateur.\n- Plus haut atteint cap_max plus tôt\n- plus bas croît graduellement, ce qui place généralement les splats avec plus de soin."),
    ES("Cuánto crece el número de splats en cada ronda, como multiplicador.\n- Más alto llega antes a cap_max\n- más bajo crece poco a poco, lo que suele colocar los splats con más cuidado."),
    PT("Quanto o número de splats cresce a cada rodada, como multiplicador.\n- Mais alto chega antes a cap_max\n- mais baixo cresce aos poucos, o que costuma posicionar os splats com mais cuidado."),
    IT("Di quanto cresce il numero di splat a ogni ciclo, come moltiplicatore.\n- Più alto raggiunge prima cap_max\n- più basso cresce gradualmente, il che di solito colloca gli splat con più cura."),
    NL("Hoeveel het aantal splats per ronde groeit, als vermenigvuldiger.\n- Hoger bereikt cap_max eerder\n- lager groeit geleidelijk, wat splats meestal zorgvuldiger plaatst."),
    RU("Насколько растёт число сплатов за каждый раунд, множителем.\n- Больше — быстрее достигается cap_max\n- меньше — рост постепенный, и сплаты обычно расставляются аккуратнее."),
    TR("Splat sayısının her turda ne kadar büyüyeceği, çarpan olarak.\n- Yüksek değerler cap_max'e daha çabuk ulaşır\n- düşük değerler yavaş büyür ve splat'ları genelde daha özenli yerleştirir."));

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

SS_MSG(dead_criterion,
    EN("Dead-splat test"), JA("不要なスプラットの判定"),
    ZH_HANS("失效泼溅的判定方式"), ZH_HANT("失效潑濺的判定方式"),
    KO("죽은 스플랫 판정 기준"),
    DE("Test für tote Splats"),
    FR("Critère de splat mort"),
    ES("Criterio de splat muerto"),
    PT("Critério de splat morto"),
    IT("Criterio di splat morto"),
    NL("Criterium voor dode splats"),
    RU("Признак мёртвого сплата"),
    TR("Ölü splat ölçütü"));
SS_MSG(dead_criterion_help,
    EN("Choose when splats are recycled.\n- opacity: below the recycle opacity.\n- max_weight: low peak pixel contribution. Decays with a half-life of max_batch_per_epoch steps."),
    JA("スプラットを不要とみなして、必要な場所へ回す条件です。 `opacity` は再利用の不透明度より薄いスプラットを回します。\n- `max_weight` はスプラットが写真に実際どれだけ写ったか（1 つの画素に加えた最大の重み。`max_batch_per_epoch` ステップの半減期で忘れていきます）を見るので、ほかの陰に隠れたものや、どの視点からも見えないものも回されます。\n- `either` はどちらかの条件に当てはまれば回します。"),
    ZH_HANS("判定泼溅失效、把它回收到需要位置的条件。 `opacity` 回收比回收不透明度更淡的泼溅。\n- `max_weight` 看泼溅在照片中实际显现了多少——它对单个像素贡献过的最大权重，以 `max_batch_per_epoch` 步为半衰期逐渐遗忘——因此被其他泼溅遮挡或不在任何视角内的泼溅也会被回收。\n- `either` 只要满足任一条件就回收。"),
    ZH_HANT("判定潑濺失效、把它回收到需要位置的條件。 `opacity` 回收比回收不透明度更淡的潑濺。\n- `max_weight` 看潑濺在照片中實際顯現了多少——它對單個像素貢獻過的最大權重，以 `max_batch_per_epoch` 步為半衰期逐漸遺忘——因此被其他潑濺遮擋或不在任何視角內的潑濺也會被回收。\n- `either` 只要滿足任一條件就回收。"),
    KO("스플랫을 죽은 것으로 보고 필요한 곳으로 재배치하는 조건입니다. `opacity` 는 재활용 불투명도보다 옅은 스플랫을 재배치합니다.\n- `max_weight` 는 스플랫이 사진에 실제로 얼마나 드러났는지(한 화소에 더한 가장 큰 가중치를 `max_batch_per_epoch` 스텝의 반감기로 잊어 감)를 보므로, 다른 것에 가려졌거나 어느 시점에서도 보이지 않는 스플랫도 재배치됩니다.\n- `either` 는 둘 중 하나라도 해당하면 재배치합니다."),
    DE("Was einen Splat als tot markiert, sodass er dorthin recycelt wird, wo er gebraucht wird. `opacity` recycelt Splats, die blasser als die Recycling-Deckkraft sind.\n- `max_weight` fragt, wie viel der Splat in den Fotos wirklich zu sehen ist -- das größte Gewicht, das er einem Pixel beigetragen hat, vergessen mit einer Halbwertszeit von `max_batch_per_epoch` Schritten --, sodass auch verdeckte oder in keiner Ansicht sichtbare Splats recycelt werden.\n- `either` recycelt einen Splat, der einen der beiden Tests nicht besteht."),
    FR("Ce qui fait qu'un splat est jugé mort et recyclé là où il manque. `opacity` recycle les splats plus pâles que l'opacité de recyclage. `max_weight` regarde ce que le splat montre réellement sur les photos -- le plus grand poids qu'il ait apporté à un pixel, oublié avec une demi-vie de `max_batch_per_epoch` pas --, si bien que les splats cachés derrière d'autres ou hors de toute vue sont recyclés aussi.\n`either` recycle un splat qui échoue à l'un ou l'autre test."),
    ES("Qué marca un splat como muerto para reciclarlo donde haga falta. `opacity` recicla los splats más tenues que la opacidad de reciclaje.\n`max_weight` mira cuánto se ve realmente el splat en las fotos -- el mayor peso que aportó a un píxel, olvidado con una vida media de `max_batch_per_epoch` pasos --, así que también se reciclan los splats tapados por otros o fuera de toda vista. `either` recicla un splat que falle cualquiera de las dos pruebas."),
    PT("O que marca um splat como morto para ser reciclado onde faz falta. `opacity` recicla os splats mais fracos que a opacidade de reciclagem. `max_weight` vê quanto o splat aparece de fato nas fotos -- o maior peso que deu a um pixel, esquecido com uma meia-vida de `max_batch_per_epoch` passos --, por isso também são reciclados os splats tapados por outros ou fora de todas as vistas.\n`either` recicla um splat que falhe qualquer um dos testes."),
    IT("Che cosa rende morto uno splat, da riciclare dove serve. `opacity` ricicla gli splat più deboli dell'opacità di riciclo. `max_weight` guarda quanto lo splat si vede davvero nelle foto -- il peso massimo che ha dato a un pixel, dimenticato con un tempo di dimezzamento di `max_batch_per_epoch` passi --, così vengono riciclati anche gli splat nascosti da altri o fuori da ogni vista.\n`either` ricicla uno splat che non supera l'uno o l'altro test."),
    NL("Wat een splat als dood markeert, zodat hij wordt hergebruikt waar hij nodig is. `opacity` hergebruikt splats die vager zijn dan de hergebruikdekking.\n`max_weight` kijkt hoeveel de splat echt in de foto's te zien is -- het grootste gewicht dat hij aan één pixel bijdroeg, vergeten met een halveringstijd van `max_batch_per_epoch` stappen --, zodat ook splats die achter andere verborgen zijn of in geen enkel beeld voorkomen worden hergebruikt. `either` hergebruikt een splat die voor een van beide tests zakt."),
    RU("Что делает сплат мёртвым, чтобы переработать его туда, где он нужен. `opacity` перерабатывает сплаты бледнее порога переработки.\n- `max_weight` смотрит, насколько сплат действительно виден на фотографиях, -- наибольший вес, который он внёс в один пиксель, забываемый с периодом полураспада `max_batch_per_epoch` шагов, -- поэтому перерабатываются и сплаты, закрытые другими или не попадающие ни в один ракурс.\n- `either` перерабатывает сплат, не прошедший любую из двух проверок."),
    TR("Bir splat'ı ölü sayıp gereken yere geri dönüştüren koşul. `opacity` geri dönüşüm saydamsızlığından daha soluk splat'ları geri dönüştürür.\n- `max_weight` splat'ın fotoğraflarda gerçekten ne kadar göründüğüne bakar -- tek bir piksele kattığı en büyük ağırlık, `max_batch_per_epoch` adımlık yarı ömürle unutulur --, böylece başkalarının arkasında kalan ya da hiçbir görüntüde yer almayan splat'lar da geri dönüştürülür.\n- `either` iki testten birini geçemeyen splat'ı geri dönüştürür."));

SS_MSG(refine_every,
    EN("Refinement interval"), JA("スプラット追加の間隔"),
    ZH_HANS("细化间隔"), ZH_HANT("細化間隔"), KO("정제 간격"),
    DE("Verfeinerungsintervall"), FR("Intervalle de raffinement"),
    ES("Intervalo de refinamiento"), PT("Intervalo de refinamento"),
    IT("Intervallo di raffinamento"), NL("Verfijningsinterval"),
    RU("Интервал уточнения"), TR("İyileştirme aralığı"));
SS_MSG(refine_every_help,
    EN("How many steps between rounds of adding and relocating splats.\nSmaller reacts to missing detail sooner; larger is calmer and slightly cheaper."),
    JA("スプラットを増やしたり移したりするラウンドの間隔をステップ数で指定します。\n短いほど足りない細部に早く反応し、長いほど落ち着いていて、わずかに軽くなります。"),
    ZH_HANS("两轮新增和迁移泼溅之间相隔多少步。\n间隔越短，对缺失细节反应越快；越长则更平稳，也略微省一点开销。"),
    ZH_HANT("兩輪新增和遷移潑濺之間相隔多少步。\n間隔越短，對缺失細節反應越快；越長則更平穩，也略微省一點開銷。"),
    KO("스플랫을 늘리고 옮기는 회차 사이의 간격을 스텝 수로 지정합니다.\n짧으면 부족한 디테일에 더 빨리 반응하고, 길면 더 차분하며 비용도 약간 적습니다."),
    DE("Wie viele Schritte zwischen den Runden liegen, in denen Splats hinzukommen und umziehen.\nKleiner reagiert früher auf fehlendes Detail; größer ist ruhiger und etwas billiger."),
    FR("Combien d'étapes séparent les cycles d'ajout et de déplacement de splats.\nPlus petit réagit plus tôt au détail manquant ; plus grand est plus calme et légèrement moins coûteux."),
    ES("Cuántos pasos median entre las rondas de añadir y reubicar splats.\nMenor reacciona antes al detalle que falta; mayor es más tranquilo y algo más barato."),
    PT("Quantos passos separam as rodadas de acrescentar e realocar splats.\nMenor reage mais cedo ao detalhe que falta; maior é mais calmo e um pouco mais barato."),
    IT("Quanti passi separano i cicli di aggiunta e spostamento degli splat.\nPiù piccolo reagisce prima al dettaglio mancante; più grande è più calmo e leggermente più economico."),
    NL("Hoeveel stappen er zitten tussen de rondes waarin splats worden toegevoegd en verplaatst.\nKleiner reageert eerder op ontbrekend detail; groter is rustiger en iets goedkoper."),
    RU("Сколько шагов между раундами добавления и перемещения сплатов.\nМеньше — быстрее реакция на нехватку деталей; больше — спокойнее и чуть дешевле."),
    TR("Splat ekleme ve taşıma turları arasında kaç adım olduğu.\nKüçük değerler eksik ayrıntıya daha erken tepki verir; büyük değerler daha sakin ve biraz daha ucuzdur."));

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
    EN("Earliest step to stop adding splats. Growth stops at the later of this step and refine_stop_num_iter steps before the end."),
    JA("スプラット追加を終了できる最も早いステップ。このステップと、終了の refine_stop_num_iter ステップ前のうち、遅い方で追加を止めます。"),
    ZH_HANS("最早停止增加点的步数。实际停止时间取此步数与训练结束前 refine_stop_num_iter 步中的较晚者。"),
    ZH_HANT("最早停止增加點的步數。實際停止時間取此步數與訓練結束前 refine_stop_num_iter 步中的較晚者。"),
    KO("스플랫 추가를 멈출 수 있는 가장 이른 스텝입니다. 이 스텝과 종료 refine_stop_num_iter 스텝 전 중 늦은 시점에 멈춥니다."),
    DE("Frühester Schritt zum Beenden des Splat-Wachstums. Es endet hier oder refine_stop_num_iter Schritte vor Schluss, je nachdem, was später liegt."),
    FR("Premier pas possible d’arrêt de croissance. Elle cesse à ce pas ou refine_stop_num_iter pas avant la fin, selon le plus tardif."),
    ES("Primer paso posible para dejar de añadir splats. Se detiene aquí o refine_stop_num_iter pasos antes del final, lo que ocurra más tarde."),
    PT("Primeiro passo possível para parar de adicionar splats. Para aqui ou refine_stop_num_iter passos antes do fim, o que ocorrer mais tarde."),
    IT("Primo passo possibile per fermare la crescita. Si ferma qui o refine_stop_num_iter passi prima della fine, a seconda di quale sia più tardi."),
    NL("Vroegste stap om de groei te stoppen. Groei stopt hier of refine_stop_num_iter stappen vóór het einde, afhankelijk van wat later is."),
    RU("Самый ранний шаг остановки роста. Он завершается на этом шаге или за refine_stop_num_iter шагов до конца — в более поздний момент."),
    TR("Splat eklemenin durabileceği en erken adım. Bu adım ile sondan refine_stop_num_iter adım öncesinden hangisi daha geçse orada durur."));

SS_MSG(noise_lr,
    EN("Position jitter"), JA("位置のゆらぎ"), ZH_HANS("位置抖动"),
    ZH_HANT("位置抖動"), KO("위치 흔들림"), DE("Positionsrauschen"),
    FR("Agitation des positions"), ES("Sacudida de las posiciones"),
    PT("Tremor das posições"), IT("Perturbazione delle posizioni"),
    NL("Positieruis"), RU("Дрожание позиций"), TR("Konum titreşimi"));
SS_MSG(noise_lr_help,
    EN("How much random jitter is applied to splat positions early in training.\n- Jitter helps splats escape bad spots and spread into unfilled areas\n- too much of it blurs detail."),
    JA("学習の序盤に、スプラットの位置へどれだけランダムなゆらぎを加えるかです。\nゆらぎはスプラットが悪い場所から抜け出して未充填の場所へ広がるのを助けますが、多すぎると細部がぼやけます。"),
    ZH_HANS("训练初期给泼溅位置施加多少随机抖动。\n抖动能帮助泼溅逃离不好的位置、扩散到未填充的区域，但过多会让细节发糊。"),
    ZH_HANT("訓練初期給潑濺位置施加多少隨機抖動。\n抖動能幫助潑濺逃離不好的位置、擴散到未填充的區域，但過多會讓細節發糊。"),
    KO("학습 초반에 스플랫 위치에 얼마나 무작위 흔들림을 줄지입니다.\n흔들림은 스플랫이 나쁜 자리에서 빠져나와 빈 곳으로 퍼지도록 돕지만, 지나치면 디테일이 뭉개집니다."),
    DE("Wie viel zufälliges Zittern früh im Training auf die Splat-Positionen gelegt wird.\n- Zittern hilft Splats, schlechte Stellen zu verlassen und sich in ungefüllte Bereiche auszubreiten\n- zu viel davon verwischt Detail."),
    FR("Quelle agitation aléatoire est appliquée aux positions des splats en début d'entraînement.\n- L'agitation aide les splats à quitter les mauvais endroits et à gagner les zones non remplies\n- trop en floute le détail."),
    ES("Cuánta sacudida aleatoria se aplica a las posiciones de los splats al principio del entrenamiento.\n- La sacudida ayuda a los splats a salir de malos sitios y extenderse a zonas sin rellenar\n- demasiada emborrona el detalle."),
    PT("Quanto tremor aleatório é aplicado às posições dos splats no início do treinamento.\n- O tremor ajuda os splats a sair de lugares ruins e se espalhar por áreas não preenchidas\n- em excesso borra o detalhe."),
    IT("Quanta perturbazione casuale viene applicata alle posizioni degli splat all'inizio dell'addestramento.\n- La perturbazione aiuta gli splat a uscire dai punti sbagliati e a diffondersi nelle zone non riempite\n- troppa sfoca il dettaglio."),
    NL("Hoeveel willekeurige trilling er vroeg in de training op de splatposities wordt gezet.\n- Trilling helpt splats slechte plekken te verlaten en zich over ongevulde gebieden te verspreiden\n- te veel maakt detail wazig."),
    RU("Сколько случайного дрожания добавляется к позициям сплатов в начале обучения.\n- Дрожание помогает сплатам выбираться из неудачных мест и расходиться в незаполненные области\n- избыток размывает детали."),
    TR("Eğitimin başında splat konumlarına ne kadar rastgele titreşim uygulandığı.\n- Titreşim splat'ların kötü noktalardan kurtulup dolmamış alanlara yayılmasına yardım eder\n- fazlası ayrıntıyı bulanıklaştırır."));

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
    EN("- mean: balanced default; geom: between mean and median.\n- max: reacts to one bad view.\n- median: ignores occasional odd views, useful with moving people or cars."),
    JA("- mean：バランスのよい既定値。geom：meanとmedianの中間。\n- max：1つの悪いビューにも反応します。\n- median：時折の異常ビューを無視し、人や車の移動に有効です。"),
    ZH_HANS("- mean：均衡的默认值；geom：介于 mean 与 median 之间。\n- max：对单个差视图作出反应。\n- median：忽略偶发异常视图，适合有人或车移动的拍摄。"),
    ZH_HANT("- mean：均衡的預設值；geom：介於 mean 與 median 之間。\n- max：對單個差視圖作出反應。\n- median：忽略偶發異常視圖，適合有人或車移動的拍攝。"),
    KO("- mean: 균형 잡힌 기본값. geom: mean과 median의 중간입니다.\n- max: 하나의 나쁜 뷰에도 반응합니다.\n- median: 간헐적 이상 뷰를 무시하며, 사람이나 차가 움직일 때 유용합니다."),
    DE("- mean: ausgewogene Vorgabe; geom: zwischen mean und median.\n- max: reagiert auf eine schlechte Ansicht.\n- median: ignoriert einzelne Ausreißeransichten, hilfreich bei bewegten Menschen oder Autos."),
    FR("- mean : défaut équilibré ; geom : entre mean et median.\n- max : réagit à une seule mauvaise vue.\n- median : ignore les vues aberrantes occasionnelles, utile avec personnes ou voitures mobiles."),
    ES("- mean: equilibrado por defecto; geom: entre mean y median.\n- max: reacciona a una sola vista mala.\n- median: ignora vistas atípicas ocasionales; útil con personas o coches en movimiento."),
    PT("- mean: padrão equilibrado; geom: entre mean e median.\n- max: reage a uma vista ruim.\n- median: ignora vistas atípicas ocasionais; útil com pessoas ou carros em movimento."),
    IT("- mean: predefinito equilibrato; geom: tra mean e median.\n- max: reagisce a una sola vista errata.\n- median: ignora viste anomale occasionali; utile con persone o auto in movimento."),
    NL("- mean: evenwichtige standaard; geom: tussen mean en median.\n- max: reageert op één slecht beeld.\n- median: negeert incidentele afwijkende beelden, nuttig bij bewegende mensen of auto’s."),
    RU("- mean: сбалансированный по умолчанию; geom: между mean и median.\n- max: реагирует на один плохой вид.\n- median: игнорирует редкие аномальные виды, полезен при движущихся людях и машинах."),
    TR("- mean: dengeli varsayılan; geom: mean ile median arası.\n- max: tek kötü görünüme tepki verir.\n- median: arada bir bozuk görünümü yok sayar; hareketli insan ve araçlarda yararlıdır."));

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
    EN("Balances image error against physical splat size. 0 uses image error only; toward 1 adds detail to large distant structures."),
    JA("画像誤差とスプラットの実寸のバランス。0は画像誤差のみ。1に近いほど大きな遠景の構造に細部を加えます。"),
    ZH_HANS("平衡图像误差与泼溅的物理尺寸。0 仅用图像误差；接近 1 时向大型远处结构添加更多细节。"),
    ZH_HANT("平衡影像誤差與潑濺的實體尺寸。0 僅用影像誤差；接近 1 時向大型遠處結構加入更多細節。"),
    KO("이미지 오차와 스플랫 실제 크기의 균형입니다. 0은 이미지 오차만 쓰며, 1에 가까울수록 크고 먼 구조에 세부를 추가합니다."),
    DE("Gewichtet Bildfehler gegen physische Splat-Größe. 0 nutzt nur Bildfehler; Richtung 1 erhält große entfernte Struktur mehr Details."),
    FR("Équilibre erreur d’image et taille physique des splats. 0 utilise l’erreur seule ; vers 1, favorise les grandes structures éloignées."),
    ES("Equilibra error de imagen y tamaño físico del splat. 0 usa solo error; hacia 1 añade detalle a estructuras grandes y lejanas."),
    PT("Equilibra erro de imagem e tamanho físico do splat. 0 usa só erro; perto de 1 adiciona detalhes a estruturas grandes e distantes."),
    IT("Bilancia errore d’immagine e dimensione fisica degli splat. 0 usa solo l’errore; verso 1 aggiunge dettagli alle grandi strutture lontane."),
    NL("Weegt beeldfout tegen fysieke splatgrootte. 0 gebruikt alleen beeldfout; richting 1 krijgen grote verre structuren meer detail."),
    RU("Баланс ошибки изображения и физического размера сплата. 0 — только ошибка; ближе к 1 добавляет детали крупным далёким структурам."),
    TR("Görüntü hatası ile fiziksel splat boyutunu dengeler. 0 yalnız hata kullanır; 1’e doğru büyük uzak yapılara ayrıntı ekler."));

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
    EN("Error used to place new splats. none spreads them evenly.\n- ssim_structure ignores brightness shifts. ssim_full, ssim_cs and loss_full use progressively more color error.\n- edge_aware follows photo edges. robust_edge_aware ignores the worst matches. _nms keeps error ridges.\n- s_cielab measures perceived color. flip measures perceptual error. flip_feature uses its edges and points.\n- auto chooses s_cielab or flip_feature for matching --perceptual-loss modes, otherwise ssim_cs."),
    JA("新しいスプラットをどこに足すかを、どの種類の誤差で決めるかです。 `ssim_structure` は明るさの違いを無視して、模様やエッジの食い違いを狙います。\n- `ssim_full`、`ssim_cs`、`loss_full` の順に、生の色の誤差をより多く取り込みます。 `edge_aware` は、すでにうまく再現できているかどうかに関わらず、元写真のエッジを追います。\n- `robust_edge_aware` は同じですが、いちばん合っていない画素を無視するので、動く人や車にスプラットが集まりません。\n- `none` は新しいスプラットを均等に散らします。 `_nms` の付いたモードは、まず誤差を稜線だけに細らせるので、ぼやけた広がりではなく、いちばん鋭いところにスプラットが乗ります。\n- `s_cielab` は目に見えるとおりの S-CIELAB 色差で評価します。 `flip` は FLIP（二つの画像を切り替えたときに気づく違い）で、`flip_feature` はそのうちエッジと点の部分だけで評価します。 `auto` は `--perceptual-loss` が `s_cielab` なら `s_cielab`、`flip` なら `flip_feature`、それ以外なら `ssim_cs` を使います。"),
    ZH_HANS("用哪种误差决定在哪里新增泼溅。 `ssim_structure` 忽略明暗差异，专门盯住图案和边缘的不一致。 `ssim_full`、`ssim_cs`、`loss_full` 依次纳入更多原始颜色误差。 `edge_aware` 追踪参考照片中的边缘，无论那里是否已经重建得很好。\n- `robust_edge_aware` 与之相同，但忽略最不匹配的像素，因此移动的行人和车辆不会吸引泼溅。 `none` 则把新泼溅均匀铺开。\n- 带 `_nms` 的模式会先把误差收细成一条线，因此泼溅落在最清晰的边缘上，而不是散布在它周围的模糊里。\n- `s_cielab` 按人眼所见的 S-CIELAB 色差打分。\n- `flip` 按 FLIP（在两张图像之间来回切换时能察觉到的差异）打分，`flip_feature` 只取其中边缘和点的部分。 `auto` 在 `--perceptual-loss` 为 `s_cielab` 时用 `s_cielab`，为 `flip` 时用 `flip_feature`，否则用 `ssim_cs`。"),
    ZH_HANT("用哪種誤差決定在哪裡新增潑濺。 `ssim_structure` 忽略明暗差異，專門盯住圖案和邊緣的不一致。 `ssim_full`、`ssim_cs`、`loss_full` 依次納入更多原始顏色誤差。 `edge_aware` 追蹤參考照片中的邊緣，無論那裡是否已經重建得很好。\n- `robust_edge_aware` 與之相同，但忽略最不吻合的像素，因此移動的行人和車輛不會吸引潑濺。 `none` 則把新潑濺均勻鋪開。\n- 帶 `_nms` 的模式會先把誤差收細成一條線，因此潑濺落在最清晰的邊緣上，而不是散布在它周圍的模糊裡。\n- `s_cielab` 按人眼所見的 S-CIELAB 色差打分。\n- `flip` 按 FLIP（在兩張圖像之間來回切換時能察覺到的差異）打分，`flip_feature` 只取其中邊緣和點的部分。 `auto` 在 `--perceptual-loss` 為 `s_cielab` 時用 `s_cielab`，為 `flip` 時用 `flip_feature`，否則用 `ssim_cs`。"),
    KO("새 스플랫을 어디에 추가할지 어떤 종류의 오차로 정할지입니다. `ssim_structure`는 밝기 차이를 무시하고 무늬와 가장자리의 불일치를 노립니다. `ssim_full`, `ssim_cs`, `loss_full` 순으로 원본 색 오차를 점점 더 많이 반영합니다. `edge_aware`는 이미 잘 복원됐는지와 상관없이 원본 사진의 가장자리를 좇습니다.\n- `robust_edge_aware`는 같지만 가장 안 맞는 픽셀을 무시하므로 움직이는 사람과 차에 스플랫이 몰리지 않습니다.\n- `none`은 새 스플랫을 고르게 흩뿌립니다. `_nms`가 붙은 방식은 오차를 먼저 능선만 남기고 얇게 깎아내므로, 흐릿하게 번진 곳이 아니라 가장 날카로운 가장자리에 스플랫이 놓입니다.\n- `s_cielab`은 눈에 보이는 대로의 S-CIELAB 색차로 평가합니다. `flip`은 FLIP(두 이미지를 번갈아 볼 때 눈에 띄는 차이)으로, `flip_feature`는 그중 가장자리와 점 부분만으로 평가합니다.\n- `auto`는 `--perceptual-loss`가 `s_cielab`이면 `s_cielab`을, `flip`이면 `flip_feature`를, 아니면 `ssim_cs`를 씁니다."),
    DE("Welche Art von Fehler entscheidet, wo neue Splats hinzukommen. `ssim_structure` zielt auf abweichende Muster und Kanten und ignoriert Helligkeitsunterschiede.\n- `ssim_full`, `ssim_cs` und `loss_full` nehmen zunehmend mehr vom reinen Farbfehler auf. `edge_aware` verfolgt Kanten in den Vorlagefotos, gleich ob sie schon gut rekonstruiert sind. `robust_edge_aware` tut dasselbe, ignoriert aber die am schlechtesten passenden Pixel, sodass bewegte Personen und Autos keine Splats anziehen.\n- `none` verteilt neue Splats gleichmäßig. Jeder Modus mit `_nms` dünnt den Fehler zuerst auf seine Gratlinien aus, sodass Splats auf der schärfsten Kante landen statt in der Unschärfe darum herum. `s_cielab` bewertet den S-CIELAB-Farbabstand, so wie das Auge ihn sieht.\n- `flip` bewertet FLIP, den Unterschied, der beim Hin- und Herschalten zwischen beiden Bildern auffällt, und `flip_feature` nur dessen Kanten- und Punktanteil.\n- `auto` nimmt `s_cielab` bzw. `flip_feature`, wenn `--perceptual-loss` auf `s_cielab` bzw. `flip` steht, sonst `ssim_cs`."),
    FR("Quel type d'erreur décide où ajouter de nouveaux splats. `ssim_structure` vise les motifs et les contours qui ne correspondent pas, en ignorant les écarts de luminosité.\n- `ssim_full`, `ssim_cs` et `loss_full` intègrent progressivement plus d'erreur de couleur brute.\n- `edge_aware` suit les contours des photos de référence, qu'ils soient déjà bien reconstruits ou non.\n- `robust_edge_aware` fait de même mais ignore les pixels qui correspondent le moins, de sorte que passants et voitures n'attirent pas de splats. `none` répartit les nouveaux splats uniformément. Tout mode avec `_nms` amincit d'abord l'erreur jusqu'à ses lignes de crête, de sorte que les splats se posent sur le contour le plus net plutôt que dans le flou autour. `s_cielab` évalue l'écart de couleur S-CIELAB, tel que l'œil le perçoit.\n- `flip` évalue FLIP, l'écart qu'on remarque en alternant entre les deux images, et `flip_feature` seulement sa part de contours et de points. `auto` prend `s_cielab` ou `flip_feature` quand `--perceptual-loss` vaut `s_cielab` ou `flip`, et `ssim_cs` sinon."),
    ES("Qué tipo de error decide dónde se añaden splats nuevos. `ssim_structure` apunta a los patrones y bordes que no coinciden e ignora las diferencias de brillo.\n- `ssim_full`, `ssim_cs` y `loss_full` incorporan progresivamente más error de color puro. `edge_aware` persigue los bordes de las fotos de referencia, estén ya bien reconstruidos o no. `robust_edge_aware` hace lo mismo pero ignora los píxeles que peor coinciden, de modo que personas y coches en movimiento no atraen splats.\n- `none` reparte los splats nuevos de manera uniforme.\n- Cualquier modo con `_nms` adelgaza primero el error hasta sus líneas de cresta, de modo que los splats caen en el borde más nítido y no en el desenfoque que lo rodea.\n- `s_cielab` puntúa la diferencia de color S-CIELAB, tal como la ve el ojo. `flip` puntúa FLIP, la diferencia que se nota al alternar entre las dos imágenes, y `flip_feature` solo su parte de bordes y puntos. `auto` usa `s_cielab` o `flip_feature` cuando `--perceptual-loss` es `s_cielab` o `flip`, y `ssim_cs` en otro caso."),
    PT("Que tipo de erro decide onde novos splats são acrescentados. `ssim_structure` mira os padrões e bordas que não coincidem e ignora diferenças de brilho.\n- `ssim_full`, `ssim_cs` e `loss_full` incorporam progressivamente mais erro de cor puro.\n- `edge_aware` persegue as bordas das fotos de referência, estejam ou não já bem reconstruídas. `robust_edge_aware` faz o mesmo, mas ignora os pixels que pior coincidem, de modo que pessoas e carros em movimento não atraem splats.\n- `none` espalha os novos splats de forma uniforme. Qualquer modo com `_nms` afina primeiro o erro até às suas linhas de crista, de modo que os splats assentam na borda mais nítida e não no desfoque à volta dela. `s_cielab` pontua a diferença de cor S-CIELAB, tal como o olho a vê.\n- `flip` pontua o FLIP, a diferença que se nota ao alternar entre as duas imagens, e `flip_feature` só a sua parte de bordas e pontos. `auto` usa `s_cielab` ou `flip_feature` quando `--perceptual-loss` é `s_cielab` ou `flip`, e `ssim_cs` caso contrário."),
    IT("Che tipo di errore decide dove aggiungere nuovi splat. `ssim_structure` punta a motivi e bordi che non corrispondono e ignora le differenze di luminosità.\n- `ssim_full`, `ssim_cs` e `loss_full` includono via via più errore di colore puro.\n- `edge_aware` insegue i bordi nelle foto di riferimento, che siano già ricostruiti bene o no. `robust_edge_aware` fa lo stesso ma ignora i pixel che corrispondono peggio, così persone e automobili in movimento non attirano splat.\n- `none` distribuisce i nuovi splat in modo uniforme. Ogni modalità con `_nms` assottiglia prima l'errore fino alle sue linee di cresta, così gli splat si posano sul bordo più nitido e non nella sfocatura attorno.\n- `s_cielab` valuta la differenza di colore S-CIELAB, come la vede l'occhio. `flip` valuta FLIP, la differenza che si nota alternando le due immagini, e `flip_feature` solo la sua parte di bordi e punti. `auto` usa `s_cielab` o `flip_feature` quando `--perceptual-loss` è `s_cielab` o `flip`, altrimenti `ssim_cs`."),
    NL("Welk soort fout bepaalt waar nieuwe splats bij komen. `ssim_structure` mikt op patronen en randen die niet kloppen en negeert helderheidsverschillen.\n- `ssim_full`, `ssim_cs` en `loss_full` nemen stapsgewijs meer van de ruwe kleurfout mee.\n- `edge_aware` volgt randen in de referentiefoto's, of die nu al goed gereconstrueerd zijn of niet. `robust_edge_aware` doet hetzelfde maar negeert de slechtst passende pixels, zodat bewegende mensen en auto's geen splats aantrekken.\n- `none` verdeelt nieuwe splats gelijkmatig. Elke modus met `_nms` dunt de fout eerst uit tot haar ribbellijnen, zodat splats op de scherpste rand landen en niet in de onscherpte eromheen. `s_cielab` beoordeelt het S-CIELAB-kleurverschil, zoals het oog het ziet.\n- `flip` beoordeelt FLIP, het verschil dat opvalt als je tussen beide beelden wisselt, en `flip_feature` alleen het rand- en puntdeel daarvan. `auto` neemt `s_cielab` of `flip_feature` als `--perceptual-loss` op `s_cielab` of `flip` staat, anders `ssim_cs`."),
    RU("Какая именно ошибка решает, куда добавлять новые сплаты. `ssim_structure` целится в несовпадающие узоры и края и не смотрит на разницу яркости.\n- `ssim_full`, `ssim_cs` и `loss_full` последовательно добавляют всё больше чистой ошибки цвета.\n- `edge_aware` гонится за краями на исходных снимках независимо от того, восстановлены они уже хорошо или нет.\n- `robust_edge_aware` делает то же, но пропускает хуже всего совпадающие пиксели, поэтому идущие люди и машины не притягивают сплаты. `none` распределяет новые сплаты равномерно. Любой режим с `_nms` сначала утончает ошибку до её гребней, поэтому сплаты ложатся на самый резкий край, а не расплываются вокруг него. `s_cielab` оценивает цветовое различие S-CIELAB так, как его видит глаз.\n- `flip` оценивает FLIP — разницу, заметную при переключении между двумя изображениями, а `flip_feature` — только её часть по краям и точкам. `auto` берёт `s_cielab` или `flip_feature`, если `--perceptual-loss` равен `s_cielab` или `flip`, иначе `ssim_cs`."),
    TR("Yeni splat'ların nereye ekleneceğine hangi tür hatanın karar vereceği. `ssim_structure` parlaklık farklarını yok sayar ve uyuşmayan desenlerle kenarları hedefler.\n- `ssim_full`, `ssim_cs` ve `loss_full` sırayla ham renk hatasından daha fazlasını katar. `edge_aware`, iyi yeniden oluşturulmuş olsun olmasın kaynak fotoğraflardaki kenarların peşine düşer. `robust_edge_aware` aynısını yapar ama en kötü uyuşan pikselleri yok sayar; böylece hareket eden insanlar ve arabalar splat çekmez.\n- `none` ise yeni splat'ları eşit dağıtır. `_nms` içeren her kip, hatayı önce sırt çizgilerine kadar inceltir;\n- böylece splat'lar çevredeki bulanıklığa yayılmak yerine en keskin kenara oturur. `s_cielab`, S-CIELAB renk farkını gözün gördüğü gibi puanlar. `flip`, iki görüntü arasında gidip gelirken fark edilen farkı, yani FLIP'i puanlar; `flip_feature` ise yalnızca onun kenar ve nokta kısmını.\n- `auto`, `--perceptual-loss` `s_cielab` ise `s_cielab`, `flip` ise `flip_feature`, değilse `ssim_cs` kullanır."));

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
    EN("Error cutoff quantile for robust_edge_aware. Lower rejects more moving distractions; higher retains more real detail in clean captures."),
    JA("robust_edge_aware の誤差除外分位点。低いほど動く邪魔物を多く除き、高いほど良好な撮影の細部を保ちます。"),
    ZH_HANS("robust_edge_aware 的误差截断分位数。较低会排除更多移动干扰；较高则保留干净拍摄中的更多真实细节。"),
    ZH_HANT("robust_edge_aware 的誤差截斷分位數。較低會排除更多移動干擾；較高則保留乾淨拍攝中的更多真實細節。"),
    KO("robust_edge_aware의 오차 차단 분위수입니다. 낮으면 움직이는 방해물을 더 제외하고, 높으면 깨끗한 촬영의 실제 세부를 더 유지합니다."),
    DE("Fehlergrenzquantil für robust_edge_aware. Niedriger verwirft mehr Bewegung; höher bewahrt echte Details sauberer Aufnahmen."),
    FR("Quantile de coupure de l’erreur pour robust_edge_aware. Plus bas rejette plus de distractions mobiles ; plus haut garde le détail des captures propres."),
    ES("Cuantil de corte del error para robust_edge_aware. Menor rechaza más distracciones móviles; mayor conserva detalle en capturas limpias."),
    PT("Quantil de corte do erro para robust_edge_aware. Menor rejeita mais distrações móveis; maior guarda detalhe em capturas limpas."),
    IT("Quantile di taglio dell’errore per robust_edge_aware. Bassa scarta più disturbi mobili; alta conserva dettagli in riprese pulite."),
    NL("Foutgrenskwantiel voor robust_edge_aware. Lager verwerpt meer bewegende afleiding; hoger bewaart details van schone opnamen."),
    RU("Квантиль отсечения ошибки для robust_edge_aware. Ниже — больше отсечения движущихся помех; выше — больше деталей чистой съёмки."),
    TR("robust_edge_aware için hata kesim niceliği. Düşük, hareketli dikkat dağıtıcıları eler; yüksek, temiz çekimde ayrıntı korur."));

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
    EN("_nms only: multiplies pixel error by this per stronger neighbor across the edge (up to two).\n- 0 keeps only crests, risking speckle in grainy/leafy scenes; 1 disables thinning.\n- Intermediate values balance sharp edges and smooth noisy areas."),
    JA("_nms専用：エッジを横切る強い隣接点ごとに誤差へこの値を掛けます（最大2点）。\n- 0は頂点のみで葉や粒状の場面が斑点化する恐れ。1は間引きなし。\n- 中間値は鋭い輪郭とノイズ領域の滑らかさを両立します。"),
    ZH_HANS("仅用于 _nms：边缘两侧每有一个更强邻点，就将像素误差乘以此值（最多两次）。\n- 0 仅保留峰顶，可能让树叶或颗粒场景变成斑点；1 关闭细化。\n- 中间值平衡清晰边缘与噪声区域的平滑度。"),
    ZH_HANT("僅用於 _nms：邊緣兩側每有一個更強鄰點，就將像素誤差乘以此值（最多兩次）。\n- 0 僅保留峰頂，可能讓樹葉或顆粒場景變成斑點；1 關閉細化。\n- 中間值平衡清晰邊緣與雜訊區域的平滑度。"),
    KO("_nms 전용: 경계 건너 더 강한 이웃마다 픽셀 오차에 이 값을 곱합니다(최대 둘).\n- 0은 능선만 남겨 잎/입자 장면에 반점을 만들 수 있고, 1은 솎기를 끕니다.\n- 중간값은 선명한 경계와 노이즈 영역의 매끄러움을 절충합니다."),
    DE("Nur _nms: multipliziert Pixelfehler je stärkerem Nachbarn quer zur Kante (bis zu zwei).\n- 0 behält nur Kämme, kann Laub/Korn zerstückeln; 1 deaktiviert Ausdünnung.\n- Zwischenwerte balancieren scharfe Kanten und glatte Rauschbereiche."),
    FR("_nms seul : multiplie l’erreur par voisin plus fort à travers le bord (deux maximum).\n- 0 garde les crêtes, risque de taches dans feuillage/grain ; 1 désactive l’amincissement.\n- Valeurs intermédiaires : bords nets et zones bruitées lisses."),
    ES("Solo _nms: multiplica error por cada vecino más fuerte a través del borde (hasta dos).\n- 0 deja crestas y puede motejar escenas con hojas/grano; 1 desactiva el adelgazado.\n- Valores intermedios equilibran bordes nítidos y zonas ruidosas suaves."),
    PT("Só _nms: multiplica erro por vizinho mais forte através da borda (até dois).\n- 0 mantém cristas e pode salpicar cenas de folhas/grãos; 1 desativa o afinamento.\n- Valores intermediários equilibram bordas nítidas e áreas ruidosas suaves."),
    IT("Solo _nms: moltiplica l’errore per vicino più forte oltre il bordo (massimo due).\n- 0 tiene le creste, rischiando macchie su foglie/grana; 1 disattiva l’assottigliamento.\n- Valori intermedi bilanciano bordi netti e aree rumorose uniformi."),
    NL("Alleen _nms: vermenigvuldigt pixelfout per sterkere buur dwars op de rand (maximaal twee).\n- 0 houdt alleen toppen, met kans op spikkels in blad/korrel; 1 schakelt uitdunnen uit.\n- Tussenwaarden balanceren scherpe randen en gladde ruisgebieden."),
    RU("Только _nms: умножает ошибку на это за каждого более сильного соседа поперёк края (до двух).\n- 0 оставляет гребни, рискуя пятнами на листве/зерне; 1 отключает прореживание.\n- Средние значения балансируют чёткие края и гладкие шумные зоны."),
    TR("Yalnız _nms: kenara dik yöndeki daha güçlü komşu başına hatayı bununla çarpar (en çok iki).\n- 0 tepeleri tutar, yapraklı/taneli sahneyi benekleyebilir; 1 inceltmeyi kapatır.\n- Ara değerler keskin kenar ve pürüzsüz gürültülü alanları dengeler."));

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
    EN("Divides each photo’s detail error by its median so photos count equally. Off uses raw errors, letting a few photos dominate splat placement."),
    JA("写真ごとの細部誤差を中央値で割り、各写真を同等に扱います。無効では生の誤差を使うため、一部の写真が追加位置を支配し得ます。"),
    ZH_HANS("将每张照片的细节误差除以其中位数，让各照片权重均衡。关闭时使用原始误差，少数照片可能主导泼溅位置。"),
    ZH_HANT("將每張照片的細節誤差除以其中位數，讓各照片權重均衡。關閉時使用原始誤差，少數照片可能主導潑濺位置。"),
    KO("사진별 세부 오차를 중앙값으로 나눠 비중을 맞춥니다. 끄면 원래 오차를 써 일부 사진이 스플랫 배치를 지배할 수 있습니다."),
    DE("Teilt den Detailfehler jedes Fotos durch seinen Median, für gleiche Gewichtung. Aus nutzt Rohfehler; wenige Fotos können die Platzierung dominieren."),
    FR("Divise l’erreur de détail de chaque photo par sa médiane pour équilibrer les vues. Désactivé : erreurs brutes ; quelques vues peuvent dominer."),
    ES("Divide el error de detalle de cada foto por su mediana para equilibrarlas. Desactivado usa errores brutos; pocas fotos pueden dominar la colocación."),
    PT("Divide o erro de detalhe de cada foto pela mediana para equilibrá-las. Desativado usa erros brutos; poucas fotos podem dominar a colocação."),
    IT("Divide l’errore di dettaglio per la mediana di ogni foto, bilanciando le viste. Disattivato usa errori grezzi: poche foto possono dominare."),
    NL("Deelt de detailfout van elke foto door zijn mediaan voor gelijke weging. Uit gebruikt ruwe fouten; enkele foto’s kunnen de plaatsing bepalen."),
    RU("Делит ошибку деталей каждого фото на его медиану для равного веса снимков. Выкл.: исходные ошибки, отдельные фото могут определять размещение."),
    TR("Fotoğrafların ağırlığını dengelemek için ayrıntı hatasını kendi ortancasına böler. Kapalıyken ham hata kullanılır; birkaç fotoğraf yerleşime hükmedebilir."));

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
    EN("Caps each photo’s detail error at this pixel quantile to limit outliers. 1 disables clipping; 0.99 clips the worst 1%."),
    JA("各写真の細部誤差をこのピクセル分位点で制限し、外れ値を抑えます。1は制限なし、0.99は最大の1%を制限します。"),
    ZH_HANS("按此像素分位数限制每张照片的细节误差，抑制异常值。1 关闭截断；0.99 截断最差的 1%。"),
    ZH_HANT("按此像素分位數限制每張照片的細節誤差，抑制異常值。1 關閉截斷；0.99 截斷最差的 1%。"),
    KO("사진별 세부 오차를 이 픽셀 분위수로 제한해 이상치를 줄입니다. 1은 제한 없음, 0.99는 최악의 1%를 제한합니다."),
    DE("Begrenzt Foto-Detailfehler auf dieses Pixelquantil gegen Ausreißer. 1 schaltet ab; 0.99 begrenzt das schlechteste 1%."),
    FR("Plafonne l’erreur de détail au quantile des pixels pour limiter les extrêmes. 1 désactive ; 0.99 plafonne le pire 1%."),
    ES("Limita el error de detalle por este cuantil de píxeles para contener atípicos. 1 desactiva; 0.99 limita el peor 1%."),
    PT("Limita o erro de detalhe por este quantil de pixels para conter extremos. 1 desativa; 0.99 limita o pior 1%."),
    IT("Limita l’errore di dettaglio a questo quantile dei pixel contro gli estremi. 1 disattiva; 0.99 limita il peggior 1%."),
    NL("Begrenst detailfout per foto op dit pixelkwantiel tegen uitschieters. 1 schakelt uit; 0.99 begrenst de slechtste 1%."),
    RU("Ограничивает ошибку деталей этим квантилем пикселей против выбросов. 1 отключает; 0.99 ограничивает худший 1%."),
    TR("Aykırı değerleri azaltmak için fotoğraf ayrıntı hatasını bu piksel niceliğinde sınırlar. 1 kapatır; 0.99 en kötü %1’i sınırlar."));

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
    EN("Pixel-error exponent before splat scoring. >1 focuses new splats on worst pixels; <1 spreads them out; 1 leaves errors unchanged."),
    JA("スプラット採点前のピクセル誤差の指数。1超は悪いピクセルに追加分を集中し、1未満は分散します。1は誤差を変えません。"),
    ZH_HANS("泼溅评分前的像素误差指数。大于 1 将新点集中到最差像素；小于 1 使其分散；1 保持误差不变。"),
    ZH_HANT("潑濺評分前的像素誤差指數。大於 1 將新點集中到最差像素；小於 1 使其分散；1 保持誤差不變。"),
    KO("스플랫 채점 전 픽셀 오차 지수입니다. 1 초과는 최악 픽셀에 새 스플랫을 집중하고, 1 미만은 분산하며, 1은 그대로 둡니다."),
    DE("Pixelfehler-Exponent vor Splat-Bewertung. >1 konzentriert neue Splats auf schlechteste Pixel; <1 streut sie; 1 lässt Fehler unverändert."),
    FR("Exposant d’erreur pixel avant le score des splats. >1 concentre les nouveaux splats sur les pires pixels ; <1 les répartit ; 1 ne change rien."),
    ES("Exponente del error de píxel antes de puntuar splats. >1 concentra nuevos splats en los peores píxeles; <1 los reparte; 1 no cambia el error."),
    PT("Expoente do erro de pixel antes de pontuar splats. >1 concentra novos splats nos piores pixels; <1 espalha; 1 não muda o erro."),
    IT("Esponente dell’errore pixel prima del punteggio. >1 concentra nuovi splat sui pixel peggiori, <1 li distribuisce, 1 lascia gli errori invariati."),
    NL("Pixelfoutexponent vóór splatbeoordeling. >1 richt nieuwe splats op de slechtste pixels; <1 spreidt ze; 1 houdt fouten gelijk."),
    RU("Показатель степени ошибки пикселя до оценки сплатов. >1 концентрирует новые сплаты на худших пикселях; <1 распределяет; 1 не меняет ошибки."),
    TR("Splat puanından önce piksel hata üssü. >1 yeni splatları en kötü piksellere yoğunlaştırır; <1 yayar; 1 hatayı değiştirmez."));

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
    EN("- `max` takes its worst pixel, so a splat covering one bad spot still counts.\n- `sum` adds the error up, which favours large splats and busy areas.\n- `avg` takes the average over the area it covers, which judges big and small splats alike."),
    JA("- `max` はいちばん悪い画素を取るので、悪い部分が 1 つでもあれば効きます。\n- `sum` は誤差を足すので、大きなスプラットや込み入った場所が有利になります。\n- `avg` は覆っている範囲の平均を取るので、大小のスプラットを同じものさしで測ります。"),
    ZH_HANS("- `max` 取其中最差的像素，因此只要覆盖到一处很差的地方就会计入。\n- `sum` 把误差累加，会偏向大泼溅和繁杂区域。\n- `avg` 取覆盖范围内的平均值，对大小泼溅一视同仁。"),
    ZH_HANT("- `max` 取其中最差的像素，因此只要覆蓋到一處很差的地方就會計入。\n- `sum` 把誤差累加，會偏向大潑濺和繁雜區域。\n- `avg` 取覆蓋範圍內的平均值，對大小潑濺一視同仁。"),
    KO("- `max` 는 가장 나쁜 화소를 취하므로 나쁜 지점 하나만 덮어도 반영됩니다.\n- `sum` 은 오차를 더하므로 큰 스플랫과 복잡한 영역에 유리합니다.\n- `avg` 는 덮은 범위의 평균을 취하므로 크고 작은 스플랫을 같은 기준으로 봅니다."),
    DE("- `max` nimmt sein schlechtestes Pixel, sodass ein Splat schon zählt, wenn er eine schlechte Stelle überdeckt.\n- `sum` addiert den Fehler auf, was große Splats und unruhige Flächen bevorzugt.\n- `avg` nimmt den Mittelwert über die überdeckte Fläche und misst große wie kleine Splats gleich."),
    FR("- `max` prend son pire pixel, donc un splat qui couvre un seul mauvais endroit compte quand même.\n- `sum` additionne l'erreur, ce qui favorise les gros splats et les zones chargées.\n- `avg` prend la moyenne sur la surface couverte, ce qui juge petits et gros splats de la même façon."),
    ES("- `max` toma su peor píxel, así que un splat que cubre un solo punto malo ya cuenta.\n- `sum` suma el error, lo que favorece a los splats grandes y las zonas recargadas.\n- `avg` toma la media sobre el área que cubre, que juzga igual a grandes y pequeños."),
    PT("- `max` toma o seu pior pixel, por isso um splat que cobre um único ponto mau já conta.\n- `sum` soma o erro, o que favorece splats grandes e zonas carregadas.\n- `avg` toma a média sobre a área que cobre, julgando grandes e pequenos por igual."),
    IT("- `max` prende il suo pixel peggiore, così uno splat che copre anche un solo punto brutto conta lo stesso.\n- `sum` somma l'errore, il che favorisce splat grandi e zone affollate.\n- `avg` prende la media sull'area coperta, giudicando grandi e piccoli allo stesso modo."),
    NL("- `max` neemt het slechtste pixel, dus een splat die één slechte plek bedekt telt al mee.\n- `sum` telt de fout op, wat grote splats en drukke vlakken bevoordeelt.\n- `avg` neemt het gemiddelde over het bedekte gebied en meet grote en kleine splats gelijk."),
    RU("- `max` берёт худший пиксель, поэтому сплат засчитывается, даже если задевает лишь одно плохое место.\n- `sum` складывает ошибку, что выгодно крупным сплатам и насыщенным участкам.\n- `avg` берёт среднее по покрытой площади и судит крупные и мелкие сплаты одинаково."),
    TR("- `max` en kötü pikseli alır; böylece tek bir kötü noktayı örten splat da sayılır.\n- `sum` hatayı toplar, bu da büyük splat'ları ve kalabalık alanları öne çıkarır.\n- `avg` örttüğü alanın ortalamasını alır ve büyük ile küçük splat'ları aynı ölçüyle değerlendirir."));

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
    EN("Per-step splat-score exponent before averaging between refinements. >1 favors one bad photo; <1 requires errors across more photos; 1 is unchanged."),
    JA("精細化間で平均する前の各ステップのスプラットスコアの指数。1超は1枚の悪い写真を重視し、1未満は複数写真の誤差を重視。1は変更なしです。"),
    ZH_HANS("在细化间平均之前，对每步泼溅评分使用的指数。大于 1 强调单张差照片；小于 1 更要求多张照片都有误差；1 不变。"),
    ZH_HANT("在細化間平均之前，對每步潑濺評分使用的指數。大於 1 強調單張差照片；小於 1 更要求多張照片都有誤差；1 不變。"),
    KO("정제 사이 평균 전 단계별 스플랫 점수 지수입니다. 1 초과는 한 장의 나쁜 사진을 중시하고, 1 미만은 여러 사진의 오차를 요구합니다. 1은 불변입니다."),
    DE("Exponent des Schritt-Scores vor Mittelung zwischen Verfeinerungen. >1 betont ein schlechtes Foto; <1 verlangt Fehler in mehr Fotos; 1 unverändert."),
    FR("Exposant du score par pas avant moyenne entre raffinements. >1 favorise une mauvaise photo ; <1 exige des erreurs sur plus de photos ; 1 inchangé."),
    ES("Exponente de puntuación por paso antes de promediar entre refinamientos. >1 favorece una foto mala; <1 requiere error en más fotos; 1 no cambia."),
    PT("Expoente da pontuação por passo antes da média entre refinamentos. >1 favorece uma foto ruim; <1 exige erro em mais fotos; 1 não altera."),
    IT("Esponente per passo prima della media tra raffinamenti. >1 privilegia una foto errata; <1 richiede errori in più foto; 1 non cambia."),
    NL("Exponent op stapscores vóór middelen tussen verfijningen. >1 benadrukt één slechte foto; <1 vraagt fouten in meer foto’s; 1 blijft gelijk."),
    RU("Степень пошаговой оценки до усреднения между уплотнениями. >1 выделяет одно плохое фото; <1 требует ошибок на многих; 1 не меняет."),
    TR("İyileştirmeler arasında ortalama öncesi adım puanının üssü. >1 tek kötü fotoğrafı öne çıkarır; <1 daha çok fotoğrafta hata gerektirir; 1 değişmez."));

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
    EN("Caps final per-splat scores at this quantile after combining views and steps, limiting outliers. Shown in refinement-score view. 1 disables clipping."),
    JA("ビューとステップを統合後、スプラットの最終スコアをこの分位点で制限して外れ値を抑えます。精細化スコア表示にも反映。1は制限なしです。"),
    ZH_HANS("汇总视图和步数后，按此分位数限制泼溅的最终评分以抑制异常值。细化评分视图显示此值。1 关闭截断。"),
    ZH_HANT("彙總視圖和步數後，按此分位數限制潑濺的最終評分以抑制異常值。細化評分視圖顯示此值。1 關閉截斷。"),
    KO("뷰와 스텝을 합산한 최종 스플랫 점수를 이 분위수로 제한합니다. 이상치를 줄이며 정제 점수 뷰에 표시됩니다. 1은 제한을 끕니다."),
    DE("Begrenzt finale Splat-Scores nach Ansichten und Schritten auf dieses Quantil. Gegen Ausreißer; im Verfeinerungsscore sichtbar. 1 schaltet ab."),
    FR("Plafonne les scores finaux des splats à ce quantile après cumul des vues et pas. Limite les extrêmes ; affiché au score de raffinement. 1 désactive."),
    ES("Limita las puntuaciones finales de splats a este cuantil tras combinar vistas y pasos. Contiene atípicos; visible en refinamiento. 1 desactiva."),
    PT("Limita pontuações finais de splats a este quantil após combinar vistas e passos. Contém extremos; visível no refinamento. 1 desativa."),
    IT("Limita i punteggi finali a questo quantile dopo viste e passi. Contiene gli estremi; visibile nel punteggio di raffinamento. 1 disattiva."),
    NL("Begrenst eindscores per splat op dit kwantiel na combineren van beelden en stappen. Tegen uitschieters; zichtbaar in verfijningsscore. 1 schakelt uit."),
    RU("Ограничивает итоговые баллы сплатов квантилем после объединения видов и шагов. Против выбросов; виден в оценке уплотнения. 1 отключает."),
    TR("Görünüm ve adımlar birleştikten sonra son splat puanlarını bu nicelikte sınırlar. Aykırıları azaltır; iyileştirme puanında görünür. 1 kapatır."));

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
    EN("Exponent on final scores before selecting splats to split. >1 focuses on highest scores; <1 distributes detail more evenly; 1 uses original scores."),
    JA("分割するスプラットを選ぶ直前の最終スコアの指数。1超は高得点に集中し、1未満は細部を均等に配分。1は元のスコアを使います。"),
    ZH_HANS("选择要拆分的泼溅之前，对最终评分使用的指数。大于 1 集中于高分；小于 1 更均匀分配细节；1 使用原始评分。"),
    ZH_HANT("選擇要拆分的潑濺之前，對最終評分使用的指數。大於 1 集中於高分；小於 1 更均勻分配細節；1 使用原始評分。"),
    KO("분할할 스플랫 선택 직전의 최종 점수 지수입니다. 1 초과는 최고 점수에 집중, 1 미만은 세부를 균등 분배하며, 1은 원래 점수입니다."),
    DE("Exponent finaler Scores vor Auswahl zum Teilen. >1 konzentriert auf höchste Scores; <1 verteilt Details gleichmäßiger; 1 nutzt die Originals."),
    FR("Exposant des scores finaux avant choix des splats à diviser. >1 favorise les meilleurs scores ; <1 répartit le détail ; 1 garde les scores initiaux."),
    ES("Exponente de puntuación final antes de elegir qué dividir. >1 favorece las más altas; <1 reparte detalle más uniformemente; 1 usa originales."),
    PT("Expoente da pontuação final antes de escolher o que dividir. >1 foca nas mais altas; <1 distribui detalhe mais igualmente; 1 usa originais."),
    IT("Esponente dei punteggi finali prima della scelta degli splat da dividere. >1 favorisce i più alti, <1 distribuisce il dettaglio, 1 usa gli originali."),
    NL("Exponent op eindscores vóór kiezen van te splitsen splats. >1 richt op hoogste scores; <1 verdeelt detail gelijkmatiger; 1 gebruikt originele scores."),
    RU("Степень итоговых баллов перед выбором сплатов для деления. >1 выделяет высшие баллы; <1 распределяет детали ровнее; 1 берёт исходные."),
    TR("Bölünecek splat seçimi öncesi son puanların üssü. >1 en yüksek puanlara odaklanır; <1 ayrıntıyı eşit yayar; 1 özgün puanları kullanır."));
SS_MSG(densify_learned_score,
    EN("Learned splat scores"), JA("学習したスプラット点数"),
    ZH_HANS("学习得到的泼溅分数"), ZH_HANT("學習得到的潑濺分數"),
    KO("학습된 스플랫 점수"),
    DE("Gelernte Splat-Bewertung"),
    FR("Notes de splat apprises"),
    ES("Puntuación de splats aprendida"),
    PT("Pontuação dos splats aprendida"),
    IT("Punteggio degli splat appreso"),
    NL("Geleerde splatscore"),
    RU("Обучаемые оценки сплатов"),
    TR("Öğrenilen splat puanları"));
SS_MSG(densify_learned_score_help,
    EN("Fit one score per splat so the blended scores reproduce the detail error map.\n- Assigns error to splats that explain the pixel, on the same scale as averaged scores.\n- Requires revised densification and a detail error measure."),
    JA("各スプラットが覆う誤差を平均する代わりに、点数を色と同じように重ねたものが誤差マップを再現するように、スプラットごとの点数を学習します。\n- 画素の誤差はそれを説明できるスプラットに割り当てられるので、再現の悪い境目に接しているだけの大きなスプラットは、その境目の誤差を負わなくなります。\n- 点数の大きさは平均したものと同じです。改良版の増やし方と誤差の測り方が必要です。"),
    ZH_HANS("不再对每个泼溅覆盖范围内的误差取平均，而是为每个泼溅学习一个分数，使这些分数像颜色一样混合后能重现误差图。\n- 像素的误差因此会分给能够解释它的泼溅：只是挨着一处重建很差的边缘的大泼溅，不再为那条边缘的误差负责。\n- 分数的大小与取平均时相同。需要开启改进的加密规则和误差度量方式。"),
    ZH_HANT("不再對每個潑濺覆蓋範圍內的誤差取平均，而是為每個潑濺學習一個分數，使這些分數像顏色一樣混合後能重現誤差圖。\n- 像素的誤差因此會分給能夠解釋它的潑濺：只是挨著一處重建很差的邊緣的大潑濺，不再為那條邊緣的誤差負責。\n- 分數的大小與取平均時相同。需要開啟改良的加密規則和誤差度量方式。"),
    KO("각 스플랫이 덮은 오차를 평균하는 대신, 점수를 색처럼 섞었을 때 오차 맵이 재현되도록 스플랫마다 점수를 학습합니다.\n- 화소의 오차는 그것을 설명할 수 있는 스플랫에 돌아가므로, 잘못 재구성된 경계에 맞닿아 있을 뿐인 큰 스플랫은 그 경계의 오차를 떠안지 않게 됩니다.\n- 점수의 크기는 평균할 때와 같습니다. 개선된 밀집화 규칙과 오차 측정 방식이 필요합니다."),
    DE("Statt den Fehler unter jedem Splat zu mitteln, je Splat eine Punktzahl lernen, sodass die wie Farben überblendeten Punktzahlen die Fehlerkarte nachbilden. Der Fehler eines Pixels geht dann an die Splats, die ihn erklären können:\n- Ein großer Splat, der nur an eine schlecht rekonstruierte Kante grenzt, trägt deren Fehler nicht mehr.\n- Die Punktzahlen behalten den Maßstab der gemittelten.\n- Setzt die überarbeitete Verdichtungsregel und ein Fehlermaß voraus."),
    FR("Au lieu de moyenner l'erreur sous chaque splat, apprendre une note par splat de sorte que les notes, mélangées comme les couleurs, reproduisent la carte d'erreur.\n- L'erreur d'un pixel revient alors aux splats capables de l'expliquer : un gros splat qui ne fait que border une arête mal reconstruite n'en porte plus l'erreur.\n- Les notes gardent l'échelle des notes moyennées. Requiert la règle de densification révisée et une mesure d'erreur."),
    ES("En lugar de promediar el error bajo cada splat, aprender una puntuación por splat de modo que las puntuaciones, mezcladas como los colores, reproduzcan el mapa de error.\n- El error de un píxel va entonces a los splats que pueden explicarlo: un splat grande que solo bordea una arista mal reconstruida deja de cargar con su error.\n- Las puntuaciones conservan la escala de las promediadas.\n- Requiere la regla de densificación revisada y una medida de error."),
    PT("Em vez de calcular a média do erro sob cada splat, aprender uma pontuação por splat de modo que as pontuações, misturadas como as cores, reproduzam o mapa de erro.\n- O erro de um pixel vai então para os splats que o podem explicar: um splat grande que apenas faz fronteira com uma aresta mal reconstruída deixa de carregar o seu erro.\n- As pontuações mantêm a escala das calculadas pela média. Exige a regra de densificação revisada e uma medida de erro."),
    IT("Invece di fare la media dell'errore sotto ogni splat, apprendere un punteggio per splat in modo che i punteggi, miscelati come i colori, riproducano la mappa d'errore.\n- L'errore di un pixel va allora agli splat che possono spiegarlo: uno splat grande che confina soltanto con un bordo ricostruito male smette di prenderne l'errore.\n- I punteggi mantengono la scala di quelli mediati.\n- Richiede la regola di densificazione rivista e una misura dell'errore."),
    NL("In plaats van de fout onder elke splat te middelen, per splat een score leren zodat de scores, gemengd zoals kleuren, de foutkaart nabootsen. De fout van een pixel gaat dan naar de splats die hem kunnen verklaren:\n- een grote splat die alleen grenst aan een slecht gereconstrueerde rand krijgt die fout niet meer.\n- De scores houden de schaal van de gemiddelde. Vereist de herziene verdichtingsregel en een foutmaat."),
    RU("Вместо усреднения ошибки под каждым сплатом обучать для каждого сплата оценку так, чтобы оценки, смешанные как цвета, воспроизводили карту ошибки.\n- Ошибка пикселя тогда достаётся сплатам, которые могут её объяснить: крупный сплат, лишь граничащий с плохо восстановленным краем, перестаёт брать на себя его ошибку.\n- Оценки сохраняют масштаб усреднённых. Требует уточнённого правила уплотнения и меры ошибки."),
    TR("Her splat'ın altındaki hatanın ortalamasını almak yerine, puanlar renkler gibi karıştırıldığında hata haritasını yeniden üretecek şekilde splat başına bir puan öğrenir.\n- Bir pikselin hatası böylece onu açıklayabilen splat'lara gider: kötü yeniden kurulmuş bir kenara yalnızca komşu olan büyük bir splat artık o kenarın hatasını üstlenmez.\n- Puanlar, ortalaması alınanlarla aynı ölçeği korur.\n- Gözden geçirilmiş yoğunlaştırma kuralını ve bir hata ölçüsünü gerektirir."));
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
    EN("Share of new splats spent splitting oversized splats so they can shrink without holes. 0 allocates all new splats by error score."),
    JA("穴を作らず縮小できるよう、大きすぎるスプラットの分割に割り当てる追加分の割合。0はすべて誤差スコアで割り当てます。"),
    ZH_HANS("新增泼溅中用于拆分超大泼溅的比例，让其缩小而不留孔洞。0 表示全部按误差评分分配。"),
    ZH_HANT("新增潑濺中用於拆分超大潑濺的比例，讓其縮小而不留孔洞。0 表示全部按誤差評分分配。"),
    KO("구멍 없이 줄이도록 과대 스플랫 분할에 쓸 새 스플랫 비율입니다. 0은 모든 새 스플랫을 오차 점수로 배분합니다."),
    DE("Anteil neuer Splats zum Teilen übergroßer Splats, damit sie ohne Löcher schrumpfen. 0 verteilt alle neuen Splats nach Fehlerscore."),
    FR("Part des nouveaux splats dédiée à diviser les trop grands, pour réduire sans trous. 0 les attribue tous selon le score d’erreur."),
    ES("Parte de nuevos splats dedicada a dividir los grandes para reducirlos sin huecos. 0 asigna todos por puntuación de error."),
    PT("Parte dos novos splats dedicada a dividir os grandes, reduzindo sem buracos. 0 distribui todos pela pontuação de erro."),
    IT("Quota di nuovi splat usata per dividere quelli troppo grandi senza creare buchi. 0 assegna tutti i nuovi splat secondo l’errore."),
    NL("Aandeel nieuwe splats voor het splitsen van te grote splats, zodat ze krimpen zonder gaten. 0 verdeelt alles op foutscore."),
    RU("Доля новых сплатов для деления слишком крупных, чтобы уменьшать их без дыр. 0 распределяет все новые сплаты по ошибке."),
    TR("Deliksiz küçülmeleri için aşırı büyük splatları bölmeye ayrılan yeni splat oranı. 0 tüm yenileri hata puanına göre dağıtır."));

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
    EN("Error-score weight when choosing oversized splats to split. 0 uses excess size only; 1 uses normal error scoring. Between favors both size and error."),
    JA("過大スプラットの分割選択に使う誤差の重み。0は超過サイズのみ、1は通常の誤差スコア。中間は大きさと誤差の両方を優先します。"),
    ZH_HANS("选择超大泼溅进行拆分时的误差权重。0 仅看超出尺寸；1 使用通常误差评分。中间值同时优先尺寸与误差。"),
    ZH_HANT("選擇超大潑濺進行拆分時的誤差權重。0 僅看超出尺寸；1 使用通常誤差評分。中間值同時優先尺寸與誤差。"),
    KO("과대 스플랫 분할 선택 시 오차 가중치입니다. 0은 초과 크기만, 1은 일반 오차 점수로 고릅니다. 사이는 크기와 오차를 함께 선호합니다."),
    DE("Fehlergewicht bei Auswahl übergroßer Splats zum Teilen. 0 nutzt nur Übergröße, 1 normale Fehlerscores. Dazwischen zählen Größe und Fehler."),
    FR("Poids de l’erreur pour choisir les splats trop grands à diviser. 0 : excès de taille seul ; 1 : score normal. Entre les deux : taille et erreur."),
    ES("Peso del error al dividir splats grandes. 0 usa solo el exceso de tamaño; 1 usa la puntuación normal. Entre ambos favorece tamaño y error."),
    PT("Peso do erro ao dividir splats grandes. 0 usa só excesso de tamanho; 1 usa a pontuação normal. Entre ambos favorece tamanho e erro."),
    IT("Peso dell’errore per dividere splat troppo grandi. 0 usa solo l’eccesso di dimensione, 1 il punteggio normale. Tra i due conta entrambi."),
    NL("Foutgewicht bij kiezen van te grote splats om te splitsen. 0 gebruikt alleen overmaat, 1 de normale foutscore. Ertussen tellen grootte en fout."),
    RU("Вес ошибки при выборе крупных сплатов для деления. 0 — лишь превышение размера, 1 — обычная оценка. Между ними учитывает размер и ошибку."),
    TR("Bölünecek büyük splat seçiminde hata ağırlığı. 0 yalnız boyut aşımını, 1 normal hata puanını kullanır. Arası boyutu ve hatayı birlikte gözetir."));

SS_MSG(use_long_axis_split,
    EN("Split along the long axis"), JA("長い軸で分割する"),
    ZH_HANS("沿长轴分裂"), ZH_HANT("沿長軸分裂"), KO("긴 축을 따라 분할"),
    DE("Entlang der langen Achse teilen"), FR("Diviser selon le grand axe"),
    ES("Dividir por el eje largo"), PT("Dividir pelo eixo longo"),
    IT("Dividere lungo l'asse maggiore"), NL("Splitsen langs de lange as"),
    RU("Делить вдоль длинной оси"), TR("Uzun eksen boyunca böl"));
SS_MSG(use_long_axis_split_help,
    EN("Split stretched splats along their long axis when adding detail, rather than splitting them evenly.\nGives less blurry distant background in large outdoor scenes."),
    JA("細部を足すとき、細長いスプラットを均等にではなく長い軸に沿って分割します。\n広い屋外のシーンで、遠景のぼやけが減ります。"),
    ZH_HANS("在补充细节时，沿长轴分裂被拉长的泼溅，而不是均匀分裂。\n可以让大型户外场景中的远景背景不那么糊。"),
    ZH_HANT("在補充細節時，沿長軸分裂被拉長的潑濺，而不是均勻分裂。\n可以讓大型戶外場景中的遠景背景不那麼糊。"),
    KO("디테일을 더할 때 길쭉한 스플랫을 균등하게가 아니라 긴 축을 따라 나눕니다.\n넓은 실외 장면에서 먼 배경이 덜 뭉개집니다."),
    DE("Gestreckte Splats beim Ergänzen von Detail entlang ihrer langen Achse teilen statt gleichmäßig.\nErgibt in großen Außenszenen einen weniger unscharfen fernen Hintergrund."),
    FR("Diviser les splats étirés selon leur grand axe quand on ajoute du détail, au lieu de les couper également.\nDonne un arrière-plan lointain moins flou dans les grandes scènes extérieures."),
    ES("Dividir los splats estirados por su eje largo al añadir detalle, en vez de partirlos por igual.\nDa un fondo lejano menos borroso en grandes escenas exteriores."),
    PT("Dividir os splats esticados pelo seu eixo longo ao acrescentar detalhe, em vez de parti-los igualmente.\nDá um fundo distante menos borrado em grandes cenas ao ar livre."),
    IT("Divide gli splat allungati lungo l’asse maggiore invece che in modo uniforme.\nRiduce la sfocatura dello sfondo lontano nelle grandi scene all’aperto."),
    NL("Uitgerekte splats bij het toevoegen van detail langs hun lange as splitsen in plaats van gelijk.\nGeeft een minder wazige verre achtergrond in grote buitenscènes."),
    RU("При добавлении деталей делить вытянутые сплаты вдоль длинной оси, а не пополам поровну.\nВ больших уличных сценах дальний фон получается менее размытым."),
    TR("Ayrıntı eklerken uzamış splat'ları eşit bölmek yerine uzun eksenleri boyunca böler.\nBüyük dış mekân sahnelerinde uzak arka planı daha az bulanık yapar."));

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
    EN("Opacity retained by each split half: initial value, final value, transition steps. Higher keeps sharp dense halves; lower fades floaters for relocation."),
    JA("分割後の各半分が保つ不透明度。初期値、最終値、移行ステップを指定。高いほど密で鮮明に保ち、低いほど浮遊物が薄れ再配置されます。"),
    ZH_HANS("拆分后各半保留的不透明度：初始值、最终值、过渡步数。较高使其浓密清晰；较低使漂浮物淡化以便重定位。"),
    ZH_HANT("拆分後各半保留的不透明度：初始值、最終值、過渡步數。較高使其濃密清晰；較低使漂浮物淡化以便重新定位。"),
    KO("분할된 각 절반이 유지할 불투명도: 초기값, 최종값, 전환 스텝입니다. 높으면 조밀하고 선명하며, 낮으면 부유물이 흐려져 재배치됩니다."),
    DE("Opazität je geteilter Hälfte: Startwert, Endwert, Übergangsschritte. Höher hält Hälften dicht und scharf; niedriger lässt Floater zum Versetzen verblassen."),
    FR("Opacité gardée par moitié : valeur initiale, finale, pas de transition. Plus haut garde dense et net ; plus bas efface les flottants pour les replacer."),
    ES("Opacidad por mitad dividida: inicial, final y pasos de transición. Mayor deja mitades densas y nítidas; menor desvanece flotantes para reubicarlos."),
    PT("Opacidade por metade dividida: inicial, final e passos de transição. Maior mantém densas e nítidas; menor apaga flutuantes para reposicionar."),
    IT("Opacità per metà divisa: valore iniziale, finale e passi di transizione. Alta mantiene densità e nitidezza; bassa sfuma i flottanti per ricollocarli."),
    NL("Dekking per gesplitste helft: startwaarde, eindwaarde, overgangsstappen. Hoger houdt helften dicht en scherp; lager vervaagt floaters voor verplaatsing."),
    RU("Непрозрачность каждой половины: начальная, конечная, шаги перехода. Выше — плотнее и чётче; ниже — плавающие сплаты гаснут для переноса."),
    TR("Bölünen yarının koruduğu opaklık: ilk değer, son değer, geçiş adımları. Yüksek yoğun ve keskin tutar; düşük yüzenleri soldurup taşımaya hazırlar."));

SS_MSG(max_split_fraction,
    EN("Split budget per round"),
    JA("1 ラウンドあたりの分割上限"),
    ZH_HANS("每轮分裂上限"),
    ZH_HANT("每輪分裂上限"),
    KO("회차당 분할 한도"),
    DE("Teilungsbudget pro Runde"),
    FR("Budget de division par cycle"),
    ES("Presupuesto de división por ronda"),
    PT("Orçamento de divisão por rodada"),
    IT("Budget di divisione per ciclo"),
    NL("Splitsbudget per ronde"),
    RU("Лимит делений за раунд"),
    TR("Tur başına bölme bütçesi"));
SS_MSG(max_split_fraction_help,
    EN("Maximum fraction of live splats split per round; revived dead splats take priority over growth.\n- Excess dead splats wait, avoiding sudden opacity loss across many live splats.\n- 1 or more removes both limits."),
    JA("各回に分割できる生存スプラットの最大割合。増加より死滅分の復活を優先します。\n- 予算を超えた死滅分は待機し、多数の生存分が急に不透明度を失うのを防ぎます。\n- 1以上は両方の制限をなくします。"),
    ZH_HANS("每轮可拆分的存活泼溅最大比例；优先复活失效泼溅，再用于增长。\n- 超出预算的失效泼溅等待后续轮次，避免大量存活泼溅突然损失不透明度。\n- 1 或更大取消两种限制。"),
    ZH_HANT("每輪可拆分的存活潑濺最大比例；優先復活失效潑濺，再用於增長。\n- 超出預算的失效潑濺等待後續輪次，避免大量存活潑濺突然損失不透明度。\n- 1 或更大取消兩種限制。"),
    KO("회당 분할할 활성 스플랫의 최대 비율입니다. 증식보다 죽은 스플랫 복원을 우선합니다.\n- 초과한 죽은 스플랫은 기다려, 많은 활성 스플랫의 급격한 불투명도 손실을 막습니다.\n- 1 이상이면 두 제한을 없앱니다."),
    DE("Maximal geteilter Anteil lebender Splats pro Runde; tote wiederbeleben geht vor Wachstum.\n- Überschüssige Tote warten, statt viele lebende Splats auf einmal blasser zu machen.\n- Ab 1 entfallen beide Grenzen."),
    FR("Fraction maximale de splats vivants divisés par cycle ; priorité aux morts réactivés avant croissance.\n- Les morts excédentaires attendent, évitant une perte soudaine d’opacité de nombreux vivants.\n- 1 ou plus supprime les deux limites."),
    ES("Fracción máxima de splats vivos divididos por ronda; prioridad a revivir muertos antes de crecer.\n- Los muertos sobrantes esperan, evitando perder opacidad en muchos vivos de golpe.\n- 1 o más elimina ambos límites."),
    PT("Fração máxima de splats vivos divididos por rodada; reviver mortos vem antes de crescer.\n- Mortos excedentes esperam, evitando perda súbita de opacidade em muitos vivos.\n- 1 ou mais remove ambos os limites."),
    IT("Frazione massima di splat vivi divisi per ciclo; priorità ai morti rianimati rispetto alla crescita.\n- I morti in eccesso attendono, evitando perdite improvvise di opacità in molti vivi.\n- 1 o più rimuove entrambi i limiti."),
    NL("Maximaal deel levende splats dat per ronde splitst; dode herstellen gaat vóór groei.\n- Overtollige dode splats wachten, tegen plots dekkingsverlies van veel levende splats.\n- Vanaf 1 vervallen beide limieten."),
    RU("Максимальная доля делимых живых сплатов за раунд; сначала восстановление мёртвых, потом рост.\n- Лишние мёртвые ждут, предотвращая резкую потерю непрозрачности многих живых.\n- 1 и выше снимает оба лимита."),
    TR("Tur başına bölünen canlı splatın en yüksek oranı; büyümeden önce ölüler canlandırılır.\n- Fazla ölüler bekler, çok sayıda canlının ani opaklık kaybını önler.\n- 1 veya üstü iki sınırı da kaldırır."));

SS_MSG(split_weight_by_renders,
    EN("Weight splits by renders"),
    JA("描画回数で分割を重み付け"),
    ZH_HANS("按渲染次数加权分裂"),
    ZH_HANT("按算圖次數加權分裂"),
    KO("렌더 횟수로 분할 가중"),
    DE("Teilungen nach Renderzahl gewichten"),
    FR("Pondérer les divisions par les rendus"),
    ES("Ponderar divisiones por renderizados"),
    PT("Ponderar divisões pelas renderizações"),
    IT("Pesare le divisioni per i rendering"),
    NL("Splitsingen wegen naar renders"),
    RU("Взвешивать деления по числу отрисовок"),
    TR("Bölmeleri işlenme sayısına göre ağırlıklandır"));
SS_MSG(split_weight_by_renders_help,
    EN("Multiplies refinement score by renders since creation or last split, giving splats time to settle before splitting again. The score view includes this weight."),
    JA("生成または前回の分割以降の描画回数を精細化スコアに掛け、再分割前に落ち着く時間を与えます。スコア表示にもこの重みを含みます。"),
    ZH_HANS("将细化评分乘以自创建或上次拆分后的渲染次数，让泼溅在再次拆分前稳定。评分视图包含此权重。"),
    ZH_HANT("將細化評分乘以自建立或上次拆分後的算繪次數，讓潑濺在再次拆分前穩定。評分視圖包含此權重。"),
    KO("생성 또는 마지막 분할 이후 렌더 횟수를 정제 점수에 곱해 재분할 전 안정 시간을 줍니다. 점수 뷰에 이 가중치가 반영됩니다."),
    DE("Multipliziert den Score mit Rendern seit Erstellung/letzter Teilung, damit Splats sich vor neuer Teilung setzen. Score-Ansicht enthält dieses Gewicht."),
    FR("Multiplie le score par les rendus depuis création ou division, laissant les splats se stabiliser avant de rediviser. La vue de score inclut ce poids."),
    ES("Multiplica la puntuación por renders desde creación o última división para estabilizar antes de dividir de nuevo. La vista incluye este peso."),
    PT("Multiplica a pontuação por renders desde criação ou última divisão para estabilizar antes de dividir de novo. A vista inclui este peso."),
    IT("Moltiplica il punteggio per i render dalla creazione o ultima divisione, lasciando stabilizzare prima di ridividere. La vista include questo peso."),
    NL("Vermenigvuldigt de score met renders sinds ontstaan/laatste splitsing, zodat splats eerst tot rust komen. De scoreweergave bevat dit gewicht."),
    RU("Умножает оценку на число рендеров с создания или деления, давая сплатам стабилизироваться перед новым делением. Вид оценки включает этот вес."),
    TR("Yeniden bölünmeden oturması için puanı oluşturma/son bölünmeden beri render sayısıyla çarpar. Puan görünümü bu ağırlığı içerir."));

SS_MSG(dead_after_epochs,
    EN("Dead after unrendered passes"),
    JA("未描画で死とみなす巡数"),
    ZH_HANS("未渲染多少遍后判为死亡"),
    ZH_HANT("未算圖多少遍後判為死亡"),
    KO("렌더되지 않은 바퀴 수 후 죽음 처리"),
    DE("Tot nach Durchläufen ohne Rendern"),
    FR("Mort après passages sans rendu"),
    ES("Muerto tras pasadas sin renderizar"),
    PT("Morto após passagens sem renderização"),
    IT("Morto dopo passaggi senza rendering"),
    NL("Dood na doorlopen zonder render"),
    RU("Мёртв после проходов без отрисовки"),
    TR("İşlenmeden geçen geçişlerden sonra ölü"));
SS_MSG(dead_after_epochs_help,
    EN("Relocates splats unseen for this many dataset passes at the next refinement, regardless of opacity. Fractional values allowed; 0 disables."),
    JA("指定回数のデータセット巡回中に描画されなかったスプラットを、不透明度によらず次の精細化で再配置します。小数可。0で無効です。"),
    ZH_HANS("在下次细化时重定位经过此轮数仍未被渲染的泼溅，不论不透明度。允许小数；0 关闭。"),
    ZH_HANT("在下次細化時重新定位經過此輪數仍未被算繪的潑濺，不論不透明度。允許小數；0 關閉。"),
    KO("이 데이터셋 순회 횟수 동안 렌더되지 않은 스플랫을 다음 정제 때 불투명도와 무관하게 재배치합니다. 소수 허용, 0은 끄기입니다."),
    DE("Versetzt beim nächsten Verfeinern Splats, die so viele Durchläufe ungesehen blieben, unabhängig von Opazität. Bruchteile erlaubt; 0 schaltet ab."),
    FR("Replace au prochain raffinement les splats non rendus depuis ce nombre de passes, quelle que soit l’opacité. Fractions admises ; 0 désactive."),
    ES("Reubica en el próximo refinamiento los splats no vistos durante estas pasadas, sin importar opacidad. Admite fracciones; 0 desactiva."),
    PT("Reposiciona no próximo refinamento splats não vistos por estas passagens, sem importar opacidade. Aceita frações; 0 desativa."),
    IT("Ricolloca al prossimo raffinamento gli splat non visti per questi passaggi, a prescindere dall’opacità. Frazioni ammesse; 0 disattiva."),
    NL("Verplaatst bij de volgende verfijning splats die zoveel rondes niet gerenderd zijn, ongeacht dekking. Breuken toegestaan; 0 schakelt uit."),
    RU("Перемещает при следующем уплотнении сплаты, не видимые столько проходов, независимо от непрозрачности. Дроби допустимы; 0 отключает."),
    TR("Bu kadar veri turunda render edilmeyen splatları sonraki iyileştirmede opaklıktan bağımsız taşır. Kesirli değer olabilir; 0 kapatır."));

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
    EN("How firmly the screen-size limit is enforced, from 1 upward.\nHigher clamps oversized splats decisively; lower eases them down."),
    JA("画面サイズの制限をどれだけ強く効かせるかです。\n- 1 以上で指定します。\n- 高いほど大きすぎるスプラットをきっぱり抑え、低いほど緩やかに縮めます。"),
    ZH_HANS("屏幕尺寸限制执行得有多严格，取值从 1 起。\n数值越高越果断地夹住过大的泼溅；越低则缓慢地把它们收下去。"),
    ZH_HANT("螢幕尺寸限制執行得有多嚴格，取值從 1 起。\n數值越高越果斷地夾住過大的潑濺；越低則緩慢地把它們收下去。"),
    KO("화면 크기 제한을 얼마나 강하게 적용할지이며 1 이상으로 지정합니다.\n값이 크면 지나치게 큰 스플랫을 단호하게 잘라내고, 작으면 서서히 줄입니다."),
    DE("Wie streng die Bildschirmgrößen-Grenze durchgesetzt wird, ab 1 aufwärts.\nHöher begrenzt übergroße Splats entschieden; niedriger führt sie sanft herunter."),
    FR("Avec quelle fermeté la limite de taille à l'écran est appliquée, à partir de 1.\nPlus haut ramène brutalement les splats trop grands ; plus bas les fait redescendre en douceur."),
    ES("Con qué firmeza se aplica el límite de tamaño en pantalla, a partir de 1.\nMás alto recorta los splats demasiado grandes de forma tajante; más bajo los va bajando con suavidad."),
    PT("Com que firmeza o limite de tamanho na tela é aplicado, a partir de 1.\nMais alto corta os splats grandes demais de forma decidida; mais baixo os reduz suavemente."),
    IT("Con quanta fermezza si applica il limite di dimensione a schermo, da 1 in su.\nPiù alto taglia gli splat troppo grandi in modo netto; più basso li riporta giù con dolcezza."),
    NL("Hoe streng de schermgroottelimiet wordt afgedwongen, vanaf 1.\nHoger kapt te grote splats resoluut af; lager brengt ze geleidelijk omlaag."),
    RU("Насколько жёстко действует предел размера на экране; значения от 1 и выше.\nБольше — переросшие сплаты обрезаются решительно; меньше — плавно уменьшаются."),
    TR("Ekran boyutu sınırının ne kadar katı uygulandığı; 1'den başlar.\nYüksek değerler aşırı büyük splat'ları kararlıca kırpar; düşük değerler onları yavaşça küçültür."));
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
    EN("Penalty strength per doubling beyond the on-screen size limit.\n- Balanced with image error so sky or broad flat surfaces can stay large.\n- 0 uses a hard clamp every step instead."),
    JA("画面上の寸法制限を2倍超えるごとの罰則の強さ。\n- 画像誤差と釣り合い、空や広い平面は大きく保てます。\n- 0は代わりに毎ステップ強制的に制限します。"),
    ZH_HANS("屏幕尺寸每超过上限一倍时的惩罚强度。\n- 与图像误差平衡，让天空或大平面可保持较大尺寸。\n- 0 改为每步硬性截断尺寸。"),
    ZH_HANT("螢幕尺寸每超過上限一倍時的懲罰強度。\n- 與影像誤差平衡，讓天空或大平面可保持較大尺寸。\n- 0 改為每步硬性截斷尺寸。"),
    KO("화면 크기 한계를 두 배 초과할 때마다 적용하는 규제 강도입니다.\n- 이미지 오차와 균형을 맞춰 하늘이나 넓은 평면은 크게 남을 수 있습니다.\n- 0은 대신 매 스텝 크기를 강제로 제한합니다."),
    DE("Strafstärke pro Verdopplung über die Bildschirmgrößengrenze hinaus.\n- Gegen Bildfehler abgewogen, damit Himmel oder große Flächen groß bleiben können.\n- 0 begrenzt stattdessen in jedem Schritt hart."),
    FR("Force de pénalité par doublement au-delà de la limite à l’écran.\n- Équilibrée avec l’erreur d’image pour laisser ciel ou grandes surfaces rester grands.\n- 0 impose une limite stricte à chaque pas."),
    ES("Fuerza por cada duplicación más allá del límite en pantalla.\n- Se equilibra con error de imagen para que cielo o grandes superficies sigan grandes.\n- 0 aplica un tope estricto cada paso."),
    PT("Força por duplicação além do limite na tela.\n- Equilibra com erro de imagem para céu ou superfícies amplas permanecerem grandes.\n- 0 aplica limite rígido a cada passo."),
    IT("Forza per ogni raddoppio oltre il limite sullo schermo.\n- Bilanciata con errore d’immagine per lasciare grandi cielo o superfici ampie.\n- 0 impone invece un limite rigido a ogni passo."),
    NL("Strafsterkte per verdubbeling boven de schermgroottelimiet.\n- Afgewogen tegen beeldfout zodat lucht of brede vlakken groot kunnen blijven.\n- 0 begrenst in plaats daarvan elke stap hard."),
    RU("Сила штрафа за каждое удвоение сверх экранного лимита размера.\n- Уравновешен с ошибкой изображения, позволяя небу и плоскостям оставаться крупными.\n- 0 вместо этого жёстко ограничивает каждый шаг."),
    TR("Ekran boyutu sınırı üzerindeki her iki kat artışın ceza gücü.\n- Gökyüzü veya geniş yüzeyler büyük kalabilsin diye görüntü hatasıyla dengelenir.\n- 0 bunun yerine her adımda sert sınır uygular."));

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
    EN("Structure-versus-pixel-color loss weight.\n- Higher emphasizes fine texture; lower gives smoother, less noisy backgrounds.\n- With perceptual-loss s_cielab or flip, weights that metric instead of SSIM."),
    JA("ピクセル色に対する構造損失の重み。\n- 高いほど細かな模様、低いほど滑らかでノイズの少ない背景を重視します。\n- perceptual-loss が s_cielab/flip ならSSIMでなくその指標の重みです。"),
    ZH_HANS("结构相对于像素颜色的损失权重。\n- 较高强调细小纹理；较低得到更平滑、噪声更少的背景。\n- perceptual-loss 为 s_cielab 或 flip 时，改为该指标的权重。"),
    ZH_HANT("結構相對於像素顏色的損失權重。\n- 較高強調細小紋理；較低得到更平滑、雜訊更少的背景。\n- perceptual-loss 為 s_cielab 或 flip 時，改為該指標的權重。"),
    KO("픽셀 색 대비 구조 손실의 가중치입니다.\n- 높으면 미세 질감, 낮으면 더 매끄럽고 노이즈 적은 배경을 중시합니다.\n- perceptual-loss가 s_cielab 또는 flip이면 SSIM 대신 그 지표의 가중치입니다."),
    DE("Gewicht für Struktur gegenüber Pixelfarbe im Verlust.\n- Höher betont feine Textur; niedriger macht Hintergründe glatter und rauschärmer.\n- Bei perceptual-loss s_cielab oder flip gewichtet es diese Metrik statt SSIM."),
    FR("Poids de structure face à la couleur pixel dans la perte.\n- Plus haut : texture fine ; plus bas : fonds plus lisses et moins bruités.\n- Avec perceptual-loss s_cielab ou flip, pondère cette métrique au lieu de SSIM."),
    ES("Peso de estructura frente a color de píxel en la pérdida.\n- Mayor resalta textura fina; menor suaviza fondos y reduce ruido.\n- Con perceptual-loss s_cielab o flip, pondera esa métrica en lugar de SSIM."),
    PT("Peso de estrutura frente à cor do pixel na perda.\n- Maior realça textura fina; menor suaviza fundos e reduz ruído.\n- Com perceptual-loss s_cielab ou flip, pondera essa métrica em vez de SSIM."),
    IT("Peso della struttura rispetto al colore pixel nella perdita.\n- Alto evidenzia texture fini; basso dà sfondi più uniformi e meno rumorosi.\n- Con perceptual-loss s_cielab o flip pesa quella metrica anziché SSIM."),
    NL("Gewicht van structuur tegenover pixelkleur in het verlies.\n- Hoger benadrukt fijne textuur; lager geeft gladdere achtergronden met minder ruis.\n- Bij perceptual-loss s_cielab of flip weegt het die metriek in plaats van SSIM."),
    RU("Вес структуры относительно цвета пикселя в функции потерь.\n- Выше — тонкие текстуры; ниже — гладкий фон с меньшим шумом.\n- При perceptual-loss s_cielab или flip взвешивает эту метрику вместо SSIM."),
    TR("Kayıpta piksel rengine karşı yapının ağırlığı.\n- Yüksek ince dokuyu vurgular; düşük daha pürüzsüz ve az gürültülü fon verir.\n- perceptual-loss s_cielab veya flip ise SSIM yerine o ölçütü ağırlıklandırır."));

SS_MSG(perceptual_loss,
    EN("Perceptual loss"), JA("知覚損失"), ZH_HANS("感知损失"),
    ZH_HANT("感知損失"), KO("지각 손실"), DE("Wahrnehmungsverlust"),
    FR("Perte perceptuelle"), ES("Pérdida perceptual"),
    PT("Perda perceptual"), IT("Perdita percettiva"),
    NL("Perceptueel verlies"), RU("Перцептивная потеря"),
    TR("Algısal kayıp"));
SS_MSG(perceptual_loss_help,
    EN("How the structure loss compares renders and photos.\n- ssim — local structure.\n- s_cielab — perceptual color. Downweights fine color noise.\n- flip — perceptual color with extra weight on edge and point errors.\n- ssim_blur_tolerant — tolerates blur in photos. Blurry renders are still penalized."),
    JA("構造損失で描画結果と写真を比較する方法です。\n- ssim — 局所構造を比較。\n- s_cielab — 知覚色差。細かな色ノイズの影響を抑えます。\n- flip — 知覚色差。輪郭と点の誤差を重視します。\n- ssim_blur_tolerant — 写真のぼけを許容。描画のぼけはペナルティの対象です。"),
    ZH_HANS("结构损失比较渲染与照片的方式。\n- ssim — 比较局部结构。\n- s_cielab — 感知色差，减弱细小色彩噪声。\n- flip — 感知色差，加重边缘和点误差。\n- ssim_blur_tolerant — 容忍照片模糊，仍惩罚渲染模糊。"),
    ZH_HANT("結構損失比較算繪與照片的方式。\n- ssim — 比較局部結構。\n- s_cielab — 感知色差，降低細小色彩雜訊的影響。\n- flip — 感知色差，加重邊緣和點誤差。\n- ssim_blur_tolerant — 容許照片模糊，仍懲罰算繪模糊。"),
    KO("구조 손실이 렌더링과 사진을 비교하는 방법입니다.\n- ssim — 국소 구조를 비교합니다.\n- s_cielab — 지각 색차. 미세한 색 잡음을 덜 반영합니다.\n- flip — 지각 색차에 가장자리와 점 오류를 더 반영합니다.\n- ssim_blur_tolerant — 사진 흐림은 허용하고 렌더링 흐림은 벌점을 줍니다."),
    DE("Vergleich von Render und Foto im Struktur-Loss.\n- ssim — lokale Struktur.\n- s_cielab — wahrgenommene Farbdifferenz mit weniger Gewicht auf feinem Farbrauschen.\n- flip — wahrgenommene Farbdifferenz mit mehr Gewicht auf Kanten- und Punktfehlern.\n- ssim_blur_tolerant — toleriert unscharfe Fotos. Unschärfe im Render zählt weiterhin."),
    FR("Perte structurelle entre rendu et photos.\n- ssim — structure locale.\n- s_cielab — écart de couleur perçu, moins sensible au bruit fin.\n- flip — écart perçu avec plus de poids sur les erreurs de bords et de points.\n- ssim_blur_tolerant — tolère le flou des photos. Le flou du rendu reste pénalisé."),
    ES("Comparación de render y fotos en la pérdida estructural.\n- ssim — estructura local.\n- s_cielab — diferencia de color percibida, con menos peso del ruido fino.\n- flip — color percibido con más peso de errores en bordes y puntos.\n- ssim_blur_tolerant — tolera fotos borrosas. Penaliza el desenfoque del render."),
    PT("Comparação entre render e fotos na perda estrutural.\n- ssim — estrutura local.\n- s_cielab — diferença de cor percebida com menos peso do ruído fino.\n- flip — cor percebida com mais peso em erros nas bordas e nos pontos.\n- ssim_blur_tolerant — tolera fotos desfocadas. Penaliza o desfoque no render."),
    IT("Confronto tra render e foto nella perdita strutturale.\n- ssim — struttura locale.\n- s_cielab — differenza cromatica percepita, meno sensibile al rumore fine.\n- flip — differenza percepita con più peso agli errori su bordi e punti.\n- ssim_blur_tolerant — tollera il mosso delle foto. Penalizza il render sfocato."),
    NL("Vergelijking van render en foto voor structuurverlies.\n- ssim — lokale structuur.\n- s_cielab — waargenomen kleurverschil met minder gewicht voor fijne kleurruis.\n- flip — kleurverschil met extra gewicht voor rand- en puntfouten.\n- ssim_blur_tolerant — verdraagt onscherpe foto’s. Straft onscherpe renders."),
    RU("Как структурная потеря сравнивает рендер и фото.\n- ssim — локальная структура.\n- s_cielab — воспринимаемая разница цвета. Мелкий шум менее важен.\n- flip — цвет с повышенным весом ошибок на краях и точках.\n- ssim_blur_tolerant — допускает размытие фото. Размытие рендера штрафуется."),
    TR("Yapı kaybı render ile fotoğrafı böyle karşılaştırır.\n- ssim — yerel yapı.\n- s_cielab — algılanan renk farkı, ince renk gürültüsüne daha az ağırlık verir.\n- flip — algılanan renk, kenar ve nokta hatalarına daha çok ağırlık verir.\n- ssim_blur_tolerant — fotoğraf bulanıklığını kabul eder. Render bulanıklığı cezalandırılır."));


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
    EN("Squared pixel-color error weight. Penalizes large errors more than L1, settling color faster but chasing outliers such as moving objects."),
    JA("ピクセル色の二乗誤差の重み。L1より大きな誤差を強く罰し、色の収束を早めますが、動く物体などの外れ値も追います。"),
    ZH_HANS("像素颜色平方误差的权重。比 L1 更惩罚大误差，使颜色收敛更快，但也会追逐移动物体等异常值。"),
    ZH_HANT("像素顏色平方誤差的權重。比 L1 更懲罰大誤差，使顏色收斂更快，但也會追逐移動物體等異常值。"),
    KO("픽셀 색 제곱 오차의 가중치입니다. L1보다 큰 오차를 강하게 벌해 색이 빨리 수렴하지만 움직이는 물체 같은 이상치도 쫓습니다."),
    DE("Gewicht des quadrierten Pixelfarbfehlers. Bestraft große Fehler stärker als L1: Farbe konvergiert schneller, folgt aber auch Ausreißern wie Bewegung."),
    FR("Poids de l’erreur couleur au carré. Punit les gros écarts plus que L1 : couleur plus vite stable, mais suit aussi les objets mobiles aberrants."),
    ES("Peso del error de color al cuadrado. Penaliza grandes errores más que L1: fija el color antes, pero persigue atípicos como objetos móviles."),
    PT("Peso do erro de cor ao quadrado. Pune erros grandes mais que L1: estabiliza a cor antes, mas segue extremos como objetos móveis."),
    IT("Peso dell’errore cromatico al quadrato. Penalizza più di L1 gli errori grandi: colore stabile prima, ma insegue anomalie come oggetti mobili."),
    NL("Gewicht van gekwadrateerde pixelfout. Straft grote fouten meer dan L1: sneller stabiele kleur, maar volgt ook uitschieters zoals bewegende objecten."),
    RU("Вес квадрата ошибки цвета пикселя. Сильнее L1 штрафует большие ошибки: быстрее стабилизирует цвет, но следует выбросам вроде движущихся объектов."),
    TR("Piksel renk karesi hatasının ağırlığı. Büyük hataları L1’den çok cezalandırır: renk çabuk oturur ama hareketli nesne gibi aykırıları da izler."));

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
    EN("Automatically halves images until the short side approaches this size, choosing loss scales per image. 0 uses num_loss_scales instead."),
    JA("短辺がこのサイズに近づくまで画像を半分にし、画像ごとに損失スケール数を自動選択します。0は num_loss_scales を使います。"),
    ZH_HANS("自动将图像逐次减半，直到短边接近此尺寸，为每张图像选择损失尺度数。0 改用 num_loss_scales。"),
    ZH_HANT("自動將影像逐次減半，直到短邊接近此尺寸，為每張影像選擇損失尺度數。0 改用 num_loss_scales。"),
    KO("짧은 변이 이 크기에 가까워질 때까지 이미지를 반으로 줄여 손실 스케일 수를 고릅니다. 0은 num_loss_scales를 사용합니다."),
    DE("Halbiert Bilder automatisch, bis die kurze Seite diese Größe erreicht, für Verlustskalen pro Bild. 0 verwendet num_loss_scales."),
    FR("Divise les images par deux jusqu’à approcher cette taille du petit côté, pour choisir les échelles par image. 0 utilise num_loss_scales."),
    ES("Reduce imágenes a la mitad hasta que el lado corto se acerque a este tamaño, eligiendo escalas por imagen. 0 usa num_loss_scales."),
    PT("Reduz imagens à metade até o lado curto se aproximar deste tamanho, escolhendo escalas por imagem. 0 usa num_loss_scales."),
    IT("Dimezza le immagini finché il lato corto si avvicina a questa misura, scegliendo le scale per immagine. 0 usa num_loss_scales."),
    NL("Halveert beelden tot de korte zijde dit formaat nadert en kiest zo verliesschalen per beeld. 0 gebruikt num_loss_scales."),
    RU("Делит изображения пополам, пока короткая сторона не приблизится к этому размеру, выбирая масштабы ошибки для каждого. 0 — num_loss_scales."),
    TR("Kısa kenar bu boyuta yaklaşana dek görüntüleri yarıya indirip kayıp ölçeği sayısını seçer. 0, num_loss_scales kullanır."));

SS_MSG(num_loss_scales,
    EN("Number of loss scales"), JA("損失スケールの数"),
    ZH_HANS("损失尺度数量"), ZH_HANT("損失尺度數量"), KO("손실 배율 개수"),
    DE("Anzahl der Verlustskalen"), FR("Nombre d'échelles de perte"),
    ES("Número de escalas de pérdida"), PT("Número de escalas de perda"),
    IT("Numero di scale di perdita"), NL("Aantal verliesschalen"),
    RU("Число масштабов потерь"), TR("Kayıp ölçeği sayısı"));
SS_MSG(num_loss_scales_help,
    EN("Loss comparisons at progressively smaller image sizes help large smooth areas converge. Normally chosen by loss_scale_min_pixels."),
    JA("画像を段階的に縮小して損失を比較する回数。大きく滑らかな領域の収束を助けます。通常は loss_scale_min_pixels が決めます。"),
    ZH_HANS("在逐渐缩小的图像尺寸上比较损失的尺度数，有助于大块平滑区域收敛。通常由 loss_scale_min_pixels 决定。"),
    ZH_HANT("在逐漸縮小的影像尺寸上比較損失的尺度數，有助於大塊平滑區域收斂。通常由 loss_scale_min_pixels 決定。"),
    KO("점차 작은 이미지 크기에서 손실을 비교하는 횟수로, 크고 매끄러운 영역의 수렴을 돕습니다. 보통 loss_scale_min_pixels가 결정합니다."),
    DE("Verlustvergleiche bei zunehmend kleineren Bildgrößen helfen großen glatten Flächen zu konvergieren. Normalerweise durch loss_scale_min_pixels bestimmt."),
    FR("Compare la perte à des tailles d’image décroissantes pour faire converger les grandes zones lisses. Normalement choisi par loss_scale_min_pixels."),
    ES("Compara la pérdida en imágenes cada vez menores para hacer converger áreas grandes y lisas. Normalmente lo decide loss_scale_min_pixels."),
    PT("Compara a perda em imagens cada vez menores para convergir áreas grandes e lisas. Normalmente escolhido por loss_scale_min_pixels."),
    IT("Confronti della perdita a dimensioni via via minori aiutano le grandi aree uniformi a convergere. Di norma scelto da loss_scale_min_pixels."),
    NL("Verliesvergelijkingen op steeds kleinere beeldmaten helpen grote gladde vlakken convergeren. Gewoonlijk gekozen door loss_scale_min_pixels."),
    RU("Число сравнений ошибки на всё меньших изображениях; помогает сходимости крупных гладких областей. Обычно задаётся loss_scale_min_pixels."),
    TR("Giderek küçük görüntülerde kayıp karşılaştırma sayısı; büyük düz alanların yakınsamasını destekler. Genelde loss_scale_min_pixels seçer."));

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

SS_MSG(loss_saturation_threshold,
    EN("Blown-highlight cutoff"), JA("白飛びとみなす明るさ"),
    ZH_HANS("过曝高光阈值"), ZH_HANT("過曝高光閾值"),
    KO("날아간 하이라이트 기준값"),
    DE("Schwelle für ausgebrannte Lichter"),
    FR("Seuil des hautes lumières brûlées"),
    ES("Umbral de altas luces quemadas"),
    PT("Limiar de altas luzes queimadas"),
    IT("Soglia delle alte luci bruciate"),
    NL("Drempel voor uitgebeten hoge lichten"),
    RU("Порог выбитых светов"),
    TR("Yanmış parlaklık eşiği"));
SS_MSG(loss_saturation_threshold_help,
    EN("Ignores a pixel when both photo and render exceed this brightness in all RGB channels. Negative (default) keeps every pixel."),
    JA("写真とレンダーのRGB全成分がこの明るさを超えたピクセルを損失から除きます。負の値（既定）はすべてのピクセルを使います。"),
    ZH_HANS("照片与渲染的 RGB 各通道都超过此亮度时，损失忽略该像素。负值（默认）保留所有像素。"),
    ZH_HANT("照片與算繪的 RGB 各通道都超過此亮度時，損失忽略該像素。負值（預設）保留所有像素。"),
    KO("사진과 렌더의 모든 RGB 채널이 이 밝기를 넘으면 해당 픽셀을 손실에서 제외합니다. 음수(기본값)는 모두 유지합니다."),
    DE("Ignoriert Pixel, wenn Foto und Render in allen RGB-Kanälen heller als dieser Wert sind. Negativ (Vorgabe) behält alle Pixel."),
    FR("Ignore un pixel si photo et rendu dépassent ce seuil dans tous les canaux RVB. Négatif (par défaut) conserve tous les pixels."),
    ES("Ignora un píxel si foto y render superan este brillo en todos los canales RGB. Negativo (predeterminado) conserva todos los píxeles."),
    PT("Ignora um pixel se foto e render excedem este brilho em todos os canais RGB. Negativo (padrão) mantém todos os pixels."),
    IT("Ignora un pixel se foto e render superano questa luminosità in tutti i canali RGB. Negativo (predefinito) conserva ogni pixel."),
    NL("Negeert een pixel als foto en render in alle RGB-kanalen helderder zijn dan deze grens. Negatief (standaard) behoudt alle pixels."),
    RU("Исключает пиксель, если фото и рендер ярче порога во всех каналах RGB. Отрицательное значение (по умолчанию) сохраняет все пиксели."),
    TR("Fotoğraf ve render tüm RGB kanallarında bu parlaklığı aşarsa pikseli kayıptan çıkarır. Negatif (varsayılan) tüm pikselleri korur."));

SS_MSG(loss_luminance_normalization,
    EN("Brightness normalization of color error"), JA("色の誤差の明るさ正規化"),
    ZH_HANS("颜色误差的亮度归一化"), ZH_HANT("顏色誤差的亮度正規化"),
    KO("색 오차의 밝기 정규화"),
    DE("Helligkeitsnormierung des Farbfehlers"),
    FR("Normalisation de l'erreur de couleur par la luminosité"),
    ES("Normalización del error de color por el brillo"),
    PT("Normalização do erro de cor pelo brilho"),
    IT("Normalizzazione dell'errore di colore per luminosità"),
    NL("Helderheidsnormalisatie van de kleurfout"),
    RU("Нормировка ошибки цвета по яркости"),
    TR("Renk hatasının parlaklık normalizasyonu"));
SS_MSG(loss_luminance_normalization_help,
    EN("Divides L1/L2/SSIM weights by (2 × mean photo brightness in sRGB)^power.\n- 0 leaves weights unchanged; 1 equalizes dark/bright captures, with intermediate values softer.\n- Intended for linear-light training."),
    JA("L1/L2/SSIMの重みを (2 × 写真のsRGB平均輝度)^指数 で割ります。\n- 0は変更なし。1は暗い・明るい撮影を同等に扱い、中間値は効果を弱めます。\n- リニア光での学習向けです。"),
    ZH_HANS("将 L1/L2/SSIM 权重除以 (2 × 照片的 sRGB 平均亮度)^指数。\n- 0 不改变权重；1 平衡暗亮拍摄；中间值减弱效果。\n- 用于线性光训练。"),
    ZH_HANT("將 L1/L2/SSIM 權重除以 (2 × 照片的 sRGB 平均亮度)^指數。\n- 0 不改變權重；1 平衡暗亮拍攝；中間值減弱效果。\n- 用於線性光訓練。"),
    KO("L1/L2/SSIM 가중치를 (2 × 사진의 sRGB 평균 밝기)^지수로 나눕니다.\n- 0은 불변, 1은 밝고 어두운 촬영의 비중을 맞추며 중간값은 효과가 완만합니다.\n- 선형 광 학습용입니다."),
    DE("Teilt L1/L2/SSIM-Gewichte durch (2 × mittlere Fotohelligkeit in sRGB)^Exponent.\n- 0 unverändert; 1 gleicht dunkle/helle Aufnahmen an, Zwischenwerte wirken schwächer.\n- Für Training in linearem Licht."),
    FR("Divise les poids L1/L2/SSIM par (2 × luminosité moyenne de la photo en sRGB)^exposant.\n- 0 inchangé ; 1 équilibre captures sombres/claires, valeurs intermédiaires plus douces.\n- Pour entraîner en lumière linéaire."),
    ES("Divide pesos L1/L2/SSIM por (2 × brillo medio de foto en sRGB)^exponente.\n- 0 no cambia; 1 equilibra capturas oscuras/claras; valores intermedios suavizan.\n- Para entrenamiento en luz lineal."),
    PT("Divide pesos L1/L2/SSIM por (2 × brilho médio da foto em sRGB)^expoente.\n- 0 não muda; 1 equilibra capturas escuras/claras; valores intermediários suavizam.\n- Para treino em luz linear."),
    IT("Divide pesi L1/L2/SSIM per (2 × luminosità media della foto in sRGB)^esponente.\n- 0 non cambia; 1 bilancia riprese scure/chiare; valori intermedi attenuano.\n- Per addestramento in luce lineare."),
    NL("Deelt L1/L2/SSIM-gewichten door (2 × gemiddelde fotohelderheid in sRGB)^exponent.\n- 0 ongewijzigd; 1 weegt donker/licht gelijk, tussenwaarden zachter.\n- Voor training in lineair licht."),
    RU("Делит веса L1/L2/SSIM на (2 × средняя яркость фото в sRGB)^степень.\n- 0 не меняет; 1 уравнивает тёмную/светлую съёмку, промежуточные мягче.\n- Для обучения в линейном свете."),
    TR("L1/L2/SSIM ağırlıklarını (2 × fotoğrafın sRGB ortalama parlaklığı)^üs ile böler.\n- 0 değişmez; 1 karanlık/aydınlık çekimi eşitler, ara değerler daha yumuşaktır.\n- Doğrusal ışıkta eğitim içindir."));


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
    EN("Tightens depth/color consistency and limits view-dependent color to reduce floaters.\n- strong: crispest geometry, but may flatten thin or translucent detail.\n- extreme: adds neighborhood regularization, potentially slower with many splats."),
    JA("深度・色の整合性を強め、視点依存色を抑えて浮遊物を減らします。\n- strong：形状が最も鮮明ですが、薄い・半透明な細部が平坦になる恐れ。\n- extreme：近傍正則化を追加。スプラットが多いと遅くなる場合があります。"),
    ZH_HANS("加强深度与颜色一致性，限制视角相关颜色以减少漂浮物。\n- strong：几何最清晰，但可能压平薄小或半透明细节。\n- extreme：增加邻域正则化，泼溅较多时可能变慢。"),
    ZH_HANT("加強深度與顏色一致性，限制視角相關顏色以減少漂浮物。\n- strong：幾何最清晰，但可能壓平薄小或半透明細節。\n- extreme：增加鄰域正規化，潑濺較多時可能變慢。"),
    KO("깊이/색 일관성을 강화하고 시점별 색을 제한해 부유물을 줄입니다.\n- strong: 가장 선명한 형상이지만 얇거나 반투명한 세부를 평평하게 만들 수 있습니다.\n- extreme: 이웃 정규화를 추가해 스플랫이 많으면 느려질 수 있습니다."),
    DE("Verstärkt Tiefen-/Farbkonsistenz und begrenzt blickabhängige Farbe gegen Floater.\n- strong: schärfste Geometrie, kann dünne oder durchscheinende Details abflachen.\n- extreme: zusätzliche Nachbarschaftsregularisierung, bei vielen Splats eventuell langsamer."),
    FR("Renforce cohérence profondeur/couleur et limite la couleur selon la vue contre les flottants.\n- strong : géométrie très nette, mais peut aplatir les détails fins ou translucides.\n- extreme : régularisation du voisinage en plus, potentiellement plus lente avec beaucoup de splats."),
    ES("Refuerza coherencia de profundidad/color y limita color por vista para reducir flotantes.\n- strong: geometría más nítida, pero puede aplanar detalles finos o translúcidos.\n- extreme: añade regularización vecinal, quizá más lenta con muchos splats."),
    PT("Reforça coerência de profundidade/cor e limita cor por vista para reduzir flutuantes.\n- strong: geometria mais nítida, mas pode achatar detalhes finos ou translúcidos.\n- extreme: regularização de vizinhança extra, talvez mais lenta com muitos splats."),
    IT("Rafforza coerenza profondità/colore e limita colore per vista contro i flottanti.\n- strong: geometria più netta, ma può appiattire dettagli sottili o traslucidi.\n- extreme: aggiunge regolarizzazione del vicinato, forse più lenta con molti splat."),
    NL("Versterkt diepte-/kleurconsistentie en beperkt kijkafhankelijke kleur tegen floaters.\n- strong: scherpste geometrie, maar kan dunne of doorschijnende details afvlakken.\n- extreme: extra buurregularisatie, mogelijk langzamer bij veel splats."),
    RU("Усиливает согласованность глубины/цвета и ограничивает цвет по ракурсу против плавающих пятен.\n- strong: чёткая геометрия, но может сгладить тонкие или полупрозрачные детали.\n- extreme: добавляет регуляризацию соседей, возможное замедление при множестве сплатов."),
    TR("Yüzenleri azaltmak için derinlik/renk tutarlılığını artırır ve görüş rengini sınırlar.\n- strong: en keskin geometri; ince veya yarı saydam ayrıntıyı düzleştirebilir.\n- extreme: komşuluk düzenlemesi ekler, çok splatta yavaşlayabilir."));

SS_MSG(depth_distortion_reg,
    EN("Depth consistency"), JA("深度の一貫性"), ZH_HANS("深度一致性"),
    ZH_HANT("深度一致性"), KO("깊이 일관성"), DE("Tiefenkonsistenz"),
    FR("Cohérence de la profondeur"), ES("Coherencia de la profundidad"),
    PT("Coerência da profundidade"), IT("Coerenza della profondità"),
    NL("Diepteconsistentie"), RU("Согласованность глубины"),
    TR("Derinlik tutarlılığı"));
SS_MSG(depth_distortion_reg_help,
    EN("Encourage each pixel's depth to come from one surface and discourage floaters.\nGives crisper geometry and better meshes; too much flattens fine translucent detail."),
    JA("各画素の深度が一つの面から来るように促し、浮遊物を抑えます。\nジオメトリがくっきりしてメッシュもよくなりますが、効かせすぎると薄い半透明の細部が平坦になります。"),
    ZH_HANS("促使每个像素的深度来自同一个表面，抑制漂浮物。\n几何更清晰、网格更好；过强则会压平细薄的半透明细节。"),
    ZH_HANT("促使每個像素的深度來自同一個表面，抑制漂浮物。\n幾何更清晰、網格更好；過強則會壓平細薄的半透明細節。"),
    KO("각 픽셀의 깊이가 한 면에서 오도록 유도하고 부유물을 억제합니다.\n지오메트리가 또렷해지고 메시가 좋아지지만, 지나치면 얇은 반투명 디테일이 납작해집니다."),
    DE("Dazu anhalten, dass die Tiefe jedes Pixels von einer Fläche stammt, und Schwebeteile unterdrücken.\nErgibt knackigere Geometrie und bessere Netze; zu viel macht feines, lichtdurchlässiges Detail flach."),
    FR("Inciter la profondeur de chaque pixel à provenir d'une seule surface et décourager les flotteurs.\nDonne une géométrie plus nette et de meilleurs maillages ; trop fort aplatit les détails fins et translucides."),
    ES("Animar a que la profundidad de cada píxel provenga de una sola superficie y desalentar los restos flotantes.\nDa una geometría más nítida y mejores mallas; demasiado aplana el detalle fino y translúcido."),
    PT("Incentivar que a profundidade de cada pixel venha de uma única superfície e desencorajar resíduos flutuantes.\nDá geometria mais nítida e malhas melhores; em excesso achata o detalhe fino e translúcido."),
    IT("Incoraggiare la profondità di ogni pixel a provenire da una sola superficie e scoraggiare i frammenti fluttuanti.\nDà geometria più nitida e mesh migliori; troppo appiattisce il dettaglio fine e traslucido."),
    NL("Aanmoedigen dat de diepte van elke pixel van één oppervlak komt en zwevers ontmoedigen.\nGeeft strakkere geometrie en betere meshes; te veel slaat fijn doorschijnend detail plat."),
    RU("Побуждать глубину каждого пикселя приходить с одной поверхности и подавлять «летающие» артефакты.\nДаёт более чёткую геометрию и лучшие меши; перебор сплющивает тонкие полупрозрачные детали."),
    TR("Her pikselin derinliğinin tek bir yüzeyden gelmesini teşvik eder ve uçuşan artıkları caydırır.\nDaha keskin geometri ve daha iyi ağ verir; fazlası ince, yarı saydam ayrıntıyı düzleştirir."));

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
    EN("How many steps the distortion penalties take to reach full strength.\nRamping in lets coarse structure form before geometry is tightened."),
    JA("一貫性のペナルティが最大になるまでのステップ数です。\n徐々に効かせることで、ジオメトリを締める前に大まかな形ができあがります。"),
    ZH_HANS("一致性惩罚达到最大强度所需的步数。\n逐步加力可以让粗略结构先成形，再收紧几何。"),
    ZH_HANT("一致性懲罰達到最大強度所需的步數。\n逐步加力可以讓粗略結構先成形，再收緊幾何。"),
    KO("일관성 페널티가 최대 강도에 이르기까지의 스텝 수입니다.\n서서히 올리면 지오메트리를 조이기 전에 굵직한 구조가 먼저 잡힙니다."),
    DE("Wie viele Schritte die Konsistenzstrafen brauchen, um volle Stärke zu erreichen.\nDas langsame Einblenden lässt die grobe Struktur entstehen, bevor die Geometrie festgezurrt wird."),
    FR("Combien d'étapes les pénalités de cohérence mettent à atteindre leur pleine force.\nUne montée progressive laisse la structure grossière se former avant de resserrer la géométrie."),
    ES("Cuántos pasos tardan las penalizaciones de coherencia en alcanzar toda su fuerza.\nSubirlas poco a poco deja que se forme la estructura burda antes de apretar la geometría."),
    PT("Quantos passos as penalidades de coerência levam para atingir força total.\nSubir aos poucos deixa a estrutura grosseira se formar antes de apertar a geometria."),
    IT("Quanti passi impiegano le penalità di coerenza a raggiungere la piena forza.\nSalire per gradi lascia formare la struttura grossolana prima di stringere la geometria."),
    NL("Hoeveel stappen de consistentiestraffen nodig hebben om op volle sterkte te komen.\nGeleidelijk opvoeren laat de grove structuur ontstaan voordat de geometrie wordt aangetrokken."),
    RU("За сколько шагов штрафы согласованности набирают полную силу.\nПостепенное включение даёт сложиться грубой структуре, прежде чем геометрию затянут."),
    TR("Tutarlılık cezalarının tam güce ulaşması için gereken adım sayısı.\nKademeli devreye girmek, geometri sıkılmadan önce kaba yapının oluşmasını sağlar."));

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
    EN("Push each pixel's coverage toward fully solid or fully empty instead of half transparent.\nClears haze and gives cleaner background cutouts, at the risk of less stable training."),
    JA("各画素の被覆を、半透明ではなく完全に不透明か完全に空かへ寄せます。\nもやが消えて背景の切り抜きもきれいになりますが、学習は不安定になりやすくなります。"),
    ZH_HANS("把每个像素的覆盖推向完全实心或完全空白，而不是半透明。\n可以清除雾感、让背景抠图更干净，但训练可能变得不那么稳定。"),
    ZH_HANT("把每個像素的覆蓋推向完全實心或完全空白，而不是半透明。\n可以清除霧感、讓背景去背更乾淨，但訓練可能變得不那麼穩定。"),
    KO("각 픽셀의 커버리지를 반투명이 아니라 완전히 채워짐 또는 완전히 비움 쪽으로 밀어냅니다.\n안개가 걷히고 배경 오려내기가 깔끔해지지만 학습이 덜 안정될 수 있습니다."),
    DE("Die Deckung jedes Pixels zu ganz voll oder ganz leer drängen statt halbdurchsichtig.\nRäumt Schleier weg und ergibt sauberere Freisteller, auf die Gefahr eines unruhigeren Trainings."),
    FR("Pousser la couverture de chaque pixel vers le plein ou le vide plutôt que le semi-transparent.\nFait disparaître le voile et donne des détourages plus propres, au risque d'un entraînement moins stable."),
    ES("Empujar la cobertura de cada píxel hacia lleno o vacío en vez de semitransparente.\nElimina la neblina y da recortes de fondo más limpios, a riesgo de un entrenamiento menos estable."),
    PT("Empurrar a cobertura de cada pixel para cheio ou vazio em vez de semitransparente.\nElimina a névoa e dá recortes de fundo mais limpos, ao risco de um treinamento menos estável."),
    IT("Spingere la copertura di ogni pixel verso il pieno o il vuoto invece che il semitrasparente.\nElimina la foschia e dà scontorni più puliti, a rischio di un addestramento meno stabile."),
    NL("De dekking van elke pixel naar helemaal vol of helemaal leeg duwen in plaats van halfdoorzichtig.\nRuimt waas op en geeft schonere uitsnedes, met het risico van minder stabiele training."),
    RU("Смещать покрытие каждого пикселя к полностью плотному или полностью пустому вместо полупрозрачного.\nУбирает дымку и даёт более чистые вырезы фона ценой менее устойчивого обучения."),
    TR("Her pikselin kaplamasını yarı saydam yerine tamamen dolu ya da tamamen boş olmaya iter.\nPusu giderir ve arka plan kesiklerini temizler; karşılığında eğitim daha az kararlı olabilir."));

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
    EN("Depth means distance along the camera ray, not straight ahead. Unset: ray depth for wide lenses split by spirula geometry, straight-ahead depth otherwise."),
    JA("深度を正面方向でなくカメラ光線に沿う距離として扱います。未指定：spirula geometry が分割した広角レンズは光線距離、それ以外は正面距離。"),
    ZH_HANS("深度表示沿相机射线的距离，而非正前方距离。留空：spirula geometry 拆分的广角镜头用射线深度，其余用正前方深度。"),
    ZH_HANT("深度表示沿相機射線的距離，而非正前方距離。留空：spirula geometry 拆分的廣角鏡頭用射線深度，其餘用正前方深度。"),
    KO("깊이를 정면 대신 카메라 광선 거리로 취급합니다. 미설정: spirula geometry가 분할한 광각 렌즈는 광선 깊이, 나머지는 정면 깊이입니다."),
    DE("Tiefe entlang des Kamerastrahls statt nach vorn. Unbelegt: Strahltiefe bei von spirula geometry geteilten Weitwinkeln, sonst Vorwärtstiefe."),
    FR("Profondeur le long du rayon, non vers l’avant. Vide : distance au rayon pour les grands-angles divisés par spirula geometry, vers l’avant sinon."),
    ES("Profundidad sobre el rayo, no hacia delante. Sin definir: por rayo para gran angular dividido por spirula geometry; hacia delante en otro caso."),
    PT("Profundidade ao longo do raio, não à frente. Vazio: pelo raio para grande angular dividida por spirula geometry; à frente nos demais casos."),
    IT("Profondità lungo il raggio, non in avanti. Vuoto: lungo il raggio per grandangoli divisi da spirula geometry, in avanti negli altri casi."),
    NL("Diepte langs de camerastraal in plaats van recht vooruit. Leeg: straaldiepte bij door spirula geometry gesplitste groothoeken, anders vooruitdiepte."),
    RU("Глубина вдоль луча, а не прямо вперёд. Не задано: вдоль луча для широких линз, разделённых spirula geometry, иначе вперёд."),
    TR("Derinlik ileri yönde değil kamera ışını boyuncadır. Boşsa: spirula geometry’nin böldüğü geniş lenslerde ışın derinliği, diğerlerinde ileri derinlik."));

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
    EN("Aligns each pixel’s mean and most-solid depths, pulling splats onto one surface. Useful for models intended for meshing."),
    JA("各ピクセルの平均深度と最も不透明な深度を一致させ、スプラットを1つの表面へ寄せます。メッシュ化するモデル向けです。"),
    ZH_HANS("让每个像素的平均深度与最实心处深度一致，将泼溅拉到同一表面。适合准备网格化的模型。"),
    ZH_HANT("讓每個像素的平均深度與最實心處深度一致，將潑濺拉到同一表面。適合準備網格化的模型。"),
    KO("각 픽셀의 평균 깊이와 가장 불투명한 깊이를 맞춰 스플랫을 하나의 표면으로 당깁니다. 메시화할 모델에 유용합니다."),
    DE("Gleicht mittlere und dichteste Pixeltiefe an und zieht Splats auf eine Oberfläche. Nützlich für Modelle zur Vermaschung."),
    FR("Rapproche les profondeurs moyenne et la plus opaque de chaque pixel pour ramener les splats sur une surface. Utile pour le maillage."),
    ES("Alinea la profundidad media y la más opaca de cada píxel, llevando splats a una superficie. Útil para modelos destinados a malla."),
    PT("Alinha profundidade média e mais opaca por pixel, puxando splats para uma superfície. Útil para modelos destinados a malha."),
    IT("Allinea profondità media e più opaca di ogni pixel, portando gli splat su una superficie. Utile per modelli da convertire in mesh."),
    NL("Brengt gemiddelde en meest dichte pixeldiepte samen en trekt splats op één oppervlak. Nuttig voor modellen die een mesh worden."),
    RU("Согласует среднюю и наиболее плотную глубину пикселя, стягивая сплаты на одну поверхность. Полезно для будущего построения сетки."),
    TR("Pikselin ortalama ve en katı derinliğini eşleyip splatları tek yüzeye çeker. Ağ oluşturulacak modellerde yararlıdır."));

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

SS_MSG(pca_reg_neighbors,
    EN("Local-PCA neighbours"),
    JA("局所 PCA の近傍数"),
    ZH_HANS("局部 PCA 邻居数"),
    ZH_HANT("局部 PCA 鄰居數"),
    KO("로컬 PCA 이웃 수"),
    DE("Nachbarn für lokale PCA"),
    FR("Voisins de la PCA locale"),
    ES("Vecinos de la PCA local"),
    PT("Vizinhos da PCA local"),
    IT("Vicini della PCA locale"),
    NL("Buren voor lokale PCA"),
    RU("Соседи локального PCA"),
    TR("Yerel PCA komşuları"));
SS_MSG(pca_reg_neighbors_help,
    EN("Nearby splats used to fit each local surface (PCAsplat), guiding poorly observed splats too. 0 disables it; maximum 31."),
    JA("局所表面の当てはめに使う近傍スプラット数（PCAsplat）。観測の少ないスプラットも表面へ寄せます。0で無効、最大31です。"),
    ZH_HANS("每个局部表面拟合使用的邻近点数量（PCAsplat），也约束观测不足的点。0 关闭；最大 31。"),
    ZH_HANT("每個局部表面擬合使用的鄰近點數量（PCAsplat），也約束觀測不足的點。0 關閉；最大 31。"),
    KO("국소 표면을 맞추는 이웃 스플랫 수입니다(PCAsplat). 관측이 부족한 스플랫도 표면으로 유도합니다. 0은 비활성화, 최대 31입니다."),
    DE("Benachbarte Splats pro lokaler Oberflächenanpassung (PCAsplat), auch für schlecht beobachtete Splats. 0 schaltet ab; maximal 31."),
    FR("Splats voisins pour ajuster chaque surface locale (PCAsplat), y compris les splats mal observés. 0 désactive ; maximum 31."),
    ES("Splats vecinos para ajustar cada superficie local (PCAsplat), guiando también los poco observados. 0 desactiva; máximo 31."),
    PT("Splats vizinhos para ajustar cada superfície local (PCAsplat), guiando também os pouco observados. 0 desativa; máximo 31."),
    IT("Splat vicini per adattare ogni superficie locale (PCAsplat), guidando anche quelli poco osservati. 0 disattiva; massimo 31."),
    NL("Nabije splats per lokale oppervlaktepassing (PCAsplat), ook om slecht waargenomen splats te sturen. 0 schakelt uit; maximaal 31."),
    RU("Соседние сплаты для подгонки локальной поверхности (PCAsplat), включая плохо видимые сплаты. 0 отключает; максимум 31."),
    TR("Yerel yüzey uydurma için komşu splat sayısı (PCAsplat); az gözlenen splatları da yönlendirir. 0 kapatır; en fazla 31."));

SS_MSG(pca_reg_knn,
    EN("Local-PCA neighbour search"),
    JA("局所 PCA の近傍探索"),
    ZH_HANS("局部 PCA 邻居搜索"),
    ZH_HANT("局部 PCA 鄰居搜尋"),
    KO("로컬 PCA 이웃 탐색"),
    DE("Nachbarsuche der lokalen PCA"),
    FR("Recherche des voisins de la PCA locale"),
    ES("Búsqueda de vecinos de la PCA local"),
    PT("Busca de vizinhos da PCA local"),
    IT("Ricerca dei vicini della PCA locale"),
    NL("Zoeken naar buren voor lokale PCA"),
    RU("Поиск соседей локального PCA"),
    TR("Yerel PCA komşu araması"));
SS_MSG(pca_reg_knn_help,
    EN("Choose how to find nearby splats.\n- exact searches a tree for true neighbors each step.\n- morton takes neighbors along a space-filling curve. Faster, but may mix distant parts.\n- morton-best selects the nearest from a longer stretch of the curve.\n- -cached reuses neighbors until refresh or splat addition or relocation."),
    JA("近くのスプラットの見つけ方です。 `exact` は毎ステップ木構造を探索して真の最近傍を求めます。\n- `morton` は空間充填曲線上で隣り合うスプラットを取り、軽い代わりに離れた部分が混ざることがあります。\n- `morton-best` はその曲線上のより長い区間から最も近いものを残します。\n- `-cached` 付きは見つけた結果を保持し、更新間隔ごとと、スプラットの追加や移動のあとにだけ探し直します。"),
    ZH_HANS("如何找到附近的泼溅。 `exact` 每一步都在树中搜索真正的最近邻； `morton` 取空间填充曲线上相邻的泼溅，开销更小，但可能混入相距很远的部分；\n- `morton-best` 从该曲线上更长的一段中保留最近的那些。\n- 带 `-cached` 的形式会保留找到的结果，只在刷新间隔到达时以及新增或迁移泼溅之后重新搜索。"),
    ZH_HANT("如何找到附近的潑濺。 `exact` 每一步都在樹中搜尋真正的最近鄰； `morton` 取空間填充曲線上相鄰的潑濺，開銷更小，但可能混入相距很遠的部分；\n- `morton-best` 從該曲線上更長的一段中保留最近的那些。\n- 帶 `-cached` 的形式會保留找到的結果，只在刷新間隔到達時以及新增或遷移潑濺之後重新搜尋。"),
    KO("근처 스플랫을 찾는 방법입니다. `exact`는 매 스텝 트리를 탐색해 실제 최근접 이웃을 찾습니다.\n- `morton`은 공간 채움 곡선에서 바로 옆의 스플랫을 취해 더 가볍지만 멀리 떨어진 부분이 섞일 수 있습니다.\n- `morton-best`는 그 곡선의 더 긴 구간에서 가장 가까운 것들을 남깁니다.\n- `-cached`가 붙은 형식은 찾은 결과를 유지하고, 갱신 간격마다 그리고 스플랫을 추가하거나 옮긴 뒤에만 다시 찾습니다."),
    DE("Wie die nahen Splats gefunden werden. `exact` durchsucht jeden Schritt einen Baum nach den wirklich nächsten;\n- `morton` nimmt die Splats, die entlang einer raumfüllenden Kurve daneben liegen – billiger, kann aber weit entfernte Teile mischen; `morton-best` behält die nächsten aus einem längeren Stück dieser Kurve.\n- Die `-cached`-Formen behalten, was sie gefunden haben, und suchen erst nach dem Auffrischungsintervall neu sowie nachdem Splats hinzukamen oder umgezogen sind."),
    FR("Comment les splats proches sont trouvés. `exact` parcourt un arbre à chaque étape pour trouver les vrais plus proches ;\n- `morton` prend les splats voisins le long d'une courbe de remplissage de l'espace, moins coûteux mais pouvant mêler des morceaux éloignés ; `morton-best` garde les plus proches sur un tronçon plus long de cette courbe.\n- Les formes `-cached` conservent ce qu'elles ont trouvé et ne cherchent à nouveau qu'à l'intervalle de rafraîchissement et après l'ajout ou le déplacement de splats."),
    ES("Cómo se encuentran los splats cercanos. `exact` recorre un árbol en cada paso para hallar los verdaderos más cercanos;\n`morton` toma los splats contiguos a lo largo de una curva que llena el espacio, más barato pero puede mezclar trozos lejanos; `morton-best` conserva los más cercanos de un tramo más largo de esa curva. Las formas `-cached` guardan lo que encontraron y solo vuelven a buscar en el intervalo de actualización y después de añadir o reubicar splats."),
    PT("Como os splats próximos são encontrados. `exact` percorre uma árvore a cada passo para achar os verdadeiros mais próximos; `morton` pega os splats vizinhos ao longo de uma curva que preenche o espaço, mais barato, mas pode misturar pedaços distantes; `morton-best` mantém os mais próximos de um trecho mais longo dessa curva.\nAs formas `-cached` guardam o que encontraram e só buscam de novo no intervalo de atualização e depois que splats são acrescentados ou realocados."),
    IT("Come vengono trovati gli splat vicini. `exact` percorre un albero a ogni passo per trovare i veri più vicini; `morton` prende gli splat adiacenti lungo una curva che riempie lo spazio, più economico ma può mescolare parti lontane;\n- `morton-best` tiene i più vicini da un tratto più lungo di quella curva.\n- Le forme `-cached` conservano ciò che hanno trovato e cercano di nuovo solo all'intervallo di aggiornamento e dopo che sono stati aggiunti o spostati degli splat."),
    NL("Hoe de nabije splats worden gevonden. `exact` doorzoekt elke stap een boom naar de echte dichtstbijzijnde;\n- `morton` neemt de splats die ernaast liggen langs een ruimtevullende kromme, goedkoper maar kan ver uit elkaar liggende stukken mengen; `morton-best` houdt de dichtstbijzijnde over uit een langer stuk van die kromme.\n- De `-cached`-vormen bewaren wat ze vonden en zoeken pas opnieuw na het verversingsinterval en nadat splats zijn toegevoegd of verplaatst."),
    RU("Как находятся ближайшие сплаты. `exact` на каждом шаге обходит дерево и находит настоящих ближайших; `morton` берёт сплаты, соседние вдоль заполняющей пространство кривой, — дешевле, но может смешивать далёкие части; `morton-best` оставляет ближайших из более длинного отрезка этой кривой.\nВарианты `-cached` сохраняют найденное и ищут заново только через интервал обновления и после добавления или перемещения сплатов."),
    TR("Yakındaki splat'ların nasıl bulunduğu. `exact` her adımda bir ağacı tarayarak gerçek en yakınları bulur; `morton` uzayı dolduran bir eğri boyunca yanındaki splat'ları alır, daha ucuzdur ama birbirinden uzak parçaları karıştırabilir;\n`morton-best` bu eğrinin daha uzun bir parçasından en yakınları tutar. `-cached` biçimleri bulduklarını saklar ve yalnızca yenileme aralığında ve splat eklendikten ya da taşındıktan sonra yeniden arar."));

SS_MSG(pca_reg_window,
    EN("Local-PCA search window"),
    JA("局所 PCA の探索幅"),
    ZH_HANS("局部 PCA 搜索窗口"),
    ZH_HANT("局部 PCA 搜尋視窗"),
    KO("로컬 PCA 탐색 창"),
    DE("Suchfenster der lokalen PCA"),
    FR("Fenêtre de recherche de la PCA locale"),
    ES("Ventana de búsqueda de la PCA local"),
    PT("Janela de busca da PCA local"),
    IT("Finestra di ricerca della PCA locale"),
    NL("Zoekvenster van lokale PCA"),
    RU("Окно поиска локального PCA"),
    TR("Yerel PCA arama penceresi"));
SS_MSG(pca_reg_window_help,
    EN("How many splats along the curve `morton-best` chooses the nearest from.\nMore finds truer neighbours and costs more; 0 uses four times the group size."),
    JA("`morton-best` が最も近いものを選ぶ、曲線上のスプラット数です。\n- 多いほど本当の近傍に近づきますが重くなります。\n- 0 でまとまりの大きさの 4 倍を使います。"),
    ZH_HANS("`morton-best` 在曲线上从多少个泼溅中挑选最近的。\n越多找到的邻居越接近真实，开销也越大；0 表示使用组大小的四倍。"),
    ZH_HANT("`morton-best` 在曲線上從多少個潑濺中挑選最近的。\n越多找到的鄰居越接近真實，開銷也越大；0 表示使用組大小的四倍。"),
    KO("`morton-best`가 곡선에서 몇 개의 스플랫 중 가장 가까운 것을 고를지입니다.\n많을수록 실제 이웃에 가까워지지만 비용이 늘어납니다. 0이면 묶음 크기의 네 배를 씁니다."),
    DE("Aus wie vielen Splats entlang der Kurve `morton-best` die nächsten auswählt.\nMehr findet echtere Nachbarn und kostet mehr; 0 nimmt das Vierfache der Gruppengröße."),
    FR("Parmi combien de splats le long de la courbe `morton-best` choisit les plus proches.\nPlus trouve des voisins plus fidèles et coûte plus ; 0 prend quatre fois la taille du groupe."),
    ES("Entre cuántos splats a lo largo de la curva elige `morton-best` los más cercanos.\nMás encuentra vecinos más fieles y cuesta más; 0 usa cuatro veces el tamaño del grupo."),
    PT("Entre quantos splats ao longo da curva `morton-best` escolhe os mais próximos.\nMais encontra vizinhos mais fiéis e custa mais; 0 usa quatro vezes o tamanho do grupo."),
    IT("Tra quanti splat lungo la curva `morton-best` sceglie i più vicini.\nDi più trova vicini più fedeli e costa di più; 0 usa quattro volte la dimensione del gruppo."),
    NL("Uit hoeveel splats langs de kromme `morton-best` de dichtstbijzijnde kiest.\nMeer vindt echtere buren en kost meer; 0 neemt vier keer de groepsgrootte."),
    RU("Из скольких сплатов вдоль кривой `morton-best` выбирает ближайших.\nБольше находит более настоящих соседей и стоит дороже; 0 берёт учетверённый размер группы."),
    TR("`morton-best`'in eğri boyunca en yakınları kaç splat arasından seçtiği.\nDaha fazlası gerçeğe daha yakın komşular bulur ve daha pahalıdır; 0 grup boyutunun dört katını kullanır."));

SS_MSG(pca_reg_refresh_every,
    EN("Local-PCA neighbour refresh"),
    JA("局所 PCA の近傍更新間隔"),
    ZH_HANS("局部 PCA 邻居刷新间隔"),
    ZH_HANT("局部 PCA 鄰居刷新間隔"),
    KO("로컬 PCA 이웃 갱신 간격"),
    DE("Auffrischung der Nachbarn der lokalen PCA"),
    FR("Rafraîchissement des voisins de la PCA locale"),
    ES("Actualización de vecinos de la PCA local"),
    PT("Atualização dos vizinhos da PCA local"),
    IT("Aggiornamento dei vicini della PCA locale"),
    NL("Verversen van buren voor lokale PCA"),
    RU("Обновление соседей локального PCA"),
    TR("Yerel PCA komşu yenileme aralığı"));
SS_MSG(pca_reg_refresh_every_help,
    EN("How many steps the `-cached` searches reuse their neighbours before searching again\nthey also search again whenever splats are added or relocated. 0 follows the interval between those rounds."),
    JA("`-cached` 付きの探索が、探し直すまでに近傍を何ステップ使い回すかです。\n- スプラットを追加したり移したりしたときにも探し直します。\n- 0 でそのラウンドの間隔に合わせます。"),
    ZH_HANS("带 `-cached` 的搜索在重新搜索前复用邻居多少步\n- 每当新增或迁移泼溅时也会重新搜索。\n- 0 表示跟随这些轮次之间的间隔。"),
    ZH_HANT("帶 `-cached` 的搜尋在重新搜尋前重複使用鄰居多少步\n- 每當新增或遷移潑濺時也會重新搜尋。\n- 0 表示跟隨這些輪次之間的間隔。"),
    KO("`-cached`가 붙은 탐색이 다시 찾기 전까지 이웃을 몇 스텝 동안 재사용할지입니다.\n스플랫을 추가하거나 옮길 때도 다시 찾습니다. 0이면 그 회차 사이의 간격을 따릅니다."),
    DE("Wie viele Schritte die `-cached`-Suchen ihre Nachbarn wiederverwenden, bevor sie neu suchen\nsie suchen auch jedes Mal neu, wenn Splats hinzukommen oder umziehen. 0 folgt dem Abstand zwischen diesen Runden."),
    FR("Pas de réutilisation des voisins par les recherches `-cached`.\n- Nouvelle recherche après ajout ou déplacement de splats.\n- 0 suit l’intervalle de ces cycles."),
    ES("Durante cuántos pasos reutilizan sus vecinos las búsquedas `-cached` antes de volver a buscar\ntambién vuelven a buscar cada vez que se añaden o reubican splats. 0 sigue el intervalo entre esas rondas."),
    PT("Por quantos passos as buscas `-cached` reutilizam seus vizinhos antes de buscar de novo\ntambém buscam de novo sempre que splats são acrescentados ou realocados. 0 segue o intervalo entre essas rodadas."),
    IT("Per quanti passi le ricerche `-cached` riusano i loro vicini prima di cercare di nuovo\ncercano di nuovo anche ogni volta che vengono aggiunti o spostati degli splat. 0 segue l'intervallo tra quei cicli."),
    NL("Stappen waarin `-cached`-zoekacties dezelfde buren gebruiken.\n- Opnieuw zoeken na toevoegen of verplaatsen van splats.\n- 0 volgt het interval van die rondes."),
    RU("Сколько шагов поиски `-cached` используют найденных соседей, прежде чем искать заново\nзаново они ищут и всякий раз, когда сплаты добавляются или перемещаются. 0 следует интервалу между этими раундами."),
    TR("`-cached` aramalarının yeniden aramadan önce komşularını kaç adım boyunca yeniden kullandığı\nsplat eklendiğinde ya da taşındığında da yeniden ararlar. 0 bu turlar arasındaki aralığı izler."));

SS_MSG(pca_reg_start_iter,
    EN("Local-PCA start step"),
    JA("局所 PCA の開始ステップ"),
    ZH_HANS("局部 PCA 起始步"),
    ZH_HANT("局部 PCA 起始步"),
    KO("로컬 PCA 시작 스텝"),
    DE("Startschritt der lokalen PCA"),
    FR("Étape de départ de la PCA locale"),
    ES("Paso de inicio de la PCA local"),
    PT("Passo inicial da PCA local"),
    IT("Passo di avvio della PCA locale"),
    NL("Startstap van lokale PCA"),
    RU("Начальный шаг локального PCA"),
    TR("Yerel PCA başlangıç adımı"));
SS_MSG(pca_reg_start_iter_help,
    EN("The step the local-PCA terms switch on. Earlier pins splats to surfaces "
       "before the scene has settled."),
    JA("局所 PCA の項が有効になるステップです。早いほど、シーンが落ち着く前から"
       "スプラットを面に留めます。"),
    ZH_HANS("局部 PCA 项从哪一步开始生效。越早，就越会在场景稳定下来之前把泼溅固定在"
            "表面上。"),
    ZH_HANT("局部 PCA 項從哪一步開始生效。越早，就越會在場景穩定下來之前把潑濺固定在"
            "表面上。"),
    KO("로컬 PCA 항이 켜지는 스텝입니다. 이를수록 장면이 자리 잡기 전부터 "
       "스플랫을 표면에 붙잡아 둡니다."),
    DE("Der Schritt, ab dem die Terme der lokalen PCA wirken. Früher heftet "
       "Splats an Flächen, bevor sich die Szene gesetzt hat."),
    FR("L'étape à laquelle les termes de la PCA locale s'activent. Plus tôt fixe "
       "les splats aux surfaces avant que la scène se soit stabilisée."),
    ES("El paso en que se activan los términos de la PCA local. Antes fija los "
       "splats a las superficies antes de que la escena se haya asentado."),
    PT("O passo em que os termos da PCA local entram em ação. Mais cedo prende "
       "os splats às superfícies antes de a cena se assentar."),
    IT("Il passo in cui si attivano i termini della PCA locale. Prima fissa gli "
       "splat alle superfici prima che la scena si sia assestata."),
    NL("De stap waarop de termen van de lokale PCA ingaan. Eerder zet splats "
       "vast op oppervlakken voordat de scène tot rust is gekomen."),
    RU("Шаг, с которого включаются члены локального PCA. Раньше — прижимает "
       "сплаты к поверхностям до того, как сцена устоится."),
    TR("Yerel PCA terimlerinin devreye girdiği adım. Daha erken, sahne oturmadan "
       "önce splat'ları yüzeylere sabitler."));

SS_MSG(pca_reg_weight,
    EN("Local-PCA flatness"),
    JA("局所 PCA の平坦さ"),
    ZH_HANS("局部 PCA 平整度"),
    ZH_HANT("局部 PCA 平整度"),
    KO("로컬 PCA 평탄도"),
    DE("Flachheit der lokalen PCA"),
    FR("Planéité de la PCA locale"),
    ES("Planitud de la PCA local"),
    PT("Planicidade da PCA local"),
    IT("Planarità della PCA locale"),
    NL("Vlakheid van lokale PCA"),
    RU("Плоскостность локального PCA"),
    TR("Yerel PCA düzlüğü"));
SS_MSG(pca_reg_weight_help,
    EN("Weight on how thick each group of splats is across its fitted surface, "
       "which pulls their centres onto it and away from floating in front of it."),
    JA("スプラットのまとまりが、当てはめた面に対してどれだけ厚いかへの重みです。"
       "中心をその面へ引き寄せ、面の手前に浮かぶのを防ぎます。"),
    ZH_HANS("对每组泼溅在拟合表面法向上有多厚的权重，它把泼溅中心拉到该表面上，避免悬"
            "浮在表面前方。"),
    ZH_HANT("對每組潑濺在擬合表面法向上有多厚的權重，它把潑濺中心拉到該表面上，避免懸"
            "浮在表面前方。"),
    KO("스플랫 묶음이 맞춘 표면에 대해 얼마나 두꺼운지에 대한 가중치입니다. "
       "중심을 그 표면으로 끌어당겨 표면 앞에 떠 있지 않게 합니다."),
    DE("Gewicht darauf, wie dick jede Gruppe von Splats quer zu ihrer "
       "angepassten Fläche ist; es zieht ihre Mittelpunkte auf die Fläche, statt "
       "sie davor schweben zu lassen."),
    FR("Poids sur l'épaisseur de chaque groupe de splats en travers de sa "
       "surface ajustée, qui attire leurs centres sur celle-ci plutôt que de les "
       "laisser flotter devant."),
    ES("Peso sobre lo grueso que es cada grupo de splats a través de su "
       "superficie ajustada, que atrae sus centros hacia ella en lugar de "
       "dejarlos flotar delante."),
    PT("Peso sobre a espessura de cada grupo de splats através da sua superfície "
       "ajustada, que puxa os centros para ela em vez de deixá-los flutuar à "
       "frente."),
    IT("Peso su quanto è spesso ogni gruppo di splat attraverso la sua "
       "superficie adattata, che ne attira i centri su di essa invece di "
       "lasciarli fluttuare davanti."),
    NL("Gewicht op hoe dik elke groep splats dwars op haar gepaste oppervlak is; "
       "het trekt hun middelpunten naar dat oppervlak in plaats van ze ervoor te "
       "laten zweven."),
    RU("Вес толщины каждой группы сплатов поперёк подогнанной к ней поверхности: "
       "он притягивает их центры к поверхности, не давая висеть перед ней."),
    TR("Her splat grubunun oturtulan yüzeyine dik doğrultuda ne kadar kalın "
       "olduğuna verilen ağırlık; merkezlerini yüzeyin önünde süzülmek yerine "
       "yüzeye çeker."));

SS_MSG(pca_reg_weight_final,
    EN("Final local-PCA flatness"),
    JA("局所 PCA の最終平坦さ"),
    ZH_HANS("局部 PCA 最终平整度"),
    ZH_HANT("局部 PCA 最終平整度"),
    KO("로컬 PCA 최종 평탄도"),
    DE("Endgültige Flachheit der lokalen PCA"),
    FR("Planéité finale de la PCA locale"),
    ES("Planitud final de la PCA local"),
    PT("Planicidade final da PCA local"),
    IT("Planarità finale della PCA locale"),
    NL("Uiteindelijke vlakheid van lokale PCA"),
    RU("Итоговая плоскостность локального PCA"),
    TR("Yerel PCA son düzlüğü"));
SS_MSG(pca_reg_weight_final_help,
    EN("The flatness weight the schedule reaches at the last step. Unset keeps "
       "it at the start value."),
    JA("最後のステップでスケジュールが到達する平坦さの重みです。未設定なら開始値"
       "のままです。"),
    ZH_HANS("调度在最后一步达到的平整度权重。不设置则保持起始值。"),
    ZH_HANT("排程在最後一步達到的平整度權重。不設定則保持起始值。"),
    KO("마지막 스텝에서 스케줄이 도달하는 평탄도 가중치입니다. 설정하지 않으면 "
       "시작 값을 유지합니다."),
    DE("Das Flachheitsgewicht, das der Verlauf im letzten Schritt erreicht. "
       "Nicht gesetzt bleibt es beim Startwert."),
    FR("Le poids de planéité que le calendrier atteint à la dernière étape. Non "
       "défini, il reste à la valeur de départ."),
    ES("El peso de planitud que alcanza la programación en el último paso. Sin "
       "definir, se queda en el valor inicial."),
    PT("O peso de planicidade que o cronograma atinge no último passo. Sem "
       "definir, fica no valor inicial."),
    IT("Il peso di planarità che la pianificazione raggiunge all'ultimo passo. "
       "Se non impostato resta al valore iniziale."),
    NL("Het vlakheidsgewicht dat het verloop bij de laatste stap bereikt. Niet "
       "ingesteld blijft het op de beginwaarde."),
    RU("Вес плоскостности, которого расписание достигает к последнему шагу. Если "
       "не задан, остаётся начальным."),
    TR("Takvimin son adımda ulaştığı düzlük ağırlığı. Ayarlanmazsa başlangıç "
       "değerinde kalır."));

SS_MSG(pca_reg_schedule,
    EN("Local-PCA flatness schedule"),
    JA("局所 PCA の平坦さの推移"),
    ZH_HANS("局部 PCA 平整度调度"),
    ZH_HANT("局部 PCA 平整度排程"),
    KO("로컬 PCA 평탄도 스케줄"),
    DE("Verlauf der Flachheit der lokalen PCA"),
    FR("Calendrier de planéité de la PCA locale"),
    ES("Programación de planitud de la PCA local"),
    PT("Cronograma de planicidade da PCA local"),
    IT("Pianificazione della planarità della PCA locale"),
    NL("Verloop van vlakheid van lokale PCA"),
    RU("Расписание плоскостности локального PCA"),
    TR("Yerel PCA düzlük takvimi"));
SS_MSG(pca_reg_schedule_help,
    EN("How the flatness weight moves from its start value to its final one: "
       "`linear` in equal steps, `exponential` by equal factors, `cosine` slowly "
       "at both ends."),
    JA("平坦さの重みが開始値から最終値へどう移るかです。`linear` は等しい幅で、`"
       "exponential` は等しい倍率で、`cosine` は両端でゆっくり変わります。"),
    ZH_HANS("平整度权重如何从起始值变到最终值：`linear` 按相等的增量，`exponential` "
            "按相等的倍数，`cosine` 在两端变化较慢。"),
    ZH_HANT("平整度權重如何從起始值變到最終值：`linear` 按相等的增量，`exponential` "
            "按相等的倍數，`cosine` 在兩端變化較慢。"),
    KO("평탄도 가중치가 시작 값에서 최종 값으로 어떻게 바뀌는지입니다. "
       "`linear`는 같은 폭으로, `exponential`은 같은 배율로, `cosine`은 양 "
       "끝에서 천천히 바뀝니다."),
    DE("Wie das Flachheitsgewicht vom Start- zum Endwert wandert: `linear` in "
       "gleichen Schritten, `exponential` um gleiche Faktoren, `cosine` an "
       "beiden Enden langsam."),
    FR("Comment le poids de planéité passe de sa valeur de départ à sa valeur "
       "finale : `linear` par pas égaux, `exponential` par facteurs égaux, "
       "`cosine` lentement aux deux bouts."),
    ES("Cómo pasa el peso de planitud de su valor inicial al final: `linear` en "
       "pasos iguales, `exponential` por factores iguales, `cosine` despacio en "
       "ambos extremos."),
    PT("Como o peso de planicidade vai do valor inicial ao final: `linear` em "
       "passos iguais, `exponential` por fatores iguais, `cosine` devagar nas "
       "duas pontas."),
    IT("Come il peso di planarità passa dal valore iniziale a quello finale: "
       "`linear` a passi uguali, `exponential` per fattori uguali, `cosine` "
       "lentamente alle due estremità."),
    NL("Hoe het vlakheidsgewicht van zijn beginwaarde naar zijn eindwaarde gaat: "
       "`linear` in gelijke stappen, `exponential` met gelijke factoren, "
       "`cosine` langzaam aan beide uiteinden."),
    RU("Как вес плоскостности переходит от начального значения к итоговому: "
       "`linear` — равными шагами, `exponential` — равными множителями, `cosine` "
       "— медленно на обоих концах."),
    TR("Düzlük ağırlığının başlangıç değerinden son değerine nasıl geçtiği: "
       "`linear` eşit adımlarla, `exponential` eşit çarpanlarla, `cosine` iki "
       "uçta yavaşça."));

SS_MSG(pca_reg_tangent_weight,
    EN("Local-PCA even spread"),
    JA("局所 PCA の均等な広がり"),
    ZH_HANS("局部 PCA 均匀铺展"),
    ZH_HANT("局部 PCA 均勻鋪展"),
    KO("로컬 PCA 고른 퍼짐"),
    DE("Gleichmäßige Verteilung der lokalen PCA"),
    FR("Étalement régulier de la PCA locale"),
    ES("Reparto uniforme de la PCA local"),
    PT("Distribuição uniforme da PCA local"),
    IT("Distribuzione uniforme della PCA locale"),
    NL("Gelijkmatige spreiding van lokale PCA"),
    RU("Равномерность локального PCA"),
    TR("Yerel PCA düzgün yayılımı"));
SS_MSG(pca_reg_tangent_weight_help,
    EN("Weight relative to flatness for unequal spread along the surface’s two axes. Higher spreads splats evenly instead of forming strands."),
    JA("表面の2方向への広がりの不均一さを罰する重み（平面性の重みに対する割合）。高いほど筋状ではなく均一に広がります。"),
    ZH_HANS("沿表面两个轴分布不均的惩罚权重，以平坦度权重的比例表示。越高越促使泼溅均匀分布而非形成条带。"),
    ZH_HANT("沿表面兩個軸分布不均的懲罰權重，以平坦度權重的比例表示。越高越促使潑濺均勻分布而非形成條帶。"),
    KO("표면 두 축의 불균등한 분포에 대한 가중치로, 평탄도 가중치의 비율입니다. 높으면 줄 대신 고르게 퍼집니다."),
    DE("Gewicht relativ zur Flachheit für ungleiche Verteilung in den zwei Oberflächenachsen. Höher verteilt Splats gleichmäßig statt in Strängen."),
    FR("Poids relatif à la planéité pour une répartition inégale sur les deux axes de surface. Plus haut répartit les splats au lieu de former des filaments."),
    ES("Peso relativo a planitud para dispersión desigual en los dos ejes de superficie. Mayor distribuye splats uniformemente en vez de hebras."),
    PT("Peso relativo à planicidade para dispersão desigual nos dois eixos da superfície. Maior espalha splats uniformemente em vez de fios."),
    IT("Peso relativo alla planarità per la dispersione disuguale sui due assi della superficie. Più alto distribuisce gli splat invece di formare filamenti."),
    NL("Gewicht ten opzichte van vlakheid voor ongelijke spreiding over de twee oppervlakassen. Hoger verdeelt splats gelijkmatig in plaats van in slierten."),
    RU("Вес неравномерности по двум осям поверхности относительно веса плоскостности. Выше — сплаты распределяются ровнее, а не нитями."),
    TR("Yüzeyin iki ekseninde eşitsiz yayılımın, düzlük ağırlığına göre ağırlığı. Yüksek değer splatları iplik yerine eşit yayar."));

SS_MSG(pca_reg_normal_weight,
    EN("Local-PCA normal alignment"),
    JA("局所 PCA の法線の揃え"),
    ZH_HANS("局部 PCA 法线对齐"),
    ZH_HANT("局部 PCA 法線對齊"),
    KO("로컬 PCA 법선 정렬"),
    DE("Normalenausrichtung der lokalen PCA"),
    FR("Alignement des normales de la PCA locale"),
    ES("Alineación de normales de la PCA local"),
    PT("Alinhamento das normais da PCA local"),
    IT("Allineamento delle normali della PCA locale"),
    NL("Normaaluitlijning van lokale PCA"),
    RU("Выравнивание нормалей локального PCA"),
    TR("Yerel PCA normal hizalaması"));
SS_MSG(pca_reg_normal_weight_help,
    EN("Weight for aligning each splat’s thinnest axis with its group’s surface normal, so splats lie flat on the surface."),
    JA("スプラットの最薄軸をグループの表面法線に揃える重み。スプラットが表面に沿って平らになります。"),
    ZH_HANS("将每个泼溅的最薄轴对齐到组表面法线的权重，使泼溅平贴表面。"),
    ZH_HANT("將每個潑濺的最薄軸對齊到組表面法線的權重，使潑濺平貼表面。"),
    KO("스플랫의 가장 얇은 축을 그룹 표면 법선에 맞추는 가중치입니다. 스플랫이 표면에 평평하게 놓이게 합니다."),
    DE("Gewicht zum Ausrichten der dünnsten Splat-Achse an der Oberflächennormale der Gruppe, damit Splats flach aufliegen."),
    FR("Poids d’alignement de l’axe le plus fin du splat sur la normale de surface du groupe, pour l’aplatir sur sa surface."),
    ES("Peso para alinear el eje más fino del splat con la normal de superficie del grupo, dejándolo plano en su superficie."),
    PT("Peso para alinhar o eixo mais fino do splat à normal de superfície do grupo, deixando-o plano sobre a superfície."),
    IT("Peso per allineare l’asse più sottile dello splat alla normale di superficie del gruppo, per appiattirlo sulla superficie."),
    NL("Gewicht voor uitlijning van de dunste splatas op de oppervlaktenormaal van de groep, zodat splats plat op het oppervlak liggen."),
    RU("Вес выравнивания тончайшей оси сплата по нормали поверхности группы, чтобы сплаты лежали плоско на поверхности."),
    TR("Splatın en ince eksenini grup yüzey normaline hizalama ağırlığı; splatların yüzeyde düz durmasını sağlar."));

SS_MSG(pca_reg_surfel_weight,
    EN("Local-PCA disc shape"),
    JA("局所 PCA の円盤化"),
    ZH_HANS("局部 PCA 圆盘化"),
    ZH_HANT("局部 PCA 圓盤化"),
    KO("로컬 PCA 원반화"),
    DE("Scheibenform der lokalen PCA"),
    FR("Forme de disque de la PCA locale"),
    ES("Forma de disco de la PCA local"),
    PT("Forma de disco da PCA local"),
    IT("Forma a disco della PCA locale"),
    NL("Schijfvorm van lokale PCA"),
    RU("Дисковая форма локального PCA"),
    TR("Yerel PCA disk biçimi"));
SS_MSG(pca_reg_surfel_weight_help,
    EN("Weight on each splat's smallest size, which flattens it into a disc."),
    JA("各スプラットの最も小さい大きさへの重みで、スプラットを円盤状に平らにしま"
       "す。"),
    ZH_HANS("对每个泼溅最小尺寸的权重，把泼溅压扁成圆盘。"),
    ZH_HANT("對每個潑濺最小尺寸的權重，把潑濺壓扁成圓盤。"),
    KO("각 스플랫의 가장 작은 크기에 대한 가중치로, 스플랫을 원반 모양으로 "
       "납작하게 만듭니다."),
    DE("Gewicht auf die kleinste Ausdehnung jedes Splats, das ihn zu einer "
       "Scheibe abflacht."),
    FR("Poids sur la plus petite taille de chaque splat, qui l'aplatit en "
       "disque."),
    ES("Peso sobre el tamaño más pequeño de cada splat, que lo aplana en un "
       "disco."),
    PT("Peso sobre o menor tamanho de cada splat, que o achata num disco."),
    IT("Peso sulla dimensione più piccola di ogni splat, che lo appiattisce in "
       "un disco."),
    NL("Gewicht op de kleinste afmeting van elke splat, waardoor die tot een "
       "schijf afplat."),
    RU("Вес наименьшего размера каждого сплата, который сплющивает его в диск."),
    TR("Her splat'ın en küçük boyutuna verilen ağırlık; splat'ı bir diske "
       "yassılaştırır."));
SS_MSG(pca_reg_erank_weight,
    EN("Local covariance rank weight"), JA("局所共分散ランクの重み"),
    ZH_HANS("局部协方差秩权重"), ZH_HANT("局部共變異數秩權重"), KO("로컬 공분산 순위 가중치"),
    DE("Gewicht des lokalen Kovarianzrangs"), FR("Poids du rang de covariance locale"),
    ES("Peso del rango de covarianza local"), PT("Peso do posto da covariância local"),
    IT("Peso del rango della covarianza locale"), NL("Gewicht lokale covariantierang"),
    RU("Вес ранга локальной ковариации"), TR("Yerel kovaryans rank ağırlığı"));
SS_MSG(pca_reg_erank_weight_help,
    EN("Weight of max(erank(C)-2, 0)², using covariance eigenvalue entropy independently of scene scale. Starts at pca_reg_start_iter; 0 disables."),
    JA("シーン尺度に依存しない共分散固有値エントロピーによる max(erank(C)-2, 0)² の重み。pca_reg_start_iter から有効。0で無効です。"),
    ZH_HANS("max(erank(C)-2, 0)² 的权重，使用与场景尺度无关的协方差特征值熵。从 pca_reg_start_iter 启用；0 关闭。"),
    ZH_HANT("max(erank(C)-2, 0)² 的權重，使用與場景尺度無關的共變異數特徵值熵。從 pca_reg_start_iter 啟用；0 關閉。"),
    KO("장면 배율과 무관한 공분산 고윳값 엔트로피 기반 max(erank(C)-2, 0)²의 가중치입니다. pca_reg_start_iter부터 적용하며 0은 끄기입니다."),
    DE("Gewicht von max(erank(C)-2, 0)² über Kovarianz-Eigenwertentropie, unabhängig vom Szenenmaßstab. Ab pca_reg_start_iter; 0 schaltet ab."),
    FR("Poids de max(erank(C)-2, 0)² via l’entropie des valeurs propres, sans effet d’échelle. Actif dès pca_reg_start_iter ; 0 désactive."),
    ES("Peso de max(erank(C)-2, 0)² mediante entropía de autovalores, independiente de escala. Desde pca_reg_start_iter; 0 desactiva."),
    PT("Peso de max(erank(C)-2, 0)² por entropia dos autovalores, independente da escala. A partir de pca_reg_start_iter; 0 desativa."),
    IT("Peso di max(erank(C)-2, 0)² tramite entropia degli autovalori, indipendente dalla scala. Da pca_reg_start_iter; 0 disattiva."),
    NL("Gewicht van max(erank(C)-2, 0)² via entropie van eigenwaarden, onafhankelijk van schaal. Vanaf pca_reg_start_iter; 0 schakelt uit."),
    RU("Вес max(erank(C)-2, 0)² по энтропии собственных значений ковариации, без влияния масштаба. С pca_reg_start_iter; 0 отключает."),
    TR("Ölçekten bağımsız kovaryans özdeğer entropisiyle max(erank(C)-2, 0)² ağırlığı. pca_reg_start_iter’de başlar; 0 kapatır."));
SS_MSG(pca_reg_transparency_weight,
    EN("Normal transparency weight"), JA("法線方向の透明度の重み"),
    ZH_HANS("法线方向透明度权重"), ZH_HANT("法線方向透明度權重"), KO("법선 방향 투명도 가중치"),
    DE("Gewicht der Transparenz in Normalenrichtung"), FR("Poids de transparence suivant la normale"),
    ES("Peso de transparencia en dirección normal"), PT("Peso de transparência na direção normal"),
    IT("Peso della trasparenza lungo la normale"), NL("Gewicht transparantie langs de normaal"),
    RU("Вес прозрачности вдоль нормали"), TR("Normal yönündeki saydamlık ağırlığı"));
SS_MSG(pca_reg_transparency_weight_help,
    EN("Weight on remaining transmittance after blending the neighbourhood along its PCA normal through each center. Enabled from pca_reg_start_iter; 0 disables it."),
    JA("各中心を通る PCA 法線に沿って近傍を合成した後の透過率の重み。pca_reg_start_iter から有効。0 で無効。"),
    ZH_HANS("沿穿过每个中心的 PCA 法线混合邻域后，剩余透射率的权重。从 pca_reg_start_iter 开始启用；0 禁用。"),
    ZH_HANT("沿穿過每個中心的 PCA 法線混合鄰域後，剩餘透射率的權重。從 pca_reg_start_iter 開始啟用；0 停用。"),
    KO("각 중심을 통과하는 PCA 법선을 따라 이웃을 합성한 후 남는 투과율의 가중치. pca_reg_start_iter부터 활성화되며 0이면 비활성화됩니다."),
    DE("Gewicht der verbleibenden Transmission nach dem Mischen der Nachbarschaft entlang ihrer PCA-Normalen durch jedes Zentrum. Ab pca_reg_start_iter aktiv; 0 deaktiviert."),
    FR("Poids de la transmittance restante après composition du voisinage suivant sa normale PCA passant par chaque centre. Actif dès pca_reg_start_iter ; 0 désactive."),
    ES("Peso de la transmitancia restante tras componer el vecindario a lo largo de su normal PCA por cada centro. Activo desde pca_reg_start_iter; 0 lo desactiva."),
    PT("Peso da transmitância restante após compor a vizinhança ao longo da normal PCA por cada centro. Ativo desde pca_reg_start_iter; 0 desativa."),
    IT("Peso della trasmittanza residua dopo la composizione del vicinato lungo la normale PCA attraverso ogni centro. Attivo da pca_reg_start_iter; 0 disattiva."),
    NL("Gewicht op de resterende transmissie na compositie van de buurt langs de PCA-normaal door elk centrum. Actief vanaf pca_reg_start_iter; 0 schakelt uit."),
    RU("Вес оставшегося пропускания после смешивания окрестности вдоль нормали PCA через каждый центр. Включается с pca_reg_start_iter; 0 отключает."),
    TR("Her merkezden geçen PCA normali boyunca komşuluk harmanlandıktan sonra kalan geçirgenliğin ağırlığı. pca_reg_start_iter ile başlar; 0 devre dışı bırakır."));
SS_MSG(pca_reg_covariance,
    EN("Local covariance model"), JA("局所共分散モデル"),
    ZH_HANS("局部协方差模型"), ZH_HANT("局部共變異數模型"), KO("로컬 공분산 모델"),
    DE("Lokales Kovarianzmodell"), FR("Modèle de covariance locale"),
    ES("Modelo de covarianza local"), PT("Modelo de covariância local"),
    IT("Modello di covarianza locale"), NL("Lokaal covariantiemodel"),
    RU("Модель локальной ковариации"), TR("Yerel kovaryans modeli"));
SS_MSG(pca_reg_covariance_help,
    EN("- centers: equal-weight centers (PCAsplat).\n- opacity-centers: opacity-weighted centers.\n- gaussian-mixture: opacity-weighted centers + covariances. Weights normalized and differentiated."),
    JA("- centers：均等重みの中心（PCAsplat）。\n- opacity-centers：不透明度で重み付けした中心。\n- gaussian-mixture：不透明度加重中心＋共分散。重みは正規化・微分します。"),
    ZH_HANS("- centers：等权中心（PCAsplat）。\n- opacity-centers：按不透明度加权的中心。\n- gaussian-mixture：不透明度加权中心＋协方差。权重归一化并求导。"),
    ZH_HANT("- centers：等權中心（PCAsplat）。\n- opacity-centers：按不透明度加權的中心。\n- gaussian-mixture：不透明度加權中心＋共變異數。權重正規化並微分。"),
    KO("- centers: 동일 가중치 중심(PCAsplat).\n- opacity-centers: 불투명도 가중 중심.\n- gaussian-mixture: 불투명도 가중 중심 + 공분산. 가중치는 정규화 및 미분됩니다."),
    DE("- centers: gleichgewichtete Zentren (PCAsplat).\n- opacity-centers: opazitätsgewichtete Zentren.\n- gaussian-mixture: opazitätsgewichtete Zentren + Kovarianzen. Gewichte normiert und differenziert."),
    FR("- centers : centres de poids égaux (PCAsplat).\n- opacity-centers : centres pondérés par opacité.\n- gaussian-mixture : centres pondérés par opacité + covariances. Poids normalisés et différenciés."),
    ES("- centers: centros de peso igual (PCAsplat).\n- opacity-centers: centros ponderados por opacidad.\n- gaussian-mixture: centros por opacidad + covarianzas. Pesos normalizados y diferenciados."),
    PT("- centers: centros de peso igual (PCAsplat).\n- opacity-centers: centros ponderados por opacidade.\n- gaussian-mixture: centros por opacidade + covariâncias. Pesos normalizados e diferenciados."),
    IT("- centers: centri di peso uguale (PCAsplat).\n- opacity-centers: centri pesati per opacità.\n- gaussian-mixture: centri pesati per opacità + covarianze. Pesi normalizzati e differenziati."),
    NL("- centers: gelijkgewogen centra (PCAsplat).\n- opacity-centers: centra gewogen op dekking.\n- gaussian-mixture: dekkinggewogen centra + covarianties. Gewichten genormaliseerd en gedifferentieerd."),
    RU("- centers: центры с равными весами (PCAsplat).\n- opacity-centers: центры с весом по непрозрачности.\n- gaussian-mixture: центры с весом непрозрачности + ковариации. Веса нормированы и дифференцируются."),
    TR("- centers: eşit ağırlıklı merkezler (PCAsplat).\n- opacity-centers: opaklık ağırlıklı merkezler.\n- gaussian-mixture: opaklık ağırlıklı merkezler + kovaryanslar. Ağırlıklar normalize ve türevlenir."));

SS_MSG(pca_reg_scale_agnostic,
    EN("Scale-agnostic local PCA"),
    JA("スケール非依存の局所 PCA"),
    ZH_HANS("与尺度无关的局部 PCA"),
    ZH_HANT("與尺度無關的局部 PCA"),
    KO("스케일 무관 로컬 PCA"),
    DE("Skalenunabhängige lokale PCA"),
    FR("PCA locale indépendante de l'échelle"),
    ES("PCA local independiente de la escala"),
    PT("PCA local independente da escala"),
    IT("PCA locale indipendente dalla scala"),
    NL("Schaalonafhankelijke lokale PCA"),
    RU("Локальный PCA, не зависящий от масштаба"),
    TR("Ölçekten bağımsız yerel PCA"));
SS_MSG(pca_reg_scale_agnostic_help,
    EN("Normalizes flatness by each neighborhood’s spread and disc shape by each splat’s largest size, so weights work at any scene scale."),
    JA("平面性を近傍の広がりで、円盤形状をスプラットの最大寸法で正規化します。同じ重みをどのシーン尺度でも使えます。"),
    ZH_HANS("按各邻域的分布范围归一化平坦度，按各泼溅的最大尺寸归一化圆盘形状，让同一权重适用于任何场景尺度。"),
    ZH_HANT("按各鄰域的分布範圍正規化平坦度，按各潑濺的最大尺寸正規化圓盤形狀，讓同一權重適用於任何場景尺度。"),
    KO("평탄도를 이웃 분포 폭으로, 원반 모양을 각 스플랫의 최대 크기로 정규화해 같은 가중치가 모든 장면 배율에서 작동하게 합니다."),
    DE("Normiert Flachheit an der Nachbarschaftsstreuung und Scheibenform an der größten Splat-Ausdehnung, für gleiche Wirkung bei jedem Szenenmaßstab."),
    FR("Normalise la planéité par dispersion du voisinage et la forme de disque par taille maximale du splat, pour des poids valables à toute échelle."),
    ES("Normaliza planitud por dispersión vecinal y forma de disco por tamaño máximo del splat, para usar los mismos pesos a cualquier escala."),
    PT("Normaliza planicidade pela dispersão vizinha e formato de disco pelo maior tamanho do splat, para pesos funcionarem em qualquer escala."),
    IT("Normalizza planarità per dispersione del vicinato e forma a disco per dimensione massima dello splat, per pesi validi a ogni scala."),
    NL("Normaliseert vlakheid op buurspreiding en schijfvorm op de grootste splatmaat, zodat gewichten op elke scèneschaal werken."),
    RU("Нормирует плоскостность разбросом соседей, а форму диска — наибольшим размером сплата, сохраняя действие весов при любом масштабе."),
    TR("Düzlüğü komşuluk yayılımıyla, disk şeklini splatın en büyük boyutuyla normalize eder; ağırlıklar her sahne ölçeğinde aynı çalışır."));


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

SS_MSG(reg_rendered_only,
    EN("Regularize rendered splats only"),
    JA("描画されたスプラットだけを正則化"),
    ZH_HANS("只正则化被渲染的泼溅"),
    ZH_HANT("只正則化被算圖的潑濺"),
    KO("렌더된 스플랫만 정규화"),
    DE("Nur gerenderte Splats regularisieren"),
    FR("Régulariser seulement les splats rendus"),
    ES("Regularizar solo los splats renderizados"),
    PT("Regularizar só os splats renderizados"),
    IT("Regolarizzare solo gli splat renderizzati"),
    NL("Alleen gerenderde splats regulariseren"),
    RU("Регуляризовать только отрисованные сплаты"),
    TR("Yalnızca işlenen splat'ları düzenlileştir"));
SS_MSG(reg_rendered_only_help,
    EN("Applies opacity, scale and shape penalties only to splats rendered this step.\nOff also shrinks unseen splats, potentially killing new splats before their first render."),
    JA("このステップで描画したスプラットだけに不透明度・寸法・形状の罰則を適用します。\n無効では未描画のものも縮み、新しいスプラットが初描画前に消える恐れがあります。"),
    ZH_HANS("仅对本步渲染过的泼溅应用不透明度、尺度和形状惩罚。\n关闭时也会缩小未见的泼溅，可能在新点首次渲染前将其消除。"),
    ZH_HANT("僅對本步算繪過的潑濺套用不透明度、尺度和形狀懲罰。\n關閉時也會縮小未見的潑濺，可能在新點首次算繪前將其消除。"),
    KO("이번 스텝에 렌더된 스플랫에만 불투명도, 크기, 모양 규제를 적용합니다.\n끄면 미관측 스플랫도 줄어들어 새 스플랫이 첫 렌더 전에 사라질 수 있습니다."),
    DE("Opazitäts-, Größen- und Formstrafen nur für in diesem Schritt gerenderte Splats.\nAus schrumpft auch ungesehene Splats; neue können vor ihrem ersten Render sterben."),
    FR("Pénalités d’opacité, taille et forme limitées aux splats rendus à ce pas.\nDésactivé réduit aussi les splats non vus, pouvant tuer les nouveaux avant leur premier rendu."),
    ES("Penaliza opacidad, escala y forma solo de splats renderizados en este paso.\nDesactivado reduce también los no vistos; los nuevos pueden morir antes de su primer render."),
    PT("Penaliza opacidade, escala e forma só de splats renderizados neste passo.\nDesativado reduz também os não vistos; novos podem morrer antes do primeiro render."),
    IT("Penalità di opacità, scala e forma solo sugli splat renderizzati in questo passo.\nDisattivato riduce anche quelli non visti, rischiando di eliminare i nuovi prima del primo render."),
    NL("Dekking-, schaal- en vormstraffen alleen voor splats die deze stap gerenderd zijn.\nUit verkleint ook ongeziene splats; nieuwe kunnen vóór hun eerste render verdwijnen."),
    RU("Штрафы непрозрачности, масштаба и формы лишь для сплатов, показанных на этом шаге.\nВыкл. уменьшает и невидимые сплаты: новые могут погибнуть до первого рендера."),
    TR("Opaklık, ölçek ve şekil cezaları yalnız bu adımda render edilen splatlara uygulanır.\nKapalıyken görünmeyenler de küçülür; yeniler ilk renderden önce ölebilir."));

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

SS_MSG(opacity_reg_decay_power,
    EN("Opacity penalty decay power"), JA("不透明度ペナルティの減衰指数"),
    ZH_HANS("不透明度惩罚衰减幂"), ZH_HANT("不透明度懲罰衰減冪"),
    KO("불투명도 페널티 감쇠 지수"),
    DE("Abklingexponent der Deckkraftstrafe"),
    FR("Exposant de décroissance de la pénalité d'opacité"),
    ES("Exponente de decaimiento de la penalización de opacidad"),
    PT("Expoente de decaimento da penalidade de opacidade"),
    IT("Esponente di decadimento della penalità di opacità"),
    NL("Vervalexponent van de dekkingsstraf"),
    RU("Показатель затухания штрафа за непрозрачность"),
    TR("Saydamsızlık cezası sönümleme üssü"));
SS_MSG(opacity_reg_decay_power_help,
    EN("Fades the opacity penalty as (1-t)^p, with t the fraction of steps done, preserving its run average. 0 keeps it constant."),
    JA("不透明度の罰則を (1-t)^p で弱めます。tは実行済みステップの割合。実行全体の平均は保ちます。0は一定です。"),
    ZH_HANS("按 (1-t)^p 衰减不透明度惩罚，t 为已完成步数比例，保持整个训练的平均惩罚不变。0 表示恒定。"),
    ZH_HANT("按 (1-t)^p 衰減不透明度懲罰，t 為已完成步數比例，保持整個訓練的平均懲罰不變。0 表示恆定。"),
    KO("불투명도 규제를 (1-t)^p로 줄입니다. t는 완료 스텝 비율이며 실행 평균은 유지됩니다. 0은 일정하게 유지합니다."),
    DE("Senkt die Opazitätsstrafe mit (1-t)^p; t ist der erledigte Schrittanteil. Der Laufmittelwert bleibt gleich. 0 hält sie konstant."),
    FR("Réduit la pénalité de opacité selon (1-t)^p, t étant la fraction des pas faits, en gardant la moyenne du parcours. 0 la garde constante."),
    ES("Reduce la penalización de opacidad como (1-t)^p, t la fracción de pasos hechos, manteniendo su media total. 0 la mantiene constante."),
    PT("Reduz a penalidade de opacidade como (1-t)^p, t a fração de passos feitos, mantendo a média total. 0 a mantém constante."),
    IT("Riduce la penalità di opacità come (1-t)^p, t la frazione di passi svolti, mantenendo la media totale. 0 la mantiene costante."),
    NL("Verlaagt de dekkingstraf met (1-t)^p; t is het voltooide stappenaandeel. Het rungemiddelde blijft gelijk. 0 houdt de straf constant."),
    RU("Снижает штраф непрозрачности по (1-t)^p, где t — доля выполненных шагов, сохраняя средний штраф за запуск. 0 — постоянный штраф."),
    TR("opaklık cezasını (1-t)^p ile azaltır; t tamamlanan adım oranıdır. Çalışma ortalaması korunur. 0 sabit tutar."));

SS_MSG(scale_reg_decay_power,
    EN("Size penalty decay power"), JA("大きさペナルティの減衰指数"),
    ZH_HANS("尺寸惩罚衰减幂"), ZH_HANT("尺寸懲罰衰減冪"),
    KO("크기 페널티 감쇠 지수"), DE("Abklingexponent der Größenstrafe"),
    FR("Exposant de décroissance de la pénalité de taille"),
    ES("Exponente de decaimiento de la penalización de tamaño"),
    PT("Expoente de decaimento da penalidade de tamanho"),
    IT("Esponente di decadimento della penalità di dimensione"),
    NL("Vervalexponent van de groottestraf"),
    RU("Показатель затухания штрафа за размер"),
    TR("Boyut cezası sönümleme üssü"));
SS_MSG(scale_reg_decay_power_help,
    EN("Fades the size penalty as (1-t)^p, with t the fraction of steps done, preserving its run average. 0 keeps it constant."),
    JA("サイズの罰則を (1-t)^p で弱めます。tは実行済みステップの割合。実行全体の平均は保ちます。0は一定です。"),
    ZH_HANS("按 (1-t)^p 衰减尺寸惩罚，t 为已完成步数比例，保持整个训练的平均惩罚不变。0 表示恒定。"),
    ZH_HANT("按 (1-t)^p 衰減尺寸懲罰，t 為已完成步數比例，保持整個訓練的平均懲罰不變。0 表示恆定。"),
    KO("크기 규제를 (1-t)^p로 줄입니다. t는 완료 스텝 비율이며 실행 평균은 유지됩니다. 0은 일정하게 유지합니다."),
    DE("Senkt die Größenstrafe mit (1-t)^p; t ist der erledigte Schrittanteil. Der Laufmittelwert bleibt gleich. 0 hält sie konstant."),
    FR("Réduit la pénalité de taille selon (1-t)^p, t étant la fraction des pas faits, en gardant la moyenne du parcours. 0 la garde constante."),
    ES("Reduce la penalización de tamaño como (1-t)^p, t la fracción de pasos hechos, manteniendo su media total. 0 la mantiene constante."),
    PT("Reduz a penalidade de tamanho como (1-t)^p, t a fração de passos feitos, mantendo a média total. 0 a mantém constante."),
    IT("Riduce la penalità di dimensione come (1-t)^p, t la frazione di passi svolti, mantenendo la media totale. 0 la mantiene costante."),
    NL("Verlaagt de groottestraf met (1-t)^p; t is het voltooide stappenaandeel. Het rungemiddelde blijft gelijk. 0 houdt de straf constant."),
    RU("Снижает штраф размера по (1-t)^p, где t — доля выполненных шагов, сохраняя средний штраф за запуск. 0 — постоянный штраф."),
    TR("boyut cezasını (1-t)^p ile azaltır; t tamamlanan adım oranıdır. Çalışma ortalaması korunur. 0 sabit tutar."));

SS_MSG(opacity_decay,
    EN("Opacity decay"), JA("不透明度の減衰"), ZH_HANS("不透明度衰减"),
    ZH_HANT("不透明度衰減"), KO("불투명도 감쇠"),
    DE("Abklingen der Deckkraft"), FR("Décroissance de l'opacité"),
    ES("Decaimiento de la opacidad"), PT("Decaimento da opacidade"),
    IT("Decadimento dell'opacità"), NL("Afname van de dekking"),
    RU("Затухание непрозрачности"), TR("Saydamsızlık sönümü"));
SS_MSG(opacity_decay_help,
    EN("Fade every splat's opacity slightly each step, freeing weak ones for reuse.\nAn alternative to opacity_reg that acts on all splats equally."),
    JA("すべてのスプラットの不透明度を毎ステップ少しずつ薄め、弱いものを再利用に回します。\nopacity_reg の代わりに使え、すべてのスプラットに等しく効きます。"),
    ZH_HANS("每一步都把所有泼溅的不透明度稍稍淡化，把弱的释放出来重新利用。\n可作为 opacity_reg 的替代，对所有泼溅一视同仁。"),
    ZH_HANT("每一步都把所有潑濺的不透明度稍稍淡化，把弱的釋放出來重新利用。\n可作為 opacity_reg 的替代，對所有潑濺一視同仁。"),
    KO("매 스텝마다 모든 스플랫의 불투명도를 조금씩 낮춰 약한 것을 재사용으로 돌립니다.\nopacity_reg의 대안이며 모든 스플랫에 똑같이 작용합니다."),
    DE("Verringert bei jedem Schritt die Opazität aller Splats, damit schwache wiederverwendet werden.\nAlternative zu opacity_reg, die alle Splats gleich behandelt."),
    FR("Estomper légèrement l'opacité de chaque splat à chaque étape, libérant les plus faibles pour réemploi.\nUne solution de rechange à opacity_reg, qui agit sur tous les splats de la même façon."),
    ES("Atenuar un poco la opacidad de cada splat en cada paso, liberando los débiles para reutilizarlos.\nUna alternativa a opacity_reg que actúa igual sobre todos los splats."),
    PT("Esmaecer um pouco a opacidade de cada splat a cada passo, liberando os fracos para reutilização.\nUma alternativa ao opacity_reg que age igualmente sobre todos os splats."),
    IT("Sbiadire un poco l'opacità di ogni splat a ogni passo, liberando i più deboli per il riuso.\nUn'alternativa a opacity_reg che agisce allo stesso modo su tutti gli splat."),
    NL("Verlaagt elke stap de dekking van alle splats, zodat zwakke kunnen worden hergebruikt.\nAlternatief voor opacity_reg dat alle splats gelijk behandelt."),
    RU("Слегка гасить непрозрачность каждого сплата на каждом шаге, освобождая слабые для повторного использования.\nЗамена opacity_reg, действующая одинаково на все сплаты."),
    TR("Her adımda her splat'ın saydamsızlığını biraz soldurur ve zayıf olanları yeniden kullanıma açar.\nopacity_reg'e alternatiftir ve tüm splat'lara eşit davranır."));

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
    EN("Keeps view-independent splat color within 0–1. Higher cleans surfaces; too high clips real brightness range, so lower it when needed."),
    JA("視点によらないスプラット色を0–1に保ちます。高いほど表面がきれいになりますが、実際の明暗幅を削る場合は下げます。"),
    ZH_HANS("将与视角无关的泼溅颜色限制在 0–1。越高表面越干净；过高会截断真实亮度范围，需要时降低。"),
    ZH_HANT("將與視角無關的潑濺顏色限制在 0–1。越高表面越乾淨；過高會截斷真實亮度範圍，需要時降低。"),
    KO("시점과 무관한 스플랫 색을 0–1로 유지합니다. 높으면 표면이 깨끗하지만 너무 높으면 실제 밝기 범위를 잘라내므로 필요 시 낮추세요."),
    DE("Hält blickunabhängige Splat-Farbe in 0–1. Höher säubert Oberflächen; zu hoch beschneidet den echten Helligkeitsbereich, dann senken."),
    FR("Maintient la couleur sans effet de vue dans 0–1. Plus haut nettoie les surfaces ; trop haut coupe la plage réelle de luminosité : réduire au besoin."),
    ES("Mantiene el color independiente de la vista en 0–1. Mayor limpia superficies; demasiado recorta el rango real de brillo: bájalo si hace falta."),
    PT("Mantém a cor independente da vista em 0–1. Maior limpa superfícies; alto demais corta a faixa real de brilho: reduza se preciso."),
    IT("Mantiene il colore indipendente dalla vista in 0–1. Più alto pulisce le superfici; troppo alto taglia la gamma luminosa reale: ridurre se serve."),
    NL("Houdt kijkonafhankelijke splatkleur binnen 0–1. Hoger geeft schonere oppervlakken; te hoog knipt het echte helderheidsbereik af, verlaag dan."),
    RU("Удерживает цвет сплата без зависимости от ракурса в 0–1. Выше — чище поверхность; слишком высоко обрезает реальный диапазон яркости."),
    TR("Görüşten bağımsız splat rengini 0–1’de tutar. Yüksek yüzeyi temizler; çok yüksek gerçek parlaklık aralığını keser, gerekirse azaltın."));

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
    EN("Limits view-dependent color so per-photo correction handles color drift. Higher improves unseen views; too high flattens real reflections."),
    JA("視点依存色を抑え、色ずれは写真ごとの補正に任せます。高いほど未知のビューが改善しますが、高すぎると本物の反射が平坦になります。"),
    ZH_HANS("限制视角相关颜色，让逐张照片校正处理色偏。较高可改善未见视图；过高会压平真实反射。"),
    ZH_HANT("限制視角相關顏色，讓逐張照片校正處理色偏。較高可改善未見視圖；過高會壓平真實反射。"),
    KO("시점에 따른 색을 제한해 사진별 보정이 색 편향을 처리하게 합니다. 높으면 새 시점이 개선되지만 너무 높으면 실제 반사가 평평해집니다."),
    DE("Begrenzt blickabhängige Farbe, damit Fotokorrekturen Farbdrift übernehmen. Höher verbessert neue Ansichten; zu hoch flacht echte Reflexe ab."),
    FR("Limite la couleur selon la vue pour laisser les corrections photo gérer les dérives. Plus haut améliore les vues inédites ; trop haut aplatit les reflets."),
    ES("Limita color según la vista para dejar la deriva a la corrección por foto. Mayor mejora vistas nuevas; demasiado aplana reflejos reales."),
    PT("Limita cor por vista para deixar desvios à correção por foto. Maior melhora vistas novas; alto demais achata reflexos reais."),
    IT("Limita il colore secondo la vista, lasciando le derive alla correzione per foto. Più alto migliora viste nuove; troppo alto appiattisce riflessi reali."),
    NL("Beperkt kijkafhankelijke kleur zodat fotocorrectie kleurdrift opvangt. Hoger verbetert nieuwe beelden; te hoog vlakt echte reflecties af."),
    RU("Ограничивает цвет по ракурсу, оставляя цветовой сдвиг коррекции фото. Выше — лучше новые виды; слишком высоко сглаживает реальные отражения."),
    TR("Renk kaymasını fotoğraf düzeltmesine bırakarak görüşe bağlı rengi sınırlar. Yüksek yeni görünümleri iyileştirir; çok yüksek gerçek yansımayı düzleştirir."));

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
    EN("Penalize splat colors that fall outside the displayable range.\nKeeps blown-out highlights from hiding inside the splats, which matters when exporting or meshing."),
    JA("表示できる範囲から外れたスプラットの色を罰します。\n白飛びしたハイライトがスプラットの中に隠れないので、書き出しやメッシュ化のときに効いてきます。"),
    ZH_HANS("惩罚落在可显示范围之外的泼溅颜色。\n这样过曝的高光就不会藏在泼溅内部，导出或生成网格时尤其重要。"),
    ZH_HANT("懲罰落在可顯示範圍之外的潑濺顏色。\n這樣過曝的高光就不會藏在潑濺內部，匯出或產生網格時尤其重要。"),
    KO("표시할 수 있는 범위를 벗어난 스플랫 색에 벌을 줍니다.\n날아간 하이라이트가 스플랫 안에 숨지 않으므로 내보내기나 메시 변환에서 중요합니다."),
    DE("Splatfarben bestrafen, die außerhalb des darstellbaren Bereichs liegen.\nSo verstecken sich ausgefressene Lichter nicht in den Splats, was beim Export oder bei der Netzerzeugung zählt."),
    FR("Pénaliser les couleurs de splat qui sortent de la plage affichable.\nÉvite que des hautes lumières cramées se cachent dans les splats, ce qui compte à l'export ou au maillage."),
    ES("Penalizar los colores de splat que caen fuera del rango mostrable.\nEvita que las altas luces quemadas se escondan dentro de los splats, algo que importa al exportar o mallar."),
    PT("Penalizar as cores de splat que ficam fora do intervalo exibível.\nEvita que altas luzes estouradas se escondam dentro dos splats, o que importa ao exportar ou gerar malha."),
    IT("Penalizzare i colori degli splat che escono dall'intervallo visualizzabile.\nEvita che le alte luci bruciate si nascondano dentro gli splat, cosa che conta all'esportazione o nella mesh."),
    NL("Splatkleuren bestraffen die buiten het weergeefbare bereik vallen.\nVoorkomt dat overbelichte highlights zich in de splats verstoppen, wat telt bij export of mesh-generatie."),
    RU("Штрафовать цвета сплатов, выходящие за отображаемый диапазон.\nНе даёт выбитым светам прятаться внутри сплатов, что важно при экспорте и построении меша."),
    TR("Görüntülenebilir aralığın dışına düşen splat renklerini cezalandırır.\nYanmış parlak alanların splat'ların içinde saklanmasını önler; dışa aktarırken ya da ağ çıkarırken önemlidir."));

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
    EN("Smooth per-photo color correction for exposure and white-balance drift. Keeps splat colors consistent. Disable for synthetic or already consistent images."),
    JA("写真ごとに露出・白バランスのずれを滑らかに補正し、スプラットの色を揃えます。合成画像や既に色が揃う画像では無効にします。"),
    ZH_HANS("逐图平滑校正曝光和白平衡漂移，保持泼溅颜色一致。合成图像或色彩已一致时关闭。"),
    ZH_HANT("逐圖平滑校正曝光和白平衡漂移，保持潑濺色彩一致。合成影像或色彩已一致時關閉。"),
    KO("사진별 노출·화이트밸런스 변화를 부드럽게 보정해 스플랫 색을 일관되게 합니다. 합성 또는 이미 색이 일정한 이미지에서는 끄세요."),
    DE("Sanfte Farbkorrektur je Foto für Belichtungs- und Weißabgleichsdrift. Hält Splatfarben einheitlich. Für synthetische oder konsistente Bilder deaktivieren."),
    FR("Correction douce par photo des dérives d’exposition et de balance des blancs. Unifie les splats. Désactiver pour images synthétiques ou déjà cohérentes."),
    ES("Corrección suave por foto de exposición y balance de blancos. Mantiene colores consistentes. Desactiva para imágenes sintéticas o ya uniformes."),
    PT("Correção suave por foto de exposição e balanço de brancos. Mantém cores consistentes. Desligue para imagens sintéticas ou já uniformes."),
    IT("Correzione dolce per foto di esposizione e bilanciamento del bianco. Mantiene colori coerenti. Disattivare per immagini sintetiche o già uniformi."),
    NL("Vloeiende kleurcorrectie per foto voor belichting en witbalans. Houdt splatkleuren gelijk. Uit voor synthetische of al consistente beelden."),
    RU("Плавная коррекция экспозиции и баланса белого по снимкам сохраняет единый цвет сплатов. Отключите для синтетических или уже согласованных снимков."),
    TR("Fotoğraf başına pozlama ve beyaz dengesi kaymasını yumuşak düzeltir. Splat renkleri tutarlı kalır. Sentetik veya zaten tutarlı resimlerde kapatın."));

SS_MSG(bilagrid_type,
    EN("Color correction model"), JA("色補正のモデル"),
    ZH_HANS("颜色校正模型"), ZH_HANT("色彩校正模型"), KO("색 보정 모델"),
    DE("Modell der Farbkorrektur"), FR("Modèle de correction des couleurs"),
    ES("Modelo de corrección de color"), PT("Modelo de correção de cor"),
    IT("Modello di correzione del colore"),
    NL("Model van de kleurcorrectie"), RU("Модель цветокоррекции"),
    TR("Renk düzeltme modeli"));
SS_MSG(bilagrid_type_help,
    EN("- `ppisp` adjusts exposure and color gain and shifts hue the least.\n- `affine` is a full color matrix, the most flexible but the most prone to color drift.\n- `loglinear` sits in between."),
    JA("- `ppisp` は露出と色ゲインを調整し、色相のずれがいちばん小さい方式です。\n- `affine` は完全な色行列で、いちばん柔軟ですが色が流れやすい方式です。\n- `loglinear` はその中間です。"),
    ZH_HANS("- `ppisp` 调整曝光和颜色增益，对色相的改动最小\n- `affine` 是完整的颜色矩阵，最灵活但最容易造成色偏\n- `loglinear` 介于两者之间。"),
    ZH_HANT("- `ppisp` 調整曝光和顏色增益，對色相的改動最小\n- `affine` 是完整的顏色矩陣，最靈活但最容易造成色偏\n- `loglinear` 介於兩者之間。"),
    KO("- ppisp: 노출과 색 게인을 조정하며 색상 변화가 가장 적습니다.\n- affine: 완전한 색 행렬로 가장 유연하지만 색 편향이 생기기 쉽습니다.\n- loglinear: 두 방식의 중간입니다."),
    DE("- `ppisp` passt Belichtung und Farbverstärkung an und verschiebt den Farbton am wenigsten.\n- `affine` ist eine volle Farbmatrix, am flexibelsten, aber am anfälligsten für Farbdrift.\n- `loglinear` liegt dazwischen."),
    FR("- `ppisp` ajuste l'exposition et le gain de couleur et déplace le moins la teinte.\n- `affine` est une matrice de couleur complète, la plus souple mais la plus sujette à la dérive de couleur.\n- `loglinear` se situe entre les deux."),
    ES("- `ppisp` ajusta la exposición y la ganancia de color y desplaza el tono lo mínimo.\n- `affine` es una matriz de color completa, la más flexible pero la más propensa a la deriva de color.\n- `loglinear` queda en medio."),
    PT("- `ppisp` ajusta a exposição e o ganho de cor e desloca o menos possível o matiz.\n- `affine` é uma matriz de cor completa, a mais flexível mas a mais sujeita a desvio de cor.\n- `loglinear` fica no meio."),
    IT("- `ppisp` regola esposizione e guadagno di colore e sposta al minimo la tinta.\n- `affine` è una matrice di colore completa, la più flessibile ma la più soggetta a deriva di colore.\n- `loglinear` sta nel mezzo."),
    NL("- `ppisp` past belichting en kleurversterking aan en verschuift de tint het minst.\n- `affine` is een volledige kleurmatrix, het flexibelst maar het gevoeligst voor kleurdrift.\n- `loglinear` zit ertussenin."),
    RU("- `ppisp` правит экспозицию и усиление цвета и меньше всего смещает оттенок.\n- `affine` — полная цветовая матрица: самая гибкая, но и самая склонная к уходу цвета.\n- `loglinear` находится посередине."),
    TR("- `ppisp` pozlamayı ve renk kazancını ayarlar ve tonu en az kaydırır.\n- `affine` tam bir renk matrisidir; en esnek olanıdır ama renk kaymasına en yatkınıdır.\n- `loglinear` ikisinin arasındadır."));

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
    EN("Color-grid width, height and brightness steps. Finer grids fix local shifts but use more VRAM and may absorb real detail on flat, low-texture surfaces."),
    JA("色グリッドの幅・高さ・明るさ段数です。細かいほど局所ずれを補正しVRAMを使いますが、平坦で模様の少ない面の実際の細部も吸収し得ます。"),
    ZH_HANS("色网格的宽、高及亮度步数。细网格可修正局部偏差，但更耗显存，也可能吞掉平坦少纹理表面的真实细节。"),
    ZH_HANT("色網格的寬、高及亮度步數。細網格可修正局部偏差，但更耗顯存，也可能吞掉平坦少紋理表面的真實細節。"),
    KO("색 그리드의 너비·높이·밝기 단계입니다. 세밀할수록 국소 변화를 보정하지만 VRAM이 늘고 평탄한 저텍스처의 실제 세부도 흡수할 수 있습니다."),
    DE("Farbgrid: Breite, Höhe und Helligkeitsstufen. Feiner korrigiert lokal, braucht mehr VRAM und kann auf texturarmen Flächen echte Details schlucken."),
    FR("Grille couleur : largeur, hauteur et niveaux lumineux. Plus fine corrige localement, utilise plus de VRAM et peut absorber les détails des surfaces peu texturées."),
    ES("Rejilla: ancho, alto y pasos de brillo. Más fina corrige cambios locales, usa más VRAM y puede absorber detalle real en superficies lisas."),
    PT("Grelha: largura, altura e passos de brilho. Mais fina corrige mudanças locais, usa mais VRAM e pode absorver detalhe real em superfícies lisas."),
    IT("Griglia colore: larghezza, altezza e livelli luminosi. Più fine corregge localmente, usa più VRAM e può assorbire dettagli reali su superfici poco texturate."),
    NL("Kleurraster: breedte, hoogte en helderheidsstappen. Fijner corrigeert lokaal maar gebruikt meer VRAM en kan echte details op textuurarme vlakken opnemen."),
    RU("Сетка цвета: ширина, высота, уровни яркости. Мельче исправляет локальные сдвиги, но тратит VRAM и может поглощать детали на малотекстурных плоскостях."),
    TR("Renk ızgarası eni, boyu ve parlaklık adımları. İncesi yerel kaymaları düzeltir ama VRAM artırır ve düz, az dokulu yüzeylerde gerçek ayrıntıyı emebilir."));

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
    EN("How smooth the per-photo color correction has to be.\n- Higher keeps corrections gentle and global\n- lower lets them vary from place to place, which can start absorbing real image detail."),
    JA("写真ごとの色補正のなめらかさを指定します。\n- 高いほど補正は穏やかで全体的になります。\n- 低いほど局所的に変えられますが、本物の細部を吸収する恐れがあります。"),
    ZH_HANS("逐张照片的颜色校正必须有多平滑。\n- 数值越高，校正越温和、越全局\n- 越低则可以逐处变化，但可能开始吸收真实的图像细节。"),
    ZH_HANT("逐張照片的色彩校正必須有多平滑。\n- 數值越高，校正越溫和、越全域\n- 越低則可以逐處變化，但可能開始吸收真實的影像細節。"),
    KO("사진별 색 보정의 매끄러움입니다.\n- 높으면 보정이 완만하고 전체적으로 적용됩니다.\n- 낮으면 국소 변화가 가능하지만 실제 세부를 흡수할 수 있습니다."),
    DE("Wie glatt die Farbkorrektur pro Foto sein muss.\n- Höher hält Korrekturen sanft und global\n- niedriger lässt sie von Ort zu Ort variieren, was beginnen kann, echtes Bilddetail aufzusaugen."),
    FR("À quel point la correction de couleur par photo doit être lisse.\n- Plus haut la garde douce et globale\n- plus bas la laisse varier d'un endroit à l'autre, au risque d'absorber du vrai détail d'image."),
    ES("Cuán suave debe ser la corrección de color por foto.\n- Más alto la mantiene leve y global\n- más bajo la deja variar de un sitio a otro, con el riesgo de absorber detalle real de la imagen."),
    PT("Quão suave a correção de cor por foto tem de ser.\n- Mais alto a mantém leve e global\n- mais baixo deixa-a variar de lugar para lugar, com o risco de absorver detalhe real da imagem."),
    IT("Quanto deve essere morbida la correzione di colore per foto.\n- Più alto la tiene leggera e globale\n- più basso la lascia variare da punto a punto, col rischio di assorbire vero dettaglio dell'immagine."),
    NL("Hoe glad de kleurcorrectie per foto moet zijn.\n- Hoger houdt correcties zacht en globaal\n- lager laat ze van plek tot plek variëren, met het risico dat ze echt beelddetail opslokken."),
    RU("Насколько плавной должна быть покадровая цветокоррекция.\n- Больше — коррекция мягкая и глобальная\n- меньше — она может меняться от места к месту и начать вбирать настоящие детали изображения."),
    TR("Fotoğraf başına renk düzeltmesinin ne kadar yumuşak olması gerektiği.\n- Yüksek değerler düzeltmeleri hafif ve genel tutar\n- düşük değerler yerden yere değişmesine izin verir ve gerçek görüntü ayrıntısını içine çekmeye başlayabilir."));

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
    EN("Limits overall tint or brightness drift from per-photo corrections. Raise if splats look warmer, cooler, darker or brighter. 0 disables; useful range 0.01–1."),
    JA("写真ごとの補正による全体の色・明るさのずれを抑えます。スプラットが暖色・寒色・暗め・明るめになるなら上げます。0で無効、目安は0.01〜1です。"),
    ZH_HANS("限制逐图校正造成的整体偏色或亮度漂移。泼溅过暖、过冷、过暗或过亮时调高。0 禁用；建议 0.01–1。"),
    ZH_HANT("限制逐圖校正造成的整體偏色或亮度漂移。潑濺過暖、過冷、過暗或過亮時調高。0 停用；建議 0.01–1。"),
    KO("사진별 보정으로 전체 색·밝기가 변하는 것을 억제합니다. 스플랫이 따뜻하거나 차갑고 어둡거나 밝으면 높이세요. 0은 해제, 권장 범위 0.01~1."),
    DE("Begrenzt Gesamtfarb- und Helligkeitsdrift der Fotokorrekturen. Erhöhen bei wärmeren, kühleren, dunkleren oder helleren Splats. 0 aus; sinnvoll: 0.01–1."),
    FR("Limite la dérive globale de teinte ou luminosité des corrections photo. Augmenter si les splats changent de chaleur ou luminosité. 0 désactive ; utile : 0.01–1."),
    ES("Limita el tinte o brillo global por correcciones de foto. Sube si los splats salen más cálidos, fríos, oscuros o claros. 0 desactiva; útil: 0.01–1."),
    PT("Limita a cor ou brilho global das correções por foto. Suba se os splats ficam mais quentes, frios, escuros ou claros. 0 desliga; útil: 0.01–1."),
    IT("Limita deriva globale di tinta o luminosità delle correzioni foto. Alzare se gli splat sono più caldi, freddi, scuri o chiari. 0 disattiva; utile: 0.01–1."),
    NL("Beperkt globale kleur- of helderheidsdrift door fotocorrecties. Verhoog bij warmere, koelere, donkerdere of lichtere splats. 0 uit; bruikbaar: 0.01–1."),
    RU("Ограничивает общий сдвиг тона и яркости от фотокоррекции. Увеличьте, если сплаты теплее, холоднее, темнее или ярче. 0 отключает; диапазон 0.01–1."),
    TR("Fotoğraf düzeltmesinin genel renk/parlaklık kaymasını sınırlar. Splatlar daha sıcak, soğuk, koyu veya açıksa artırın. 0 kapatır; yararlı aralık 0.01–1."));

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
    EN("How many steps the color-shift check averages over.\n- It should be roughly one pass over the dataset so the average reflects every photo.\n- Ignored when color_shift_reg_weight is 0."),
    JA("色かぶりの判定を何ステップ分平均するかです。\n- すべての写真が平均に反映されるよう、データセットをおおよそ一巡する長さにしてください。\n- color_shift_reg_weight が 0 のときは無視されます。"),
    ZH_HANS("色偏检查在多少步上取平均。\n- 它大致应当等于遍历一遍数据集的长度，这样每张照片都能进入平均。\n- 当 color_shift_reg_weight 为 0 时该项被忽略。"),
    ZH_HANT("色偏檢查在多少步上取平均。\n- 它大致應當等於走完一遍資料集的長度，這樣每張照片都能進入平均。\n- 當 color_shift_reg_weight 為 0 時該項被忽略。"),
    KO("색조 검사를 몇 스텝에 걸쳐 평균낼지입니다.\n- 모든 사진이 평균에 들어가도록 데이터셋을 한 바퀴 도는 길이쯤으로 두십시오.\n- color_shift_reg_weight가 0이면 무시됩니다."),
    DE("Über wie viele Schritte die Farbstichprüfung mittelt.\n- Es sollte etwa einem Durchlauf durch den Datensatz entsprechen, damit der Mittelwert jedes Foto erfasst.\n- Wird ignoriert, wenn color_shift_reg_weight 0 ist."),
    FR("Sur combien d'étapes le contrôle de teinte fait sa moyenne.\n- Cela devrait valoir à peu près un passage sur le jeu de données, pour que la moyenne reflète chaque photo.\n- Ignoré quand color_shift_reg_weight vaut 0."),
    ES("Sobre cuántos pasos promedia la comprobación del tinte.\n- Debería equivaler más o menos a una pasada por el conjunto de datos, para que la media refleje cada foto.\n- Se ignora cuando color_shift_reg_weight es 0."),
    PT("Sobre quantos passos a verificação de tonalidade faz a média.\n- Deveria equivaler a mais ou menos uma passagem pelo conjunto de dados, para que a média reflita cada foto.\n- É ignorado quando color_shift_reg_weight é 0."),
    IT("Su quanti passi la verifica della dominante fa la media.\n- Dovrebbe valere all'incirca un passaggio sul set di dati, così la media rispecchia ogni foto.\n- Viene ignorato quando color_shift_reg_weight è 0."),
    NL("Over hoeveel stappen de kleurzweemcontrole middelt.\n- Dat zou ongeveer één doorloop van de dataset moeten zijn, zodat het gemiddelde elke foto weerspiegelt.\n- Wordt genegeerd als color_shift_reg_weight 0 is."),
    RU("По скольким шагам усредняется проверка оттенка.\n- Это должно быть примерно один проход по набору данных, чтобы среднее охватило каждый снимок.\n- Игнорируется, когда color_shift_reg_weight равен 0."),
    TR("Renk kayması denetiminin kaç adım üzerinden ortalama aldığı.\n- Ortalamanın her fotoğrafı yansıtması için kabaca veri kümesinde bir geçiş kadar olmalıdır.\n- color_shift_reg_weight 0 iken yok sayılır."));

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
    EN("Apply the same per-photo correction to depth and normal maps.\nBiased AI-generated maps can then still be used without dragging the geometry off."),
    JA("同じ写真ごとの補正を、深度マップと法線マップにも当てはめます。\n偏りのある AI 生成のマップでも、ジオメトリを狂わせずに使えるようになります。"),
    ZH_HANS("把同样的逐张照片校正也应用到深度图和法线图上。\n这样即使 AI 生成的图有偏差，也仍能使用而不至于把几何带偏。"),
    ZH_HANT("把同樣的逐張照片校正也套用到深度圖和法線圖上。\n這樣即使 AI 產生的圖有偏差，也仍能使用而不至於把幾何帶偏。"),
    KO("같은 사진별 보정을 깊이 맵과 노멀 맵에도 적용합니다.\n치우친 AI 생성 맵이라도 지오메트리를 어긋나게 하지 않고 쓸 수 있습니다."),
    DE("Dieselbe Korrektur pro Foto auch auf Tiefen- und Normalenkarten anwenden.\nVerzerrte KI-erzeugte Karten lassen sich dann nutzen, ohne die Geometrie zu verziehen."),
    FR("Appliquer la même correction par photo aux cartes de profondeur et de normales.\nDes cartes générées par IA et biaisées restent alors utilisables sans tirer la géométrie de travers."),
    ES("Aplicar la misma corrección por foto a los mapas de profundidad y normales.\nAsí, mapas generados por IA con sesgo siguen siendo utilizables sin torcer la geometría."),
    PT("Aplicar a mesma correção por foto aos mapas de profundidade e normais.\nAssim, mapas gerados por IA com viés continuam utilizáveis sem entortar a geometria."),
    IT("Applicare la stessa correzione per foto anche alle mappe di profondità e normali.\nMappe generate dall'IA e distorte restano così utilizzabili senza storcere la geometria."),
    NL("Dezelfde correctie per foto ook op diepte- en normalenkaarten toepassen.\nVertekende, door AI gemaakte kaarten blijven dan bruikbaar zonder de geometrie scheef te trekken."),
    RU("Применять ту же покадровую коррекцию к картам глубины и нормалей.\nТогда смещённые карты, созданные ИИ, можно использовать, не уводя геометрию."),
    TR("Aynı fotoğraf başına düzeltmeyi derinlik ve normal haritalarına da uygular.\nBöylece yanlı yapay zekâ haritaları geometriyi bozmadan kullanılabilir."));

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
    EN("Use a steadier update rule for the per-photo color correction.\n- Generally more stable and needs less tuning\n- turn off to use the scheduled learning rates instead."),
    JA("写真ごとの色補正に、より落ち着いた更新則を使います。\n- おおむね安定していて調整も少なくて済みます。\n- オフにすると、代わりにスケジュールされた学習率が使われます。"),
    ZH_HANS("为逐张照片的颜色校正采用更平稳的更新规则。\n- 通常更稳定，也更少需要调参\n- 关闭后改用按计划变化的学习率。"),
    ZH_HANT("為逐張照片的色彩校正採用更平穩的更新規則。\n- 通常更穩定，也更少需要調參\n- 關閉後改用按排程變化的學習率。"),
    KO("사진별 색 보정에 더 안정적인 갱신 규칙을 씁니다.\n- 대체로 더 안정적이고 손볼 일이 적습니다.\n- 끄면 대신 스케줄된 학습률을 사용합니다."),
    DE("Für die Farbkorrektur pro Foto eine ruhigere Aktualisierungsregel verwenden.\n- Meist stabiler und weniger einstellungsbedürftig\n- abgeschaltet gelten stattdessen die geplanten Lernraten."),
    FR("Utiliser une règle de mise à jour plus stable pour la correction de couleur par photo.\n- Généralement plus sûre et demandant moins de réglage\n- décoché, ce sont les taux d'apprentissage programmés qui s'appliquent."),
    ES("Usar una regla de actualización más estable para la corrección de color por foto.\n- Suele ser más estable y necesitar menos ajuste\n- sin marcar se usan en su lugar las tasas de aprendizaje programadas."),
    PT("Usar uma regra de atualização mais estável para a correção de cor por foto.\n- Costuma ser mais estável e exigir menos ajuste\n- desmarcado, usam-se as taxas de aprendizado programadas."),
    IT("Usare una regola di aggiornamento più stabile per la correzione di colore per foto.\n- In genere più stabile e con meno bisogno di regolazioni\n- deselezionato si usano invece i tassi di apprendimento pianificati."),
    NL("Voor de kleurcorrectie per foto een stabielere updateregel gebruiken.\n- Meestal stabieler en met minder afstemwerk\n- uitgevinkt gelden in plaats daarvan de geplande leersnelheden."),
    RU("Использовать более ровное правило обновления для покадровой цветокоррекции.\n- Обычно устойчивее и требует меньше настройки\n- без флажка применяются запланированные скорости обучения."),
    TR("Fotoğraf başına renk düzeltmesi için daha durağan bir güncelleme kuralı kullanır.\n- Genelde daha kararlıdır ve daha az ayar ister\n- kapatılırsa çizelgelenmiş öğrenme oranları kullanılır."));

SS_MSG(use_ppisp,
    EN("PPISP camera correction"), JA("PPISP によるカメラ補正"),
    ZH_HANS("PPISP 相机校正"), ZH_HANT("PPISP 相機校正"),
    KO("PPISP 카메라 보정"), DE("PPISP-Kamerakorrektur"),
    FR("Correction caméra PPISP"), ES("Corrección de cámara PPISP"),
    PT("Correção de câmera PPISP"), IT("Correzione camera PPISP"),
    NL("PPISP-cameracorrectie"), RU("Коррекция камеры PPISP"),
    TR("PPISP kamera düzeltmesi"));
SS_MSG(use_ppisp_help,
    EN("Model per-pixel camera effects such as vignetting, exposure and lens color response.\nKeeps darkened corners and per-photo exposure shifts out of the splats themselves."),
    JA("周辺減光、露出、レンズの色特性といった画素ごとのカメラ効果をモデル化します。\n四隅の暗がりや写真ごとの露出のずれが、スプラット自体に焼き付くのを防ぎます。"),
    ZH_HANS("对暗角、曝光、镜头色彩响应等逐像素的相机效应建模。\n可以避免暗角和逐张曝光偏差被烙进泼溅本身。"),
    ZH_HANT("對暗角、曝光、鏡頭色彩響應等逐像素的相機效應建模。\n可以避免暗角和逐張曝光偏差被烙進潑濺本身。"),
    KO("비네팅, 노출, 렌즈 색 특성 같은 픽셀별 카메라 효과를 모델링합니다.\n어두워진 모서리와 사진별 노출 차이가 스플랫 자체에 새겨지지 않게 합니다."),
    DE("Pixelweise Kameraeffekte wie Vignettierung, Belichtung und Farbverhalten des Objektivs modellieren.\nHält abgedunkelte Ecken und Belichtungsverschiebungen pro Foto aus den Splats selbst heraus."),
    FR("Modéliser les effets caméra pixel par pixel : vignetage, exposition, réponse chromatique de l'objectif.\nÉvite que les coins assombris et les écarts d'exposition par photo se gravent dans les splats eux-mêmes."),
    ES("Modelar los efectos de cámara píxel a píxel: viñeteado, exposición y respuesta de color del objetivo.\nEvita que las esquinas oscurecidas y las variaciones de exposición por foto se graben en los propios splats."),
    PT("Modelar os efeitos de câmera pixel a pixel: vinhetagem, exposição e resposta de cor da lente.\nEvita que cantos escurecidos e variações de exposição por foto fiquem gravados nos próprios splats."),
    IT("Modella effetti della camera per pixel: vignettatura, esposizione e risposta cromatica.\nEvita che gli splat assorbano angoli scuriti e variazioni di esposizione."),
    NL("Cameraeffecten per pixel modelleren, zoals vignettering, belichting en de kleurrespons van de lens.\nHoudt donkere hoeken en belichtingsverschuivingen per foto uit de splats zelf."),
    RU("Моделировать попиксельные эффекты камеры: виньетирование, экспозицию и цветовой отклик объектива.\nНе даёт затемнённым углам и покадровым сдвигам экспозиции впечататься в сами сплаты."),
    TR("Vinyet, pozlama ve merceğin renk tepkisi gibi piksel başına kamera etkilerini modeller.\nKararmış köşelerin ve fotoğraf başına pozlama kaymalarının splat'lara işlemesini önler."));

SS_MSG(ppisp_param_type,
    EN("Camera effects modeled"), JA("モデル化するカメラ効果"),
    ZH_HANS("建模的相机效应"), ZH_HANT("建模的相機效應"),
    KO("모델링할 카메라 효과"), DE("Modellierte Kameraeffekte"),
    FR("Effets caméra modélisés"), ES("Efectos de cámara modelados"),
    PT("Efeitos de câmera modelados"), IT("Effetti della camera modellati"),
    NL("Gemodelleerde cameraeffecten"), RU("Моделируемые эффекты камеры"),
    TR("Modellenen kamera etkileri"));
SS_MSG(ppisp_param_type_help,
    EN("- no_crf: exposure, vignetting and color; original adds a tone curve, rqs improves dark tones.\n- Add _no_vig to omit vignetting or _clamp for output in 0–1."),
    JA("- no_crf：露出・周辺減光・色。originalはトーン曲線追加、rqsは暗部を改善。\n- _no_vigで周辺減光を外し、_clampで出力を0〜1に制限します。"),
    ZH_HANS("- no_crf：曝光、暗角和颜色；original 加色调曲线，rqs 改善暗部。\n- 加 _no_vig 排除暗角，或 _clamp 将输出限制在 0–1。"),
    ZH_HANT("- no_crf：曝光、暗角和色彩；original 加色調曲線，rqs 改善暗部。\n- 加 _no_vig 排除暗角，或 _clamp 將輸出限制在 0–1。"),
    KO("- no_crf: 노출·비네팅·색. original은 톤 곡선 추가, rqs는 어두운 톤 개선.\n- _no_vig는 비네팅 제외, _clamp는 출력을 0~1로 제한합니다."),
    DE("- no_crf: Belichtung, Vignettierung, Farbe; original ergänzt Tonkurve, rqs verbessert dunkle Töne.\n- _no_vig lässt Vignettierung weg; _clamp begrenzt auf 0–1."),
    FR("- no_crf : exposition, vignettage, couleur ; original ajoute une courbe, rqs améliore les tons sombres.\n- _no_vig omet le vignettage ; _clamp limite à 0–1."),
    ES("- no_crf: exposición, viñeteo, color; original añade curva, rqs mejora tonos oscuros.\n- _no_vig omite viñeteo; _clamp limita a 0–1."),
    PT("- no_crf: exposição, vinheta, cor; original acrescenta curva, rqs melhora tons escuros.\n- _no_vig omite vinheta; _clamp limita a 0–1."),
    IT("- no_crf: esposizione, vignettatura, colore; original aggiunge una curva, rqs migliora i toni scuri.\n- _no_vig omette vignettatura; _clamp limita a 0–1."),
    NL("- no_crf: belichting, vignettering, kleur; original voegt tooncurve toe, rqs verbetert donkere tonen.\n- _no_vig laat vignettering weg; _clamp begrenst op 0–1."),
    RU("- no_crf: экспозиция, виньетка, цвет; original добавляет кривую, rqs улучшает тёмные тона.\n- _no_vig убирает виньетку; _clamp ограничивает 0–1."),
    TR("- no_crf: pozlama, vinyet, renk; original ton eğrisi ekler, rqs koyu tonları iyileştirir.\n- _no_vig vinyeti çıkarır; _clamp 0–1’e sınırlar."));

SS_MSG(ppisp_exposure_from_exif,
    EN("Exposure init from EXIF"), JA("EXIF による露出の初期化"),
    ZH_HANS("用 EXIF 初始化曝光"), ZH_HANT("用 EXIF 初始化曝光"),
    KO("EXIF로 노출 초기화"), DE("Belichtungsstart aus EXIF"),
    FR("Exposition initiale depuis l'EXIF"), ES("Exposición inicial desde EXIF"),
    PT("Exposição inicial do EXIF"), IT("Esposizione iniziale da EXIF"),
    NL("Belichting starten vanuit EXIF"), RU("Начальная экспозиция из EXIF"),
    TR("EXIF'ten pozlama başlangıcı"));
SS_MSG(ppisp_exposure_from_exif_help,
    EN("Initializes exposure corrections from EXIF shutter, aperture and ISO relative to the dataset average. Missing tags use the average."),
    JA("EXIFのシャッター、絞り、ISOからデータセット平均を基準に露出補正を初期化します。タグがなければ平均を使います。"),
    ZH_HANS("用 EXIF 快门、光圈及 ISO，相对数据集平均值初始化曝光校正。缺少标签时使用平均值。"),
    ZH_HANT("用 EXIF 快門、光圈及 ISO，相對資料集平均值初始化曝光校正。缺少標籤時使用平均值。"),
    KO("EXIF 셔터·조리개·ISO로 데이터셋 평균 대비 노출 보정을 초기화합니다. 태그가 없으면 평균을 씁니다."),
    DE("Initialisiert Belichtungskorrekturen aus EXIF-Verschluss, Blende und ISO relativ zum Datensatzmittel. Fehlende Tags nutzen den Mittelwert."),
    FR("Initialise l’exposition depuis obturateur, ouverture et ISO EXIF par rapport à la moyenne du jeu. Sans balises, utilise la moyenne."),
    ES("Inicializa exposición con obturación, apertura e ISO EXIF respecto a la media del dataset. Sin etiquetas usa la media."),
    PT("Inicializa exposição com obturador, abertura e ISO EXIF relativos à média do conjunto. Sem etiquetas usa a média."),
    IT("Inizializza l’esposizione da otturatore, apertura e ISO EXIF rispetto alla media del dataset. Senza tag usa la media."),
    NL("Initialiseert belichting uit EXIF-sluitertijd, diafragma en ISO ten opzichte van het datasetgemiddelde. Zonder tags wordt het gemiddelde gebruikt."),
    RU("Начальная экспокоррекция из выдержки, диафрагмы и ISO EXIF относительно среднего набора. При отсутствии тегов — среднее."),
    TR("Pozlamayı EXIF enstantane, diyafram ve ISO’dan veri kümesi ortalamasına göre başlatır. Etiketler yoksa ortalamayı kullanır."));

SS_MSG(ppisp_exposure_arithmetic_mean,
    EN("Neutral exposure by average gain"), JA("平均の倍率で露出を中立に"),
    ZH_HANS("按平均倍率保持曝光中性"), ZH_HANT("按平均倍率保持曝光中性"),
    KO("평균 배율로 노출 중립"), DE("Neutrale Belichtung über mittleren Faktor"),
    FR("Exposition neutre par gain moyen"),
    ES("Exposición neutra por ganancia media"),
    PT("Exposição neutra pelo ganho médio"),
    IT("Esposizione neutra per guadagno medio"),
    NL("Neutrale belichting via gemiddelde factor"),
    RU("Нейтральная экспозиция по среднему множителю"),
    TR("Ortalama çarpanla nötr pozlama"));
SS_MSG(ppisp_exposure_arithmetic_mean_help,
    EN("Centers exposure multipliers at mean 1 instead of stops at mean 0. Used for penalty and EXIF initialization; avoids dark splats with varied exposures."),
    JA("露出倍率の平均を1にし、段数平均0の代わりに使います。ペナルティとEXIF初期化に適用し、露出差でスプラットが暗くなるのを防ぎます。"),
    ZH_HANS("令曝光倍率平均为 1，而非档位平均为 0。用于惩罚和 EXIF 初始化，避免曝光变化时泼溅偏暗。"),
    ZH_HANT("令曝光倍率平均為 1，而非檔位平均為 0。用於懲罰和 EXIF 初始化，避免曝光變化時潑濺偏暗。"),
    KO("스톱 평균 0 대신 노출 배율 평균 1로 중심을 맞춥니다. 페널티와 EXIF 초기화에 적용해 노출 차이가 커도 스플랫이 어두워지지 않게 합니다."),
    DE("Zentriert Belichtungsmultiplikatoren auf Mittel 1 statt Stops auf 0. Für Strafe und EXIF-Start; vermeidet dunkle Splats bei variierender Belichtung."),
    FR("Centre les multiplicateurs à moyenne 1 plutôt que les stops à 0. Pour pénalité et départ EXIF ; évite les splats sombres avec expositions variées."),
    ES("Centra multiplicadores de exposición en media 1, no pasos en 0. Para penalización e inicio EXIF; evita splats oscuros con exposiciones variadas."),
    PT("Centra multiplicadores de exposição na média 1, não passos em 0. Para penalidade e início EXIF; evita splats escuros com exposições variadas."),
    IT("Centra i moltiplicatori d’esposizione a media 1 anziché gli stop a 0. Per penalità e avvio EXIF; evita splat scuri con esposizioni diverse."),
    NL("Centreert belichtingsfactoren op gemiddelde 1 in plaats van stops op 0. Voor straf en EXIF-start; voorkomt donkere splats bij wisselende belichting."),
    RU("Центрирует множители экспозиции по среднему 1 вместо ступеней по 0. Для штрафа и старта EXIF; избегает тёмных сплатов при разных экспозициях."),
    TR("Durak ortalaması 0 yerine pozlama çarpanı ortalamasını 1 yapar. Ceza ve EXIF başlangıcında kullanılır; değişken pozlamada koyu splatları önler."));

SS_MSG(apply_ppisp_before_bilagrid,
    EN("Camera correction first"), JA("カメラ補正を先に適用"),
    ZH_HANS("先做相机校正"), ZH_HANT("先做相機校正"),
    KO("카메라 보정을 먼저"), DE("Kamerakorrektur zuerst"),
    FR("Correction caméra en premier"), ES("Corrección de cámara primero"),
    PT("Correção de câmera primeiro"), IT("Correzione camera per prima"),
    NL("Cameracorrectie eerst"), RU("Сначала коррекция камеры"),
    TR("Önce kamera düzeltmesi"));
SS_MSG(apply_ppisp_before_bilagrid_help,
    EN("Run the camera-effect model before the per-photo color correction rather than after.\nOnly matters when both are on, and decides which of the two absorbs a given color difference."),
    JA("カメラ効果のモデルを、写真ごとの色補正のあとではなく前に適用します。\n両方を有効にしているときだけ意味があり、ある色の差をどちらが吸収するかを決めます。"),
    ZH_HANS("让相机效应模型在逐张照片的颜色校正之前运行，而不是之后。\n只有两者都开启时才有意义，它决定某个颜色差异由哪一方吸收。"),
    ZH_HANT("讓相機效應模型在逐張照片的色彩校正之前執行，而不是之後。\n只有兩者都開啟時才有意義，它決定某個顏色差異由哪一方吸收。"),
    KO("카메라 효과 모델을 사진별 색 보정 뒤가 아니라 앞에서 적용합니다.\n둘 다 켰을 때만 의미가 있으며, 어떤 색 차이를 어느 쪽이 흡수할지 정합니다."),
    DE("Führt das Kameraeffektmodell vor der Farbkorrektur pro Foto aus, statt danach.\nWirkt nur, wenn beide aktiv sind; bestimmt, welches die Farbabweichung aufnimmt."),
    FR("Exécuter le modèle d'effets caméra avant la correction de couleur par photo plutôt qu'après.\nN'a d'importance que si les deux sont actifs, et détermine lequel absorbe une différence de couleur donnée."),
    ES("Ejecutar el modelo de efectos de cámara antes de la corrección de color por foto en vez de después.\nSolo importa si ambos están activos, y decide cuál de los dos absorbe una diferencia de color dada."),
    PT("Executar o modelo de efeitos de câmera antes da correção de cor por foto em vez de depois.\nSó importa quando ambos estão ligados e decide qual dos dois absorve uma dada diferença de cor."),
    IT("Eseguire il modello degli effetti della camera prima invece che dopo la correzione di colore per foto.\nConta solo quando entrambi sono attivi e decide quale dei due assorbe una data differenza di colore."),
    NL("Het cameraeffectmodel vóór in plaats van na de kleurcorrectie per foto uitvoeren.\nDoet er alleen toe als beide aan staan en bepaalt welke van de twee een bepaald kleurverschil opvangt."),
    RU("Запускать модель эффектов камеры до, а не после покадровой цветокоррекции.\nИмеет значение, только когда включены оба, и решает, кто из двоих вбирает данную разницу цвета."),
    TR("Kamera etkisi modelini fotoğraf başına renk düzeltmesinden sonra değil önce çalıştırır.\nYalnızca ikisi de açıkken önemlidir ve belirli bir renk farkını hangisinin soğuracağını belirler."));

SS_MSG(apply_ppisp_before_color_space,
    EN("Camera correction in the splat's own colors"),
    JA("スプラット自身の色空間でカメラ補正"),
    ZH_HANS("在高斯自身的色彩空间中做相机校正"),
    ZH_HANT("在高斯自身的色彩空間中做相機校正"),
    KO("스플랫 자체 색 공간에서 카메라 보정"),
    DE("Kamerakorrektur im Farbraum der Splats"),
    FR("Correction caméra dans les couleurs des splats"),
    ES("Corrección de cámara en el color propio de los splats"),
    PT("Correção de câmera nas cores dos próprios splats"),
    IT("Correzione camera nei colori propri degli splat"),
    NL("Cameracorrectie in de eigen kleuren van de splats"),
    RU("Коррекция камеры в собственном цвете сплатов"),
    TR("Kamera düzeltmesi splat'ların kendi renklerinde"));
SS_MSG(apply_ppisp_before_color_space_help,
    EN("Applies camera effects in the splats’ color space before display conversion. Requires a separate splat color space and camera correction running first."),
    JA("表示変換前にスプラットの色空間でカメラ効果を適用します。独自のスプラット色空間と、先に動くカメラ補正が必要です。"),
    ZH_HANS("显示转换前在泼溅色彩空间应用相机效果。需独立的泼溅色彩空间，并先运行相机校正。"),
    ZH_HANT("顯示轉換前在潑濺色彩空間套用相機效果。需獨立的潑濺色彩空間，並先執行相機校正。"),
    KO("표시 변환 전에 스플랫 색 공간에서 카메라 효과를 적용합니다. 별도 스플랫 색 공간과 먼저 실행되는 카메라 보정이 필요합니다."),
    DE("Kameraeffekte im Splat-Farbraum vor der Displayumwandlung. Erfordert eigenen Splat-Farbraum und zuerst laufende Kamerakorrektur."),
    FR("Effets caméra dans l’espace des splats avant conversion d’affichage. Exige un espace splat distinct et une correction caméra exécutée en premier."),
    ES("Efectos de cámara en el espacio splat antes de convertir a pantalla. Requiere espacio splat propio y corrección de cámara ejecutada primero."),
    PT("Efeitos de câmara no espaço splat antes da conversão de ecrã. Requer espaço splat próprio e correção de câmara executada primeiro."),
    IT("Effetti camera nello spazio splat prima della conversione display. Richiede spazio splat proprio e correzione camera eseguita prima."),
    NL("Camera-effecten in de splatkleurruimte vóór schermconversie. Vereist aparte splatkleurruimte en eerst uitgevoerde cameracorrectie."),
    RU("Эффекты камеры в цветовом пространстве сплатов до вывода на экран. Требует отдельного пространства сплатов и коррекции камеры первой."),
    TR("Kamera etkilerini ekran dönüşümünden önce splat renk uzayında uygular. Ayrı splat renk uzayı ve önce çalışan kamera düzeltmesi gerekir."));

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
    EN("Use a steadier update rule for the camera-effect model.\n- Generally more stable, needs less tuning, and leads to fewer floaters\n- turn off to use the scheduled learning rate instead."),
    JA("カメラ効果のモデルに、より落ち着いた更新則を使います。\n- おおむね安定していて調整も少なくて済み、浮遊ノイズも減ります。\n- オフにすると、代わりにスケジュールされた学習率が使われます。"),
    ZH_HANS("为相机效应模型采用更平稳的更新规则。\n- 通常更稳定、更少需要调参，漂浮物也更少\n- 关闭后改用按计划变化的学习率。"),
    ZH_HANT("為相機效應模型採用更平穩的更新規則。\n- 通常更穩定、更少需要調參，漂浮物也更少\n- 關閉後改用按排程變化的學習率。"),
    KO("카메라 효과 모델에 더 안정적인 갱신 규칙을 씁니다.\n- 대체로 더 안정적이고 손볼 일이 적으며 부유물도 줄어듭니다.\n- 끄면 대신 스케줄된 학습률을 사용합니다."),
    DE("Für das Kameraeffektmodell eine ruhigere Aktualisierungsregel verwenden.\n- Meist stabiler, weniger einstellungsbedürftig und mit weniger Schwebeteilen\n- abgeschaltet gilt stattdessen die geplante Lernrate."),
    FR("Utiliser une règle de mise à jour plus stable pour le modèle d'effets caméra.\n- Généralement plus sûre, demandant moins de réglage et produisant moins de flotteurs\n- décoché, c'est le taux d'apprentissage programmé qui s'applique."),
    ES("Usar una regla de actualización más estable para el modelo de efectos de cámara.\n- Suele ser más estable, necesitar menos ajuste y dejar menos restos flotantes\n- sin marcar se usa en su lugar la tasa de aprendizaje programada."),
    PT("Usar uma regra de atualização mais estável para o modelo de efeitos de câmera.\n- Costuma ser mais estável, exigir menos ajuste e deixar menos resíduos flutuantes\n- desmarcado, usa-se a taxa de aprendizado programada."),
    IT("Usare una regola di aggiornamento più stabile per il modello degli effetti della camera.\n- In genere più stabile, con meno bisogno di regolazioni e meno frammenti fluttuanti\n- deselezionato si usa invece il tasso di apprendimento pianificato."),
    NL("Voor het cameraeffectmodel een stabielere updateregel gebruiken.\n- Meestal stabieler, met minder afstemwerk en minder zwevers\n- uitgevinkt geldt in plaats daarvan de geplande leersnelheid."),
    RU("Использовать более ровное правило обновления для модели эффектов камеры.\n- Обычно устойчивее, требует меньше настройки и даёт меньше «летающих» артефактов\n- без флажка применяется запланированная скорость обучения."),
    TR("Kamera etkisi modeli için daha durağan bir güncelleme kuralı kullanır.\n- Genelde daha kararlıdır, daha az ayar ister ve daha az uçuşan artık bırakır\n- kapatılırsa çizelgelenmiş öğrenme oranı kullanılır."));

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
    EN("How fast the per-photo color correction adapts.\n- Higher tracks exposure changes sooner but can start absorbing real detail.\n- Ignored when use_adagrad_bilagrid_optim is on."),
    JA("写真ごとの色補正がどれだけ速く適応するかです。\n- 高いほど露出の変化に早く追いつきますが、本物の細部を吸い込み始めることがあります。\n- use_adagrad_bilagrid_optim が有効なときは無視されます。"),
    ZH_HANS("逐张照片的颜色校正适应得有多快。\n- 数值越高越快跟上曝光变化，但可能开始吸收真实细节。\n- 当 use_adagrad_bilagrid_optim 打开时该项被忽略。"),
    ZH_HANT("逐張照片的色彩校正適應得有多快。\n- 數值越高越快跟上曝光變化，但可能開始吸收真實細節。\n- 當 use_adagrad_bilagrid_optim 開啟時該項被忽略。"),
    KO("사진별 색 보정이 얼마나 빨리 적응하는지입니다.\n- 값이 크면 노출 변화를 빨리 따라가지만 실제 디테일을 흡수하기 시작할 수 있습니다.\n- use_adagrad_bilagrid_optim이 켜져 있으면 무시됩니다."),
    DE("Wie schnell sich die Farbkorrektur pro Foto anpasst.\n- Höher folgt Belichtungsänderungen früher, kann aber beginnen, echtes Detail aufzusaugen.\n- Wird ignoriert, wenn use_adagrad_bilagrid_optim an ist."),
    FR("À quelle vitesse la correction de couleur par photo s'adapte.\n- Plus haut suit plus tôt les changements d'exposition mais peut commencer à absorber du vrai détail.\n- Ignoré quand use_adagrad_bilagrid_optim est activé."),
    ES("Con qué rapidez se adapta la corrección de color por foto.\n- Más alto sigue antes los cambios de exposición pero puede empezar a absorber detalle real.\n- Se ignora cuando use_adagrad_bilagrid_optim está activo."),
    PT("Com que rapidez a correção de cor por foto se adapta.\n- Mais alto acompanha as mudanças de exposição mais cedo, mas pode começar a absorver detalhe real.\n- É ignorado quando use_adagrad_bilagrid_optim está ligado."),
    IT("Con quanta rapidità si adatta la correzione di colore per foto.\n- Più alto segue prima i cambi di esposizione ma può cominciare ad assorbire dettaglio vero.\n- Viene ignorato quando use_adagrad_bilagrid_optim è attivo."),
    NL("Hoe snel de kleurcorrectie per foto zich aanpast.\n- Hoger volgt belichtingsveranderingen eerder maar kan echt detail gaan opslokken.\n- Wordt genegeerd als use_adagrad_bilagrid_optim aan staat."),
    RU("Насколько быстро подстраивается покадровая цветокоррекция.\n- Больше — быстрее догоняет изменения экспозиции, но может начать вбирать настоящие детали.\n- Игнорируется при включённом use_adagrad_bilagrid_optim."),
    TR("Fotoğraf başına renk düzeltmesinin ne kadar hızlı uyum sağladığı.\n- Yüksek değerler pozlama değişimlerini daha erken yakalar ama gerçek ayrıntıyı içine çekmeye başlayabilir.\n- use_adagrad_bilagrid_optim açıkken yok sayılır."));

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
    EN("How many steps the per-photo color correction takes to reach full adaptation speed.\nRamping in stops it from claiming color before the splats have any."),
    JA("写真ごとの色補正が最大の適応速度に達するまでのステップ数です。\n徐々に効かせることで、スプラットがまだ色を持たないうちに補正が色を取ってしまうのを防ぎます。"),
    ZH_HANS("逐张照片的颜色校正达到最快适应速度所需的步数。\n逐步加力可以避免它在泼溅还没有颜色时就先把颜色占为己有。"),
    ZH_HANT("逐張照片的色彩校正達到最快適應速度所需的步數。\n逐步加力可以避免它在潑濺還沒有顏色時就先把顏色占為己有。"),
    KO("사진별 색 보정이 최대 적응 속도에 이르기까지의 스텝 수입니다.\n서서히 올리면 스플랫이 아직 색을 갖기 전에 보정이 색을 가져가 버리는 일을 막습니다."),
    DE("Wie viele Schritte die Farbkorrektur pro Foto braucht, um volle Anpassungsgeschwindigkeit zu erreichen.\nDas langsame Einblenden hindert sie daran, Farbe zu beanspruchen, bevor die Splats welche haben."),
    FR("Combien d'étapes la correction de couleur par photo met à atteindre sa pleine vitesse d'adaptation.\nUne montée progressive l'empêche de s'approprier la couleur avant que les splats n'en aient."),
    ES("Cuántos pasos tarda la corrección de color por foto en alcanzar su máxima velocidad de adaptación.\nSubirla poco a poco evita que se apropie del color antes de que los splats tengan alguno."),
    PT("Quantos passos a correção de cor por foto leva para atingir a velocidade máxima de adaptação.\nSubir aos poucos evita que ela tome a cor antes de os splats terem alguma."),
    IT("Quanti passi impiega la correzione di colore per foto a raggiungere la piena velocità di adattamento.\nSalire per gradi le impedisce di prendersi il colore prima che gli splat ne abbiano."),
    NL("Hoeveel stappen de kleurcorrectie per foto nodig heeft om op volle aanpassingssnelheid te komen.\nGeleidelijk opvoeren belet haar de kleur op te eisen voordat de splats die hebben."),
    RU("За сколько шагов покадровая цветокоррекция выходит на полную скорость подстройки.\nПостепенное включение мешает ей забрать цвет раньше, чем тот появится у сплатов."),
    TR("Fotoğraf başına renk düzeltmesinin tam uyum hızına ulaşması için gereken adım sayısı.\nKademeli devreye girmek, splat'lar henüz renk kazanmadan düzeltmenin rengi sahiplenmesini engeller."));

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
    EN("How many steps the camera-effect model takes to reach full adaptation speed.\nRamping in stops it from claiming brightness before the splats have any."),
    JA("カメラ効果のモデルが最大の適応速度に達するまでのステップ数です。\n徐々に効かせることで、スプラットがまだ明るさを持たないうちにモデルが明るさを取ってしまうのを防ぎます。"),
    ZH_HANS("相机效应模型达到最快适应速度所需的步数。\n逐步加力可以避免它在泼溅还没有亮度时就先把亮度占为己有。"),
    ZH_HANT("相機效應模型達到最快適應速度所需的步數。\n逐步加力可以避免它在潑濺還沒有亮度時就先把亮度占為己有。"),
    KO("카메라 효과 모델이 최대 적응 속도에 이르기까지의 스텝 수입니다.\n서서히 올리면 스플랫이 아직 밝기를 갖기 전에 모델이 밝기를 가져가 버리는 일을 막습니다."),
    DE("Wie viele Schritte das Kameraeffektmodell braucht, um volle Anpassungsgeschwindigkeit zu erreichen.\nDas langsame Einblenden hindert es daran, Helligkeit zu beanspruchen, bevor die Splats welche haben."),
    FR("Combien d'étapes le modèle d'effets caméra met à atteindre sa pleine vitesse d'adaptation.\nUne montée progressive l'empêche de s'approprier la luminosité avant que les splats n'en aient."),
    ES("Cuántos pasos tarda el modelo de efectos de cámara en alcanzar su máxima velocidad de adaptación.\nSubirlo poco a poco evita que se apropie del brillo antes de que los splats tengan alguno."),
    PT("Quantos passos o modelo de efeitos de câmera leva para atingir a velocidade máxima de adaptação.\nSubir aos poucos evita que ele tome o brilho antes de os splats terem algum."),
    IT("Quanti passi impiega il modello degli effetti della camera a raggiungere la piena velocità di adattamento.\nSalire per gradi gli impedisce di prendersi la luminosità prima che gli splat ne abbiano."),
    NL("Hoeveel stappen het cameraeffectmodel nodig heeft om op volle aanpassingssnelheid te komen.\nGeleidelijk opvoeren belet het de helderheid op te eisen voordat de splats die hebben."),
    RU("За сколько шагов модель эффектов камеры выходит на полную скорость подстройки.\nПостепенное включение мешает ей забрать яркость раньше, чем та появится у сплатов."),
    TR("Kamera etkisi modelinin tam uyum hızına ulaşması için gereken adım sayısı.\nKademeli devreye girmek, splat'lar henüz parlaklık kazanmadan modelin parlaklığı sahiplenmesini engeller."));

SS_MSG(calib_correction,
    EN("Calibration correction"), JA("キャリブレーション補正"),
    ZH_HANS("标定校正"), ZH_HANT("校準修正"),
    KO("캘리브레이션 보정"), DE("Kalibrierkorrektur"),
    FR("Correction d'étalonnage"), ES("Corrección de calibración"),
    PT("Correção de calibração"), IT("Correzione della calibrazione"),
    NL("Kalibratiecorrectie"), RU("Коррекция калибровки"),
    TR("Kalibrasyon düzeltmesi"));
SS_MSG(calib_correction_help,
    EN("Warp each photo to compensate for calibration errors such as rolling shutter or incorrect poses.\n- basic warps in the image plane.\n- accurate moves near content more and renders depth each step."),
    JA("カメラのキャリブレーションで取りきれなかったずれ（フォーカスブリージング、手ぶれ補正、ローリングシャッター、わずかな姿勢の誤差）を、写真ごとに再構成と位置が合うような滑らかなワープを与えて補正します。\n- `basic` は写真を画像面の中でゆがめ、`accurate` はさらに、カメラ位置の誤差がそうするように近くのものを遠くのものより大きく動かし、そのために毎ステップ深度を描画します。"),
    ZH_HANS("给每张照片各自加上一个使它与重建对齐的平滑形变，校正相机标定没有消除的偏差：对焦呼吸、防抖、卷帘快门、略有偏差的位姿。\n- `basic` 在像平面内形变照片； `accurate` 还像相机位置的误差那样把近处的内容移得比远处多，为此每一步都要渲染深度。"),
    ZH_HANT("給每張照片各自加上一個使它與重建對齊的平滑形變，校正相機校準沒有消除的偏差：對焦呼吸、防手震、捲簾快門、略有偏差的位姿。\n- `basic` 在像平面內形變照片； `accurate` 還像相機位置的誤差那樣把近處的內容移得比遠處多，為此每一步都要算繪深度。"),
    KO("사진마다 재구성과 맞도록 부드러운 워프를 따로 주어, 카메라 캘리브레이션이 놓친 오차(포커스 브리딩, 손떨림 보정, 롤링 셔터, 약간 틀린 자세)를 보정합니다.\n- `basic`은 사진을 이미지 평면 안에서 휘게 하고, `accurate`는 카메라 위치의 오차가 그러하듯 가까운 내용을 먼 내용보다 더 많이 움직이며, 이를 위해 매 스텝 깊이를 렌더링합니다."),
    DE("Korrigieren, was die Kamerakalibrierung übersehen hat (Fokusatmen, Bildstabilisierung, Rolling Shutter, eine leicht falsche Pose), indem jedes Foto eine eigene glatte Verzerrung bekommt, die es mit der Rekonstruktion zur Deckung bringt.\n- `basic` verzerrt jedes Foto in seiner Bildebene;\n- `accurate` verschiebt zusätzlich nahe Inhalte stärker als ferne, wie es ein Fehler in der Kameraposition tut, und rendert dafür in jedem Schritt die Tiefe."),
    FR("Corriger ce que l'étalonnage de la caméra a manqué (respiration de mise au point, stabilisation, obturateur roulant, une pose légèrement fausse) en donnant à chaque photo sa propre déformation lisse qui l'aligne sur la reconstruction.\n- `basic` déforme chaque photo dans son plan image ;\n- `accurate` déplace aussi le contenu proche plus que le lointain, comme le fait une erreur sur la position de la caméra, et rend pour cela la profondeur à chaque étape."),
    ES("Corregir lo que la calibración de la cámara pasó por alto (respiración del enfoque, estabilización, obturador rodante, una pose ligeramente errónea) dando a cada foto su propia deformación suave que la alinea con la reconstrucción.\n- `basic` deforma cada foto en su plano de imagen;\n- `accurate` además mueve el contenido cercano más que el lejano, como lo hace un error en la posición de la cámara, y para ello renderiza la profundidad en cada paso."),
    PT("Corrigir o que a calibração da câmera deixou passar (respiração do foco, estabilização, obturador rolante, uma pose ligeiramente errada) dando a cada foto sua própria deformação suave que a alinha com a reconstrução.\n- `basic` deforma cada foto em seu plano de imagem;\n- `accurate` também move o conteúdo próximo mais do que o distante, como faz um erro na posição da câmera, e para isso renderiza a profundidade a cada passo."),
    IT("Correggere ciò che la calibrazione della camera ha mancato (respirazione della messa a fuoco, stabilizzazione, rolling shutter, una posa leggermente sbagliata) dando a ogni foto una propria deformazione liscia che la allinea alla ricostruzione. `basic` deforma ogni foto nel suo piano immagine; `accurate` sposta anche il contenuto vicino più di quello lontano, come fa un errore nella posizione della camera, e per farlo renderizza la profondità a ogni passo."),
    NL("Corrigeren wat de camerakalibratie heeft gemist (focus breathing, beeldstabilisatie, rolling shutter, een iets verkeerde pose) door elke foto een eigen gladde vervorming te geven die hem op de reconstructie laat aansluiten.\n`basic` vervormt elke foto in zijn beeldvlak; `accurate` verschuift daarnaast nabije inhoud meer dan verre, zoals een fout in de camerapositie doet, en rendert daarvoor elke stap de diepte."),
    RU("Исправлять то, что упустила калибровка камеры («дыхание» фокуса, стабилизацию, скользящий затвор, слегка неверную позу), давая каждому снимку собственную гладкую деформацию, совмещающую его с реконструкцией.\n- `basic` деформирует снимок в его плоскости изображения;\n- `accurate` вдобавок сдвигает близкое сильнее далёкого, как это делает ошибка в положении камеры, и для этого рендерит глубину на каждом шаге."),
    TR("Kamera kalibrasyonunun kaçırdığını (odak nefes alması, sabitleme, kayan perde, biraz yanlış bir poz) her fotoğrafa onu yeniden yapılandırmayla hizalayan kendine ait yumuşak bir bükme vererek düzeltir.\n- `basic` her fotoğrafı kendi görüntü düzleminde büker;\n- `accurate` ayrıca, kamera konumundaki bir hatanın yaptığı gibi, yakındaki içeriği uzaktakinden daha çok kaydırır ve bunun için her adımda derinlik işler."));

SS_MSG(use_calib_warp,
    EN("Per-photo calibration warp"), JA("写真ごとのキャリブレーションワープ"),
    ZH_HANS("逐张照片的标定形变"), ZH_HANT("逐張照片的校準形變"),
    KO("사진별 캘리브레이션 워프"), DE("Kalibrierverzerrung je Foto"),
    FR("Déformation d'étalonnage par photo"),
    ES("Deformación de calibración por foto"),
    PT("Deformação de calibração por foto"),
    IT("Deformazione di calibrazione per foto"),
    NL("Kalibratievervorming per foto"),
    RU("Деформация калибровки для каждого снимка"),
    TR("Fotoğraf başına kalibrasyon bükmesi"));
SS_MSG(use_calib_warp_help,
    EN("Fits a smooth per-photo warp during training to align each image with the reconstruction. Enabled by calibration correction."),
    JA("学習中に写真ごとの滑らかなワープを調整し、再構成に合わせます。キャリブレーション補正で有効になります。"),
    ZH_HANS("训练时为每张照片拟合平滑形变，使其对齐重建。由标定校正启用。"),
    ZH_HANT("訓練時為每張照片擬合平滑形變，使其對齊重建。由校正修正啟用。"),
    KO("학습 중 사진별 부드러운 워프를 맞춰 재구성에 정렬합니다. 캘리브레이션 보정으로 켜집니다."),
    DE("Passt beim Training eine glatte Verformung je Foto an die Rekonstruktion an. Wird durch Kalibrierkorrektur aktiviert."),
    FR("Ajuste une déformation douce par photo pendant l’entraînement pour l’aligner à la reconstruction. Activée par correction de calibration."),
    ES("Ajusta una deformación suave por foto al entrenar para alinearla a la reconstrucción. La activa la corrección de calibración."),
    PT("Ajusta uma deformação suave por foto no treino para alinhar à reconstrução. Ativada pela correção de calibração."),
    IT("Adatta una deformazione dolce per foto nel training per allinearla alla ricostruzione. Attivata dalla correzione di calibrazione."),
    NL("Past tijdens training een vloeiende vervorming per foto aan voor uitlijning met de reconstructie. Ingeschakeld door kalibratiecorrectie."),
    RU("Обучает плавную деформацию каждого снимка для совмещения с реконструкцией. Включается коррекцией калибровки."),
    TR("Eğitimde her fotoğrafa yumuşak büküm uydurup modele hizalar. Kalibrasyon düzeltmesiyle açılır."));

SS_MSG(calib_warp_grid,
    EN("Calibration correction grid"), JA("キャリブレーション補正のグリッド"),
    ZH_HANS("标定校正网格"), ZH_HANT("校準修正網格"),
    KO("캘리브레이션 보정 격자"), DE("Raster der Kalibrierkorrektur"),
    FR("Grille de la correction d'étalonnage"),
    ES("Rejilla de la corrección de calibración"),
    PT("Grade da correção de calibração"),
    IT("Griglia della correzione di calibrazione"),
    NL("Raster van de kalibratiecorrectie"),
    RU("Сетка коррекции калибровки"),
    TR("Kalibrasyon düzeltmesinin ızgarası"));
SS_MSG(calib_warp_grid_help,
    EN("Warp control points per image side, 2–16. More points follow finer distortion, including rolling shutter, but need more image evidence per point."),
    JA("画像各辺のワープ制御点数、2〜16です。多いとローリングシャッターなど細かい歪みに追従しますが、各点の決定に十分な画像情報が必要です。"),
    ZH_HANS("每边形变控制点数，2–16。更多点可跟随滚动快门等细畸变，但每点需要更多图像证据。"),
    ZH_HANT("每邊形變控制點數，2–16。更多點可跟隨滾動快門等細畸變，但每點需要更多影像證據。"),
    KO("이미지 변당 워프 제어점 수로 2~16입니다. 많으면 롤링 셔터 등 미세 왜곡을 따르지만 점마다 더 많은 이미지 근거가 필요합니다."),
    DE("Verformungs-Kontrollpunkte je Bildseite, 2–16. Mehr folgt feiner Verzeichnung, etwa Rolling Shutter, braucht aber mehr Bildinformation je Punkt."),
    FR("Points de contrôle par côté, 2–16. Plus suit les distorsions fines dont rolling shutter, mais exige plus d’information d’image par point."),
    ES("Puntos de control por lado, 2–16. Más sigue distorsiones finas como rolling shutter, pero exige más evidencia de imagen por punto."),
    PT("Pontos de controlo por lado, 2–16. Mais segue distorções finas como rolling shutter, mas exige mais evidência da imagem por ponto."),
    IT("Punti di controllo per lato, 2–16. Più punti seguono distorsioni fini come rolling shutter, ma richiedono più evidenza d’immagine per punto."),
    NL("Vervormingspunten per beeldzijde, 2–16. Meer volgt fijnere vervorming, zoals rolling shutter, maar vereist meer beeldinformatie per punt."),
    RU("Точки управления на сторону, 2–16. Больше следует мелкой дисторсии, включая rolling shutter, но каждой нужно больше данных снимка."),
    TR("Kenar başına büküm kontrol noktası, 2–16. Fazlası rolling shutter gibi ince bozulmayı izler ama nokta başına daha çok resim kanıtı ister."));

SS_MSG(calib_warp_parallax,
    EN("Depth-dependent calibration correction"), JA("奥行きに応じたキャリブレーション補正"),
    ZH_HANS("随深度变化的标定校正"), ZH_HANT("隨深度變化的校準修正"),
    KO("깊이에 따른 캘리브레이션 보정"), DE("Tiefenabhängige Kalibrierkorrektur"),
    FR("Correction d'étalonnage selon la profondeur"),
    ES("Corrección de calibración según la profundidad"),
    PT("Correção de calibração dependente da profundidade"),
    IT("Correzione di calibrazione dipendente dalla profondità"),
    NL("Diepteafhankelijke kalibratiecorrectie"),
    RU("Коррекция калибровки с учётом глубины"),
    TR("Derinliğe bağlı kalibrasyon düzeltmesi"));
SS_MSG(calib_warp_parallax_help,
    EN("Moves near content more than distant content to correct small camera-position errors. Renders depth every step, slowing training."),
    JA("小さなカメラ位置誤差を補うため、遠景より近景を大きく動かします。毎ステップ深度を描画するため、学習が遅くなります。"),
    ZH_HANS("让近景比远景移动更多，以修正小的相机位置误差。每步渲染深度，会减慢训练。"),
    ZH_HANT("讓近景比遠景移動更多，以修正小的相機位置誤差。每步渲染深度，會減慢訓練。"),
    KO("작은 카메라 위치 오차를 고치도록 가까운 장면을 더 많이 움직입니다. 매 단계 깊이를 렌더링해 학습이 느려집니다."),
    DE("Verschiebt Nahes stärker als Fernes gegen kleine Kamerapositionsfehler. Rendert Tiefe bei jedem Schritt und verlangsamt das Training."),
    FR("Déplace davantage le proche pour corriger de petites erreurs de position caméra. Rend la profondeur à chaque pas, ce qui ralentit l’entraînement."),
    ES("Mueve más lo cercano para corregir pequeños errores de posición de cámara. Renderiza profundidad en cada paso, ralentizando el entrenamiento."),
    PT("Move mais o próximo para corrigir pequenos erros de posição da câmara. Renderiza profundidade a cada passo, tornando o treino mais lento."),
    IT("Muove di più il vicino per correggere piccoli errori di posizione camera. Renderizza profondità a ogni passo, rallentando il training."),
    NL("Verplaatst nabije inhoud meer om kleine camerapositiefouten te corrigeren. Rendert diepte bij elke stap, wat training vertraagt."),
    RU("Смещает близкое сильнее дальнего, исправляя малые ошибки позиции камеры. Рендерит глубину каждый шаг, замедляя обучение."),
    TR("Küçük kamera konum hatalarını düzeltmek için yakını uzaktan fazla hareket ettirir. Her adım derinlik çizer, eğitimi yavaşlatır."));

SS_MSG(calib_warp_start_iter,
    EN("Calibration correction start"), JA("キャリブレーション補正の開始"),
    ZH_HANS("标定校正的起始步"), ZH_HANT("校準修正的起始步"),
    KO("캘리브레이션 보정 시작"), DE("Start der Kalibrierkorrektur"),
    FR("Début de la correction d'étalonnage"),
    ES("Inicio de la corrección de calibración"),
    PT("Início da correção de calibração"),
    IT("Inizio della correzione di calibrazione"),
    NL("Start van de kalibratiecorrectie"),
    RU("Начало коррекции калибровки"),
    TR("Kalibrasyon düzeltmesinin başlangıcı"));
SS_MSG(calib_warp_start_iter_help,
    EN("The step from which the warp is fitted. Before it the renders are too rough to "
       "line photos up with."),
    JA("ワープの当てはめを始めるステップです。それより前の描画は粗すぎて、写真を合わせる"
       "基準になりません。"),
    ZH_HANS("从哪一步开始拟合形变。在此之前渲染太粗糙，无法拿来对齐照片。"),
    ZH_HANT("從哪一步開始擬合形變。在此之前算繪太粗糙，無法拿來對齊照片。"),
    KO("워프를 맞추기 시작하는 스텝입니다. 그 전의 렌더링은 너무 거칠어서 사진을 맞출 "
       "기준이 되지 못합니다."),
    DE("Der Schritt, ab dem die Verzerrung angepasst wird. Davor sind die Renderings zu "
       "grob, um Fotos daran auszurichten."),
    FR("L'étape à partir de laquelle la déformation est ajustée. Avant, les rendus sont "
       "trop grossiers pour y aligner les photos."),
    ES("El paso a partir del cual se ajusta la deformación. Antes, los renderizados son "
       "demasiado toscos para alinear las fotos con ellos."),
    PT("O passo a partir do qual a deformação é ajustada. Antes disso, as renderizações "
       "são toscas demais para alinhar as fotos a elas."),
    IT("Il passo da cui la deformazione viene adattata. Prima, i rendering sono troppo "
       "grezzi per allinearvi le foto."),
    NL("De stap vanaf waar de vervorming wordt aangepast. Daarvoor zijn de renders te "
       "ruw om foto's op af te stemmen."),
    RU("Шаг, с которого подбирается деформация. До него рендеры слишком грубы, чтобы "
       "совмещать с ними снимки."),
    TR("Bükmenin uydurulmaya başladığı adım. Öncesinde işlemeler, fotoğrafları onlarla "
       "hizalamak için fazla kabadır."));

SS_MSG(calib_warp_step,
    EN("Calibration correction step size"), JA("キャリブレーション補正のステップ幅"),
    ZH_HANS("标定校正步长"), ZH_HANT("校準修正步長"),
    KO("캘리브레이션 보정 스텝 크기"), DE("Schrittweite der Kalibrierkorrektur"),
    FR("Pas de la correction d'étalonnage"),
    ES("Tamaño de paso de la corrección de calibración"),
    PT("Tamanho do passo da correção de calibração"),
    IT("Ampiezza del passo della correzione di calibrazione"),
    NL("Stapgrootte van de kalibratiecorrectie"),
    RU("Размер шага коррекции калибровки"),
    TR("Kalibrasyon düzeltmesinin adım boyu"));
SS_MSG(calib_warp_step_help,
    EN("How much of each Gauss-Newton step the warp takes when its photo is trained on; "
       "1 takes the whole step."),
    JA("その写真で学習するたびに、ワープがガウス・ニュートン法の一歩のうちどれだけ進むか"
       "です。1 で一歩をまるごと進みます。"),
    ZH_HANS("每次用某张照片训练时，形变走高斯-牛顿一步中的多少；1 表示走完整一步。"),
    ZH_HANT("每次用某張照片訓練時，形變走高斯-牛頓一步中的多少；1 表示走完整一步。"),
    KO("해당 사진으로 학습할 때마다 워프가 가우스-뉴턴 한 스텝 중 얼마나 나아가는지"
       "입니다. 1이면 한 스텝을 다 갑니다."),
    DE("Welchen Anteil jedes Gauß-Newton-Schritts die Verzerrung geht, wenn mit ihrem "
       "Foto trainiert wird; 1 geht den ganzen Schritt."),
    FR("Quelle part de chaque pas de Gauss-Newton la déformation effectue quand sa "
       "photo sert à l'entraînement ; 1 fait le pas entier."),
    ES("Qué parte de cada paso de Gauss-Newton da la deformación cuando se entrena con "
       "su foto; 1 da el paso completo."),
    PT("Que parte de cada passo de Gauss-Newton a deformação dá quando sua foto é usada "
       "no treinamento; 1 dá o passo inteiro."),
    IT("Quanta parte di ogni passo di Gauss-Newton compie la deformazione quando si "
       "addestra sulla sua foto; 1 compie il passo intero."),
    NL("Welk deel van elke Gauss-Newton-stap de vervorming zet wanneer op zijn foto "
       "wordt getraind; 1 zet de hele stap."),
    RU("Какую долю каждого шага Гаусса — Ньютона делает деформация, когда обучение идёт "
       "на её снимке; 1 — весь шаг."),
    TR("Fotoğrafıyla eğitim yapıldığında bükmenin her Gauss-Newton adımının ne kadarını "
       "attığı; 1 adımın tamamını atar."));

SS_MSG(calib_warp_smooth,
    EN("Calibration correction smoothness"), JA("キャリブレーション補正のなめらかさ"),
    ZH_HANS("标定校正平滑度"), ZH_HANT("校準修正平滑度"),
    KO("캘리브레이션 보정 매끄러움"), DE("Glätte der Kalibrierkorrektur"),
    FR("Lissage de la correction d'étalonnage"),
    ES("Suavidad de la corrección de calibración"),
    PT("Suavidade da correção de calibração"),
    IT("Levigatezza della correzione di calibrazione"),
    NL("Gladheid van de kalibratiecorrectie"),
    RU("Гладкость коррекции калибровки"),
    TR("Kalibrasyon düzeltmesinin pürüzsüzlüğü"));
SS_MSG(calib_warp_smooth_help,
    EN("How strongly neighbouring control points are pulled together, relative to how "
       "much the photo itself pins them down."),
    JA("隣り合う制御点どうしを引き寄せる強さです。写真そのものがそれらをどれだけ決めて"
       "いるかに対する比で表します。"),
    ZH_HANS("相邻控制点彼此拉近的力度，相对于照片本身对它们的约束程度。"),
    ZH_HANT("相鄰控制點彼此拉近的力度，相對於照片本身對它們的約束程度。"),
    KO("이웃한 제어점끼리 서로 끌어당기는 세기로, 사진 자체가 그 점들을 얼마나 붙잡는지"
       "에 대한 비율입니다."),
    DE("Wie stark benachbarte Kontrollpunkte zueinander gezogen werden, gemessen daran, "
       "wie fest das Foto selbst sie festlegt."),
    FR("Avec quelle force les points de contrôle voisins sont rapprochés, par rapport à "
       "ce que la photo elle-même impose."),
    ES("Con qué fuerza se atraen entre sí los puntos de control vecinos, en relación con "
       "cuánto los fija la propia foto."),
    PT("Com que força os pontos de controle vizinhos são puxados uns para os outros, em "
       "relação a quanto a própria foto os determina."),
    IT("Con quanta forza i punti di controllo vicini vengono attratti tra loro, rispetto "
       "a quanto li vincola la foto stessa."),
    NL("Hoe sterk naburige controlepunten naar elkaar worden getrokken, vergeleken met "
       "hoe vast de foto zelf ze legt."),
    RU("Насколько сильно соседние контрольные точки стягиваются друг к другу — "
       "относительно того, насколько их задаёт сам снимок."),
    TR("Komşu kontrol noktalarının birbirine ne kadar güçlü çekildiği; fotoğrafın "
       "kendisinin onları ne kadar sabitlediğine göre."));


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
    EN("Treat the input images as linear light rather than ordinary display-encoded photos.\n- `auto` reads an EXR as linear, which is what that format promises, and everything else as display-encoded\n- set it explicitly for an EXR whose pixels are display-encoded anyway."),
    JA("入力画像を、通常の表示用に符号化された写真ではなくリニア光として扱います。\n- auto では EXR をリニア（その形式の約束）として、それ以外は表示用の符号化として読み込みます。\n- 中身が表示用に符号化された EXR では明示的に設定してください。"),
    ZH_HANS("把输入图像当作线性光，而不是普通的显示编码照片。\n- auto 会把 EXR 按线性读取（这是该格式的约定），其他格式按显示编码\n- 若 EXR 里存的其实是显示编码的像素，请明确设置。"),
    ZH_HANT("把輸入影像當作線性光，而不是普通的顯示編碼照片。\n- auto 會把 EXR 依線性讀取（這是該格式的約定），其他格式依顯示編碼\n- 若 EXR 裡存的其實是顯示編碼的像素，請明確設定。"),
    KO("입력 이미지를 일반적인 디스플레이 인코딩 사진이 아니라 선형 광으로 취급합니다.\n- auto는 EXR을 선형으로(그 형식의 약속입니다), 그 밖의 형식은 디스플레이 인코딩으로 읽습니다.\n- 내용이 디스플레이 인코딩인 EXR이라면 직접 설정하십시오."),
    DE("Die Eingabebilder als lineares Licht behandeln statt als übliche, für die Anzeige codierte Fotos.\n- `auto` liest eine EXR als linear -- das sagt dieses Format zu -- und alles andere als für die Anzeige codiert\n- bei einer EXR mit trotzdem anzeigecodierten Pixeln ausdrücklich setzen."),
    FR("Traiter les images d'entrée comme de la lumière linéaire plutôt que comme des photos encodées pour l'affichage.\n- « auto » lit un EXR comme linéaire, ce que ce format promet, et tout le reste comme encodé pour l'affichage\n- à régler explicitement pour un EXR dont les pixels sont malgré tout encodés pour l'affichage."),
    ES("Tratar las imágenes de entrada como luz lineal en vez de fotos codificadas para pantalla.\n- «auto» lee un EXR como lineal, que es lo que ese formato promete, y todo lo demás como codificado para pantalla\n- indíquelo explícitamente si un EXR guarda píxeles codificados para pantalla."),
    PT("Tratar as imagens de entrada como luz linear em vez de fotos codificadas para exibição.\n- «auto» lê um EXR como linear, que é o que esse formato promete, e todo o resto como codificado para exibição\n- defina explicitamente se um EXR guardar pixels codificados para exibição."),
    IT("Tratta le immagini in ingresso come luce lineare anziché valori di visualizzazione.\n- auto: EXR lineare, altri formati codificati per la visualizzazione.\n- Impostare esplicitamente per EXR con valori già codificati."),
    NL("De invoerbeelden als lineair licht behandelen in plaats van als gewone, voor weergave gecodeerde foto's.\n- `auto` leest een EXR als lineair -- dat belooft dat formaat -- en al het andere als voor weergave gecodeerd\n- stel het uitdrukkelijk in voor een EXR met toch voor weergave gecodeerde pixels."),
    RU("Считать входные изображения линейным светом, а не обычными фотографиями с кодировкой для дисплея.\n- «auto» читает EXR как линейный -- это обещает сам формат -- а всё остальное как закодированное для дисплея\n- задайте явно, если в EXR всё же лежат пиксели с кодировкой для дисплея."),
    TR("Girdi görüntülerini ekran için kodlanmış sıradan fotoğraflar yerine doğrusal ışık olarak ele alır.\n- `auto`, bir EXR'yi doğrusal okur -- o biçimin verdiği söz budur -- diğer her şeyi ekran için kodlanmış sayar\n- pikselleri yine de ekran için kodlanmış bir EXR'de açıkça ayarlayın."));

SS_MSG(image_color_transfer,
    EN("Input tone curve"), JA("入力のトーンカーブ"),
    ZH_HANS("输入色调曲线"), ZH_HANT("輸入色調曲線"),
    KO("입력 톤 커브"), DE("Eingabe-Tonkurve"),
    FR("Courbe de tonalité d'entrée"), ES("Curva tonal de entrada"),
    PT("Curva tonal de entrada"), IT("Curva tonale in ingresso"),
    NL("Toonkromme van de invoer"), RU("Тоновая кривая входа"),
    TR("Girdi ton eğrisi"));
SS_MSG(image_color_transfer_help,
    EN("Encode input light values for loss comparison.\n- srgb is suitable for ordinary photos.\n- aces, filmic and uncharted2 roll off highlights for scene-linear HDR values above 1.0.\n- --image-color-is-linear separately indicates whether input is linear."),
    JA("入力の光の値を、損失を測る絵に変えるカーブです。 srgb は素の sRGB 符号化で、普通の写真はこれです。 aces・filmic・uncharted2 はハイライトを切り捨てずになだらかに丸めるので、1.0 をはるかに超えるシーンリニアの HDR 入力向けです。\n入力がリニアかどうかとは独立で、それは --image-color-is-linear です。"),
    ZH_HANS("把输入的光值变成计算损失的那幅图的曲线。 srgb 是普通的 sRGB 编码，一般照片用它。 aces、filmic、uncharted2 会把高光平滑压下来而不是直接截断，适合远超 1.0 的场景线性 HDR 输入。\n这与输入是否线性无关，那由 --image-color-is-linear 决定。"),
    ZH_HANT("把輸入的光值變成計算損失的那幅圖的曲線。 srgb 是普通的 sRGB 編碼，一般照片用它。 aces、filmic、uncharted2 會把高光平滑壓下來而不是直接截斷，適合遠超 1.0 的場景線性 HDR 輸入。\n這與輸入是否線性無關，那由 --image-color-is-linear 決定。"),
    KO("입력의 빛 값을 손실을 재는 그림으로 바꾸는 곡선입니다. srgb는 평범한 sRGB 인코딩이고 보통 사진은 이것을 씁니다. aces, filmic, uncharted2는 밝은 부분을 잘라내지 않고 완만하게 눌러 주므로 1.0을 크게 넘는 장면 선형 HDR 입력에 알맞습니다.\n입력이 선형인지와는 무관하며 그것은 --image-color-is-linear 입니다."),
    DE("Die Kurve, die die Lichtwerte der Eingabe in das Bild überführt, an dem der Verlust gemessen wird.\n- `srgb` ist die schlichte sRGB-Codierung und das, was ein gewöhnliches Foto will.\n- `aces`, `filmic` und `uncharted2` rollen die Lichter ab, statt sie abzuschneiden -- für szenenlineare HDR-Eingaben weit über 1.0. Unabhängig davon, ob die Eingabe linear ist; das ist `--image-color-is-linear`."),
    FR("La courbe qui transforme les valeurs de lumière de l'entrée en l'image sur laquelle la perte est mesurée. « srgb » est l'encodage sRGB simple, ce que veut une photo ordinaire.\n- « aces », « filmic » et « uncharted2 » adoucissent les hautes lumières au lieu de les écrêter, pour une entrée HDR linéaire scène bien au-dessus de 1.0.\n- Indépendant du fait que l'entrée soit linéaire : c'est « --image-color-is-linear »."),
    ES("La curva que convierte los valores de luz de la entrada en la imagen sobre la que se mide la pérdida. «srgb» es la codificación sRGB simple, lo que quiere una foto normal.\n- «aces», «filmic» y «uncharted2» suavizan las altas luces en vez de recortarlas, para entrada HDR lineal de escena muy por encima de 1.0.\n- Independiente de si la entrada es lineal: eso es «--image-color-is-linear»."),
    PT("A curva que converte os valores de luz da entrada na imagem sobre a qual a perda é medida. «srgb» é a codificação sRGB simples, o que uma foto normal quer.\n- «aces», «filmic» e «uncharted2» suavizam as altas luzes em vez de as cortar, para entrada HDR linear de cena muito acima de 1.0.\n- Independente de a entrada ser linear: isso é «--image-color-is-linear»."),
    IT("La curva che trasforma i valori di luce dell'ingresso nell'immagine su cui si misura la perdita. «srgb» è la semplice codifica sRGB, ciò che vuole una foto normale.\n- «aces», «filmic» e «uncharted2» addolciscono le alte luci invece di troncarle, per ingressi HDR lineari di scena molto sopra 1.0.\n- Indipendente dal fatto che l'ingresso sia lineare: quello è «--image-color-is-linear»."),
    NL("De kromme die de lichtwaarden van de invoer omzet in het beeld waarop het verlies wordt gemeten. `srgb` is de gewone sRGB-codering en is wat een normale foto wil.\n- `aces`, `filmic` en `uncharted2` laten de hoge lichten aflopen in plaats van ze af te kappen, voor scène-lineaire HDR-invoer ver boven 1.0.\n- Los van of de invoer lineair is; dat is `--image-color-is-linear`."),
    RU("Кривая, превращающая значения света на входе в картинку, на которой меряется потеря. «srgb» -- обычное кодирование sRGB, то, что нужно обычной фотографии.\n- «aces», «filmic» и «uncharted2» плавно сводят света вместо обрезки, для сценарно-линейного HDR-входа сильно выше 1.0.\n- Независимо от того, линеен ли вход: это «--image-color-is-linear»."),
    TR("Girdinin ışık değerlerini, kaybın ölçüldüğü resme çeviren eğri. `srgb` sade sRGB kodlamasıdır ve sıradan bir fotoğrafın istediği budur.\n- `aces`, `filmic` ve `uncharted2` parlak bölgeleri kırpmak yerine yumuşatarak indirir;\n- 1.0'ın çok üstüne çıkan sahne-doğrusal HDR girdi içindir.\n- Girdinin doğrusal olup olmamasından bağımsızdır, o `--image-color-is-linear`'dır."));

SS_MSG(image_color_gamut,
    EN("Input color space"), JA("入力の色空間"), ZH_HANS("输入色彩空间"),
    ZH_HANT("輸入色彩空間"), KO("입력 색 공간"), DE("Farbraum der Eingabe"),
    FR("Espace colorimétrique d'entrée"), ES("Espacio de color de entrada"),
    PT("Espaço de cor de entrada"), IT("Spazio colore in ingresso"),
    NL("Kleurruimte van de invoer"), RU("Цветовое пространство входа"),
    TR("Girdi renk uzayı"));
SS_MSG(image_color_gamut_help,
    EN("Input color primaries. none reads EXR metadata, otherwise assumes sRGB; Rec.709 forces sRGB primaries. No tone mapping."),
    JA("入力の原色です。noneはEXRのメタデータを読み、それ以外はsRGBとみなします。Rec.709はsRGB原色を強制します。トーンマッピングはしません。"),
    ZH_HANS("输入色彩原色。none 读取 EXR 元数据，否则假设 sRGB；Rec.709 强制 sRGB 原色。不做色调映射。"),
    ZH_HANT("輸入色彩原色。none 讀取 EXR 中繼資料，否則假設 sRGB；Rec.709 強制 sRGB 原色。不做色調映射。"),
    KO("입력 원색입니다. none은 EXR 메타데이터, 아니면 sRGB로 가정합니다. Rec.709는 sRGB 원색을 강제하며 톤 매핑은 없습니다."),
    DE("Eingabeprimärfarben. none liest EXR-Metadaten, nimmt sonst sRGB an; Rec.709 erzwingt sRGB-Primärfarben. Kein Tonemapping."),
    FR("Primaires d’entrée. none lit les métadonnées EXR, sinon suppose sRGB ; Rec.709 force les primaires sRGB. Pas de tone mapping."),
    ES("Primarias de entrada. none lee metadatos EXR, si no supone sRGB; Rec.709 fuerza primarias sRGB. Sin mapeo tonal."),
    PT("Primárias de entrada. none lê metadados EXR, senão assume sRGB; Rec.709 força primárias sRGB. Sem mapeamento tonal."),
    IT("Primarie d’ingresso. none legge metadati EXR, altrimenti assume sRGB; Rec.709 impone primarie sRGB. Nessun tone mapping."),
    NL("Invoerprimairen. none leest EXR-metadata, anders sRGB; Rec.709 forceert sRGB-primairen. Geen tonemapping."),
    RU("Первичные цвета входа. none читает EXR, иначе считает sRGB; Rec.709 задаёт первичные sRGB. Без тонального отображения."),
    TR("Girdi ana renkleri. none EXR verisini okur, yoksa sRGB varsayar; Rec.709 sRGB ana renklerini zorlar. Ton eşleme yoktur."));

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
    EN("Stores linear-light splat colors; unset follows input. Enables square-root color optimization with a trust region. Output curve: --splat-color-transfer."),
    JA("線形光でスプラット色を保存します。未指定は入力に従います。平方根の色最適化と信頼領域を使います。出力曲線は--splat-color-transferです。"),
    ZH_HANS("以线性光存储泼溅颜色，未设置则跟随输入。启用平方根颜色优化和信赖域。输出曲线用 --splat-color-transfer。"),
    ZH_HANT("以線性光儲存潑濺色彩，未設定則跟隨輸入。啟用平方根色彩最佳化和信賴域。輸出曲線用 --splat-color-transfer。"),
    KO("스플랫 색을 선형광으로 저장하며 미설정은 입력을 따릅니다. 제곱근 색 최적화와 신뢰 영역을 켭니다. 출력 곡선: --splat-color-transfer."),
    DE("Speichert lineare Splatfarben; ungesetzt folgt Eingabe. Aktiviert Wurzel-Farboptimierung mit Vertrauensregion. Ausgabekurve: --splat-color-transfer."),
    FR("Stocke les couleurs linéaires ; non défini suit l’entrée. Active l’optimisation racine avec région de confiance. Courbe : --splat-color-transfer."),
    ES("Almacena colores lineales; sin definir sigue la entrada. Activa optimización raíz con región de confianza. Curva: --splat-color-transfer."),
    PT("Guarda cores lineares; indefinido segue a entrada. Ativa otimização raiz com região de confiança. Curva: --splat-color-transfer."),
    IT("Memorizza colori lineari; non impostato segue l’ingresso. Attiva ottimizzazione radice con regione di fiducia. Curva: --splat-color-transfer."),
    NL("Slaat lineaire splatkleuren op; oningesteld volgt invoer. Wortelkleuroptimalisatie met vertrouwensgebied. Uitvoercurve: --splat-color-transfer."),
    RU("Хранит линейные цвета; не задано — по входу. Включает корневую оптимизацию цвета с доверительной областью. Кривая: --splat-color-transfer."),
    TR("Doğrusal splat renkleri saklar; boşsa girdiyi izler. Güven bölgesiyle karekök renk optimizasyonunu açar. Çıktı eğrisi: --splat-color-transfer."));

SS_MSG(splat_color_transfer,
    EN("Splat tone curve"), JA("スプラットのトーンカーブ"),
    ZH_HANS("泼溅色调曲线"), ZH_HANT("潑濺色調曲線"),
    KO("스플랫 톤 커브"), DE("Splat-Tonkurve"),
    FR("Courbe de tonalité des splats"), ES("Curva tonal de los splats"),
    PT("Curva tonal dos splats"), IT("Curva tonale degli splat"),
    NL("Toonkromme van de splats"), RU("Тоновая кривая сплатов"),
    TR("Splat ton eğrisi"));
SS_MSG(splat_color_transfer_help,
    EN("Choose the render curve used in the loss comparison.\n- none uses the input curve.\n- aces, filmic and uncharted2 roll off highlights, letting bright splats fit near-white photos."),
    JA("損失で比較するレンダーの色変換カーブを選びます。\n- none は入力のカーブを使います。\n- aces、filmic、uncharted2 はハイライトを抑え、明るいスプラットをほぼ白の写真に合わせます。"),
    ZH_HANS("选择损失比较时使用的渲染曲线。\n- none 使用输入曲线。\n- aces、filmic 和 uncharted2 平滑压缩高光，让明亮的泼溅匹配接近白色的照片。"),
    ZH_HANT("選擇損失比較時使用的算繪曲線。\n- none 使用輸入曲線。\n- aces、filmic 和 uncharted2 平滑壓縮高光，讓明亮的潑濺符合接近白色的照片。"),
    KO("손실 비교에 쓰는 렌더링 곡선을 선택합니다.\n- none은 입력 곡선을 사용합니다.\n- aces, filmic, uncharted2는 밝은 영역을 부드럽게 압축해 밝은 스플랫을 거의 흰 사진에 맞춥니다."),
    DE("Renderkurve für den Loss-Vergleich.\n- none nutzt die Eingabekurve.\n- aces, filmic und uncharted2 rollen Lichter ab, damit helle Splats zu fast weißen Fotos passen."),
    FR("Courbe de rendu pour le calcul de la perte.\n- none reprend la courbe d’entrée.\n- aces, filmic et uncharted2 atténuent les hautes lumières pour ajuster des splats lumineux aux photos presque blanches."),
    ES("Curva del render para calcular la pérdida.\n- none usa la curva de entrada.\n- aces, filmic y uncharted2 suavizan las altas luces para ajustar splats brillantes a fotos casi blancas."),
    PT("Curva do render para calcular a perda.\n- none usa a curva de entrada.\n- aces, filmic e uncharted2 suavizam as altas luzes para ajustar splats brilhantes a fotos quase brancas."),
    IT("Curva del render per calcolare la perdita.\n- none usa la curva di ingresso.\n- aces, filmic e uncharted2 attenuano le alte luci per adattare splat luminosi a foto quasi bianche."),
    NL("Rendercurve voor de verliesberekening.\n- none gebruikt de invoercurve.\n- aces, filmic en uncharted2 dempen hoge lichten zodat heldere splats bij bijna witte foto’s passen."),
    RU("Кривая рендера для расчёта потерь.\n- none использует кривую входа.\n- aces, filmic и uncharted2 смягчают света, чтобы яркие сплаты подходили к почти белым снимкам."),
    TR("Kayıp karşılaştırmasında kullanılan render eğrisi.\n- none girdi eğrisini kullanır.\n- aces, filmic ve uncharted2 parlak alanları yumuşatıp parlak splatları beyaza yakın fotoğraflara uyarlar."));

SS_MSG(splat_color_gamut,
    EN("Splat color space"), JA("スプラットの色空間"),
    ZH_HANS("泼溅色彩空间"), ZH_HANT("潑濺色彩空間"), KO("스플랫 색 공간"),
    DE("Farbraum der Splats"), FR("Espace colorimétrique des splats"),
    ES("Espacio de color de los splats"), PT("Espaço de cor dos splats"),
    IT("Spazio colore degli splat"), NL("Kleurruimte van de splats"),
    RU("Цветовое пространство сплатов"), TR("Splat renk uzayı"));
SS_MSG(splat_color_gamut_help,
    EN("Stored splat color space; unset follows input. Wide gamut preserves saturated colors for grading but requires a compatible viewer. No tone mapping."),
    JA("スプラット保存用の色空間です。未指定は入力に従います。広色域は鮮やかな色を調整用に保ちますが、対応ビューアが必要です。トーンマッピングはしません。"),
    ZH_HANS("泼溅存储色彩空间，未设置跟随输入。广色域保留饱和色以便调色，但需兼容查看器。不做色调映射。"),
    ZH_HANT("潑濺儲存色彩空間，未設定跟隨輸入。廣色域保留飽和色以便調色，但需相容檢視器。不做色調映射。"),
    KO("저장할 스플랫 색 공간이며 미설정은 입력을 따릅니다. 넓은 색역은 조정용 포화색을 보존하지만 호환 뷰어가 필요합니다. 톤 매핑은 없습니다."),
    DE("Gespeicherter Splat-Farbraum; ungesetzt folgt Eingabe. Weites Gamut erhält satte Farben für Grading, benötigt passenden Viewer. Kein Tonemapping."),
    FR("Espace stocké des splats ; non défini suit l’entrée. Gamut large conserve les couleurs saturées pour étalonnage, mais exige un lecteur adapté. Pas de tone mapping."),
    ES("Espacio splat almacenado; sin definir sigue entrada. Gamut amplio preserva saturación para etalonar, pero exige visor compatible. Sin mapeo tonal."),
    PT("Espaço splat guardado; indefinido segue entrada. Gama ampla preserva saturação para gradação, mas exige visualizador compatível. Sem mapeamento tonal."),
    IT("Spazio splat salvato; non impostato segue l’ingresso. Gamut ampio conserva saturazione per grading, ma serve un viewer compatibile. Nessun tone mapping."),
    NL("Opgeslagen splatkleurruimte; oningesteld volgt invoer. Breed gamut behoudt verzadigde kleuren voor grading, vereist passende viewer. Geen tonemapping."),
    RU("Цветовое пространство сплатов; не задано — по входу. Широкий охват хранит насыщенные цвета для коррекции, но требует совместимого просмотра. Без тонмаппинга."),
    TR("Saklanan splat renk uzayı; boşsa girdiyi izler. Geniş gam düzenleme için doygun renkleri korur ama uyumlu görüntüleyici ister. Ton eşleme yoktur."));

SS_MSG(point_color_is_linear,
    EN("Seed point colors are linear"), JA("初期点群の色はリニア"),
    ZH_HANS("初始点云颜色为线性"), ZH_HANT("初始點雲顏色為線性"),
    KO("초기 포인트 색이 선형"), DE("Farben der Startpunkte sind linear"),
    FR("Couleurs des points initiaux linéaires"),
    ES("Los colores de los puntos iniciales son lineales"),
    PT("As cores dos pontos iniciais são lineares"),
    IT("I colori dei punti iniziali sono lineari"),
    NL("Kleuren van de startpunten zijn lineair"),
    RU("Цвета начальных точек линейные"),
    TR("Başlangıç nokta renkleri doğrusal"));
SS_MSG(point_color_is_linear_help,
    EN("Whether the seed point cloud's colors are linear light rather than display values.\n- Leave unset to follow the input images, which is where a reconstruction samples them.\n- The seeds are converted into the splat space wherever the two differ."),
    JA("初期点群の色が表示値ではなくリニア光かどうか。\n- 未設定なら、再構成が色を取得した入力画像に合わせます。\n- 色空間が異なる場合、初期点の色をスプラットの空間へ変換します。"),
    ZH_HANS("初始点云的颜色是否为线性光而不是显示值。\n- 不设置则跟随输入图像——重建正是从那里取色的。\n- 凡与泼溅空间不同之处，初始点都会被转换过去。"),
    ZH_HANT("初始點雲的顏色是否為線性光而不是顯示值。\n- 不設定則跟隨輸入影像——重建正是從那裡取色的。\n- 凡與潑濺空間不同之處，初始點都會被轉換過去。"),
    KO("초기 점군의 색이 표시 값 대신 선형 광인지 여부입니다.\n- 미설정 시 재구성이 색을 가져온 입력 이미지를 따릅니다.\n- 색 공간이 다르면 초기 점의 색을 스플랫 공간으로 변환합니다."),
    DE("Ob die Farben der Startpunktwolke lineares Licht statt Anzeigewerte sind.\n- Nicht gesetzt richtet es sich nach den Eingabebildern, aus denen eine Rekonstruktion sie entnimmt.\n- Wo beide Räume abweichen, werden die Startpunkte in den Splat-Raum umgerechnet."),
    FR("Indique si les couleurs du nuage de points initial sont de la lumière linéaire plutôt que des valeurs d'affichage.\n- Non défini, suit les images d'entrée, d'où une reconstruction les tire.\n- Partout où les deux espaces diffèrent, les points initiaux sont convertis vers celui des splats."),
    ES("Si los colores de la nube de puntos inicial son luz lineal en vez de valores de pantalla.\n- Sin definir, sigue a las imágenes de entrada, de donde una reconstrucción los toma.\n- Donde ambos espacios difieren, los puntos iniciales se convierten al de los splats."),
    PT("Se as cores da nuvem de pontos inicial são luz linear em vez de valores de exibição.\n- Sem definir, segue as imagens de entrada, de onde uma reconstrução as tira.\n- Onde os dois espaços diferem, os pontos iniciais são convertidos para o dos splats."),
    IT("Se i colori della nuvola di punti iniziale sono luce lineare anziché valori di visualizzazione.\n- Se non impostato, segue le immagini in ingresso, da cui una ricostruzione li prende.\n- Dove i due spazi differiscono, i punti iniziali vengono convertiti in quello degli splat."),
    NL("Of de kleuren van de startpuntenwolk lineair licht zijn in plaats van weergavewaarden.\n- Niet ingesteld volgt het de invoerbeelden, waaruit een reconstructie ze haalt.\n- Waar beide ruimtes verschillen, worden de startpunten naar die van de splats omgezet."),
    RU("Являются ли цвета исходного облака точек линейным светом, а не экранными значениями.\n- Без значения следует за входными изображениями, откуда их берёт реконструкция.\n- Где пространства различаются, начальные точки переводятся в пространство сплатов."),
    TR("Başlangıç nokta bulutunun renklerinin ekran değerleri yerine doğrusal ışık olup olmadığı.\n- Ayarlanmazsa, bir yeniden yapılandırmanın renkleri aldığı girdi görüntülerini izler.\n- İki uzayın farklı olduğu yerde başlangıç noktaları splat uzayına dönüştürülür."));

SS_MSG(point_color_transfer,
    EN("Seed point tone curve"), JA("初期点群のトーンカーブ"),
    ZH_HANS("初始点云色调曲线"), ZH_HANT("初始點雲色調曲線"),
    KO("초기 포인트 톤 커브"), DE("Tonkurve der Startpunkte"),
    FR("Courbe de tonalité des points initiaux"),
    ES("Curva tonal de los puntos iniciales"),
    PT("Curva tonal dos pontos iniciais"),
    IT("Curva tonale dei punti iniziali"),
    NL("Toonkromme van de startpunten"),
    RU("Тоновая кривая начальных точек"),
    TR("Başlangıç noktası ton eğrisi"));
SS_MSG(point_color_transfer_help,
    EN("Seed-color display curve; none follows input. If different from the splat curve, converts through display values to preserve the seeds’ appearance."),
    JA("初期点色の表示曲線です。noneは入力に従います。スプラットの曲線と違う場合、見た目を保つよう表示値を介して変換します。"),
    ZH_HANS("种子点颜色的显示曲线，none 跟随输入。与泼溅曲线不同时，经显示值转换以保持种子点外观。"),
    ZH_HANT("種子點色彩的顯示曲線，none 跟隨輸入。與潑濺曲線不同時，經顯示值轉換以保持種子點外觀。"),
    KO("시드 색 표시 곡선이며 none은 입력을 따릅니다. 스플랫 곡선과 다르면 표시 값을 거쳐 변환해 시드의 외관을 보존합니다."),
    DE("Anzeigekurve der Startpunktfarben; none folgt Eingabe. Bei anderer Splatkurve wird über Anzeigewerte umgerechnet, damit das Aussehen bleibt."),
    FR("Courbe d’affichage des points initiaux ; none suit l’entrée. Si différente des splats, conversion via valeurs d’affichage pour garder l’apparence."),
    ES("Curva de visualización de semillas; none sigue entrada. Si difiere de los splats, convierte vía valores de pantalla para preservar el aspecto."),
    PT("Curva de visualização das sementes; none segue entrada. Se difere dos splats, converte via valores de ecrã para preservar o aspeto."),
    IT("Curva display dei punti iniziali; none segue l’ingresso. Se diversa dagli splat, converte tramite valori display per conservarne l’aspetto."),
    NL("Schermcurve van startpuntkleuren; none volgt invoer. Bij andere splatcurve wordt via schermwaarden omgerekend om hun uiterlijk te behouden."),
    RU("Экранная кривая начальных точек; none — по входу. Если отличается от сплатов, преобразует через экранные значения, сохраняя вид точек."),
    TR("Tohum rengi ekran eğrisi; none girdiyi izler. Splat eğrisinden farklıysa görünümü korumak için ekran değerleri üzerinden dönüştürür."));

SS_MSG(point_color_gamut,
    EN("Seed point color space"), JA("初期点群の色空間"),
    ZH_HANS("初始点云色彩空间"), ZH_HANT("初始點雲色彩空間"),
    KO("초기 포인트 색 공간"), DE("Farbraum der Startpunkte"),
    FR("Espace colorimétrique des points initiaux"),
    ES("Espacio de color de los puntos iniciales"),
    PT("Espaço de cor dos pontos iniciais"),
    IT("Spazio colore dei punti iniziali"),
    NL("Kleurruimte van de startpunten"),
    RU("Цветовое пространство начальных точек"),
    TR("Başlangıç noktası renk uzayı"));
SS_MSG(point_color_gamut_help,
    EN("Color space of the seed point cloud's colors.\n- Leave unset to follow the input images.\n- Set Rec.709 for a cloud left in plain sRGB, such as one the built-in SfM wrote with its point colors in sRGB."),
    JA("初期点群の色の色空間です。\n- 未設定なら入力画像に合わせます。\n- 内蔵 SfM が点の色を sRGB で書いた場合など、素の sRGB のままの点群には Rec.709 を指定してください。"),
    ZH_HANS("初始点云颜色所在的色彩空间。\n- 不设置则跟随输入图像。\n- 点云保持普通 sRGB 时（例如内置 SfM 以 sRGB 写出点颜色）请设为 Rec.709。"),
    ZH_HANT("初始點雲顏色所在的色彩空間。\n- 不設定則跟隨輸入影像。\n- 點雲保持普通 sRGB 時（例如內建 SfM 以 sRGB 寫出點顏色）請設為 Rec.709。"),
    KO("초기 포인트 클라우드 색의 색 공간입니다.\n- 설정하지 않으면 입력 이미지를 따릅니다.\n- 내장 SfM이 점 색을 sRGB로 쓴 경우처럼 일반 sRGB로 남은 점 구름이라면 Rec.709로 설정하십시오."),
    DE("Farbraum der Startpunktwolke.\n- Unbelegt folgt den Eingabebildern.\n- Rec.709 für sRGB-Punktfarben, etwa aus dem eingebauten SfM."),
    FR("Espace colorimétrique des couleurs du nuage de points initial.\n- Non défini, suit les images d'entrée.\n- Choisir Rec.709 pour un nuage resté en sRGB simple, comme celui dont la SfM intégrée a écrit les couleurs en sRGB."),
    ES("Espacio de color de los colores de la nube de puntos inicial.\n- Sin definir, sigue a las imágenes de entrada.\n- Elija Rec.709 para una nube que quedó en sRGB simple, como una cuyos colores escribió en sRGB la SfM integrada."),
    PT("Espaço de cor das cores da nuvem de pontos inicial.\n- Sem definir, segue as imagens de entrada.\n- Escolha Rec.709 para uma nuvem que ficou em sRGB simples, como uma cujas cores o SfM integrado escreveu em sRGB."),
    IT("Spazio colore dei colori della nuvola di punti iniziale.\n- Se non impostato, segue le immagini in ingresso.\n- Scegliere Rec.709 per una nuvola rimasta in sRGB semplice, come una i cui colori la SfM integrata ha scritto in sRGB."),
    NL("Kleurruimte van de startpuntenwolk.\n- Leeg volgt de invoerbeelden.\n- Rec.709 voor sRGB-puntkleuren, bijvoorbeeld uit de ingebouwde SfM."),
    RU("Цветовое пространство цветов исходного облака точек.\n- Если не задано, следует за входными изображениями.\n- Выберите Rec.709 для облака, оставшегося в обычном sRGB, например такого, чьи цвета встроенный SfM записал в sRGB."),
    TR("Başlangıç nokta bulutunun renk uzayı.\n- Boşsa girdi görüntülerini izler.\n- Yerleşik SfM gibi sRGB nokta renkleri üreten bulutlar için Rec.709 seçin."));


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
    EN("Where decoded training images are kept between steps.\n`disk` re-reads them and uses the least memory, `cpu` keeps them in RAM for faster steps."),
    JA("デコードした学習画像をステップの間どこに置くかです。\n- `disk` は毎回読み直すのでメモリをいちばん使いません。\n- `cpu` は RAM に保持してステップを速くします。"),
    ZH_HANS("解码后的训练图像在两步之间存放在哪里。\n`disk` 每次重新读取，占用内存最少；`cpu` 把它们留在内存里，让每步更快。"),
    ZH_HANT("解碼後的訓練影像在兩步之間存放在哪裡。\n`disk` 每次重新讀取，佔用記憶體最少；`cpu` 把它們留在記憶體裡，讓每步更快。"),
    KO("디코딩한 학습 이미지를 스텝 사이에 어디에 둘지입니다.\n`disk`는 매번 다시 읽어 메모리를 가장 적게 쓰고, `cpu`는 RAM에 담아 스텝을 빠르게 합니다."),
    DE("Wo dekodierte Trainingsbilder zwischen den Schritten liegen.\n`disk` liest sie neu ein und braucht am wenigsten Speicher, `cpu` hält sie im RAM für schnellere Schritte."),
    FR("Où sont conservées les images d'entraînement décodées entre deux étapes.\n`disk` les relit et consomme le moins de mémoire, `cpu` les garde en RAM pour des étapes plus rapides."),
    ES("Dónde se guardan las imágenes de entrenamiento decodificadas entre pasos.\n`disk` vuelve a leerlas y usa la menor memoria, `cpu` las mantiene en RAM para pasos más rápidos."),
    PT("Onde as imagens de treinamento decodificadas ficam entre os passos.\n`disk` volta a lê-las e usa menos memória, `cpu` mantém-nas na RAM para passos mais rápidos."),
    IT("Dove restano le immagini di addestramento decodificate tra un passo e l'altro.\n`disk` le rilegge e usa meno memoria, `cpu` le tiene in RAM per passi più rapidi."),
    NL("Waar gedecodeerde trainingsbeelden tussen stappen blijven staan.\n`disk` leest ze opnieuw in en gebruikt het minste geheugen, `cpu` houdt ze in RAM voor snellere stappen."),
    RU("Где между шагами хранятся раскодированные обучающие изображения.\n`disk` перечитывает их и расходует меньше всего памяти, `cpu` держит их в ОЗУ ради более быстрых шагов."),
    TR("Kodu çözülmüş eğitim görüntülerinin adımlar arasında nerede tutulacağı.\n`disk` onları yeniden okur ve en az belleği kullanır, `cpu` daha hızlı adımlar için RAM'de tutar."));

SS_MSG(max_batch_per_epoch,
    EN("Steps per pass"), JA("1 巡あたりのステップ数"),
    ZH_HANS("每轮数据的步数"), ZH_HANT("每輪資料的步數"),
    KO("한 바퀴당 스텝 수"), DE("Schritte pro Durchlauf"),
    FR("Étapes par passage"), ES("Pasos por pasada"),
    PT("Passos por passagem"), IT("Passi per passaggio"),
    NL("Stappen per doorloop"), RU("Шагов за проход"),
    TR("Geçiş başına adım"));
SS_MSG(max_batch_per_epoch_help,
    EN("Target steps per dataset pass. Higher uses fewer images per step; lower is steadier but slower and uses more memory. Smaller datasets use one image per step."),
    JA("データセット1周の目標ステップ数です。高いと1ステップの画像が減り、低いと安定しますが遅くメモリが増えます。小規模データは1枚ずつです。"),
    ZH_HANS("每轮数据集的目标步数。高值每步用图更少；低值更稳但较慢、更耗内存。较小数据集每步一图。"),
    ZH_HANT("每輪資料集的目標步數。高值每步用圖更少；低值更穩但較慢、更耗記憶體。較小資料集每步一圖。"),
    KO("데이터셋 한 바퀴의 목표 단계 수입니다. 높으면 단계당 이미지가 적고, 낮으면 안정적이지만 느리고 메모리를 더 씁니다. 작은 데이터는 한 장씩입니다."),
    DE("Zielschritte je Datensatzdurchlauf. Höher: weniger Bilder je Schritt; niedriger: stabiler, langsamer, mehr Speicher. Kleine Datensätze: ein Bild je Schritt."),
    FR("Pas visés par parcours. Plus haut : moins d’images par pas ; plus bas : plus stable, lent et gourmand. Petits jeux : une image par pas."),
    ES("Pasos objetivo por recorrido. Más: menos imágenes por paso; menos: más estable, lento y usa más memoria. Datasets pequeños: una imagen por paso."),
    PT("Passos-alvo por passagem. Mais: menos imagens por passo; menos: mais estável, lento e usa mais memória. Conjuntos pequenos: uma imagem por passo."),
    IT("Passi obiettivo per giro. Più alto: meno immagini per passo; più basso: più stabile, lento e usa più memoria. Dataset piccoli: un’immagine per passo."),
    NL("Doelstappen per datasetronde. Hoger: minder beelden per stap; lager: stabieler, trager, meer geheugen. Kleine datasets: één beeld per stap."),
    RU("Шагов на проход набора. Больше — меньше снимков на шаг; меньше — стабильнее, медленнее, больше памяти. Малые наборы: снимок на шаг."),
    TR("Veri kümesi geçişi başına hedef adım. Yüksek: adımda az resim; düşük: daha kararlı, yavaş, fazla bellek. Küçük kümelerde adım başına bir resim."));

SS_MSG(min_renders_per_refine,
    EN("Min renders per round"),
    JA("ラウンドあたりの最小描画回数"),
    ZH_HANS("每轮最少渲染次数"),
    ZH_HANT("每輪最少算圖次數"),
    KO("회차당 최소 렌더 횟수"),
    DE("Mindestzahl Renderings pro Runde"),
    FR("Rendus minimaux par cycle"),
    ES("Renderizados mínimos por ronda"),
    PT("Renderizações mínimas por rodada"),
    IT("Rendering minimi per ciclo"),
    NL("Minimaal aantal renders per ronde"),
    RU("Мин. отрисовок за раунд"),
    TR("Tur başına en az işlenme"));
SS_MSG(min_renders_per_refine_help,
    EN("Sets images per step so a poorly seen splat at the render quantile gets this many renders per refinement. Estimated from seeds/frusta. 0 uses steps per pass."),
    JA("描画分位点の見えにくいスプラットが、細分化間にこの回数描かれるよう画像数を設定します。初期点と視錐台から推定。0は1周のステップ数に従います。"),
    ZH_HANS("设置每步图像数，使渲染分位点处少见的泼溅每次细化间渲染此次数。由种子和视锥估计。0 使用每轮步数规则。"),
    ZH_HANT("設定每步影像數，使算繪分位點處少見的潑濺每次細化間算繪此次數。由種子和視錐估計。0 使用每輪步數規則。"),
    KO("렌더 분위수의 잘 안 보이는 스플랫이 세분화 사이에 이만큼 렌더되도록 단계별 이미지 수를 정합니다. 시드·절두체로 추정합니다. 0은 순회 단계 규칙입니다."),
    DE("Bilder je Schritt so wählen, dass selten gesehene Splats am Renderquantil so oft je Verfeinerung erscheinen. Aus Startpunkten/Frusta geschätzt. 0: Schritte je Pass."),
    FR("Images par pas pour rendre autant les splats peu vus au quantile, entre raffinements. Estimé des points/frustums. 0 utilise les pas par parcours."),
    ES("Imágenes por paso para renderizar así los splats poco vistos del cuantil entre refinados. Estima desde semillas/frustums. 0 usa pasos por recorrido."),
    PT("Imagens por passo para renderizar assim os splats pouco vistos do quantil entre refinos. Estima das sementes/frustums. 0 usa passos por passagem."),
    IT("Immagini per passo affinché gli splat poco visti al quantile abbiano questi render tra affinamenti. Stima da semi/frusta. 0 usa passi per giro."),
    NL("Beelden per stap zodat slecht geziene splats op het renderkwantiel zo vaak tussen verfijningen verschijnen. Geschat uit startpunten/frusta. 0: stappen per ronde."),
    RU("Изображений на шаг, чтобы редко видимый сплат на квантиле рендера набрал столько рендеров между уточнениями. Оценка по точкам/фрустумам. 0: шаги за проход."),
    TR("Render kantilinde az görülen splatın iyileştirmeler arasında bu kadar çizilmesi için adım resim sayısı. Tohum/kesik koniden tahmin. 0: geçiş adımı kuralı."));

SS_MSG(render_quantile,
    EN("Render quantile"),
    JA("描画の分位点"),
    ZH_HANS("渲染分位数"),
    ZH_HANT("算圖分位數"),
    KO("렌더 분위수"),
    DE("Render-Quantil"),
    FR("Quantile de rendu"),
    ES("Cuantil de renderizado"),
    PT("Quantil de renderização"),
    IT("Quantile di rendering"),
    NL("Renderkwantiel"),
    RU("Квантиль отрисовок"),
    TR("İşlenme yüzdeliği"));
SS_MSG(render_quantile_help,
    EN("Which splat min_renders_per_refine protects: 0.1 is the splat seen by "
       "fewer cameras than 90% of the others."),
    JA("min_renders_per_refine がどのスプラットを守るかです。0.1 は、ほかの "
       "90% よりも少ないカメラにしか見えないスプラットです。"),
    ZH_HANS("min_renders_per_refine 保护哪个泼溅：0.1 指看到它的相机数少于其余"
            " 90% 泼溅的那个泼溅。"),
    ZH_HANT("min_renders_per_refine 保護哪個潑濺：0.1 指看到它的相機數少於其餘"
            " 90% 潑濺的那個潑濺。"),
    KO("min_renders_per_refine가 어느 스플랫을 보호할지입니다. 0.1은 나머지 "
       "90%보다 적은 카메라에 보이는 스플랫입니다."),
    DE("Welchen Splat min_renders_per_refine schützt: 0.1 ist der Splat, den "
       "weniger Kameras sehen als 90 % der übrigen."),
    FR("Le splat que protège min_renders_per_refine : 0.1 est celui que "
       "voient moins de caméras que 90 % des autres."),
    ES("Qué splat protege min_renders_per_refine: 0.1 es el que ven menos "
       "cámaras que al 90 % de los demás."),
    PT("Qual splat min_renders_per_refine protege: 0.1 é o splat visto por "
       "menos câmeras que 90% dos outros."),
    IT("Quale splat protegge min_renders_per_refine: 0.1 è lo splat visto da "
       "meno fotocamere del 90% degli altri."),
    NL("Welke splat min_renders_per_refine beschermt: 0.1 is de splat die "
       "door minder camera's wordt gezien dan 90% van de andere."),
    RU("Какой сплат защищает min_renders_per_refine: 0.1 — сплат, который "
       "видят меньше камер, чем 90% остальных."),
    TR("min_renders_per_refine'ın hangi splat'ı koruduğu: 0.1, diğerlerinin "
       "%90'ından daha az kameranın gördüğü splat'tır."));

SS_MSG(max_train_batch_size,
    EN("Max images per step"),
    JA("1 ステップの最大画像数"),
    ZH_HANS("每步最多图像数"),
    ZH_HANT("每步最多影像數"),
    KO("스텝당 최대 이미지 수"),
    DE("Max. Bilder pro Schritt"),
    FR("Images max. par étape"),
    ES("Máx. de imágenes por paso"),
    PT("Máx. de imagens por passo"),
    IT("Immagini max per passo"),
    NL("Max. beelden per stap"),
    RU("Макс. изображений на шаг"),
    TR("Adım başına en fazla görüntü"));
SS_MSG(max_train_batch_size_help,
    EN("Upper bound on the images one step may use under "
       "min_renders_per_refine, which is what bounds its memory and time."),
    JA("min_renders_per_refine のもとで 1 ステップが使える画像枚数の上限で、こ"
       "れがステップのメモリと時間を抑えます。"),
    ZH_HANS("在 min_renders_per_refine 下每步可使用的图像数上限，每步的内存和"
            "时间也由它限定。"),
    ZH_HANT("在 min_renders_per_refine 下每步可使用的影像數上限，每步的記憶體"
            "和時間也由它限定。"),
    KO("min_renders_per_refine 아래에서 한 스텝이 쓸 수 있는 이미지 수의 상한"
       "으로, 이것이 스텝의 메모리와 시간을 제한합니다."),
    DE("Obergrenze für die Bilder, die ein Schritt unter "
       "min_renders_per_refine nutzen darf; sie begrenzt seinen Speicher und "
       "seine Zeit."),
    FR("Plafond d'images qu'une étape peut utiliser sous "
       "min_renders_per_refine ; c'est ce qui borne sa mémoire et son temps."),
    ES("Límite de imágenes que un paso puede usar con min_renders_per_refine; "
       "es lo que acota su memoria y su tiempo."),
    PT("Limite de imagens que um passo pode usar sob min_renders_per_refine; "
       "é o que limita sua memória e seu tempo."),
    IT("Limite alle immagini che un passo può usare con "
       "min_renders_per_refine; è ciò che ne limita memoria e tempo."),
    NL("Bovengrens voor de beelden die één stap onder min_renders_per_refine "
       "mag gebruiken; die begrenst het geheugen en de tijd van de stap."),
    RU("Верхняя граница числа изображений, которое шаг может взять при "
       "min_renders_per_refine; именно она ограничивает его память и время."),
    TR("min_renders_per_refine altında bir adımın kullanabileceği görüntü "
       "sayısının üst sınırı; adımın belleğini ve süresini sınırlayan budur."));

SS_MSG(view_sampling,
    EN("View sampling"),
    JA("視点のサンプリング"),
    ZH_HANS("视角采样"),
    ZH_HANT("視角取樣"),
    KO("시점 샘플링"),
    DE("Ansichtsauswahl"),
    FR("Échantillonnage des vues"),
    ES("Muestreo de vistas"),
    PT("Amostragem de vistas"),
    IT("Campionamento delle viste"),
    NL("Selectie van aanzichten"),
    RU("Выбор ракурсов"),
    TR("Görünüm örnekleme"));
SS_MSG(view_sampling_help,
    EN("uniform visits every image once per pass. deficit favors images showing under-rendered splats, using actual contributions from the last render."),
    JA("uniformは1周で全画像を1回ずつ使います。deficitは前回の実際の描画寄与から、描画不足のスプラットを映す画像を優先します。"),
    ZH_HANS("uniform 每轮每图一次。deficit 根据上次渲染的实际贡献，优先选择显示渲染不足泼溅的图像。"),
    ZH_HANT("uniform 每輪每圖一次。deficit 根據上次算繪的實際貢獻，優先選擇顯示算繪不足潑濺的影像。"),
    KO("uniform은 순회마다 모든 이미지를 한 번 방문합니다. deficit은 직전 렌더의 실제 기여로 덜 렌더된 스플랫을 보여 주는 이미지를 우선합니다."),
    DE("uniform besucht jedes Bild einmal je Pass. deficit bevorzugt Bilder mit unterrenderten Splats anhand ihrer tatsächlichen Beiträge beim letzten Rendern."),
    FR("uniform visite chaque image une fois par parcours. deficit favorise les images de splats peu rendus selon leurs contributions réelles au dernier rendu."),
    ES("uniform visita cada imagen una vez por recorrido. deficit favorece imágenes con splats poco renderizados, según contribuciones reales del último render."),
    PT("uniform visita cada imagem uma vez por passagem. deficit favorece imagens com splats pouco renderizados, pelas contribuições reais do último render."),
    IT("uniform visita ogni immagine una volta per giro. deficit favorisce immagini di splat poco renderizzati, dai contributi reali dell’ultimo render."),
    NL("uniform bezoekt elk beeld één keer per ronde. deficit verkiest beelden van weinig gerenderde splats op basis van hun echte vorige renderbijdrage."),
    RU("uniform — каждый снимок раз за проход. deficit предпочитает снимки редко отрисованных сплатов по их реальному вкладу в прошлый рендер."),
    TR("uniform her resmi geçişte bir kez ziyaret eder. deficit, önceki gerçek render katkılarına göre az çizilen splatları gösteren resimleri öne alır."));

SS_MSG(view_deficit_power,
    EN("View deficit power"),
    JA("視点不足の指数"),
    ZH_HANS("视角不足指数"),
    ZH_HANT("視角不足指數"),
    KO("시점 부족 지수"),
    DE("Exponent des Ansichtsdefizits"),
    FR("Exposant du déficit de vues"),
    ES("Exponente del déficit de vistas"),
    PT("Expoente do déficit de vistas"),
    IT("Esponente del deficit di viste"),
    NL("Exponent van het aanzichttekort"),
    RU("Степень дефицита ракурсов"),
    TR("Görünüm açığı üssü"));
SS_MSG(view_deficit_power_help,
    EN("Exponent for deficit sampling. 1 weights images inversely to their splats’ render counts; 0.5 halves that pull."),
    JA("deficitサンプリングの指数です。1はスプラット描画回数に反比例して画像を重み付けし、0.5はその強さを半分にします。"),
    ZH_HANS("deficit 采样指数。1 按泼溅渲染次数的倒数加权图像；0.5 将这种作用减半。"),
    ZH_HANT("deficit 採樣指數。1 按潑濺算繪次數的倒數加權影像；0.5 將這種作用減半。"),
    KO("deficit 샘플링 지수입니다. 1은 스플랫 렌더 횟수에 반비례해 이미지를 가중하고, 0.5는 그 당김을 절반으로 합니다."),
    DE("Exponent für deficit-Sampling. 1 gewichtet Bilder umgekehrt zur Renderzahl ihrer Splats; 0.5 halbiert diesen Zug."),
    FR("Exposant deficit. 1 pondère les images à l’inverse du nombre de rendus de leurs splats ; 0.5 réduit cet effet de moitié."),
    ES("Exponente deficit. 1 pondera imágenes inversamente a los renders de sus splats; 0.5 reduce ese efecto a la mitad."),
    PT("Expoente deficit. 1 pesa imagens inversamente ao total de renders dos splats; 0.5 reduz esse efeito a metade."),
    IT("Esponente deficit. 1 pesa le immagini inversamente ai render dei loro splat; 0.5 dimezza tale effetto."),
    NL("Exponent voor deficit-sampling. 1 weegt beelden omgekeerd aan de renderaantallen van hun splats; 0.5 halveert die werking."),
    RU("Степень deficit. 1 взвешивает снимки обратно числу рендеров их сплатов; 0.5 ослабляет это действие вдвое."),
    TR("deficit örnekleme üssü. 1 resimleri splat render sayısına ters ağırlıklar; 0.5 bu çekimi yarıya indirir."));

SS_MSG(view_deficit_max_ratio,
    EN("View deficit max ratio"),
    JA("視点不足の最大比"),
    ZH_HANS("视角不足最大比"),
    ZH_HANT("視角不足最大比"),
    KO("시점 부족 최대 비율"),
    DE("Max. Verhältnis des Ansichtsdefizits"),
    FR("Rapport max. du déficit de vues"),
    ES("Razón máx. del déficit de vistas"),
    PT("Razão máx. do déficit de vistas"),
    IT("Rapporto max del deficit di viste"),
    NL("Max. verhouding van het aanzichttekort"),
    RU("Макс. отношение дефицита ракурсов"),
    TR("Görünüm açığı en büyük oranı"));
SS_MSG(view_deficit_max_ratio_help,
    EN("No image is drawn more than this many times as often, or this many "
       "times less often, than the median image under `deficit` view sampling."),
    JA("`deficit` の視点サンプリングでは、どの画像も中央値の画像と比べて、この"
       "倍率を超えて多く、または少なく選ばれることはありません。"),
    ZH_HANS("在 `deficit` 视角采样下，任何图像被抽中的频率，与中位图像相比都不"
            "会高出或低于这个倍数以上。"),
    ZH_HANT("在 `deficit` 視角取樣下，任何影像被抽中的頻率，與中位影像相比都不"
            "會高出或低於這個倍數以上。"),
    KO("`deficit` 시점 샘플링에서 어떤 이미지도 중앙값 이미지보다 이 배수를 넘"
       "게 더 자주, 또는 더 드물게 뽑히지 않습니다."),
    DE("Bei der Ansichtsauswahl `deficit` wird kein Bild mehr als so viele "
       "Male häufiger oder seltener gezogen als das Medianbild."),
    FR("Avec l'échantillonnage `deficit`, aucune image n'est tirée plus de ce "
       "nombre de fois plus souvent, ou moins souvent, que l'image médiane."),
    ES("Con el muestreo `deficit`, ninguna imagen sale más de este número de "
       "veces más a menudo, ni menos a menudo, que la imagen mediana."),
    PT("Com a amostragem `deficit`, nenhuma imagem é sorteada mais do que "
       "este número de vezes mais, ou menos, frequentemente que a imagem "
       "mediana."),
    IT("Con il campionamento `deficit`, nessuna immagine viene estratta più "
       "di questo numero di volte più spesso, o meno spesso, dell'immagine "
       "mediana."),
    NL("Bij selectie `deficit` wordt geen beeld meer dan zoveel keer vaker, "
       "of zoveel keer minder vaak, getrokken dan het mediane beeld."),
    RU("При выборе ракурсов `deficit` ни одно изображение не выбирается чаще "
       "или реже медианного больше чем во столько раз."),
    TR("`deficit` görünüm örneklemesinde hiçbir görüntü, medyan görüntüden bu "
       "kattan daha sık ya da daha seyrek seçilmez."));

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
    EN("Processes step images one at a time with the same result. Reduces peak GPU memory roughly in proportion to the number of images per step."),
    JA("同じ結果を保ちながらステップ内の画像を1枚ずつ処理します。ピークGPUメモリはステップ内の画像数にほぼ比例して減ります。"),
    ZH_HANS("逐张处理每步图像，结果相同。峰值 GPU 内存大致按每步图像数的比例降低。"),
    ZH_HANT("逐張處理每步影像，結果相同。峰值 GPU 記憶體大致按每步影像數的比例降低。"),
    KO("단계의 이미지를 한 장씩 처리해 결과는 같습니다. 최대 GPU 메모리가 단계당 이미지 수에 대략 비례해 줄어듭니다."),
    DE("Verarbeitet Schrittbilder einzeln bei gleichem Ergebnis. Senkt GPU-Spitzenspeicher ungefähr proportional zur Bildzahl je Schritt."),
    FR("Traite les images du pas une à une, résultat identique. Réduit le pic mémoire GPU à peu près proportionnellement au nombre d’images par pas."),
    ES("Procesa imágenes del paso de una en una, con igual resultado. Reduce el pico de memoria GPU aproximadamente según las imágenes por paso."),
    PT("Processa imagens do passo uma a uma, com o mesmo resultado. Reduz o pico de memória GPU aproximadamente conforme as imagens por passo."),
    IT("Elabora le immagini del passo una per volta, stesso risultato. Riduce il picco di memoria GPU circa in proporzione alle immagini per passo."),
    NL("Verwerkt stapbeelden één voor één met gelijk resultaat. Verlaagt GPU-piekgeheugen ongeveer evenredig met de beeldtelling per stap."),
    RU("Обрабатывает снимки шага по одному с тем же результатом. Снижает пик памяти GPU примерно пропорционально числу снимков в шаге."),
    TR("Adım resimlerini aynı sonuçla tek tek işler. En yüksek GPU belleğini adım resim sayısına yaklaşık orantılı azaltır."));

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
    EN("Combines the backward pass and parameter update to save memory at high splat counts. Automatically disabled with split_batch."),
    JA("逆伝播とパラメーター更新を統合し、大量のスプラットでのメモリ使用を減らします。split_batch 使用時は自動的に無効になります。"),
    ZH_HANS("合并反向传播与参数更新，减少大量点时的内存占用。使用 split_batch 时自动关闭。"),
    ZH_HANT("合併反向傳播與參數更新，減少大量點時的記憶體用量。使用 split_batch 時自動關閉。"),
    KO("역전파와 매개변수 갱신을 합쳐 스플랫이 많을 때 메모리를 절약합니다. split_batch 사용 시 자동으로 꺼집니다."),
    DE("Vereint Rückwärtspass und Parameterupdate, um bei vielen Splats Speicher zu sparen. Mit split_batch automatisch deaktiviert."),
    FR("Fusionne rétropropagation et mise à jour pour économiser la mémoire avec beaucoup de splats. Désactivé automatiquement avec split_batch."),
    ES("Combina retropropagación y actualización para ahorrar memoria con muchos splats. Se desactiva automáticamente con split_batch."),
    PT("Combina retropropagação e atualização para poupar memória com muitos splats. Desativado automaticamente com split_batch."),
    IT("Unisce retropropagazione e aggiornamento per risparmiare memoria con molti splat. Disattivato automaticamente con split_batch."),
    NL("Combineert terugpropagatie en parameterupdate om bij veel splats geheugen te besparen. Automatisch uit met split_batch."),
    RU("Объединяет обратный проход и обновление параметров, экономя память при большом числе сплатов. Автоматически отключается с split_batch."),
    TR("Çok sayıda splatta bellek kazanmak için geri yayılım ve parametre güncellemesini birleştirir. split_batch ile otomatik kapanır."));

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
    EN("Tile edge in pixels: power of two, 8–128. Larger reduces sorting pairs/memory but increases raster work. 0 auto-sizes and coarsens if intersections overflow."),
    JA("タイル辺のピクセル数は8〜128の2の累乗です。大きいとソート・メモリが減り、描画作業が増えます。0は自動調整し、交差数超過時は粗くします。"),
    ZH_HANS("瓦片边长，像素，8–128 的二次幂。更大减少排序对数和内存，但增加光栅工作。0 自动选择，交集溢出时进一步放大。"),
    ZH_HANT("瓦片邊長，像素，8–128 的二次冪。更大減少排序對數和記憶體，但增加光柵工作。0 自動選擇，交集溢出時進一步放大。"),
    KO("타일 변의 픽셀 수: 8~128의 2제곱수. 크면 정렬·메모리가 줄지만 래스터 작업은 늘어납니다. 0은 자동 조정하며 교차 수 초과 시 더 키웁니다."),
    DE("Kachelkante in Pixeln: Zweierpotenz, 8–128. Größer senkt Sortierpaare/Speicher, erhöht Rasterarbeit. 0 wählt automatisch und vergröbert bei Überlauf."),
    FR("Côté de tuile en pixels : puissance de 2, 8–128. Plus grand réduit tri/mémoire, augmente le raster. 0 adapte et grossit si trop d’intersections."),
    ES("Lado de tesela en píxeles: potencia de 2, 8–128. Mayor reduce pares/memoria de ordenación, aumenta ráster. 0 ajusta y agranda si desbordan intersecciones."),
    PT("Lado do bloco em píxeis: potência de 2, 8–128. Maior reduz pares/memória de ordenação, aumenta raster. 0 ajusta e aumenta se as interseções excedem o limite."),
    IT("Lato tile in pixel: potenza di 2, 8–128. Maggiore riduce coppie/memoria di ordinamento, aumenta raster. 0 adatta e ingrandisce se troppe intersezioni."),
    NL("Tegelrand in pixels: macht van 2, 8–128. Groter verlaagt sorteerparen/geheugen, verhoogt rasterwerk. 0 past aan en vergroot bij te veel intersecties."),
    RU("Ребро тайла в пикселях: степень 2, 8–128. Больше — меньше пар/памяти сортировки, больше растра. 0 подбирает и укрупняет при избытке пересечений."),
    TR("Piksel cinsinden karo kenarı: 2 kuvveti, 8–128. Büyük: az sıralama çifti/belleği, fazla raster işi. 0 otomatik ayarlar, kesişimler taşarsa büyütür."));
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
    EN("1 roughly halves view-dependent color memory and uses half-precision render/loss images, with little visible change. 0 keeps full precision."),
    JA("1は視点依存色のメモリをほぼ半減し、描画・損失間の画像を半精度にします。見た目の変化は小さく、0は完全精度を保ちます。"),
    ZH_HANS("1 将视点相关颜色内存约减半，渲染与损失间的图像用半精度，外观变化很小。0 保留完整精度。"),
    ZH_HANT("1 將視點相關色彩記憶體約減半，算繪與損失間的影像用半精度，外觀變化很小。0 保留完整精度。"),
    KO("1은 시점 의존 색 메모리를 약 절반으로 줄이고 렌더·손실 사이 이미지를 반정밀도로 저장해 외관 변화가 작습니다. 0은 전체 정밀도입니다."),
    DE("1 halbiert etwa den Speicher für blickabhängige Farbe und nutzt halbpräzise Render-/Verlustbilder bei wenig sichtbarer Änderung. 0 behält volle Präzision."),
    FR("1 divise environ par deux la mémoire couleur dépendante de la vue et utilise des images rendu/perte en demi-précision, peu visible. 0 garde la pleine précision."),
    ES("1 reduce a la mitad memoria de color por vista e imágenes render/pérdida en media precisión, con poco cambio visible. 0 conserva precisión completa."),
    PT("1 reduz a metade memória de cor por vista e usa imagens render/perda em meia precisão, com pouca mudança visível. 0 mantém precisão total."),
    IT("1 dimezza circa memoria colore dipendente dalla vista e usa immagini render/perdita a metà precisione, con poche differenze visive. 0 mantiene piena precisione."),
    NL("1 halveert ongeveer geheugen voor kijkafhankelijke kleur en gebruikt halfprecieze render/verliesbeelden met weinig zichtbaar verschil. 0 behoudt volle precisie."),
    RU("1 примерно вдвое уменьшает память цвета по ракурсам и хранит изображения рендера/потерь в половинной точности, почти без видимой разницы. 0 — полная точность."),
    TR("1, bakışa bağlı renk belleğini yaklaşık yarılar ve render/kayıp resimlerinde az görünür farkla yarım hassasiyet kullanır. 0 tam hassasiyeti korur."));

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
    EN("Reserves memory for the maximum splat count at startup. Avoids later GPU allocation failure as splats grow, but holds the memory from the start."),
    JA("開始時に最大スプラット数分のメモリを確保します。増加時のGPUメモリ不足を避けますが、そのメモリを最初から保持します。"),
    ZH_HANS("启动时为最大泼溅数预留内存。避免增长时 GPU 分配失败，但从一开始就占用该内存。"),
    ZH_HANT("啟動時為最大潑濺數預留記憶體。避免增長時 GPU 配置失敗，但從一開始就占用該記憶體。"),
    KO("시작 때 최대 스플랫 수의 메모리를 예약합니다. 증가 중 GPU 할당 실패를 피하지만 처음부터 그 메모리를 점유합니다."),
    DE("Reserviert Speicher für die maximale Splatzahl beim Start. Vermeidet spätere GPU-Zuweisungsfehler beim Wachstum, belegt den Speicher aber von Anfang an."),
    FR("Réserve au départ la mémoire du nombre maximal de splats. Évite les échecs GPU pendant la croissance, mais occupe cette mémoire dès le début."),
    ES("Reserva al inicio memoria para el máximo de splats. Evita fallos GPU posteriores al crecer, pero ocupa esa memoria desde el comienzo."),
    PT("Reserva no início memória para o máximo de splats. Evita falhas GPU posteriores ao crescer, mas ocupa essa memória desde o começo."),
    IT("Riserva all’avvio memoria per il massimo di splat. Evita errori GPU durante la crescita, ma occupa tale memoria dall’inizio."),
    NL("Reserveert bij de start geheugen voor het maximale aantal splats. Vermijdt latere GPU-toewijzingsfouten bij groei, maar bezet dat geheugen vanaf het begin."),
    RU("Резервирует память под максимум сплатов при запуске. Избегает позднего отказа выделения GPU при росте, но занимает память с начала."),
    TR("Başlangıçta en çok splat için bellek ayırır. Büyümede sonraki GPU ayırma hatalarını önler, ama belleği baştan tutar."));


// ===========================================================================
// Learning Rates
// ===========================================================================

SS_MSG(projective_depth_lr,
    EN("Depth learning rate"), JA("奥行きの学習率"), ZH_HANS("深度学习率"),
    ZH_HANT("深度學習率"), KO("깊이 학습률"), DE("Tiefenlernrate"),
    FR("Taux d'apprentissage de profondeur"), ES("Tasa de aprendizaje de profundidad"),
    PT("Taxa de aprendizado de profundidade"), IT("Tasso di apprendimento della profondità"),
    NL("Leersnelheid voor diepte"), RU("Скорость обучения глубины"), TR("Derinlik öğrenme oranı"));
SS_MSG(projective_depth_lr_help,
    EN("How quickly uncertain geometry adjusts its depth. Set to 0 to disable."),
    JA("不確かな形状の奥行きを調整する速さです。0 で無効になります。"),
    ZH_HANS("不确定几何体调整深度的速度。设为 0 可禁用。"),
    ZH_HANT("不確定幾何體調整深度的速度。設為 0 可停用。"),
    KO("불확실한 형상의 깊이를 조정하는 속도입니다. 0으로 설정하면 비활성화됩니다."),
    DE("Wie schnell unsichere Geometrie ihre Tiefe anpasst. 0 deaktiviert die Anpassung."),
    FR("Vitesse d'ajustement de la profondeur des géométries incertaines. 0 désactive l'ajustement."),
    ES("Rapidez con la que la geometría incierta ajusta su profundidad. 0 desactiva el ajuste."),
    PT("Rapidez com que a geometria incerta ajusta sua profundidade. 0 desativa o ajuste."),
    IT("Velocità di regolazione della profondità della geometria incerta. 0 disattiva la regolazione."),
    NL("Hoe snel onzekere geometrie de diepte aanpast. Zet op 0 om dit uit te schakelen."),
    RU("Скорость уточнения глубины неопределённой геометрии. 0 отключает уточнение."),
    TR("Belirsiz geometrinin derinliğini ayarlama hızı. Devre dışı bırakmak için 0 yapın."));

SS_MSG(projective_depth_lr_final,
    EN("Final depth learning rate"), JA("最後の奥行きの学習率"), ZH_HANS("最终深度学习率"),
    ZH_HANT("最終深度學習率"), KO("최종 깊이 학습률"), DE("Abschließende Tiefenlernrate"),
    FR("Taux final d'apprentissage de profondeur"), ES("Tasa final de aprendizaje de profundidad"),
    PT("Taxa final de aprendizado de profundidade"), IT("Tasso finale di apprendimento della profondità"),
    NL("Uiteindelijke leersnelheid voor diepte"), RU("Итоговая скорость обучения глубины"),
    TR("Son derinlik öğrenme oranı"));
SS_MSG(projective_depth_lr_final_help,
    EN("Depth learning rate at the end of training. Set to none to keep it constant."),
    JA("学習終了時の奥行きの学習率です。none で一定に保ちます。"),
    ZH_HANS("训练结束时的深度学习率。设为 none 可保持不变。"),
    ZH_HANT("訓練結束時的深度學習率。設為 none 可保持不變。"),
    KO("학습 종료 시의 깊이 학습률입니다. none으로 설정하면 일정하게 유지됩니다."),
    DE("Tiefenlernrate am Ende des Trainings. none hält sie konstant."),
    FR("Taux d'apprentissage de profondeur en fin d'entraînement. none le garde constant."),
    ES("Tasa de aprendizaje de profundidad al final del entrenamiento. none la mantiene constante."),
    PT("Taxa de aprendizado de profundidade no fim do treinamento. none mantém a taxa constante."),
    IT("Tasso di apprendimento della profondità a fine addestramento. none lo mantiene costante."),
    NL("Leersnelheid voor diepte aan het einde van de training. none houdt deze constant."),
    RU("Скорость обучения глубины в конце обучения. none сохраняет её постоянной."),
    TR("Eğitim sonundaki derinlik öğrenme oranı. Sabit tutmak için none yapın."));

SS_MSG(means_lr,
    EN("Movement rate"), JA("移動の学習率"), ZH_HANS("移动学习率"),
    ZH_HANT("移動學習率"), KO("이동 학습률"), DE("Lernrate der Bewegung"),
    FR("Taux de déplacement"), ES("Tasa de movimiento"),
    PT("Taxa de movimento"), IT("Tasso di movimento"),
    NL("Bewegingssnelheid"), RU("Скорость перемещения"), TR("Hareket oranı"));
SS_MSG(means_lr_help,
    EN("How fast splats move through space.\n- Higher rearranges geometry quickly but can jitter and blur\n- lower stays closer to the starting point cloud."),
    JA("スプラットが空間をどれだけ速く動くかです。\n- 高いほどジオメトリが早く組み替わりますが、ぶれてぼやけることがあります。\n- 低いほど初期の点群に近いまま残ります。"),
    ZH_HANS("泼溅在空间中移动的快慢。\n- 数值越高，几何重排越快，但可能抖动和发糊\n- 越低则更贴近初始点云。"),
    ZH_HANT("潑濺在空間中移動的快慢。\n- 數值越高，幾何重排越快，但可能抖動和發糊\n- 越低則更貼近初始點雲。"),
    KO("스플랫이 공간에서 얼마나 빨리 움직이는지입니다.\n값이 크면 지오메트리가 빨리 재배치되지만 흔들리고 뭉개질 수 있고, 작으면 초기 포인트 클라우드에 가깝게 남습니다."),
    DE("Wie schnell sich Splats durch den Raum bewegen.\n- Höher ordnet die Geometrie rasch um, kann aber zittern und verwischen\n- niedriger bleibt näher an der Startpunktwolke."),
    FR("À quelle vitesse les splats se déplacent dans l'espace.\n- Plus haut réorganise vite la géométrie mais peut trembler et flouter\n- plus bas reste proche du nuage de points de départ."),
    ES("Con qué rapidez se mueven los splats por el espacio.\n- Más alto reordena la geometría deprisa pero puede temblar y emborronar\n- más bajo se queda cerca de la nube de puntos inicial."),
    PT("Com que rapidez os splats se movem pelo espaço.\n- Mais alto reorganiza a geometria depressa, mas pode tremer e borrar\n- mais baixo fica perto da nuvem de pontos inicial."),
    IT("Con quanta rapidità gli splat si muovono nello spazio.\n- Più alto riorganizza in fretta la geometria ma può tremolare e sfocare\n- più basso resta vicino alla nuvola di punti di partenza."),
    NL("Hoe snel splats zich door de ruimte verplaatsen.\n- Hoger herschikt de geometrie snel maar kan trillen en vervagen\n- lager blijft dichter bij de beginpuntenwolk."),
    RU("Насколько быстро сплаты перемещаются в пространстве.\n- Больше — геометрия перестраивается быстро, но может дрожать и мылить\n- меньше — остаётся ближе к исходному облаку точек."),
    TR("Splat'ların uzayda ne kadar hızlı hareket ettiği.\n- Yüksek değerler geometriyi çabuk yeniden düzenler ama titreyip bulanıklaşabilir\n- düşük değerler başlangıç nokta bulutuna yakın kalır."));

SS_MSG(means_lr_final,
    EN("Final movement rate"), JA("最後の移動の学習率"),
    ZH_HANS("最终移动学习率"), ZH_HANT("最終移動學習率"),
    KO("최종 이동 학습률"), DE("Lernrate der Bewegung am Ende"),
    FR("Taux de déplacement final"), ES("Tasa de movimiento final"),
    PT("Taxa de movimento final"), IT("Tasso di movimento finale"),
    NL("Bewegingssnelheid aan het eind"),
    RU("Итоговая скорость перемещения"), TR("Son hareket oranı"));
SS_MSG(means_lr_final_help,
    EN("How fast splats move by the end of training.\n- Positions ease to a stop so detail can settle.\n- Set to none to keep the rate constant."),
    JA("学習の終わりごろにスプラットがどれだけ速く動くかです。\n- 位置がゆっくり止まっていき、細部が落ち着きます。\n- none にすると学習率は一定のままになります。"),
    ZH_HANS("训练接近结束时泼溅移动的快慢。\n- 位置会缓缓停下，让细节稳定下来。\n- 设为 none 可保持学习率不变。"),
    ZH_HANT("訓練接近結束時潑濺移動的快慢。\n- 位置會緩緩停下，讓細節穩定下來。\n- 設為 none 可保持學習率不變。"),
    KO("학습이 끝날 무렵 스플랫이 얼마나 빨리 움직이는지입니다.\n- 위치가 서서히 멈춰 디테일이 자리 잡습니다.\n- none으로 두면 학습률이 일정하게 유지됩니다."),
    DE("Wie schnell sich Splats gegen Ende des Trainings bewegen.\n- Die Positionen laufen sanft aus, damit sich Detail setzen kann.\n- Auf none setzen, um die Rate konstant zu halten."),
    FR("À quelle vitesse les splats se déplacent en fin d'entraînement.\n- Les positions s'immobilisent en douceur pour que le détail se fixe.\n- Mettre à none pour garder un taux constant."),
    ES("Con qué rapidez se mueven los splats al final del entrenamiento.\n- Las posiciones se detienen con suavidad para que el detalle se asiente.\n- Póngalo en none para mantener la tasa constante."),
    PT("Com que rapidez os splats se movem no fim do treinamento.\n- As posições param suavemente para o detalhe assentar.\n- Defina como none para manter a taxa constante."),
    IT("Con quanta rapidità gli splat si muovono verso la fine dell'addestramento.\n- Le posizioni si fermano dolcemente così il dettaglio si assesta.\n- Impostare a none per tenere il tasso costante."),
    NL("Hoe snel splats aan het eind van de training bewegen.\n- De posities komen zachtjes tot stilstand zodat detail kan bezinken.\n- Op none zetten om de snelheid constant te houden."),
    RU("Насколько быстро сплаты движутся к концу обучения.\n- Позиции плавно останавливаются, чтобы детали устоялись.\n- Установите none, чтобы скорость оставалась постоянной."),
    TR("Eğitimin sonuna doğru splat'ların ne kadar hızlı hareket ettiği.\n- Konumlar yumuşakça durur ve ayrıntı oturur.\n- Oranı sabit tutmak için none yapın."));

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
    EN("How fast splat transparency changes.\n- Higher clears haze and prunes faint splats sooner\n- lower gives weak structure more time to prove itself."),
    JA("スプラットの透明度がどれだけ速く変わるかです。\n- 高いほどもやが早く消え、薄いスプラットも早く整理されます。\n- 低いほど弱い構造に自分を証明する時間が与えられます。"),
    ZH_HANS("泼溅透明度变化的快慢。\n- 数值越高，雾感消散和淡泼溅的清除越早\n- 越低则给弱结构更多时间证明自己。"),
    ZH_HANT("潑濺透明度變化的快慢。\n- 數值越高，霧感消散和淡潑濺的清除越早\n- 越低則給弱結構更多時間證明自己。"),
    KO("스플랫 투명도가 얼마나 빨리 변하는지입니다.\n값이 크면 안개가 빨리 걷히고 흐린 스플랫이 일찍 정리되며, 작으면 약한 구조에 스스로를 증명할 시간이 더 주어집니다."),
    DE("Wie schnell sich die Transparenz der Splats ändert.\n- Höher räumt Schleier weg und entfernt blasse Splats früher\n- niedriger gibt schwacher Struktur mehr Zeit, sich zu bewähren."),
    FR("À quelle vitesse la transparence des splats change.\n- Plus haut dissipe le voile et élague plus tôt les splats pâles\n- plus bas laisse aux structures faibles le temps de faire leurs preuves."),
    ES("Con qué rapidez cambia la transparencia de los splats.\n- Más alto disipa la neblina y poda antes los splats tenues\n- más bajo da a la estructura débil más tiempo para demostrar su valía."),
    PT("Com que rapidez a transparência dos splats muda.\n- Mais alto dissipa a névoa e poda mais cedo os splats fracos\n- mais baixo dá à estrutura frágil mais tempo para se provar."),
    IT("Con quanta rapidità cambia la trasparenza degli splat.\n- Più alto dissolve la foschia e pota prima gli splat deboli\n- più basso lascia alle strutture fragili più tempo per dimostrare il proprio valore."),
    NL("Hoe snel de doorzichtigheid van splats verandert.\n- Hoger ruimt waas op en snoeit vage splats eerder weg\n- lager geeft zwakke structuur meer tijd om zich te bewijzen."),
    RU("Насколько быстро меняется прозрачность сплатов.\n- Больше — дымка уходит и бледные сплаты вычищаются раньше\n- меньше — слабой структуре даётся больше времени показать себя."),
    TR("Splat saydamlığının ne kadar hızlı değiştiği.\n- Yüksek değerler pusu giderir ve soluk splat'ları erken budar\n- düşük değerler zayıf yapıya kendini kanıtlaması için daha çok süre tanır."));

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

SS_MSG(features_sb_lr,
    EN("Highlight lobe rate"), JA("ハイライトのローブの学習率"),
    ZH_HANS("高光波瓣学习率"), ZH_HANT("高光波瓣學習率"),
    KO("하이라이트 로브 학습률"), DE("Lernrate der Glanzlicht-Keulen"),
    FR("Taux des lobes de reflets"), ES("Tasa de los lóbulos de brillo"),
    PT("Taxa dos lóbulos de brilho"), IT("Tasso dei lobi di riflesso"),
    NL("Snelheid van de glanslobben"), RU("Скорость лепестков бликов"),
    TR("Parlama lobu oranı"));
SS_MSG(features_sb_lr_help,
    EN("How fast the spherical-beta lobes change. Their color, direction and "
       "sharpness share this rate."),
    JA("球面ベータのローブがどれだけ速く変わるかです。色、向き、鋭さがこの学"
       "習率を共有します。"),
    ZH_HANS("球面 Beta 波瓣变化的快慢。它们的颜色、方向和锐度共用这个学习率。"),
    ZH_HANT("球面 Beta 波瓣變化的快慢。它們的顏色、方向和銳度共用這個學習率。"),
    KO("구면 베타 로브가 얼마나 빨리 변하는지입니다. 색, 방향, 날카로움이 이 "
       "학습률을 함께 씁니다."),
    DE("Wie schnell sich die sphärischen Beta-Keulen ändern. Farbe, Richtung "
       "und Schärfe teilen sich diese Rate."),
    FR("À quelle vitesse les lobes bêta sphériques changent. Leur couleur, leur"
       " direction et leur netteté partagent ce taux."),
    ES("Con qué rapidez cambian los lóbulos beta esféricos. Su color, su "
       "dirección y su nitidez comparten esta tasa."),
    PT("Com que rapidez os lóbulos beta esféricos mudam. A cor, a direção e a "
       "nitidez deles compartilham esta taxa."),
    IT("Quanto in fretta cambiano i lobi beta sferici. Colore, direzione e "
       "nitidezza condividono questo tasso."),
    NL("Hoe snel de sferische bètalobben veranderen. Hun kleur, richting en "
       "scherpte delen deze snelheid."),
    RU("Как быстро меняются сферические бета-лепестки. Их цвет, направление и "
       "резкость делят эту скорость."),
    TR("Küresel beta loblarının ne hızla değiştiği. Renkleri, yönleri ve "
       "keskinlikleri bu oranı paylaşır."));
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
    EN("Learning-rate schedule length in steps. Unset follows num_iterations; set separately to keep the same schedule when changing run length."),
    JA("学習率スケジュールの長さ（ステップ数）。未指定なら num_iterations に合わせます。実行の長さを変えても同じスケジュールを使うには別に設定します。"),
    ZH_HANS("学习率计划的步数。留空时跟随 num_iterations；单独设置可在改变训练长度时保持原计划。"),
    ZH_HANT("學習率排程的步數。留空時跟隨 num_iterations；單獨設定可在改變訓練長度時保持原排程。"),
    KO("학습률 일정의 스텝 수입니다. 미설정 시 num_iterations를 따릅니다. 실행 길이를 바꿔도 같은 일정을 유지하려면 따로 설정하세요."),
    DE("Länge des Lernratenplans in Schritten. Unbelegt folgt num_iterations; separat setzen, um den Plan bei geänderter Laufdauer beizubehalten."),
    FR("Durée du programme de taux en pas. Vide suit num_iterations ; définir séparément pour garder ce programme si la durée d’entraînement change."),
    ES("Duración del plan de tasas en pasos. Sin definir sigue num_iterations; fíjalo aparte para mantener el plan al cambiar la duración del entrenamiento."),
    PT("Duração do plano de taxas em passos. Vazio segue num_iterations; defina à parte para manter o plano ao mudar a duração do treino."),
    IT("Durata del piano dei tassi in passi. Vuoto segue num_iterations; impostare a parte per mantenere il piano cambiando la durata della sessione."),
    NL("Lengte van het leersnelheidsschema in stappen. Leeg volgt num_iterations; stel apart in om het schema te behouden bij een andere runduur."),
    RU("Длина графика скорости обучения в шагах. Не задано: num_iterations. Задайте отдельно, чтобы сохранить график при изменении длины обучения."),
    TR("Öğrenme hızı takviminin adım uzunluğu. Boşsa num_iterations izlenir; çalışma süresi değişince aynı takvimi korumak için ayrıca ayarlayın."));

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
    EN("Makes splat movement independent of scene size. Off scales movement with the scene, as in original 3DGS."),
    JA("スプラットの移動速度をシーンの大きさに依存させません。オフでは元の3DGS同様、シーンに合わせて変わります。"),
    ZH_HANS("让泼溅移动速度不依赖场景大小。关闭则像原始 3DGS 一样随场景缩放。"),
    ZH_HANT("讓潑濺移動速度不依賴場景大小。關閉則像原始 3DGS 一樣隨場景縮放。"),
    KO("스플랫 이동을 장면 크기와 무관하게 합니다. 끄면 원래 3DGS처럼 장면에 따라 이동이 비례합니다."),
    DE("Macht Splatbewegung unabhängig von der Szenengröße. Aus skaliert die Bewegung mit der Szene, wie ursprüngliches 3DGS."),
    FR("Rend le mouvement des splats indépendant de la taille de scène. Désactivé le met à l’échelle de la scène, comme 3DGS d’origine."),
    ES("Hace el movimiento splat independiente del tamaño de escena. Desactivado lo escala con la escena, como 3DGS original."),
    PT("Torna o movimento splat independente do tamanho da cena. Desligado escala com a cena, como 3DGS original."),
    IT("Rende il movimento splat indipendente dalla dimensione della scena. Spento scala con la scena, come 3DGS originale."),
    NL("Maakt splatbeweging onafhankelijk van scènegrootte. Uit schaalt beweging met de scène, zoals oorspronkelijk 3DGS."),
    RU("Движение сплатов не зависит от размера сцены. Выкл. масштабирует его со сценой, как исходный 3DGS."),
    TR("Splat hareketini sahne boyutundan bağımsız kılar. Kapalıyken özgün 3DGS gibi sahneyle ölçeklenir."));

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
    EN("Restarts optimizer bias correction for new splats so they move at full speed rather than inheriting their parent’s momentum, sharpening detail faster."),
    JA("新スプラットの最適化バイアス補正を再開始し、親の勢いを継がず全速で動かして細部を早く鮮明にします。"),
    ZH_HANS("为新泼溅重启优化器偏差校正，使其全速移动而非继承父级动量，更快锐化细节。"),
    ZH_HANT("為新潑濺重啟最佳化器偏差校正，使其全速移動而非繼承父級動量，更快銳化細節。"),
    KO("새 스플랫의 최적화 편향 보정을 다시 시작해 부모 모멘텀 대신 최대 속도로 움직여 세부를 빠르게 선명하게 합니다."),
    DE("Startet Optimierer-Biaskorrektur für neue Splats neu, damit sie mit voller Geschwindigkeit statt geerbtem Impuls arbeiten und Details schneller schärfen."),
    FR("Relance la correction de biais de l’optimiseur des nouveaux splats : pleine vitesse au lieu de l’élan hérité, détails affinés plus vite."),
    ES("Reinicia corrección de sesgo del optimizador en splats nuevos: plena velocidad sin momento heredado, detalle más rápido."),
    PT("Reinicia correção de viés do otimizador em splats novos: velocidade total sem momento herdado, detalhe mais rápido."),
    IT("Riavvia la correzione del bias dell’ottimizzatore per nuovi splat: piena velocità senza momento ereditato, dettagli più rapidi."),
    NL("Herstart optimizerbiascorrectie voor nieuwe splats: volle snelheid in plaats van geërfd momentum, waardoor details sneller verscherpen."),
    RU("Сбрасывает коррекцию смещения оптимизатора новых сплатов: полная скорость вместо унаследованного импульса, быстрее уточняются детали."),
    TR("Yeni splatlarda iyileştirici yanlılık düzeltmesini başlatır: kalıtsal momentum yerine tam hızla hareket ederek ayrıntıyı daha çabuk netleştirir."));


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

SS_MSG(choice_extreme,
    EN("extreme"),       JA("最大"),          ZH_HANS("极强"),     ZH_HANT("極強"),
    KO("매우 강하게"),    DE("extrem"),       FR("extrême"),      ES("extrema"),
    PT("extrema"),       IT("estrema"),      NL("extreem"),      RU("экстремальное"),
    TR("aşırı"));

SS_MSG(choice_basic,
    EN("basic"),         JA("基本"),          ZH_HANS("基本"),     ZH_HANT("基本"),
    KO("기본"),           DE("einfach"),      FR("simple"),       ES("básica"),
    PT("básica"),        IT("base"),         NL("eenvoudig"),    RU("базовая"),
    TR("temel"));

SS_MSG(choice_accurate,
    EN("accurate"),      JA("高精度"),        ZH_HANS("精确"),     ZH_HANT("精確"),
    KO("정밀"),           DE("genau"),        FR("précise"),      ES("precisa"),
    PT("precisa"),       IT("accurata"),     NL("nauwkeurig"),   RU("точная"),
    TR("hassas"));

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

SS_MSG(choice_none,
    EN("none"),          JA("なし"),          ZH_HANS("无"),       ZH_HANT("無"),
    KO("없음"),           DE("keine"),        FR("aucun"),        ES("ninguno"),
    PT("nenhum"),        IT("nessuno"),      NL("geen"),         RU("нет"),
    TR("yok"));

SS_MSG(choice_point_median,
    EN("point cloud median"), JA("点群の中央値"), ZH_HANS("点云中位数"),
    ZH_HANT("點雲中位數"), KO("점군 중앙값"), DE("Median der Punktwolke"),
    FR("médiane du nuage de points"), ES("mediana de la nube de puntos"),
    PT("mediana da nuvem de pontos"), IT("mediana della nuvola di punti"),
    NL("mediaan van de puntenwolk"), RU("медиана облака точек"),
    TR("nokta bulutu ortancası"));

SS_MSG(choice_camera_median,
    EN("camera position median"), JA("カメラ位置の中央値"),
    ZH_HANS("相机位置中位数"), ZH_HANT("相機位置中位數"), KO("카메라 위치 중앙값"),
    DE("Median der Kamerapositionen"), FR("médiane des positions de caméra"),
    ES("mediana de las posiciones de cámara"),
    PT("mediana das posições das câmeras"),
    IT("mediana delle posizioni delle camere"),
    NL("mediaan van de cameraposities"), RU("медиана положений камер"),
    TR("kamera konumu ortancası"));

SS_MSG(choice_camera_focus,
    EN("camera focus"),  JA("カメラの注視点"), ZH_HANS("相机注视点"),
    ZH_HANT("相機注視點"), KO("카메라 주시점"), DE("Kamerafokus"),
    FR("point de convergence des caméras"), ES("foco de las cámaras"),
    PT("foco das câmeras"), IT("fuoco delle camere"), NL("camerafocus"),
    RU("фокус камер"), TR("kamera odağı"));

SS_MSG(choice_point_mean,
    EN("point cloud mean"), JA("点群の平均"), ZH_HANS("点云平均"),
    ZH_HANT("點雲平均"), KO("점군 평균"), DE("Mittelwert der Punktwolke"),
    FR("moyenne du nuage de points"), ES("media de la nube de puntos"),
    PT("média da nuvem de pontos"), IT("media della nuvola di punti"),
    NL("gemiddelde van de puntenwolk"), RU("среднее облака точек"),
    TR("nokta bulutu ortalaması"));

SS_MSG(choice_far_from_origin,
    EN("when far from the origin"), JA("原点から遠いとき"), ZH_HANS("远离原点时"),
    ZH_HANT("遠離原點時"), KO("원점에서 멀 때"), DE("wenn weit vom Ursprung"),
    FR("si loin de l'origine"), ES("si está lejos del origen"),
    PT("se estiver longe da origem"), IT("se lontana dall'origine"),
    NL("als ver van de oorsprong"), RU("если далеко от начала координат"),
    TR("başlangıçtan uzaksa"));

SS_MSG(choice_camera_mean,
    EN("camera position mean"), JA("カメラ位置の平均"), ZH_HANS("相机位置平均"),
    ZH_HANT("相機位置平均"), KO("카메라 위치 평균"),
    DE("Mittelwert der Kamerapositionen"), FR("moyenne des positions de caméra"),
    ES("media de las posiciones de cámara"), PT("média das posições das câmeras"),
    IT("media delle posizioni delle camere"), NL("gemiddelde van de cameraposities"),
    RU("среднее положений камер"), TR("kamera konumu ortalaması"));

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

SS_MSG(choice_never,
    EN("never"),
    JA("しない"),
    ZH_HANS("从不"),
    ZH_HANT("從不"),
    KO("안 함"),
    DE("nie"),
    FR("jamais"),
    ES("nunca"),
    PT("nunca"),
    IT("mai"),
    NL("nooit"),
    RU("никогда"),
    TR("asla"));

SS_MSG(choice_without_points,
    EN("when the dataset has no points"),
    JA("点群がないとき"),
    ZH_HANS("数据集无点云时"),
    ZH_HANT("資料集無點雲時"),
    KO("점군이 없을 때"),
    DE("wenn der Datensatz keine Punkte hat"),
    FR("si le jeu n'a pas de points"),
    ES("si el conjunto no tiene puntos"),
    PT("se o conjunto não tiver pontos"),
    IT("se il set non ha punti"),
    NL("als de dataset geen punten heeft"),
    RU("если в наборе нет точек"),
    TR("veri kümesinde nokta yoksa"));

SS_MSG(choice_always,
    EN("always"),
    JA("常に"),
    ZH_HANS("总是"),
    ZH_HANT("總是"),
    KO("항상"),
    DE("immer"),
    FR("toujours"),
    ES("siempre"),
    PT("sempre"),
    IT("sempre"),
    NL("altijd"),
    RU("всегда"),
    TR("her zaman"));

SS_MSG(choice_isotropic_gaussian,
    EN("isotropic Gaussian"),
    JA("等方ガウス分布"),
    ZH_HANS("各向同性高斯"),
    ZH_HANT("各向同性高斯"),
    KO("등방성 가우시안"),
    DE("isotrope Gauß-Verteilung"),
    FR("gaussienne isotrope"),
    ES("gaussiana isótropa"),
    PT("gaussiana isotrópica"),
    IT("gaussiana isotropa"),
    NL("isotrope gaussverdeling"),
    RU("изотропное гауссово"),
    TR("izotropik Gauss"));

SS_MSG(choice_anisotropic_gaussian,
    EN("anisotropic Gaussian"),
    JA("異方ガウス分布"),
    ZH_HANS("各向异性高斯"),
    ZH_HANT("各向異性高斯"),
    KO("비등방성 가우시안"),
    DE("anisotrope Gauß-Verteilung"),
    FR("gaussienne anisotrope"),
    ES("gaussiana anisótropa"),
    PT("gaussiana anisotrópica"),
    IT("gaussiana anisotropa"),
    NL("anisotrope gaussverdeling"),
    RU("анизотропное гауссово"),
    TR("anizotropik Gauss"));

SS_MSG(choice_solid_ellipsoid,
    EN("uniform solid ellipsoid"),
    JA("一様な中実楕円体"),
    ZH_HANS("均匀实心椭球"),
    ZH_HANT("均勻實心橢球"),
    KO("균일한 속이 찬 타원체"),
    DE("gleichmäßig gefülltes Ellipsoid"),
    FR("ellipsoïde plein uniforme"),
    ES("elipsoide macizo uniforme"),
    PT("elipsoide sólido uniforme"),
    IT("ellissoide pieno uniforme"),
    NL("gelijkmatig gevulde ellipsoïde"),
    RU("равномерный сплошной эллипсоид"),
    TR("düzgün dolu elipsoid"));

SS_MSG(choice_oriented_box,
    EN("uniform oriented box"),
    JA("一様な向き付きの箱"),
    ZH_HANS("均匀定向长方体"),
    ZH_HANT("均勻定向長方體"),
    KO("균일한 방향 상자"),
    DE("gleichmäßig gefüllter ausgerichteter Quader"),
    FR("boîte orientée uniforme"),
    ES("caja orientada uniforme"),
    PT("caixa orientada uniforme"),
    IT("scatola orientata uniforme"),
    NL("gelijkmatig gevulde gerichte doos"),
    RU("равномерный ориентированный параллелепипед"),
    TR("düzgün yönlü kutu"));

SS_MSG(choice_world_origin,
    EN("world origin"),
    JA("ワールド原点"),
    ZH_HANS("世界原点"),
    ZH_HANT("世界原點"),
    KO("월드 원점"),
    DE("Koordinatenursprung"),
    FR("origine du repère"),
    ES("origen de coordenadas"),
    PT("origem das coordenadas"),
    IT("origine delle coordinate"),
    NL("oorsprong"),
    RU("начало координат"),
    TR("koordinat başlangıcı"));

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
    {"floater_suppression", "extreme", &choice_extreme},

    {"distraction_robustness", "off",    &choice_off},
    {"distraction_robustness", "mild",   &choice_mild},
    {"distraction_robustness", "strong", &choice_strong},

    {"calib_correction", "off",      &choice_off},
    {"calib_correction", "basic",    &choice_basic},
    {"calib_correction", "accurate", &choice_accurate},

    {"eval_mode", "all",      &choice_all},
    {"eval_mode", "interval", &choice_interval},
    {"eval_mode", "fraction", &choice_fraction},
    {"eval_mode", "filename", &choice_filename},

    {"downscale_rounding_mode", "floor", &choice_floor},
    {"downscale_rounding_mode", "ceil",  &choice_ceil},
    {"downscale_rounding_mode", "round", &choice_round},

    {"scene_center", "auto",          &choice_far_from_origin},
    {"scene_center", "none",          &choice_none},
    {"scene_center", "point-median",  &choice_point_median},
    {"scene_center", "camera-median", &choice_camera_median},
    {"scene_center", "camera-focus",  &choice_camera_focus},
    {"scene_center", "point-mean",    &choice_point_mean},
    {"scene_center", "camera-mean",   &choice_camera_mean},

    {"densify_score_mode", "mean",   &choice_mean},
    {"densify_score_mode", "max",    &choice_max},
    {"densify_score_mode", "median", &choice_median},

    {"densify_accum_mode", "max", &choice_max},
    {"densify_accum_mode", "sum", &choice_sum},
    {"densify_accum_mode", "avg", &choice_mean},

    {"cache_images", "cpu",  &choice_cpu},
    {"cache_images", "disk", &choice_disk},

    // `none` is the UNSET value for these six, not a colour space -- Rec.709
    // and `srgb` are the explicit ones. Labelled so the dropdown cannot read
    // as "no gamut" / "no transfer".
    {"image_color_gamut", "none", &choice_from_the_file},
    {"splat_color_gamut", "none", &choice_same_as_input},
    {"image_color_transfer", "none", &choice_srgb_default},
    {"splat_color_transfer", "none", &choice_same_as_input},
    {"point_color_gamut", "none", &choice_same_as_input},
    {"point_color_transfer", "none", &choice_same_as_input},

    {"random_init", "never",  &choice_never},
    {"random_init", "auto",   &choice_without_points},
    {"random_init", "always", &choice_always},

    {"random_init_distribution", "isotropic-gaussian",   &choice_isotropic_gaussian},
    {"random_init_distribution", "anisotropic-gaussian", &choice_anisotropic_gaussian},
    {"random_init_distribution", "ellipsoid",            &choice_solid_ellipsoid},
    {"random_init_distribution", "box",                  &choice_oriented_box},

    {"random_init_center", "camera-median", &choice_camera_median},
    {"random_init_center", "camera-focus",  &choice_camera_focus},
    {"random_init_center", "camera-mean",   &choice_camera_mean},
    {"random_init_center", "origin",        &choice_world_origin},

    {"random_init_spread", "median", &choice_median},
    {"random_init_spread", "mean",   &choice_mean},
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
