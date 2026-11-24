"""
iout_ml_plots.py -- SCRIPT 2 of 3

ALL PLOTTING, reads CSVs produced by iout_ml_train_and_export.py (script 1).
No sklearn / xgboost / joblib needed here -- just pandas + matplotlib, so
this starts fast and is safe to re-run as many times as you want while you
dial in figure size, fonts, colors, etc. for the paper. Never retrains
anything.

Edit the CONFIG block below to change style, then just re-run:
    python iout_ml_plots.py

Regenerate only one category while iterating on it (much faster):
    python iout_ml_plots.py --only r2
    python iout_ml_plots.py --only r2_tuned
    python iout_ml_plots.py --only importance
    python iout_ml_plots.py --only importance_tuned
    python iout_ml_plots.py --only error
    python iout_ml_plots.py --only error_abs
    python iout_ml_plots.py --only data_volume
    python iout_ml_plots.py --only r2,importance      (comma-separated, any combo)

Categories, all 4 targets (DropRate, Delay, Throughput, Jitter) covered
the same way in every category. Every plot saves BOTH .png (for quick
viewing) and .svg (vector, for the paper) -- see SAVE_SVG in CONFIG to
turn the .svg output off if you don't want it.

    r2            -> outputs_ensemble_iout_2026/R2_comparison/R2_comparison_<target>_<tag>.{png,svg}
                      (7 baseline models only; _Tuned rows excluded -- see r2_tuned)
    r2_tuned      -> outputs_ensemble_iout_2026/R2_comparison_tuned/R2_tuned_vs_untuned_<target>_with_sent.{png,svg}
                      (RandomForest/GradientBoosting/XGBoost vs. their _Tuned counterparts;
                      requires accuracy_<target>_with_sent.csv from a --use-tuned-params run)
    importance    -> outputs_ensemble_iout_2026/feature_importance/feature_importance_<target>_<tag>.{png,svg}
                      (untuned RandomForest, as always)
    importance_tuned -> outputs_ensemble_iout_2026/feature_importance_tuned/feature_importance_<Model>_Tuned_<target>_<tag>.{png,svg}
                      (requires generate_tuned_feature_importance.py to have been run first)
    error         -> outputs_ensemble_iout_2026/prediction_error/pred_error/prediction_error_vs_*_<target>_<tag>.{png,svg} (signed)
    error_abs     -> outputs_ensemble_iout_2026/prediction_error/pred_error_absolute/prediction_error_vs_*_<target>_<tag>_AbsError.{png,svg} (|error|)
                      (both error/error_abs now cover BOTH with_sent and no_sent tags;
                      no_sent requires generate_no_sent_corner_case_data.py to have been run first)
    data_volume   -> outputs_ensemble_iout_2026/accuracy_vs_data_volume/accuracy_vs_data_volume_<target>_<tag>_R2.{png,svg}
                      (cumulative-mode only -- see the toggle in main() to disable this section)

Note: error/error_abs require the source CSV to exist. As of the merged
iout_ml_train_and_export.py, a normal --mode full run produces the
with_sent version of all 4 targets' error CSVs natively; no_sent requires
generate_no_sent_corner_case_data.py (compute_throughput_jitter_errors.py
is a separate, older backfill script only needed for pre-merge Throughput/
Jitter data).
"""

import os
import argparse
import pandas as pd
import matplotlib
matplotlib.use("Agg")   # never opens a blocking GUI window
import matplotlib.pyplot as plt

# =========================== CONFIG -- EDIT THIS FOR STYLE ===========================
OUTPUT_FOLDER = "outputs_ensemble_iout_2026"

# Subfolder layout -- one folder per plot category, all nested under OUTPUT_FOLDER.
# prediction_error/ has two subfolders of its own: signed error vs. |error|.
SUBFOLDER_R2 = os.path.join(OUTPUT_FOLDER, "R2_comparison")
SUBFOLDER_R2_TUNED = os.path.join(OUTPUT_FOLDER, "R2_comparison_tuned")
SUBFOLDER_IMPORTANCE = os.path.join(OUTPUT_FOLDER, "feature_importance")
SUBFOLDER_IMPORTANCE_TUNED = os.path.join(OUTPUT_FOLDER, "feature_importance_tuned")
SUBFOLDER_ERROR = os.path.join(OUTPUT_FOLDER, "prediction_error")
SUBFOLDER_ERROR_SIGNED = os.path.join(SUBFOLDER_ERROR, "pred_error")
SUBFOLDER_ERROR_ABS = os.path.join(SUBFOLDER_ERROR, "pred_error_absolute")
SUBFOLDER_DATA_VOLUME = os.path.join(OUTPUT_FOLDER, "accuracy_vs_data_volume")

DPI = 300
SAVE_SVG = True   # set False to skip vector output and only write .png

FIGSIZE_R2 = (10, 5)
FIGSIZE_IMPORTANCE = (8, 5)
FIGSIZE_ERROR = (8, 5)
FIGSIZE_DATA_VOLUME = (8, 5)

TITLE_FONTSIZE = 15
LABEL_FONTSIZE = 14
TICK_FONTSIZE = 12
LEGEND_FONTSIZE = 10

MODEL_COLORS = {
    "LinearRegression": "orange",
    "RandomForest": "green",
    "GradientBoosting": "blue",
    "XGBoost": "red",
    "MLPRegressor": "purple",
    "VotingEnsemble": "brown",
    "StackingEnsemble": "black",
}

TARGET_TAG_COMBOS = [
    (t, tag)
    for t in ["DropRate_percent", "Delay_ns", "AvgThroughput_kbps", "Jitter_ns"]
    for tag in ["with_sent", "no_sent"]
]

# The 3 base learners that got RandomizedSearchCV-tuned variants added to
# accuracy_<target>_with_sent.csv (see run_experiment(tuned_params=...) in
# iout_ml_train_and_export.py). Only ever present for tag="with_sent".
TUNED_MODEL_KEYS = ["RandomForest", "GradientBoosting", "XGBoost"]

# Tags to sweep for the error-vs-feature (corner-case) plots. no_sent CSVs
# only exist if you've run generate_no_sent_corner_case_data.py (or
# --mode full --include-no-sent-corner-case) -- missing files are skipped
# gracefully, same as always.
ERROR_TAGS = ["with_sent", "no_sent"]

# (source_csv_pattern_piece, x_column, x_axis_label) for the error-vs-feature plots
ERROR_FEATURE_SPECS = [
    ("concrete_depth", "conc_t", "Concrete Depth (inch)"),
    ("VWC", "VWC", "Volumetric Water Content, VWC"),
    ("b_d", "b_d", "Bulk Density, b_d"),
    ("soil_d", "soil_d", "Soil/Communication Distance (m)"),
]

ERROR_TARGET_SPECS = [
    ("Delay_ns", "Delay (ns)"),
    ("DropRate_percent", "DropRate_percent"),
    ("AvgThroughput_kbps", "AvgThroughput (kbps)"),
    ("Jitter_ns", "Jitter (ns)"),
]
# =======================================================================================


def safe_name(s):
    return (
        str(s).replace(" ", "_").replace("(", "").replace(")", "")
        .replace("/", "_").replace("%", "percent")
    )


def ensure_output_subfolders():
    for folder in [
        SUBFOLDER_R2, SUBFOLDER_R2_TUNED,
        SUBFOLDER_IMPORTANCE, SUBFOLDER_IMPORTANCE_TUNED,
        SUBFOLDER_ERROR_SIGNED, SUBFOLDER_ERROR_ABS,
        SUBFOLDER_DATA_VOLUME,
    ]:
        os.makedirs(folder, exist_ok=True)


def save_current_figure(out_png_path):
    """
    Saves the current matplotlib figure as .png (always) and .svg (if
    SAVE_SVG is True, same base filename/folder), then closes the figure.
    Every category calls this one function so png+svg behavior stays
    consistent everywhere -- change it once here, not in five places.
    """
    plt.savefig(
        out_png_path,
        dpi=DPI,
        bbox_inches="tight"
    )
    if SAVE_SVG:
        out_svg_path = os.path.splitext(out_png_path)[0] + ".svg"
        plt.savefig(
            out_svg_path,
            bbox_inches="tight"
        )
    plt.close()
    print(f"  Saved: {out_png_path}" + ("  (+ .svg)" if SAVE_SVG else ""))


# ---------------------------- CATEGORY 1: R2 comparison (7 baseline models) ----------------------------
def plot_r2_baseline():
    """
    Fig. 2: R2 comparison across the seven baseline models.
    """
    print("\n--- R2 comparison bar charts (7 baseline models) ---")

    model_order = [
        "LinearRegression",
        "RandomForest",
        "GradientBoosting",
        "XGBoost",
        "MLPRegressor",
        "VotingEnsemble",
        "StackingEnsemble",
    ]

    model_short_names = {
        "LinearRegression": "LR",
        "RandomForest": "RF",
        "GradientBoosting": "GB",
        "XGBoost": "XGB",
        "MLPRegressor": "MLP",
        "VotingEnsemble": "VE",
        "StackingEnsemble": "SE",
    }

    target_titles = {
        "DropRate_percent": "Drop Rate",
        "Delay_ns": "Delay",
        "AvgThroughput_kbps": "Avg Throughput",
        "Jitter_ns": "Jitter",
    }

    # Requested sizes relative to the current configuration:
    # y-axis label: 2x; x-axis tick labels: 3x; title: 2x.
    y_label_size = LABEL_FONTSIZE * 2 #28
    x_tick_size = TICK_FONTSIZE * 3  #36
    y_tick_size = 24                #TICK_FONTSIZE
    title_size = TITLE_FONTSIZE * 2   #30

    # Matplotlib's default bar width is 0.8.
    # 66% of 0.8 = 0.528.
    narrower_bar_width = 0.528

    for target, tag in TARGET_TAG_COMBOS:
        csv_path = os.path.join(
            OUTPUT_FOLDER,
            f"accuracy_{target}_{tag}.csv"
        )

        if not os.path.exists(csv_path):
            continue

        df = pd.read_csv(csv_path)
        df = df[df["Model"].isin(model_order)].copy()

        if df.empty:
            continue

        # Force the requested order: LR, RF, GB, XGB, MLP, VE, SE.
        df["Model"] = pd.Categorical(
            df["Model"],
            categories=model_order,
            ordered=True
        )
        df = df.sort_values("Model")

        short_labels = [
            model_short_names[str(model)]
            for model in df["Model"]
        ]

        colors = [
            MODEL_COLORS.get(str(model), "gray")
            for model in df["Model"]
        ]

        plt.figure(figsize=FIGSIZE_R2)

        plt.bar(
            short_labels,
            df["R2"],
            color=colors,
            width=narrower_bar_width
        )

        plt.ylabel(
            "R2 Score",
            fontsize=y_label_size
        )

        # No bottom x-axis caption such as "Model".
        plt.xlabel("")

        # Add internal headroom for the caption.
        plt.ylim(
            min(0, df["R2"].min() - 0.05),
            1.18
        )

        # Put the caption inside the plotting box.
        plt.gca().text(
            0.5,
            0.97,
            target_titles[target],
            transform=plt.gca().transAxes,
            fontsize=title_size,
            ha="center",
            va="top"
        )

        plt.xticks(
            rotation=0,
            fontsize=x_tick_size
        )

        plt.yticks(
            fontsize=y_tick_size
        )

        plt.tight_layout()

        out_png = os.path.join(
            SUBFOLDER_R2,
            f"R2_comparison_{target}_{tag}.png"
        )

        save_current_figure(out_png)


# --------------------- CATEGORY 1b: R2 tuned vs. untuned (RF/GB/XGB, with_sent only) ---------------------
def plot_r2_tuned_vs_untuned():
    """
    Fig. 3: tuned-versus-untuned R2 comparison for RF, GB, and XGB.
    """
    print("\n--- R2 comparison bar charts: tuned vs. untuned ---")

    targets = [
        "DropRate_percent",
        "Delay_ns",
        "AvgThroughput_kbps",
        "Jitter_ns",
    ]

    model_order = [
        "RandomForest",
        "RandomForest_Tuned",
        "GradientBoosting",
        "GradientBoosting_Tuned",
        "XGBoost",
        "XGBoost_Tuned",
    ]

    model_short_names = {
        "RandomForest": "RF",
        "RandomForest_Tuned": "RF Tuned",
        "GradientBoosting": "GB",
        "GradientBoosting_Tuned": "GB Tuned",
        "XGBoost": "XGB",
        "XGBoost_Tuned": "XGB Tuned",
    }

    target_titles = {
        "DropRate_percent": "Tuned vs Untuned for Drop Rate",
        "Delay_ns": "Tuned vs Untuned for Delay",
        "AvgThroughput_kbps": "Tuned vs Untuned for Throughput",
        "Jitter_ns": "Tuned vs Untuned for Jitter",
    }

    y_label_size = LABEL_FONTSIZE * 2  # 28
    x_tick_size = 27  # 75% of Fig. 2's 36
    y_tick_size = 24  # 1.5 × current Fig. 3 size of 16
    title_size = TITLE_FONTSIZE * 2  # 30


    # Default width 0.8 reduced to 66%.
    narrower_bar_width = 0.528

    for target in targets:
        csv_path = os.path.join(
            OUTPUT_FOLDER,
            f"accuracy_{target}_with_sent.csv"
        )

        if not os.path.exists(csv_path):
            continue

        df = pd.read_csv(csv_path)
        df = df[df["Model"].isin(model_order)].copy()

        if df.empty:
            print(
                f"  ({target}: no tuned rows found -- "
                f"run --mode full --use-tuned-params first, skipped)"
            )
            continue

        df["Model"] = pd.Categorical(
            df["Model"],
            categories=model_order,
            ordered=True
        )
        df = df.sort_values("Model")

        short_labels = [
            model_short_names[str(model)]
            for model in df["Model"]
        ]

        colors = [
            MODEL_COLORS.get(
                str(model).replace("_Tuned", ""),
                "gray"
            )
            for model in df["Model"]
        ]

        hatches = [
            "///" if str(model).endswith("_Tuned") else ""
            for model in df["Model"]
        ]

        plt.figure(figsize=FIGSIZE_R2)

        bars = plt.bar(
            short_labels,
            df["R2"],
            color=colors,
            width=narrower_bar_width
        )

        for bar, hatch in zip(bars, hatches):
            bar.set_hatch(hatch)
            bar.set_edgecolor("black")

        plt.ylabel(
            "R2 Score",
            fontsize=y_label_size
        )

        # Remove the lowest caption "Model".
        plt.xlabel("")


        # No internal title for Fig. 3.
        # The LaTeX subcaptions and main figure caption identify the plots.

        # plt.gca().text(
        #     0.5,
        #     0.97,
        #     target_titles[target],
        #     transform=plt.gca().transAxes,
        #     fontsize=title_size,
        #     ha="center",
        #     va="top"
        # )

        plt.ylim(
            min(0, df["R2"].min() - 0.05),
            1.05
        )

        plt.xticks(
            rotation=35,
            ha="right",
            rotation_mode="anchor",
            fontsize=x_tick_size
        )

        plt.yticks(
            fontsize=y_tick_size
        )

        plt.tight_layout()

        out_png = os.path.join(
            SUBFOLDER_R2_TUNED,
            f"R2_tuned_vs_untuned_{target}_with_sent.png"
        )

        save_current_figure(out_png)

# ------------------------ CATEGORY 2: Feature importance (RF) ------------------------
def plot_feature_importances():
    print("\n--- Feature importance bar charts (RandomForest) ---")
    for target, tag in TARGET_TAG_COMBOS:
        csv_path = os.path.join(OUTPUT_FOLDER, f"feature_importance_{target}_{tag}.csv")
        if not os.path.exists(csv_path):
            continue
        df = pd.read_csv(csv_path)

        plt.figure(figsize=FIGSIZE_IMPORTANCE)
        plt.bar(df["Feature"], df["Importance"])
        plt.xticks(rotation=30, ha="right", fontsize=TICK_FONTSIZE)
        plt.yticks(fontsize=TICK_FONTSIZE)
        plt.ylabel("Feature Importance", fontsize=LABEL_FONTSIZE)
        plt.title(f"Random Forest Feature Importance for {target} ({tag})", fontsize=TITLE_FONTSIZE)
        plt.tight_layout()

        out_png = os.path.join(SUBFOLDER_IMPORTANCE, f"feature_importance_{target}_{tag}.png")
        save_current_figure(out_png)


# ------------------ CATEGORY 2b: Feature importance (tuned RF/GB/XGB) ------------------
def plot_feature_importances_tuned():
    """
    Fig. 4: feature importance for tuned models.
    """
    print(
        "\n--- Feature importance bar charts "
        "(tuned RandomForest/GradientBoosting/XGBoost) ---"
    )

    feature_short_names = {
        "conc_t": "conc_t",
        "b_d": "b_d",
        "soil_d": "soil_d",
        "VWC": "VWC",

        "Pkt_Size (Byte)": "p_size",
        "Pkt_Size_Byte": "p_size",
        "Pkt_Size": "p_size",
        "p_size": "p_size",

        "Pkt_Intvl (ms)": "p_itvl",
        "Pkt_Intvl_ms": "p_itvl",
        "Pkt_Intvl": "p_itvl",
        "p_itvl": "p_itvl",

        "cluster_size": "clstr_size",
        "ClusterSize": "clstr_size",
        "clstr_size": "clstr_size",

        "#SentPackets": "#sent_pkts",
        "SentPackets": "#sent_pkts",
        "sent_packets": "#sent_pkts",
        "#sent_pkts": "#sent_pkts",
    }

    target_short_names = {
        "DropRate_percent": "Drop Rate",
        "Delay_ns": "Delay",
        "AvgThroughput_kbps": "Throughput",
        "Jitter_ns": "Jitter",
    }

    model_short_names = {
        "RandomForest": "RF_Tuned",
        "GradientBoosting": "GB_Tuned",
        "XGBoost": "XGB_Tuned",
    }

    # Same title-font size used for the revised Figs. 2 and 3:
    # TITLE_FONTSIZE is 15, so this gives 30 pt.
    inside_title_size = TITLE_FONTSIZE * 2

    # Matplotlib's default bar width is 0.8.
    # 80% of the current width: 0.8 × 0.8 = 0.64.
    narrower_bar_width = 0.64

    found_any = False

    for target, tag in TARGET_TAG_COMBOS:
        for base_name in TUNED_MODEL_KEYS:
            csv_path = os.path.join(
                OUTPUT_FOLDER,
                f"feature_importance_{base_name}_Tuned_"
                f"{target}_{tag}.csv"
            )

            if not os.path.exists(csv_path):
                continue

            found_any = True
            df = pd.read_csv(csv_path)

            # Shorten the displayed feature names without changing bar order.
            x_labels = [
                feature_short_names.get(str(feature), str(feature))
                for feature in df["Feature"]
            ]

            fig, ax = plt.subplots(figsize=FIGSIZE_IMPORTANCE)

            ax.bar(
                x_labels,
                df["Importance"],
                color=MODEL_COLORS.get(base_name, "gray"),
                width=narrower_bar_width
            )

            # Keep the existing x-axis typography and rotation.
            ax.tick_params(
                axis="x",
                labelsize=16
            )

            plt.setp(
                ax.get_xticklabels(),
                rotation=30,
                ha="right"
            )

            # Keep the existing y-axis tick size.
            ax.tick_params(
                axis="y",
                labelsize=16
            )

            # Keep the existing y-axis label size.
            ax.set_ylabel(
                "Feature Importance",
                fontsize=18
            )

            # No conventional title above the plotting box.
            # Place the short caption inside the upper-right corner.
            inside_caption = (
                f"{model_short_names[base_name]}, "
                f"{target_short_names[target]}"
            )

            ax.text(
                0.97,
                0.95,
                inside_caption,
                transform=ax.transAxes,
                fontsize=inside_title_size,
                horizontalalignment="right",
                verticalalignment="top"
            )

            fig.tight_layout()

            out_png = os.path.join(
                SUBFOLDER_IMPORTANCE_TUNED,
                f"feature_importance_{base_name}_Tuned_"
                f"{target}_{tag}.png"
            )

            save_current_figure(out_png)

    if not found_any:
        print(
            "  (No feature_importance_*_Tuned_*.csv files found -- "
            "run generate_tuned_feature_importance.py first, skipped)"
        )


# ------------------ CATEGORY 3 & 4: prediction error vs feature ------------------
def _error_csv_path(feature_piece, target, tag="with_sent"):
    if feature_piece == "concrete_depth":
        base = f"prediction_error_vs_concrete_depth_{target}_{tag}"
    else:
        base = f"prediction_error_vs_{feature_piece}_{target}_{tag}"
    # tag="with_sent" is generated natively by every --mode full run;
    # tag="no_sent" requires generate_no_sent_corner_case_data.py (or
    # --mode full --include-no-sent-corner-case) to have been run first.
    return os.path.join(OUTPUT_FOLDER, f"{base}.csv"), base


def plot_error_vs_feature(abs_error=False):
    label = "Abs error" if abs_error else "Signed error"
    out_subfolder = (
        SUBFOLDER_ERROR_ABS
        if abs_error
        else SUBFOLDER_ERROR_SIGNED
    )

    print(
        f"\n--- Prediction error vs feature plots "
        f"({label}) -> {out_subfolder} ---"
    )

    model_short_names = {
        "LinearRegression": "LR",
        "RandomForest": "RF",
        "GradientBoosting": "GB",
        "XGBoost": "XGB",
        "MLPRegressor": "MLP",
        "VotingEnsemble": "VE",
        "StackingEnsemble": "SE",
    }

    # Requested typography, matching Fig. 4.
    x_label_size = 16
    x_tick_size = 16
    y_tick_size = 16
    y_label_size = 18

    # Current legend size is 10; requested size is 1.5 times larger.
    legend_size = LEGEND_FONTSIZE * 1.5

    missing = []

    for feature_piece, x_col, x_label in ERROR_FEATURE_SPECS:

        # Shorter/customized x-axis labels.
        displayed_x_label = x_label

        if feature_piece == "b_d":
            displayed_x_label = "Bulk Density, b_d (gm/cc)"

        for target, target_display in ERROR_TARGET_SPECS:
            for tag in ERROR_TAGS:
                csv_path, base_name = _error_csv_path(
                    feature_piece,
                    target,
                    tag
                )

                if not os.path.exists(csv_path):
                    missing.append(os.path.basename(csv_path))
                    continue

                df = pd.read_csv(csv_path)

                if "PredictionError" not in df.columns:
                    continue

                y_col = (
                    "Abs_Error"
                    if abs_error
                    else "PredictionError"
                )

                if abs_error:
                    df["Abs_Error"] = (
                        df["PredictionError"].abs()
                    )

                fig, ax = plt.subplots(
                    figsize=FIGSIZE_ERROR
                )

                for model_name in df["Model"].unique():
                    model_df = df[
                        df["Model"] == model_name
                    ]

                    grouped = (
                        model_df
                        .groupby(x_col, as_index=False)[y_col]
                        .mean()
                        .sort_values(x_col)
                    )

                    ax.plot(
                        grouped[x_col],
                        grouped[y_col],
                        marker="o",
                        linewidth=2,
                        color=MODEL_COLORS.get(
                            model_name,
                            "gray"
                        ),
                        label=model_short_names.get(
                            model_name,
                            model_name
                        )
                    )

                if not abs_error:
                    ax.axhline(
                        y=0,
                        linestyle="--",
                        linewidth=1
                    )

                # X-axis label: 16 pt.
                ax.set_xlabel(
                    displayed_x_label,
                    fontsize=x_label_size
                )

                # Shortened y-axis label: 18 pt.
                ax.set_ylabel(
                    "Mean Abs. Error, Prediction",
                    fontsize=y_label_size
                )

                # Remove the top caption completely.
                # Do not call ax.set_title() or plt.title().

                # Both axes' tick labels: 16 pt.
                ax.tick_params(
                    axis="x",
                    labelsize=x_tick_size
                )

                ax.tick_params(
                    axis="y",
                    labelsize=y_tick_size
                )

                # Abbreviated model names and 1.5× legend font.
                ax.legend(
                    fontsize=legend_size,
                    ncol=2,
                    loc="best"
                )

                fig.tight_layout()

                suffix = (
                    "_AbsError"
                    if abs_error
                    else ""
                )

                out_png = os.path.join(
                    out_subfolder,
                    f"{base_name}{suffix}.png"
                )

                save_current_figure(out_png)

    if missing:
        print(
            f"  ({len(missing)} source CSV(s) not found, skipped -- "
            f"run iout_ml_train_and_export.py --mode full first, or "
            f"compute_throughput_jitter_errors.py if backfilling "
            f"an older run)"
        )

# ------------------------- CATEGORY 5: accuracy vs data volume -------------------------
def plot_accuracy_vs_data_volume():
    print("\n--- Accuracy vs data volume (cumulative training) ---")
    # Prefer the "COMPLETED" version (graft_full_run_into_progress.py output)
    # if it exists, since it has the full 6-segment point; fall back otherwise.
    candidates = [
        os.path.join(OUTPUT_FOLDER, "segment_progress_summary_COMPLETED.csv"),
        os.path.join(OUTPUT_FOLDER, "segment_progress_summary.csv"),
    ]
    csv_path = next((c for c in candidates if os.path.exists(c)), None)
    if csv_path is None:
        print("  No segment_progress_summary*.csv found -- skipping (run --mode cumulative first).")
        return

    progress_df = pd.read_csv(csv_path)
    print(f"  Using: {csv_path}")

    for (target, tag), group in progress_df.groupby(["Target", "FeatureSet"]):
        plt.figure(figsize=FIGSIZE_DATA_VOLUME)
        for model_name, mgroup in group.groupby("Model"):
            mgroup = mgroup.sort_values("CumulativeRows")
            plt.plot(
                mgroup["CumulativeRows"], mgroup["R2"], marker="o", linewidth=2,
                color=MODEL_COLORS.get(model_name, "gray"), label=model_name
            )
        plt.xlabel("Cumulative training rows", fontsize=LABEL_FONTSIZE)
        plt.ylabel("R2", fontsize=LABEL_FONTSIZE)
        plt.title(f"R2 vs cumulative data volume - {target} ({tag})", fontsize=TITLE_FONTSIZE)
        plt.legend(fontsize=LEGEND_FONTSIZE)
        plt.tight_layout()

        out_png = os.path.join(
            SUBFOLDER_DATA_VOLUME, f"accuracy_vs_data_volume_{safe_name(target)}_{tag}_R2.png"
        )
        save_current_figure(out_png)


CATEGORY_FUNCS = {
    "r2": plot_r2_baseline,
    "r2_tuned": plot_r2_tuned_vs_untuned,
    "importance": plot_feature_importances,
    "importance_tuned": plot_feature_importances_tuned,
    "error": lambda: plot_error_vs_feature(abs_error=False),
    "error_abs": lambda: plot_error_vs_feature(abs_error=True),
    "data_volume": plot_accuracy_vs_data_volume,
}


def main():
    parser = argparse.ArgumentParser(description="Plot-only script, reads CSVs from iout_ml_train_and_export.py")
    parser.add_argument(
        "--only", default="all",
        help=f"Comma-separated categories to regenerate: {','.join(CATEGORY_FUNCS)}. Default: all"
    )
    args = parser.parse_args()

    ensure_output_subfolders()

    if args.only != "all":
        selected = [c.strip() for c in args.only.split(",")]
        unknown = [c for c in selected if c not in CATEGORY_FUNCS]
        if unknown:
            print(f"Unknown categories: {unknown}. Valid: {list(CATEGORY_FUNCS)}")
            return
        for cat in selected:
            CATEGORY_FUNCS[cat]()
        print("\nDone.")
        return

    # ---- Default (no --only given): run every category, in this fixed order ----
    plot_r2_baseline()
    plot_r2_tuned_vs_untuned()
    plot_feature_importances()
    plot_feature_importances_tuned()
    plot_error_vs_feature(abs_error=False)
    plot_error_vs_feature(abs_error=True)

    # ==========================================================================
    # ACCURACY vs. CUMULATIVE DATA VOLUME -- only meaningful after you've run
    # `python iout_ml_train_and_export.py --mode cumulative` (and optionally
    # graft_full_run_into_progress.py). To skip this section entirely -- e.g.
    # you haven't run cumulative mode yet, or just don't want it regenerated
    # right now -- comment out the single line below and nothing else changes.
    # ==========================================================================
    plot_accuracy_vs_data_volume()

    print("\nDone.")


if __name__ == "__main__":
    main()
