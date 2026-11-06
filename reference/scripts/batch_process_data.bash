#!/bin/bash

# Batch data processing script, for small object-centered scenes with masks
# Assume you already have the `scripts` folder added to path

extract_frame_skip=50

camera_model=OPENCV
max_num_features=8192
# Path to a COLMAP vocabulary tree (e.g. vocab_tree_flickr100K_words32K.bin,
# downloadable from https://demuc.de/colmap/). Override with the environment
# variable, or edit the default below to point at your local copy.
vocab_tree_path="${SS_VOCAB_TREE:?set SS_VOCAB_TREE to your COLMAP vocab tree .bin}"

extension="*.mov"

# process data

if false; then

mapfile -t files < <(ls)
sam2_cmd=""
for file in "${files[@]}"; do
  if [[ "$file" == $extension ]]; then
    echo "* Processing $file"

    extract_frames.py $file -s $extract_frame_skip --mask 1
    cd ${file%.*}

    colmap feature_extractor --database_path database.db --image_path ./images --ImageReader.single_camera 1 --ImageReader.camera_model $camera_model --SiftExtraction.max_num_features $max_num_features
    # colmap exhaustive_matcher --database_path database.db
    colmap vocab_tree_matcher --database_path database.db --VocabTreeMatching.vocab_tree_path $vocab_tree_path
    mkdir -p sparse
    colmap mapper --database_path database.db --image_path ./images --output_path sparse
    colmap bundle_adjuster --input_path sparse/0 --output_path sparse/0 --BundleAdjustment.refine_principal_point 1
    ns-process-data images --data ./$image_path --output-dir . --skip-image-processing --skip-colmap --colmap-model-path sparse/0/
    #cp transforms.json transforms_no_masks.json
    #sed -E 's/"file_path": "images\/(.*?\.jpg)",/"file_path": "images\/\1", "mask_path": "masks\/\1.png",/g' transforms_no_masks.json > transforms.json

    enhance_images.py ./ --max_tile_size 1024

    sam2_cmd+=$(cat "run_sam2.bash" | grep python3)
    sam2_cmd+=$'\\n'
    cd ..
  fi
done
echo ''
echo "Run the following commands from SAM2 folder to manually generate masks:"
echo -e $sam2_cmd

fi

# train

if true; then

mapfile -t files < <(ls)
sam2_cmd=""
for file in "${files[@]}"; do
  if [[ "$file" == $extension ]]; then
    echo "* Training $file"

    dirname=${file%.*}

    num_gs=1000000
    sh_degree=3

    ns-train spirulae --data $dirname \
      --max_num_iterations 30000 \
      --pipeline.model.apply-loss-for-mask True \
      --pipeline.model.randomize_background False \
      --pipeline.model.mcmc_cap_max $num_gs \
      --pipeline.model.sh-degree $sh_degree \
      --pipeline.model.use-bilateral-grid False \
      --pipeline.model.use-ppisp True \
      --pipeline.model.mcmc_prob_grad_weight 1.0 \
      --pipeline.model.mcmc_use_long_axis_split True \
      --viewer.quit_on_train_completion True \
      nerfstudio-data --validation_fraction 0.1

    # ns-train spirulae --data $dirname \
    #   --max_num_iterations 30000 \
    #   --pipeline.model.apply-loss-for-mask True \
    #   --pipeline.model.randomize_background False \
    #   --pipeline.model.mcmc_cap_max $num_gs \
    #   --pipeline.model.sh-degree $sh_degree \
    #   --viewer.quit_on_train_completion True \
    #   nerfstudio-data --validation_fraction 0.1

    # ns-train spirulae-patched --data $dirname \
    #   --max_num_iterations 30000 \
    #   --pipeline.model.apply-loss-for-mask True \
    #   --pipeline.model.randomize_background False \
    #   --pipeline.model.mcmc_cap_max $num_gs \
    #   --pipeline.model.sh-degree $sh_degree \
    #   --viewer.quit_on_train_completion True \
    #   nerfstudio-data --validation_fraction 0.1

    # ns-train spirulae --data $dirname \
    #   --max_num_iterations 30000 \
    #   --pipeline.model.randomize_background True \
    #   --pipeline.model.mcmc_cap_max $num_gs \
    #   --pipeline.model.sh-degree $sh_degree \
    #   --pipeline.model.num_loss_scales 2 \
    #   --viewer.quit_on_train_completion True

    outputs=$(find outputs/$dirname | grep config.yml)
    export_ply_3dgs.py $outputs --no_convert_to_input_frame
    cp ${outputs%/*}/splat.ply outputs/${dirname}_${num_gs}_sh${sh_degree}.ply

  fi
done

fi

# ns-train spirulae --data data/adr_food/251211_chao_3 --pipeline.model.apply-loss-for-mask True --pipeline.model.randomize_background False --pipeline.model.mcmc_cap_max 100000 --pipeline.model.sh-degree 3 --viewer.quit_on_train_completion True
