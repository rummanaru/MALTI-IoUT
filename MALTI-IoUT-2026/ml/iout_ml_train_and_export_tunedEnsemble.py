"""
iout_ml_train_and_export.py -- SCRIPT 1 of 3

Data merging + model training + CSV export ONLY. No plotting at all (no
matplotlib import on purpose) -- every result is written to CSV, and the
companion scripts read those CSVs back:

    SCRIPT 1 (this file)      -> data + training + CSV export
    SCRIPT 2 (iout_ml_plots.py)     -> all plots, reads CSVs from script 1,
                                        safe to re-run as many times as you
                                        want (different aspect ratios/styles)
                                        WITHOUT ever re-training a model
    SCRIPT 3 (find_corner_cases.py) -> corner-case analysis, also reads the
                                        prediction_error_vs_*_with_sent.csv
                                        files from script 1, independent of
                                        script 2

Usage:
    python iout_ml_train_and_export.py --mode full          (default, ~1h, all 4 targets)
    python iout_ml_train_and_export.py --mode cumulative     (opt-in, hours --
                                                                see build_models()
                                                                docstring for cost)
    python iout_ml_train_and_export.py --targets delay,jitter
                                                              (only train/export these
                                                               targets -- cuts runtime
                                                               roughly proportionally,
                                                               since each target retrains
                                                               the full model zoo on its
                                                               own; valid keys: droprate,
                                                               delay, throughput, jitter)

Where results land -- see OUTPUT_FOLDER below (outputs_ensemble_iout_2026/):
    merged_cleaned_ns3_dataset_new_iout_2026.csv   -- full merged dataset
    model_accuracy_summary_forward.csv             -- headline R2/MAE/RMSE table
    accuracy_<target>_<tag>.csv                    -- per-target model accuracy
    feature_importance_<target>_<tag>.csv          -- RandomForest importances
    prediction_error_vs_concrete_depth_<target>_with_sent.csv
    prediction_error_vs_<VWC|b_d|soil_d>_<target>_with_sent.csv
    segment_progress_summary.csv                   -- cumulative mode only
    models/segmentN_.../*.joblib                   -- cumulative mode only (checkpoints)
"""

import os
import sys
import glob
import re
import ast
import time
import datetime
import argparse
import numpy as np
import pandas as pd

import joblib

from sklearn.model_selection import train_test_split, RandomizedSearchCV, GridSearchCV
from sklearn.pipeline import Pipeline
from sklearn.impute import SimpleImputer
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
from sklearn.linear_model import LinearRegression, Ridge
from sklearn.ensemble import (
    RandomForestRegressor,
    GradientBoostingRegressor,
    VotingRegressor,
    StackingRegressor,
)
from sklearn.neural_network import MLPRegressor
from xgboost import XGBRegressor

DATA_FOLDER = "data_new_iout_2026/Network_flow_MAME"   # legacy single-folder fallback
OUTPUT_FOLDER = "outputs_ensemble_iout_2026"

os.makedirs(OUTPUT_FOLDER, exist_ok=True)

#=====================ADDED 7/27/2026: 6-segment dataset support==========================#
# The NS-3 run produced 6 segment folders (segment0_batch0_depthgrp0 ...
# segment5_batch1_depthgrp2), each holding many per-run CSVs. This script now
# auto-discovers those folders next to itself instead of assuming one flat
# Network_flow_MAME folder.
DATA_ROOT = "."                              # run this script from the folder that contains segmentN_... subfolders
SEGMENT_FOLDER_PATTERN = "segment*_batch*_depthgrp*"
MODELS_FOLDER = "models"                     # checkpointed fitted models, per segment
PROGRESS_SUMMARY_CSV = os.path.join(OUTPUT_FOLDER, "segment_progress_summary.csv")

os.makedirs(MODELS_FOLDER, exist_ok=True)


def discover_segment_folders(data_root=DATA_ROOT):
    """
    Find all segmentN_batchB_depthgrpD folders under data_root and return
    them sorted by segment index N (0..5). Returns [] if none are found.
    """
    candidates = glob.glob(os.path.join(data_root, SEGMENT_FOLDER_PATTERN))
    candidates = [c for c in candidates if os.path.isdir(c)]

    def seg_index(path):
        m = re.search(r"segment(\d+)_", os.path.basename(os.path.normpath(path)))
        return int(m.group(1)) if m else -1

    candidates = [c for c in candidates if seg_index(c) >= 0]
    candidates.sort(key=seg_index)
    return candidates


def safe_name(s):
    """Filesystem-safe version of a column/target name, used consistently
    for output file names and checkpoint file names."""
    return (
        str(s).replace(" ", "_")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
        .replace("%", "percent")
    )


class _Tee:
    """Duplicates everything written to stdout into a log file as well, so
    an unattended run (left overnight / walked away from) always has a full
    transcript on disk -- no need to manually redirect the terminal."""
    def __init__(self, *streams):
        self.streams = streams

    def write(self, data):
        for s in self.streams:
            s.write(data)
            s.flush()

    def flush(self):
        for s in self.streams:
            s.flush()


def start_run_log(mode):
    """Redirect stdout to console + a timestamped log file under OUTPUT_FOLDER.
    Returns the log file path so the caller can print it back to the user."""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(OUTPUT_FOLDER, f"run_log_{mode}_{timestamp}.txt")
    log_file = open(log_path, "w", encoding="utf-8")
    sys.stdout = _Tee(sys.__stdout__, log_file)
    sys.stderr = _Tee(sys.__stderr__, log_file)
    return log_path
#===========================================================================================#

# Default values for missing feature columns
DEFAULT_VWC = 0.15
DEFAULT_BD = 1.75
DEFAULT_CLUSTER_SIZE = 5
DEFAULT_SOIL_D = 2.0
DEFAULT_CONC_T = 0.2

KEEP_COLS = [
    "VWC",
    "Pkt_Intvl (ms)",
    "Pkt_Size (Byte)",
    "#SentPackets",
    "#ReceivedPackets",
    "#LostPackets",
    "Pkt_Deliv_Rate (%)",
    "AvgThroughput (kbps)",
    "Delay (ns)",
    "Jitter (ns)",
    "b_d",
    "cluster_size",
    "soil_d",
    "conc_t",
    "SourceFile",
    "RowInFile",
    "Segment",
    "Shard"
]

# FEATURE_COLS = [
#     "VWC",
#     "Pkt_Intvl (ms)",
#     "Pkt_Size (Byte)",
#     # "#SentPackets",
#     "b_d",
#     "cluster_size",
#     "soil_d",
#     "conc_t"
# ]
FEATURE_COLS_WITH_SENT = [
    "VWC",
    "Pkt_Intvl (ms)",
    "Pkt_Size (Byte)",
    "#SentPackets",
    "b_d",
    "cluster_size",
    "soil_d",
    "conc_t"
]

FEATURE_COLS_NO_SENT = [
    "VWC",
    "Pkt_Intvl (ms)",
    "Pkt_Size (Byte)",
    "b_d",
    "cluster_size",
    "soil_d",
    "conc_t"
]

#=====================ADDED 8/6/2026: single target registry, all 4 merged=================#
# Single source of truth for every target this pipeline predicts. Previously
# the error-vs-feature wrappers only covered Delay/DropRate (Throughput/Jitter
# needed a separate compute_throughput_jitter_errors.py script); now every
# code path -- run_experiment, error-vs-concrete-depth, error-vs-VWC/b_d/soil_d,
# and cumulative training -- reads from this ONE registry, so adding/removing
# a target only ever requires editing it here.
#
# Order matches the original target_cols_forward order (DropRate first).
TARGET_REGISTRY = {
    "droprate":    {"col": "DropRate_percent",     "label": "Packet Drop Rate"},
    "delay":       {"col": "Delay (ns)",           "label": "Delay"},
    "throughput":  {"col": "AvgThroughput (kbps)", "label": "Throughput"},
    "jitter":      {"col": "Jitter (ns)",          "label": "Jitter"},
}
TARGET_KEYS_ORDERED = ["droprate", "delay", "throughput", "jitter"]


def resolve_target_selection(keys_arg):
    """
    Parse a comma-separated --targets argument (e.g. "delay,jitter" or "all")
    into an ordered list of (target_col, label) tuples, validating against
    TARGET_REGISTRY. Raises ValueError with the list of valid keys on a typo.
    """
    if keys_arg.strip().lower() == "all":
        selected_keys = TARGET_KEYS_ORDERED
    else:
        selected_keys = [k.strip().lower() for k in keys_arg.split(",") if k.strip()]
        unknown = [k for k in selected_keys if k not in TARGET_REGISTRY]
        if unknown:
            raise ValueError(
                f"Unknown target key(s) {unknown}. Valid keys: {TARGET_KEYS_ORDERED} "
                f"(or 'all')."
            )
        # Keep canonical ordering regardless of the order the user typed them in.
        selected_keys = [k for k in TARGET_KEYS_ORDERED if k in selected_keys]

    return [(TARGET_REGISTRY[k]["col"], TARGET_REGISTRY[k]["label"]) for k in selected_keys]
#===========================================================================================#

#=====================ADDED 7/14/2026: Ensemble roadmap step==========================#
# Single source of truth for the model zoo used by run_experiment(),
# compute_prediction_error_vs_concrete_depth(), and compute_prediction_error_vs_feature().
# Keeping this in one place means adding/removing a model only requires editing it here.
def build_models(tuned_params=None):
    """
    Returns a fresh dict of {model_name: sklearn Pipeline/estimator}.
    Built fresh on every call so each experiment gets unfitted estimators
    (sklearn Pipelines/estimators carry fitted state after .fit()).

    tuned_params : dict or None (ADDED 8/7/2026)
        Optional output of tune_hyperparameters() / run_hyperparameter_tuning(),
        shaped like:
            {
                "RandomForest":     {"n_estimators": 300, "max_depth": 20, ...},
                "GradientBoosting": {"n_estimators": 200, "learning_rate": 0.1, ...},
                "XGBoost":          {"n_estimators": 400, "max_depth": 8, ...},
            }
        Any model/key not present falls back to the manually-fixed defaults
        below, so this stays fully backward compatible -- existing --mode
        full / --mode cumulative runs are unaffected unless you explicitly
        pass tuned_params (see --use-tuned-params in main()).

    NOTE on runtime at ~1M-row scale: StackingRegressor(cv=5) refits its 3
    base learners 5x each just for out-of-fold predictions, on top of the
    final fit -- this is by far the most expensive model here. If a full
    6-segment run is too slow on your machine, the first things to try are
    (in order of impact): lower cv=5 to cv=3 in StackingEnsemble below,
    lower RandomForest/XGBoost n_estimators, or drop StackingEnsemble from
    the zoo for the --mode cumulative sweep and only run it once on the
    final full dataset.
    """
    tuned_params = tuned_params or {}
    rf_params = {"n_estimators": 200, "random_state": 42}
    rf_params.update(tuned_params.get("RandomForest", {}))

    gb_params = {"random_state": 42}
    gb_params.update(tuned_params.get("GradientBoosting", {}))

    xgb_params = {
        "n_estimators": 300,
        "max_depth": 6,
        "learning_rate": 0.05,
        "subsample": 0.8,
        "colsample_bytree": 0.8,
        "reg_lambda": 1.0,
        "random_state": 42,
        "n_jobs": -1,
    }
    xgb_params.update(tuned_params.get("XGBoost", {}))

    base_linear = Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("scaler", StandardScaler()),
        ("model", LinearRegression())
    ])

    base_rf = Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("model", RandomForestRegressor(**rf_params))
    ])

    base_gb = Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("model", GradientBoostingRegressor(**gb_params))
    ])

    # XGBoost: regularized gradient boosting, usually a stronger/faster
    # alternative to sklearn's GradientBoostingRegressor on tabular data.
    base_xgb = Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("model", XGBRegressor(**xgb_params))
    ])

    # Simple feed-forward DNN baseline (fast path per roadmap: MLPRegressor
    # instead of a full Keras/TensorFlow model).
    base_mlp = Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("scaler", StandardScaler()),
        ("model", MLPRegressor(
            hidden_layer_sizes=(64, 32),
            activation="relu",
            solver="adam",
            alpha=1e-4,
            learning_rate_init=1e-3,
            max_iter=2000,
            early_stopping=True,
            n_iter_no_change=20,
            random_state=42
        ))
    ])

    models = {
        "LinearRegression": base_linear,
        "RandomForest": base_rf,
        "GradientBoosting": base_gb,
        "XGBoost": base_xgb,
        "MLPRegressor": base_mlp,
    }

    # Voting ensemble: simple average of the four base learners' predictions.
    # Cheap variance-reduction baseline.
    models["VotingEnsemble"] = VotingRegressor(estimators=[
        ("lr", base_linear),
        ("rf", base_rf),
        ("gb", base_gb),
        ("xgb", base_xgb),
    ])

    # Stacking ensemble: out-of-fold predictions from the tree-based learners
    # feed a Ridge meta-learner. Excludes LinearRegression/MLP as base learners
    # since they add little diversity beyond what Ridge itself already captures.
    models["StackingEnsemble"] = StackingRegressor(
        estimators=[
            ("rf", base_rf),
            ("gb", base_gb),
            ("xgb", base_xgb),
        ],
        final_estimator=Ridge(),
        cv=5,
        n_jobs=-1
    )

    return models


# NOTE: build_model_colors() moved to iout_ml_plots.py -- it's only used for
# plotting, so script-1 (this file) has no need for it.
#========================================================================================#

#=====================ADDED 8/7/2026: hyperparameter tuning (RandomizedSearchCV)============#
# Separate, OPT-IN step (python iout_ml_train_and_export.py --mode tune). Does NOT run as
# part of --mode full / --mode cumulative, so existing runs/timings are unaffected.
#
# Why RandomizedSearchCV instead of GridSearchCV as the default:
#   RandomForest's grid alone (4 x 4 x 3 x 3 x 3 = 432 combos) x cv=3 folds = 1,296 fits.
#   At ~312k rows that is many hours. RandomizedSearchCV instead draws N_ITER random
#   combinations from the same search space, so the cost is capped at N_ITER x cv fits
#   regardless of how large the grid is. GridSearchCV is still available (--search-type grid)
#   for a final, narrower confirmation pass once RandomizedSearchCV has told you roughly
#   where the good region is.
def build_param_distributions():
    """
    Search spaces for RandomForest and the StackingEnsemble's tree-based base
    learners (GradientBoosting, XGBoost). Keys are prefixed "model__" because
    each estimator sits inside a Pipeline (see build_models()) under the step
    name "model" -- sklearn's *SearchCV needs that prefix to reach through
    the Pipeline to the underlying estimator's parameters.

    Ranges are centered around your current manually-fixed values (e.g.
    RandomForest n_estimators=200, XGBoost max_depth=6) so the search checks
    both smaller/cheaper and larger/more-expressive alternatives around what
    you already picked, rather than an arbitrary unrelated range.
    """
    rf_space = {
        "model__n_estimators": [100, 200, 300, 400],
        "model__max_depth": [None, 10, 20, 30],
        "model__min_samples_split": [2, 5, 10],
        "model__min_samples_leaf": [1, 2, 4],
        "model__max_features": ["sqrt", "log2", 1.0],
    }

    gb_space = {
        "model__n_estimators": [100, 200, 300],
        "model__learning_rate": [0.01, 0.05, 0.1, 0.2],
        "model__max_depth": [2, 3, 4, 5],
        "model__subsample": [0.6, 0.8, 1.0],
    }

    xgb_space = {
        "model__n_estimators": [100, 200, 300, 400],
        "model__max_depth": [3, 4, 6, 8],
        "model__learning_rate": [0.01, 0.05, 0.1, 0.2],
        "model__subsample": [0.6, 0.8, 1.0],
        "model__colsample_bytree": [0.6, 0.8, 1.0],
        "model__reg_lambda": [0.5, 1.0, 2.0, 5.0],
    }

    return {
        "RandomForest": rf_space,
        "GradientBoosting": gb_space,
        "XGBoost": xgb_space,
    }


def tune_hyperparameters(
        df,
        target_col,
        feature_cols,
        tag="with_sent",
        sample_frac=0.15,
        n_iter=20,
        cv=3,
        search_type="random",
        random_state=42,
):
    """
    Runs RandomizedSearchCV (default) or GridSearchCV (search_type="grid") for
    RandomForest, GradientBoosting, and XGBoost -- i.e. RF plus the two
    StackingEnsemble base learners that have real hyperparameters worth
    tuning (LinearRegression/Ridge have none in the same sense).

    sample_frac : tuning on the FULL ~312k-row merged dataset would make even
        RandomizedSearchCV slow (n_iter x cv separate model fits). Since the
        goal here is to find a good hyperparameter *region*, not squeeze out
        the last 0.1% of R2, this draws a stratified sub-sample (same
        SplitStratum column already used by run_experiment(), so the sample
        stays representative across files/segments/row-position blocks)
        before searching. Increase this later (e.g. 0.4-0.5) for a final
        confirmation pass on your best models if time allows.

    Returns
    -------
    best_params : dict, shaped for build_models(tuned_params=...), e.g.
        {"RandomForest": {"n_estimators": 300, "max_depth": 20, ...}, ...}
    summary_df : pd.DataFrame, one row per model, with best_score_ (mean CV
        R2) and best_params_, also written to
        OUTPUT_FOLDER/best_hyperparameters_<target>_<tag>.csv
    """
    print("\n" + "=" * 70)
    print(f"HYPERPARAMETER TUNING ({search_type}) -- TARGET: {target_col} ({tag})")
    print("=" * 70)

    needed_cols = feature_cols + [target_col, "SplitStratum"]
    work_df = df[needed_cols].dropna().copy()

    if sample_frac < 1.0:
        work_df = work_df.groupby("SplitStratum", group_keys=False)[work_df.columns].apply(
            lambda g: g.sample(frac=sample_frac, random_state=random_state)
        )
    print(f"Tuning on {len(work_df)} rows ({sample_frac * 100:.0f}% stratified sample)")

    X = work_df[feature_cols]
    y = work_df[target_col]

    # NOTE (fix, 8/9/2026): do NOT stratify this split by SplitStratum. The
    # groupby-sample step above already draws proportionally from every
    # SourceFile/RowBlock stratum (up to ~6000 distinct strata across the
    # full dataset), so representativeness is already preserved. Re-applying
    # that same fine-grained stratify= here fails once the sub-sample is
    # small (sklearn requires at least 1 sample per stratum in the test
    # split, i.e. test_size >= number of distinct strata) -- e.g. a 5% or
    # 15% sub-sample plus a 20% test split can easily have fewer test rows
    # than there are strata. A plain shuffled split is sufficient here since
    # this data is only used for the hyperparameter search, not the paper's
    # headline accuracy numbers.
    X_train, X_test, y_train, y_test = train_test_split(
        X, y,
        test_size=0.2,
        random_state=random_state,
        shuffle=True,
    )

    base_models = build_models()  # unfitted Pipelines with the current fixed defaults
    param_spaces = build_param_distributions()

    SearchCls = RandomizedSearchCV if search_type == "random" else GridSearchCV

    results = []
    best_params = {}

    for model_name in ["RandomForest", "GradientBoosting", "XGBoost"]:
        print(f"\nTuning {model_name} ({search_type} search, cv={cv})...")
        pipeline = base_models[model_name]
        space = param_spaces[model_name]

        search_kwargs = dict(
            estimator=pipeline,
            cv=cv,
            scoring="r2",
            n_jobs=-1,
            verbose=1,
        )
        if search_type == "random":
            search = SearchCls(param_distributions=space, n_iter=n_iter,
                                random_state=random_state, **search_kwargs)
        else:
            search = SearchCls(param_grid=space, **search_kwargs)

        start = time.time()
        search.fit(X_train, y_train)
        elapsed = time.time() - start

        test_r2 = r2_score(y_test, search.predict(X_test))

        # Strip the "model__" prefix so this dict can be fed straight into
        # build_models(tuned_params=...) / the estimator's own constructor.
        clean_params = {k.replace("model__", ""): v for k, v in search.best_params_.items()}
        best_params[model_name] = clean_params

        print(f"  Best CV R2 (train folds) = {search.best_score_:.6f}")
        print(f"  Held-out test R2         = {test_r2:.6f}")
        print(f"  Best params              = {clean_params}")
        print(f"  Search time              = {elapsed:.1f} sec "
              f"({n_iter if search_type == 'random' else 'full grid'} combos x cv={cv})")

        results.append({
            "Target": target_col,
            "FeatureSet": tag,
            "Model": model_name,
            "SearchType": search_type,
            "BestCV_R2": search.best_score_,
            "HeldOutTest_R2": test_r2,
            "BestParams": str(clean_params),
            "SearchSeconds": elapsed,
            "TuningRows": len(work_df),
        })

    summary_df = pd.DataFrame(results)
    safe_target = safe_name(target_col)
    out_csv = os.path.join(OUTPUT_FOLDER, f"best_hyperparameters_{safe_target}_{tag}.csv")
    summary_df.to_csv(out_csv, index=False)
    print(f"\nSaved tuning summary to: {out_csv}")

    return best_params, summary_df


def run_hyperparameter_tuning(df, target_specs, sample_frac=0.15, n_iter=20, cv=3,
                               search_type="random"):
    """
    Loops tune_hyperparameters() over every selected target (using the
    FEATURE_COLS_WITH_SENT feature set, matching the headline "with_sent"
    results elsewhere in this script) and prints a combined best-params
    block you can paste into build_models() or hand to --use-tuned-params.
    """
    all_summaries = []
    per_target_best = {}

    for target_col, target_label in target_specs:
        best_params, summary_df = tune_hyperparameters(
            df, target_col=target_col, feature_cols=FEATURE_COLS_WITH_SENT,
            tag="with_sent", sample_frac=sample_frac, n_iter=n_iter, cv=cv,
            search_type=search_type,
        )
        per_target_best[target_col] = best_params
        all_summaries.append(summary_df)

    combined = pd.concat(all_summaries, ignore_index=True)
    combined_csv = os.path.join(OUTPUT_FOLDER, "best_hyperparameters_ALL_TARGETS.csv")
    combined.to_csv(combined_csv, index=False)
    print(f"\nSaved combined tuning summary to: {combined_csv}")

    print("\n" + "=" * 70)
    print("BEST HYPERPARAMETERS PER TARGET (paste into build_models() tuned_params=... )")
    print("=" * 70)
    for target_col, best_params in per_target_best.items():
        print(f"\n{target_col}:")
        for model_name, params in best_params.items():
            print(f"  {model_name}: {params}")

    return per_target_best, combined
#========================================================================================#


#=====================ADDED 8/10/2026: load tuned params for --mode full================#
def load_tuned_params_by_target(csv_path):
    """
    Reads best_hyperparameters_ALL_TARGETS.csv (written by run_hyperparameter_tuning(),
    i.e. a prior --mode tune run) and reshapes it into:
        {target_col: {"RandomForest": {...}, "GradientBoosting": {...}, "XGBoost": {...}}}
    ready to hand to build_models(tuned_params=...) / run_experiment(tuned_params=...).

    target_col here matches the actual dataframe column name used during tuning
    (e.g. "Delay (ns)"), NOT the short --targets key ("delay") -- this lines up
    directly with target_cols_forward in main().
    """
    if not os.path.exists(csv_path):
        print(f"WARNING: tuned-params file not found: {csv_path}")
        print("  --use-tuned-params will have no effect; all models fall back to "
              "fixed defaults. Run '--mode tune' first to generate this file.")
        return {}

    tuned_df = pd.read_csv(csv_path)
    tuned_by_target = {}
    n_parsed = 0
    for _, row in tuned_df.iterrows():
        target_col = row["Target"]
        model_name = row["Model"]
        try:
            params = ast.literal_eval(row["BestParams"])
        except (ValueError, SyntaxError) as e:
            print(f"WARNING: could not parse BestParams for {target_col}/{model_name}: {e}")
            continue
        tuned_by_target.setdefault(target_col, {})[model_name] = params
        n_parsed += 1

    print(f"Loaded tuned hyperparameters for {len(tuned_by_target)} target(s) "
          f"({n_parsed} model entries) from: {csv_path}")
    for target_col, model_dict in tuned_by_target.items():
        print(f"  {target_col}: {list(model_dict.keys())}")

    return tuned_by_target
#========================================================================================#

def load_and_clean_all_csvs(data_folder_or_folders, recursive=True):
    """
    Load & clean CSVs.

    Accepts either:
      - a single folder path (str)      -> legacy single-folder mode
      - a list/tuple of folder paths    -> multi-segment mode; each row is
                                            tagged with a "Segment" column
                                            derived from its folder name
                                            (e.g. "segment0_batch0_depthgrp0")

    Every CSV found is searched for recursively within each folder, since a
    segment folder may hold nested subfolders of output CSVs.
    """
    if isinstance(data_folder_or_folders, (str, os.PathLike)):
        folder_list = [str(data_folder_or_folders)]
    else:
        folder_list = list(data_folder_or_folders)

    csv_files = []
    file_to_segment = {}
    file_to_shard = {}
    for folder in folder_list:
        # Recursive glob reaches segmentN/.../shardK_bdX.XXXXXX/*.csv without
        # needing to know the shard folders explicitly.
        pattern = os.path.join(folder, "**", "*.csv")
        found = glob.glob(pattern, recursive=recursive)
        segment_label = os.path.basename(os.path.normpath(folder))
        for f in found:
            file_to_segment[f] = segment_label
            # Pull out the "shardK_bdX.XXXXXX" path component, if present,
            # for per-shard bookkeeping/sanity-checking.
            shard_match = re.search(r"shard\d+_bd[\d.]+", f.replace("\\", "/"))
            file_to_shard[f] = shard_match.group(0) if shard_match else "unknown_shard"
        csv_files.extend(found)

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in: {folder_list}")

    print(f"\nFound {len(csv_files)} CSV files across {len(folder_list)} folder(s)")

    dfs = []

    COLUMN_ALIASES = {
        "VWC (cc/cc)": "VWC",
        "VWC (%)": "VWC",
        "vwc": "VWC",

        # New dataset column names -> old internal names
        "ConcreteDepth_inch": "conc_t",
        "BulkDensity": "b_d",
        "Distance_m": "soil_d",
        "ClusterSize": "cluster_size"
    }

    DEFAULT_VALUES = {
        "VWC": DEFAULT_VWC,
        "b_d": DEFAULT_BD,
        "cluster_size": DEFAULT_CLUSTER_SIZE,
        "soil_d": DEFAULT_SOIL_D,
        "conc_t": DEFAULT_CONC_T
    }

    numeric_cols = [
        "VWC",
        "Pkt_Intvl (ms)",
        "Pkt_Size (Byte)",
        "#SentPackets",
        "#ReceivedPackets",
        "#LostPackets",
        "Pkt_Deliv_Rate (%)",
        "AvgThroughput (kbps)",
        "Delay (ns)",
        "Jitter (ns)",
        "b_d",
        "cluster_size",
        "soil_d",
        "conc_t"
    ]

    for file in csv_files:
        print(f"\nLoading: {os.path.basename(file)}")

        df = pd.read_csv(file)
        df.columns = df.columns.str.strip()

        # Rename new dataset columns to old internal names
        df = df.rename(columns=COLUMN_ALIASES)

        # Future-proofing:
        # If repeated header rows are injected inside future CSV files,
        # they will contain text like "VWC" or "ConcreteDepth_inch" in data rows.
        # Converting numeric columns with errors='coerce' will turn those rows into NaN,
        # and we drop them later.

        # Add source file and row index before filtering
        df["SourceFile"] = os.path.basename(file)
        df["RowInFile"] = np.arange(len(df))
        df["Segment"] = file_to_segment[file]
        df["Shard"] = file_to_shard[file]

        # Add missing feature columns with defaults, if needed
        for col, default_val in DEFAULT_VALUES.items():
            if col not in df.columns:
                df[col] = default_val
                print(f"  {col} missing -> inserted default {default_val}")

        missing_cols = [col for col in KEEP_COLS if col not in df.columns]
        if missing_cols:
            print(f"  Skipping {os.path.basename(file)}; missing columns: {missing_cols}")
            continue

        df = df[KEEP_COLS].copy()

        # Convert numeric columns
        for col in numeric_cols:
            df[col] = pd.to_numeric(df[col], errors="coerce")

        # Drop repeated-header rows or malformed rows
        before_rows = len(df)
        df = df.dropna(subset=numeric_cols).copy()
        after_rows = len(df)

        if after_rows < before_rows:
            print(f"  Dropped {before_rows - after_rows} non-numeric/header-like rows")

        print("  Rows kept:", len(df))
        print("  Unique VWC values:", sorted(df["VWC"].dropna().unique())[:20])
        print("  Unique b_d values:", sorted(df["b_d"].dropna().unique())[:20])
        print("  Unique conc_t values:", sorted(df["conc_t"].dropna().unique())[:20])
        print("  Unique pkt interval values:", sorted(df["Pkt_Intvl (ms)"].dropna().unique())[:20])

        dfs.append(df)

    if not dfs:
        raise ValueError("No usable CSV files found after column filtering.")

    merged = pd.concat(dfs, ignore_index=True)

    # Derived target columns
    merged["DropRate"] = merged["#LostPackets"] / merged["#SentPackets"]
    merged["DropRate"] = merged["DropRate"].replace([np.inf, -np.inf], np.nan)

    merged["DropRate_percent"] = 100.0 - merged["Pkt_Deliv_Rate (%)"]

    # Numeric segment index (0-5) parsed from the "Segment" folder-name tag,
    # used for cumulative training order and the accuracy-vs-data-volume plot.
    merged["SegmentIndex"] = (
        merged["Segment"].astype(str).str.extract(r"segment(\d+)_")[0]
    )
    merged["SegmentIndex"] = pd.to_numeric(merged["SegmentIndex"], errors="coerce")

    # Create row-position blocks inside each source file
    # This helps ensure top/middle/bottom regions of each CSV are represented in train/test.
    merged["RowBlock"] = merged.groupby("SourceFile")["RowInFile"].transform(
        lambda s: pd.qcut(
            s.rank(method="first"),
            q=5,
            labels=False,
            duplicates="drop"
        )
    )

    merged["SplitStratum"] = (
        merged["SourceFile"].astype(str)
        + "_block_"
        + merged["RowBlock"].astype(str)
    )

    # Remove exact duplicate rows only after SplitStratum is created
    # merged = merged.drop_duplicates().reset_index(drop=True)
    # Commented out for NS-3 synthetic dataset:
    # We want to preserve repeated rows because they may represent valid repeated simulation observations.
    # Keep row index clean without dropping duplicates
    merged = merged.reset_index(drop=True)

    print("\nMerged dataset shape:", merged.shape)
    print("\nMerged unique VWC values:")
    print(sorted(merged["VWC"].dropna().unique()))

    print("\nMerged unique BulkDensity/b_d values:")
    print(sorted(merged["b_d"].dropna().unique()))

    print("\nMerged unique ConcreteDepth/conc_t values:")
    print(sorted(merged["conc_t"].dropna().unique()))

    print("\nMerged unique Distance/soil_d values:")
    print(sorted(merged["soil_d"].dropna().unique())[:30])

    print("\nMerged unique ClusterSize values:")
    print(sorted(merged["cluster_size"].dropna().unique()))

    print("\nRows per segment:")
    print(merged.groupby("Segment").size().sort_index())

    print("\nRows per segment/shard (expect ~4 shards per segment):")
    print(merged.groupby(["Segment", "Shard"]).size().sort_index())

    return merged

def evaluate_model(name, model, X_train, X_test, y_train, y_test):
    model.fit(X_train, y_train)
    pred = model.predict(X_test)

    mae = mean_absolute_error(y_test, pred)
    rmse = np.sqrt(mean_squared_error(y_test, pred))
    r2 = r2_score(y_test, pred)

    print(f"{name}:")
    print(f"  MAE  = {mae:.6f}")
    print(f"  RMSE = {rmse:.6f}")
    print(f"  R2   = {r2:.6f}")

    return model

def run_experiment(df, target_col, feature_cols, tag="with_sent", tuned_params=None):
    """
    tuned_params : dict or None (ADDED 8/10/2026, extended 8/11/2026)
        Optional {"RandomForest": {...}, "GradientBoosting": {...}, "XGBoost": {...}}
        for this target_col, e.g. from load_tuned_params_by_target(). When given,
        tuned COPIES of those three models are trained and evaluated ALONGSIDE
        the standard fixed-default models below (as "RandomForest_Tuned",
        "GradientBoosting_Tuned", "XGBoost_Tuned"), so the resulting
        accuracy_<target>_<tag>.csv contains both untuned and tuned rows side
        by side for direct comparison -- no separate run needed.

        As of 8/11/2026, this also adds "VotingEnsemble_Tuned" and
        "StackingEnsemble_Tuned" -- ensembles built from the SAME tuned
        RF/GB/XGBoost base learners above, rather than the untuned defaults
        used by the standard "VotingEnsemble"/"StackingEnsemble" rows. This
        answers the paper's open question of whether narrowing RandomForest's
        overfitting risk (or tuning GB/XGBoost) changes the Stacking
        Ensemble's standing relative to a standalone RandomForest.
    """
    print("\n" + "=" * 70)
    print(f"TARGET: {target_col}")
    print(f"FEATURE SET: {tag}")
    print(f"FEATURES: {feature_cols}")
    print("=" * 70)

    needed_cols = feature_cols + [target_col, "SplitStratum"]
    work_df = df[needed_cols].dropna().copy()

    X = work_df[feature_cols]
    y = work_df[target_col]

    # Stratified shuffled split:
    # stratify=SplitStratum forces sampling from each file and each row-position block.
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y,
        test_size=0.2,
        random_state=42,
        shuffle=True,
        stratify=work_df["SplitStratum"]
    )

    print(f"Training rows: {len(X_train)}")
    print(f"Testing rows : {len(X_test)}")

    models = build_models()  # standard fixed-default zoo, unchanged

    if tuned_params:
        tuned_zoo = build_models(tuned_params=tuned_params)
        added_tuned_names = []
        for base_name in ["RandomForest", "GradientBoosting", "XGBoost"]:
            if base_name in tuned_params:
                models[f"{base_name}_Tuned"] = tuned_zoo[base_name]
                added_tuned_names.append(f"{base_name}_Tuned")

        # ADDED 8/11/2026: tuned ensembles. build_models() already wires
        # VotingEnsemble/StackingEnsemble from base_rf/base_gb/base_xgb, so
        # when ANY of the three base learners were tuned, tuned_zoo's
        # ensembles reflect that -- surface them as their own rows so the
        # accuracy CSV can compare StackingEnsemble vs. StackingEnsemble_Tuned
        # directly (same for VotingEnsemble), not just the individual trees.
        if any(name in tuned_params for name in ["RandomForest", "GradientBoosting", "XGBoost"]):
            models["VotingEnsemble_Tuned"] = tuned_zoo["VotingEnsemble"]
            models["StackingEnsemble_Tuned"] = tuned_zoo["StackingEnsemble"]
            added_tuned_names += ["VotingEnsemble_Tuned", "StackingEnsemble_Tuned"]

        print(f"Added tuned variant(s): {added_tuned_names}")

    results = []
    trained = {}

    for name, model in models.items():
        model.fit(X_train, y_train)
        pred = model.predict(X_test)

        mae = mean_absolute_error(y_test, pred)
        rmse = np.sqrt(mean_squared_error(y_test, pred))
        r2 = r2_score(y_test, pred)

        print(f"{name}:")
        print(f"  MAE  = {mae:.6f}")
        print(f"  RMSE = {rmse:.6f}")
        print(f"  R2   = {r2:.6f}")

        results.append({
            "Target": target_col,
            "FeatureSet": tag,
            "Model": name,
            "MAE": mae,
            "RMSE": rmse,
            "R2": r2,
            "TrainRows": len(X_train),
            "TestRows": len(X_test)
        })

        trained[name] = model

    results_df = pd.DataFrame(results)

    # Clean target name for file names
    safe_target = (
        target_col.replace(" ", "_")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
        .replace("%", "percent")
    )

    # Save per-target results
    results_csv = os.path.join(
        OUTPUT_FOLDER,
        f"accuracy_{safe_target}_{tag}.csv"
    )
    results_df.to_csv(results_csv, index=False)
    print(f"\nSaved accuracy result to: {results_csv}")
    print("(R2 comparison plot: run iout_ml_plots.py -- it reads this CSV)")

    # Feature importance from Random Forest -- CSV export only, no plot here.
    rf_model = trained["RandomForest"].named_steps["model"]
    importances = rf_model.feature_importances_

    importance_df = pd.DataFrame({
        "Feature": feature_cols,
        "Importance": importances
    }).sort_values("Importance", ascending=False)

    print("\nFeature importances (Random Forest):")
    print(importance_df)

    importance_csv = os.path.join(
        OUTPUT_FOLDER,
        f"feature_importance_{safe_target}_{tag}.csv"
    )
    importance_df.to_csv(importance_csv, index=False)
    print(f"Saved feature-importance data to: {importance_csv}")
    print("(Feature-importance plot: run iout_ml_plots.py -- it reads this CSV)")

    return results_df

#=====================ADDED 7/27/2026: cumulative retraining across 6 segments==========================#
def train_cumulative_by_segment(
    segment_folders,
    target_cols_forward,
    feature_sets=None,
    up_to_segment=None,
):
    """
    Cumulative retraining (matches the roadmap's run_segment_training.py idea,
    folded into this script): for each segment in order, load ALL CSVs from
    segment 0 through the current segment, retrain the FULL model zoo from
    scratch on that cumulative dataset (not incremental — see the analytical
    note in the chat response for why), checkpoint every fitted model via
    joblib, and append one row per (segment, target, feature-set, model) to
    segment_progress_summary.csv. That file is the accuracy-vs-data-volume
    table/plot for the paper's results section.

    Progress is saved to disk after every segment, so a crash partway through
    (e.g. segment 4 of 6) does not lose earlier segments' results.

    Parameters
    ----------
    segment_folders : list of str
        Segment folders in ascending order, e.g. from discover_segment_folders().
    target_cols_forward : list of str
        Targets to model, e.g. ["DropRate_percent", "Delay (ns)", ...]
    feature_sets : list of (feature_cols, tag) tuples, default both with/without #SentPackets
    up_to_segment : int or None
        If set, stop after this segment index (0-5) instead of running all
        discovered segments — useful for resuming/testing.
    """
    if feature_sets is None:
        feature_sets = [
            (FEATURE_COLS_WITH_SENT, "with_sent"),
            (FEATURE_COLS_NO_SENT, "no_sent"),
        ]

    all_progress_rows = []
    running_folders = []

    for seg_path in segment_folders:
        seg_name = os.path.basename(os.path.normpath(seg_path))
        m = re.search(r"segment(\d+)_", seg_name)
        seg_idx = int(m.group(1)) if m else -1

        running_folders.append(seg_path)

        if up_to_segment is not None and seg_idx > up_to_segment:
            break

        print("\n" + "#" * 70)
        print(f"# CUMULATIVE TRAINING THROUGH {seg_name}  "
              f"({len(running_folders)} segment(s) merged so far)")
        print("#" * 70)

        df = load_and_clean_all_csvs(running_folders)
        cumulative_rows = len(df)

        seg_model_dir = os.path.join(MODELS_FOLDER, seg_name)
        os.makedirs(seg_model_dir, exist_ok=True)

        for feature_cols, tag in feature_sets:
            for target_col in target_cols_forward:
                needed_cols = list(dict.fromkeys(feature_cols + [target_col, "SplitStratum"]))
                work_df = df[needed_cols].dropna().copy()
                if work_df.empty:
                    print(f"  Skipping {target_col} / {tag}: no valid rows")
                    continue

                X = work_df[feature_cols]
                y = work_df[target_col]

                X_train, X_test, y_train, y_test = train_test_split(
                    X, y,
                    test_size=0.2,
                    random_state=42,
                    shuffle=True,
                    stratify=work_df["SplitStratum"]
                )

                models = build_models()
                for model_name, model in models.items():
                    t0 = time.time()
                    model.fit(X_train, y_train)
                    train_secs = time.time() - t0

                    pred = model.predict(X_test)
                    mae = mean_absolute_error(y_test, pred)
                    rmse = np.sqrt(mean_squared_error(y_test, pred))
                    r2 = r2_score(y_test, pred)

                    print(f"[{seg_name}] {target_col} ({tag}) - {model_name}: "
                          f"MAE={mae:.4f} RMSE={rmse:.4f} R2={r2:.4f} "
                          f"({train_secs:.1f}s, n={len(X_train)})")

                    ckpt_path = os.path.join(
                        seg_model_dir, f"{safe_name(target_col)}_{tag}_{model_name}.joblib"
                    )
                    joblib.dump(model, ckpt_path)

                    all_progress_rows.append({
                        "Segment": seg_name,
                        "SegmentIndex": seg_idx,
                        "CumulativeSegments": len(running_folders),
                        "CumulativeRows": cumulative_rows,
                        "Target": target_col,
                        "FeatureSet": tag,
                        "Model": model_name,
                        "MAE": mae,
                        "RMSE": rmse,
                        "R2": r2,
                        "TrainRows": len(X_train),
                        "TestRows": len(X_test),
                        "TrainTimeSec": train_secs,
                        "CheckpointPath": ckpt_path,
                    })

        # Save/append after every segment so partial progress is never lost.
        progress_df = pd.DataFrame(all_progress_rows)
        progress_df.to_csv(PROGRESS_SUMMARY_CSV, index=False)
        print(f"\nSaved cumulative progress summary through {seg_name} -> {PROGRESS_SUMMARY_CSV}")

    return pd.DataFrame(all_progress_rows)


# NOTE: plot_accuracy_vs_data_volume() moved to iout_ml_plots.py -- it only
# reads segment_progress_summary.csv, no training involved, so it belongs
# with the other plotting code, not here.
#
# NOTE: delay_vs_cluster_size() removed -- it was dead code, never called
# from main() in the original script (confirmed via grep before this split).
#===========================================================================================================#
def compute_prediction_error_vs_concrete_depth(
        df,
        target_col,
        feature_cols,
        tag="with_sent",
        group_by_depth=True
):
    """
    Plot model prediction error vs concrete depth.

    Prediction error = predicted value - true value

    X-axis:
        conc_t, which represents ConcreteDepth_inch

    Y-axis:
        prediction error for the selected target

    Models (see build_model_colors() for the authoritative mapping):
        Linear Regression: orange
        Random Forest: green
        Gradient Boosting: blue
        XGBoost: red
        MLPRegressor (simple DNN): purple
        VotingEnsemble: brown
        StackingEnsemble: black

    Parameters
    ----------
    df : pandas DataFrame
        Cleaned merged dataset returned by load_and_clean_all_csvs()

    target_col : str
        Target metric to predict, for example:
            "Delay (ns)"
            "DropRate_percent"

    feature_cols : list
        Feature columns to use for training.
        Use FEATURE_COLS_WITH_SENT or FEATURE_COLS_NO_SENT.

    tag : str
        Label for saving files, e.g., "with_sent" or "no_sent"

    group_by_depth : bool
        If True, plot mean prediction error per concrete depth.
        If False, plot every test sample as scatter points.
    """

    print("\n" + "=" * 70)
    print(f"PREDICTION ERROR PLOT VS CONCRETE DEPTH")
    print(f"TARGET: {target_col}")
    print(f"FEATURE SET: {tag}")
    print("=" * 70)

    # Use dict.fromkeys(...) to remove duplicate column names while preserving order.
    # This is important because "conc_t" is already inside feature_cols.
    needed_cols = list(dict.fromkeys(feature_cols + [target_col, "conc_t", "SplitStratum"]))

    missing = [c for c in needed_cols if c not in df.columns]
    if missing:
        print(f"Missing columns for this plot: {missing}")
        return

    work_df = df[needed_cols].dropna().copy()

    if work_df.empty:
        print(f"No valid rows found for target: {target_col}")
        return

    X = work_df[feature_cols]
    y = work_df[target_col]

    # Stratified shuffled split:
    # This keeps samples from different files and row-position blocks
    # distributed into both train and test.
    X_train, X_test, y_train, y_test, idx_train, idx_test = train_test_split(
        X,
        y,
        work_df.index,
        test_size=0.2,
        random_state=42,
        shuffle=True,
        stratify=work_df["SplitStratum"]
    )

    conc_test = work_df.loc[idx_test, "conc_t"].to_numpy().ravel()

    print(f"Training rows: {len(X_train)}")
    print(f"Testing rows : {len(X_test)}")

    models = build_models()

    all_error_rows = []

    for model_name, model in models.items():
        print(f"\nTraining model: {model_name}")

        model.fit(X_train, y_train)
        y_pred = model.predict(X_test)

        # Convert everything to clean 1-D arrays
        y_true_arr = np.asarray(y_test).ravel()
        y_pred_arr = np.asarray(y_pred).ravel()
        conc_arr = conc_test

        error_arr = y_pred_arr - y_true_arr

        # Extra safety check
        print("  Array lengths:",
              "conc_arr =", len(conc_arr),
              "y_true_arr =", len(y_true_arr),
              "y_pred_arr =", len(y_pred_arr),
              "error_arr =", len(error_arr))

        temp_df = pd.DataFrame({
            "conc_t": conc_arr,
            "TrueValue": y_true_arr,
            "PredictedValue": y_pred_arr,
            "PredictionError": error_arr,
            "Model": [model_name] * len(y_true_arr)
        })

        all_error_rows.append(temp_df)

        mae = mean_absolute_error(y_true_arr, y_pred_arr)
        rmse = np.sqrt(mean_squared_error(y_true_arr, y_pred_arr))
        r2 = r2_score(y_true_arr, y_pred_arr)

        print(f"{model_name}:")
        print(f"  MAE  = {mae:.6f}")
        print(f"  RMSE = {rmse:.6f}")
        print(f"  R2   = {r2:.6f}")

    error_df = pd.concat(all_error_rows, ignore_index=True)

    safe_target = (
        target_col.replace(" ", "_")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
        .replace("%", "percent")
    )

    out_csv = os.path.join(
        OUTPUT_FOLDER,
        f"prediction_error_vs_concrete_depth_{safe_target}_{tag}.csv"
    )
    error_df.to_csv(out_csv, index=False)
    print(f"\nSaved prediction-error data to: {out_csv}")
    print("(Plot: run iout_ml_plots.py -- it reads this CSV, no need to retrain)")

#---------Wrapper Function for the above------------#
def compute_error_vs_concrete_depth(df, target_specs, include_no_sent=False, include_with_sent=True):
    """
    Generate prediction-error-vs-concrete-depth data for every (target_col,
    label) pair in target_specs -- e.g. all 4 targets, or a subset when
    --targets restricts the run.

    include_no_sent : bool (ADDED 8/11/2026)
        Default False = only "with_sent" (original behavior, unchanged for
        existing --mode full runs). True also runs the "no_sent" feature
        set, roughly doubling this stage's runtime -- use
        generate_no_sent_corner_case_data.py for a standalone pass that
        reuses the already-merged CSV instead of re-running the full
        1200-file data load.
    include_with_sent : bool
        Set False (paired with include_no_sent=True) to run ONLY the
        no_sent pass -- used by generate_no_sent_corner_case_data.py so it
        doesn't waste time redoing the with_sent files that already exist.
    """
    feature_set_specs = []
    if include_with_sent:
        feature_set_specs.append(("with_sent", FEATURE_COLS_WITH_SENT))
    if include_no_sent:
        feature_set_specs.append(("no_sent", FEATURE_COLS_NO_SENT))

    for tag, feature_cols in feature_set_specs:
        for target_col, label in target_specs:
            print(f"\n===== {label.upper()} PREDICTION ERROR VS CONCRETE DEPTH ({tag}) =====")
            compute_prediction_error_vs_concrete_depth(
                df=df,
                target_col=target_col,
                feature_cols=feature_cols,
                tag=tag,
                group_by_depth=True
            )
#======================================================================================#
def compute_prediction_error_vs_feature(
        df,
        target_col,
        feature_cols,
        x_feature,
        x_label,
        tag="with_sent",
        group_by_feature=True
):
    """
    Plot model prediction error vs a selected feature.

    Prediction error = predicted value - true value

    X-axis:
        selected feature, for example:
            VWC
            b_d
            soil_d

    Y-axis:
        prediction error for the selected target

    Models (see build_model_colors() for the authoritative mapping):
        Linear Regression: orange
        Random Forest: green
        Gradient Boosting: blue
        XGBoost: red
        MLPRegressor (simple DNN): purple
        VotingEnsemble: brown
        StackingEnsemble: black
    """

    print("\n" + "=" * 70)
    print(f"PREDICTION ERROR PLOT VS {x_feature}")
    print(f"TARGET: {target_col}")
    print(f"FEATURE SET: {tag}")
    print("=" * 70)

    # Remove duplicate columns while preserving order.
    # This is important because x_feature may already be inside feature_cols.
    needed_cols = list(dict.fromkeys(feature_cols + [target_col, x_feature, "SplitStratum"]))

    missing = [c for c in needed_cols if c not in df.columns]
    if missing:
        print(f"Missing columns for this plot: {missing}")
        return

    work_df = df[needed_cols].dropna().copy()

    if work_df.empty:
        print(f"No valid rows found for target: {target_col}")
        return

    X = work_df[feature_cols]
    y = work_df[target_col]

    X_train, X_test, y_train, y_test, idx_train, idx_test = train_test_split(
        X,
        y,
        work_df.index,
        test_size=0.2,
        random_state=42,
        shuffle=True,
        stratify=work_df["SplitStratum"]
    )

    x_test = work_df.loc[idx_test, x_feature].to_numpy().ravel()

    print(f"Training rows: {len(X_train)}")
    print(f"Testing rows : {len(X_test)}")

    models = build_models()

    all_error_rows = []

    for model_name, model in models.items():
        print(f"\nTraining model: {model_name}")

        model.fit(X_train, y_train)
        y_pred = model.predict(X_test)

        y_true_arr = np.asarray(y_test).ravel()
        y_pred_arr = np.asarray(y_pred).ravel()
        x_arr = np.asarray(x_test).ravel()

        error_arr = y_pred_arr - y_true_arr

        print("  Array lengths:",
              "x_arr =", len(x_arr),
              "y_true_arr =", len(y_true_arr),
              "y_pred_arr =", len(y_pred_arr),
              "error_arr =", len(error_arr))

        temp_df = pd.DataFrame({
            x_feature: x_arr,
            "TrueValue": y_true_arr,
            "PredictedValue": y_pred_arr,
            "PredictionError": error_arr,
            "Model": [model_name] * len(y_true_arr)
        })

        all_error_rows.append(temp_df)

        mae = mean_absolute_error(y_true_arr, y_pred_arr)
        rmse = np.sqrt(mean_squared_error(y_true_arr, y_pred_arr))
        r2 = r2_score(y_true_arr, y_pred_arr)

        print(f"{model_name}:")
        print(f"  MAE  = {mae:.6f}")
        print(f"  RMSE = {rmse:.6f}")
        print(f"  R2   = {r2:.6f}")

    error_df = pd.concat(all_error_rows, ignore_index=True)

    safe_target = (
        target_col.replace(" ", "_")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
        .replace("%", "percent")
    )

    safe_feature = (
        x_feature.replace(" ", "_")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
        .replace("%", "percent")
    )

    out_csv = os.path.join(
        OUTPUT_FOLDER,
        f"prediction_error_vs_{safe_feature}_{safe_target}_{tag}.csv"
    )
    error_df.to_csv(out_csv, index=False)
    print(f"\nSaved prediction-error data to: {out_csv}")
    print("(Plot: run iout_ml_plots.py -- it reads this CSV, no need to retrain)")
#---------Wrapper Function for the above------------#
def compute_error_vs_vwc_bd_soild(df, target_specs, include_no_sent=False, include_with_sent=True):
    """
    Generate prediction-error-vs-feature data for every (target_col, label)
    pair in target_specs, against three input features:

        1. VWC
        2. b_d
        3. soil_d

    include_no_sent / include_with_sent : bool (ADDED 8/11/2026) -- see
    compute_error_vs_concrete_depth().
    """

    feature_plot_specs = [
        ("VWC", "Volumetric Water Content, VWC"),
        ("b_d", "Bulk Density, b_d"),
        ("soil_d", "Soil/Communication Distance (m)")
    ]

    feature_set_specs = []
    if include_with_sent:
        feature_set_specs.append(("with_sent", FEATURE_COLS_WITH_SENT))
    if include_no_sent:
        feature_set_specs.append(("no_sent", FEATURE_COLS_NO_SENT))

    for tag, feature_cols in feature_set_specs:
        for x_feature, x_label in feature_plot_specs:
            for target_col, target_label in target_specs:
                print(f"\n===== {target_label.upper()} PREDICTION ERROR VS {x_feature} ({tag}) =====")

                compute_prediction_error_vs_feature(
                    df=df,
                    target_col=target_col,
                    feature_cols=feature_cols,
                    x_feature=x_feature,
                    x_label=x_label,
                    tag=tag,
                    group_by_feature=True
                )
#======================================================================================#
def main():
    parser = argparse.ArgumentParser(
        description="Ensemble ML pipeline for the NS-3 underground IoT dataset (6 segments)"
    )
    parser.add_argument(
        "--mode", choices=["full", "cumulative", "tune"], default="full",
        help="'full' = one train/test pass on ALL discovered segments merged together "
             "(the main-results run); 'cumulative' = retrain from scratch after each "
             "segment and checkpoint, producing the accuracy-vs-data-volume table/plot; "
             "'tune' = RandomizedSearchCV/GridSearchCV over RandomForest, GradientBoosting, "
             "and XGBoost (the StackingEnsemble base learners) on a stratified sub-sample -- "
             "does not train the full model zoo or write the usual accuracy/plot CSVs."
    )
    parser.add_argument(
        "--tune-sample-frac", type=float, default=0.15,
        help="[--mode tune only] Fraction of the merged dataset to search over "
             "(stratified by SplitStratum). Default 0.15 keeps an overnight full-dataset "
             "run's search time reasonable; raise later for a final confirmation pass."
    )
    parser.add_argument(
        "--tune-n-iter", type=int, default=20,
        help="[--mode tune only, search_type=random] Number of random hyperparameter "
             "combinations to try per model (RandomizedSearchCV n_iter)."
    )
    parser.add_argument(
        "--tune-cv", type=int, default=3,
        help="[--mode tune only] Number of cross-validation folds used to score each "
             "hyperparameter combination."
    )
    parser.add_argument(
        "--search-type", choices=["random", "grid"], default="random",
        help="[--mode tune only] 'random' = RandomizedSearchCV (fast, recommended first "
             "pass). 'grid' = GridSearchCV, exhaustive over the same search space -- much "
             "slower, best used later on a narrowed-down grid."
    )
    parser.add_argument(
        "--use-tuned-params", action="store_true",
        help="[--mode full only] Load best_hyperparameters_ALL_TARGETS.csv (from a prior "
             "--mode tune run) and additionally train/evaluate RandomForest_Tuned, "
             "GradientBoosting_Tuned, and XGBoost_Tuned alongside the standard "
             "fixed-default models, for each target's 'with_sent' feature set (matching "
             "what --mode tune was run on). Results land in the SAME accuracy_<target>_"
             "with_sent.csv, so tuned and untuned rows are directly comparable."
    )
    parser.add_argument(
        "--tuned-params-csv", default=None,
        help="Override path to the tuned-hyperparameters CSV (default: "
             "<OUTPUT_FOLDER>/best_hyperparameters_ALL_TARGETS.csv)."
    )
    parser.add_argument(
        "--include-no-sent-corner-case", action="store_true",
        help="[--mode full only] Also generate prediction_error_vs_*_no_sent.csv corner-case "
             "files (default: with_sent only). Roughly doubles the corner-case stage's "
             "runtime. Prefer generate_no_sent_corner_case_data.py for a standalone pass "
             "that reuses the already-merged dataset CSV instead."
    )
    parser.add_argument(
        "--data-root", default=DATA_ROOT,
        help="Folder containing segmentN_batchB_depthgrpD subfolders (default: current folder)."
    )
    parser.add_argument(
        "--segment", type=int, default=None,
        help="In --mode cumulative, stop after this segment index (0-5). Default: run all found."
    )
    parser.add_argument(
        "--targets", default="all",
        help=f"Comma-separated targets to train (reduces runtime roughly proportionally, "
             f"since each target retrains the full model zoo independently): "
             f"{TARGET_KEYS_ORDERED}. Default: all 4. Example: --targets delay,jitter"
    )
    args = parser.parse_args()

    try:
        target_specs = resolve_target_selection(args.targets)
    except ValueError as e:
        print(str(e))
        return
    target_cols_forward = [col for col, _label in target_specs]

    log_path = start_run_log(args.mode)
    print(f"Logging full run output to: {log_path}")
    print(f"Run started: {datetime.datetime.now().isoformat()}")
    print(f"Targets selected: {[label for _col, label in target_specs]} "
          f"({len(target_specs)} of {len(TARGET_KEYS_ORDERED)} available)")

    segment_folders = discover_segment_folders(args.data_root)
    if not segment_folders:
        print(f"No segmentN_batchB_depthgrpD folders found under '{args.data_root}'.")
        print(f"Falling back to legacy single-folder DATA_FOLDER = {DATA_FOLDER}")
        data_source = DATA_FOLDER
    else:
        print(f"\nDiscovered {len(segment_folders)} segment folder(s):")
        for f in segment_folders:
            print(f"  - {f}")
        data_source = segment_folders

    if args.mode == "tune":
        df = load_and_clean_all_csvs(data_source)
        print("\nFinal merged dataset shape:", df.shape)
        run_hyperparameter_tuning(
            df, target_specs,
            sample_frac=args.tune_sample_frac,
            n_iter=args.tune_n_iter,
            cv=args.tune_cv,
            search_type=args.search_type,
        )
        print(f"\nRun finished: {datetime.datetime.now().isoformat()}")
        return

    if args.mode == "cumulative":
        progress_df = train_cumulative_by_segment(
            segment_folders if segment_folders else [DATA_FOLDER],
            target_cols_forward=target_cols_forward,
            up_to_segment=args.segment,
        )
        print("\nCumulative training complete.")
        print(f"Checkpoints saved under: {MODELS_FOLDER}/segmentN_.../")
        print(f"Progress summary saved to: {PROGRESS_SUMMARY_CSV}")
        print("(Accuracy-vs-data-volume plot: run iout_ml_plots.py -- it reads that CSV)")
        print(f"Run finished: {datetime.datetime.now().isoformat()}")
        return

    # ---- 'full' mode (default): legacy single-pass behavior, now across ALL discovered segments ----
    df = load_and_clean_all_csvs(data_source)

    print("\nFinal merged dataset shape:", df.shape)
    print("\nColumns:")
    print(df.columns.tolist())
    print("\nFirst 5 rows:")
    print(df.head())

    merged_csv = os.path.join(
        OUTPUT_FOLDER,
        "merged_cleaned_ns3_dataset_new_iout_2026.csv"
    )
    df.to_csv(merged_csv, index=False)
    print(f"\nSaved cleaned merged dataset to: {merged_csv}")

    compute_error_vs_concrete_depth(df, target_specs, include_no_sent=args.include_no_sent_corner_case)
    compute_error_vs_vwc_bd_soild(df, target_specs, include_no_sent=args.include_no_sent_corner_case)

    tuned_by_target = {}
    if args.use_tuned_params:
        tuned_csv_path = args.tuned_params_csv or os.path.join(
            OUTPUT_FOLDER, "best_hyperparameters_ALL_TARGETS.csv"
        )
        tuned_by_target = load_tuned_params_by_target(tuned_csv_path)

    all_results = []

    print("\n===== FORWARD PREDICTION: MODEL WITH #SentPackets =====")
    for target in target_cols_forward:
        # NOTE: tuned params only ever get applied here, on the "with_sent"
        # feature set -- that's the exact feature set run_hyperparameter_tuning()
        # searched over, so applying the same tuned values to the "no_sent"
        # feature set below would be an unvalidated extrapolation.
        result_df = run_experiment(
            df,
            target_col=target,
            feature_cols=FEATURE_COLS_WITH_SENT,
            tag="with_sent",
            tuned_params=tuned_by_target.get(target)
        )
        all_results.append(result_df)

    print("\n===== FORWARD PREDICTION: MODEL WITHOUT #SentPackets =====")
    for target in target_cols_forward:
        result_df = run_experiment(
            df,
            target_col=target,
            feature_cols=FEATURE_COLS_NO_SENT,
            tag="no_sent"
        )
        all_results.append(result_df)

    # Save combined summary
    summary_df = pd.concat(all_results, ignore_index=True)
    summary_csv = os.path.join(
        OUTPUT_FOLDER,
        "model_accuracy_summary_forward.csv"
    )
    summary_df.to_csv(summary_csv, index=False)

    print(f"\nSaved combined model accuracy summary to: {summary_csv}")
    print("\nCombined summary:")
    print(summary_df)
    print(f"\nRun finished: {datetime.datetime.now().isoformat()}")

if __name__ == "__main__":
    main()