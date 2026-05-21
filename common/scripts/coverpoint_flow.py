#!/usr/bin/env python3
"""Offline CoverPoint flow for arm-priv-test.

This script intentionally uses only Python stdlib so it can run in the
current arm-priv-test Docker image. It parses the NORM/*.yaml subset used by
this project, validates testcase links, and generates EDA-facing skeletons and
human-readable traceability reports.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


@dataclass
class TestCase:
    testcase_id: str
    file_path: str = ""
    function: str = ""


@dataclass
class CoverPoint:
    name: str
    bins: list[str] = field(default_factory=list)


@dataclass
class Rule:
    rule_id: str
    name: str = ""
    coverpoints: list[CoverPoint] = field(default_factory=list)
    testcases: list[TestCase] = field(default_factory=list)


@dataclass
class NormModel:
    source_path: Path
    module_name: str
    rules: list[Rule]


def sanitize_identifier(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if sanitized and sanitized[0].isdigit():
        sanitized = "_" + sanitized
    return sanitized.upper()


def get_scalar_value(line: str) -> str:
    return line.split(":", 1)[1].strip().strip('"').strip("'")


def parse_norm_file(path: Path) -> NormModel:
    module_name = path.stem.lower()
    rules: list[Rule] = []
    current_rule: Rule | None = None
    current_coverpoint: CoverPoint | None = None
    current_testcase: TestCase | None = None
    section = ""
    in_metadata = False

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        indent = len(line) - len(line.lstrip(" "))

        if stripped == "metadata:":
            in_metadata = True
            continue
        if indent == 0 and stripped.endswith(":") and stripped != "metadata:":
            in_metadata = False

        if in_metadata and stripped.startswith("module:"):
            module_name = get_scalar_value(stripped)
            continue

        if stripped == "coverpoints:":
            section = "coverpoints"
            current_coverpoint = None
            current_testcase = None
            continue
        if stripped == "testcases:":
            section = "testcases"
            current_coverpoint = None
            current_testcase = None
            continue
        if stripped == "bins:":
            section = "bins"
            current_testcase = None
            continue
        if stripped.endswith(":") and stripped not in {"coverpoints:", "testcases:", "bins:"}:
            if indent <= 4:
                section = ""

        rule_match = re.match(r"^\s*- id:\s*([A-Za-z0-9_.:-]+)\s*$", line)
        if rule_match and "RULE" in rule_match.group(1):
            current_rule = Rule(rule_id=rule_match.group(1))
            rules.append(current_rule)
            current_coverpoint = None
            current_testcase = None
            section = "rule"
            continue

        if current_rule is None:
            continue

        if indent == 4 and stripped.startswith("name:"):
            current_rule.name = get_scalar_value(stripped)
            continue

        if indent == 6 and stripped.startswith("- name:"):
            current_coverpoint = CoverPoint(name=get_scalar_value(stripped))
            current_rule.coverpoints.append(current_coverpoint)
            section = "coverpoints"
            continue

        if section == "bins" and indent == 10 and stripped.startswith("- name:"):
            if current_coverpoint is not None:
                current_coverpoint.bins.append(get_scalar_value(stripped))
            continue

        testcase_match = re.match(r"^\s*- id:\s*([A-Za-z0-9_.:-]+)\s*$", line)
        if section == "testcases" and testcase_match:
            current_testcase = TestCase(testcase_id=testcase_match.group(1))
            current_rule.testcases.append(current_testcase)
            continue

        if section == "testcases" and current_testcase is not None:
            if stripped.startswith("file:"):
                current_testcase.file_path = get_scalar_value(stripped)
            elif stripped.startswith("function:"):
                current_testcase.function = get_scalar_value(stripped)

    return NormModel(source_path=path, module_name=module_name, rules=rules)


def load_models(norm_dir: Path) -> list[NormModel]:
    return [parse_norm_file(path) for path in sorted(norm_dir.glob("*.yaml")) if not path.name.endswith("TEMPLATE.yaml")]


def validate_models(repo_root: Path, models: Iterable[NormModel]) -> list[str]:
    errors: list[str] = []
    seen_rule_ids: set[str] = set()

    for model in models:
        if not model.rules:
            errors.append(f"{model.source_path}: no normative rules found")
        for rule in model.rules:
            if rule.rule_id in seen_rule_ids:
                errors.append(f"duplicate rule id: {rule.rule_id}")
            seen_rule_ids.add(rule.rule_id)
            if not rule.name:
                errors.append(f"{model.source_path}: {rule.rule_id} missing rule name")
            if not rule.coverpoints:
                errors.append(f"{model.source_path}: {rule.rule_id} missing coverpoints")
            if not rule.testcases:
                errors.append(f"{model.source_path}: {rule.rule_id} missing testcases")
            for coverpoint in rule.coverpoints:
                if not coverpoint.bins:
                    errors.append(f"{model.source_path}: {rule.rule_id}/{coverpoint.name} missing bins")
            for testcase in rule.testcases:
                if not testcase.file_path or not testcase.function:
                    errors.append(f"{model.source_path}: {rule.rule_id}/{testcase.testcase_id} incomplete testcase link")
                    continue
                source_file = repo_root / testcase.file_path
                if not source_file.exists():
                    errors.append(f"{model.source_path}: testcase file not found: {testcase.file_path}")
                    continue
                source_text = source_file.read_text(encoding="utf-8", errors="ignore")
                if testcase.function not in source_text:
                    errors.append(f"{model.source_path}: function {testcase.function} not found in {testcase.file_path}")
    return errors


def generate_svh(model: NormModel, output_dir: Path) -> Path:
    output_path = output_dir / f"{model.module_name.upper()}_coverage.svh"
    guard = f"_{sanitize_identifier(model.module_name)}_COVERAGE_SVH_"

    rule_ids = {rule.rule_id: index for index, rule in enumerate(model.rules, 1)}
    coverpoint_names = []
    bin_names = []
    for rule in model.rules:
        for coverpoint in rule.coverpoints:
            coverpoint_names.append(coverpoint.name)
            for bin_name in coverpoint.bins:
                bin_names.append(f"{coverpoint.name}__{bin_name}")
    coverpoint_ids = {name: index for index, name in enumerate(coverpoint_names, 1)}
    bin_ids = {name: index for index, name in enumerate(bin_names, 1)}

    lines = [
        f"// Generated from {model.source_path.as_posix()}",
        "// This is an EDA-facing functional coverage skeleton.",
        "// Replace numeric sample IDs with DUT/trace adapter fields when integrating RTL.",
        f"`ifndef {guard}",
        f"`define {guard}",
        "",
        "// Rule ID mapping",
    ]
    for rule_name, rule_id in rule_ids.items():
        lines.append(f"localparam int unsigned {sanitize_identifier(rule_name)} = {rule_id}; // {rule_name}")
    lines.append("")
    lines.append("// CoverPoint ID mapping")
    for coverpoint_name, coverpoint_id in coverpoint_ids.items():
        lines.append(f"localparam int unsigned {sanitize_identifier(coverpoint_name)} = {coverpoint_id}; // {coverpoint_name}")
    lines.append("")
    lines.append("// Bin ID mapping")
    for bin_name, bin_id in bin_ids.items():
        lines.append(f"localparam int unsigned {sanitize_identifier(bin_name)} = {bin_id}; // {bin_name}")

    lines.extend([
        "",
        f"covergroup {model.module_name.upper()}_cg with function sample(int unsigned rule_id, int unsigned coverpoint_id, int unsigned bin_id);",
        "  option.per_instance = 1;",
        "",
        "  cp_rule: coverpoint rule_id {",
    ])
    for rule_name in rule_ids:
        lines.append(f"    bins {sanitize_identifier(rule_name)}_BIN = {{{sanitize_identifier(rule_name)}}};")
    lines.extend(["  }", "", "  cp_coverpoint: coverpoint coverpoint_id {"])
    for coverpoint_name in coverpoint_ids:
        lines.append(f"    bins {sanitize_identifier(coverpoint_name)}_BIN = {{{sanitize_identifier(coverpoint_name)}}};")
    lines.extend(["  }", "", "  cp_bin: coverpoint bin_id {"])
    for bin_name in bin_ids:
        lines.append(f"    bins {sanitize_identifier(bin_name)}_BIN = {{{sanitize_identifier(bin_name)}}};")
    lines.extend(["  }", "", "  cross_rule_coverpoint: cross cp_rule, cp_coverpoint;", "  cross_coverpoint_bin: cross cp_coverpoint, cp_bin;", "endgroup", "", f"`endif // {guard}", ""])
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path


def generate_reports(models: list[NormModel], report_dir: Path, svh_paths: list[Path]) -> None:
    summary = []
    markdown = ["# CoverPoint Traceability Report", "", "Generated by `common/scripts/coverpoint_flow.py`.", ""]
    for model, svh_path in zip(models, svh_paths):
        rule_count = len(model.rules)
        coverpoint_count = sum(len(rule.coverpoints) for rule in model.rules)
        bin_count = sum(len(coverpoint.bins) for rule in model.rules for coverpoint in rule.coverpoints)
        testcase_count = sum(len(rule.testcases) for rule in model.rules)
        summary.append({
            "module": model.module_name,
            "source": model.source_path.as_posix(),
            "generated_svh": svh_path.as_posix(),
            "rules": rule_count,
            "coverpoints": coverpoint_count,
            "bins": bin_count,
            "testcases": testcase_count,
        })
        markdown.extend([
            f"## {model.module_name}",
            "",
            f"- Source: `{model.source_path.as_posix()}`",
            f"- Generated SVH: `{svh_path.as_posix()}`",
            f"- Rules: **{rule_count}**",
            f"- CoverPoints: **{coverpoint_count}**",
            f"- Bins: **{bin_count}**",
            f"- TestCases: **{testcase_count}**",
            "",
            "| Rule | CoverPoint | Bins | TestCases |",
            "|---|---|---:|---|",
        ])
        for rule in model.rules:
            testcase_ids = ", ".join(testcase.testcase_id for testcase in rule.testcases)
            for coverpoint in rule.coverpoints:
                markdown.append(f"| `{rule.rule_id}` | `{coverpoint.name}` | {len(coverpoint.bins)} | {testcase_ids} |")
        markdown.append("")

    (report_dir / "coverpoint_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    (report_dir / "coverpoint_traceability.md").write_text("\n".join(markdown), encoding="utf-8")


def run_flow(args: argparse.Namespace) -> int:
    repo_root = Path(args.repo_root).resolve()
    norm_dir = repo_root / args.norm_dir
    output_dir = repo_root / args.output_dir
    report_dir = repo_root / args.report_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    report_dir.mkdir(parents=True, exist_ok=True)

    models = load_models(norm_dir)
    errors = validate_models(repo_root, models)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    if args.command == "check":
        print(f"[coverpoint] CHECK PASS: modules={len(models)}")
        return 0

    svh_paths = [generate_svh(model, output_dir) for model in models]
    generate_reports(models, report_dir, svh_paths)
    print(f"[coverpoint] GENERATE PASS: modules={len(models)} output={output_dir} report={report_dir}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Offline CoverPoint flow")
    parser.add_argument("command", choices=["check", "generate", "flow"])
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--norm-dir", default="NORM")
    parser.add_argument("--output-dir", default="COVERPOINT/generated")
    parser.add_argument("--report-dir", default="reports/coverpoint")
    return run_flow(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
