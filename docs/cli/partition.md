# `spirula partition`

Splits a large reconstruction into parts that train separately, then merges
the trained models back into one. Design notes are in
`docs/notes/scene-partition.md`.

```text
spirula partition split <dataset> [options]
spirula partition merge <partition.json> [options]
spirula partition --help
```

`split` reads a COLMAP, Nerfstudio or Metashape dataset, cuts its cameras
where they share the least, gives every part the region its cameras see plus
a ring of outside cameras that see it too, and writes `partition.json`. Each
part is then trained on its own:

```text
spirula train --data <dataset> --partition <dataset>/partition.json --partition-part 0
spirula train --data <dataset> --partition <dataset>/partition.json --partition-part 1
...
```

`merge` keeps each model's splats inside its own region and writes them as one
file.

## `split`

| flag | default | meaning |
|---|---|---|
| `--parts <n>` | 0 | How many parts. 0 splits until every part has at most `--max-images` cameras. |
| `--max-images <n>` | 2000 | Most cameras a part trains with, core and ring, when `--parts` is 0. |
| `--method graph\|viewgraph` | `viewgraph` | `graph` cuts the cameras and the points together where the least view crosses; `viewgraph` cuts the cameras alone and takes regions from their positions. |
| `--ring <fraction>` | 0.1 | An outside camera joins a part's ring when at least this share of the points it sees belongs to the part ... |
| `--ring-min-points <n>` | 20 | ... and at least this many of them. |
| `--max-seeds <n>` | 1000000 | Seed points the ownership field keeps, strided from the cloud (at least 1000). |
| `--source auto\|tracks\|projection\|proximity` | `auto` | Where covisibility comes from. `auto` takes the model's own tracks, else projects the seed cloud into every frame, else uses camera positions alone. |
| `-o`, `--output <file>` | `<dataset>/partition.json` | Where to write the partition. |

`--help` gives `graph` as the `--method` default; the command starts from
`viewgraph`.

## `merge`

| flag | default | meaning |
|---|---|---|
| `--runs <dir>` | `<dataset>/outputs` | Folder of training runs to find the parts' models in, by their `config.json`. |
| `--part <k> <run\|ply>` | | Part `k`'s run folder or `splat.ply`, instead of searching `--runs`. Repeatable. |
| `-o`, `--output <file>` | `merged.ply` beside the partition | The merged model file. |

Exit status is 0 on success, 1 when the run fails and 2 for a usage error.
