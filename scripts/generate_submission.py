import argparse
from pathlib import Path
import re
import string


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / 'submission.cpp'
CODEFORCES_CHARACTER_LIMIT = 65_535
V7_STAGES = {
    'tracking': 0,
    'locality': 1,
    'up': 2,
    'down': 3,
    'combined': 4,
}

PARTS = (
    'include/Protocol.hpp',
    'include/SystemConfig.hpp',
    'include/TaskTimeTable.hpp',
    'include/LinkState.hpp',
    'include/WorldState.hpp',
    'include/RemotePlacement.hpp',
    'include/DecodeBatchScheduler.hpp',
    'include/ScoreAwareScheduler.hpp',
    'include/AdaptiveScheduler.hpp',
    'include/MultiprocessorScheduler.hpp',
    'include/SharedLinkScheduler.hpp',
    'src/SchedulerCore.hpp',
    'src/Protocol.cpp',
    'src/SystemConfig.cpp',
    'src/TaskTimeTable.cpp',
    'src/WorldState.cpp',
    'src/DecodeBatchScheduler.cpp',
    'src/ScoreAwareScheduler.cpp',
    'src/AdaptiveScheduler.cpp',
    'src/MultiprocessorScheduler.cpp',
    'src/SharedLinkScheduler.cpp',
    'src/main.cpp',
)

LOCAL_HEADERS = {Path(part).name for part in PARTS if part.endswith('.hpp')}
INCLUDE_PATTERN = re.compile(r'^\s*#include\s*[<\x22]([^>\x22]+)[>\x22]\s*$')
IDENTIFIER_PATTERN = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
COMPACT_IDENTIFIERS = (
    'RequestState', 'WorldState', 'num_layers', 'Assignment', 'assignment', 'TransferDirection',
    'LinkTransferSpec', 'SharedLinkSchedulerConfig', 'SystemConfig', 'RequestClass', 'remote_selector',
    'MultiprocessorSchedulerConfig', 'request_class', 'ScoreAwareSchedulerConfig', 'target_hot_set_size',
    'getRequest', 'DecodeBatchPolicy', 'AdaptiveScoreRegime', 'AdaptiveSchedulerConfig', 'direction',
    'batch_size', 'requests', 'TimingCurves', 'ReadyDecodePre', 'transfer', 'last_token_time', 'transfers',
    'scheduler_detail', 'classifyRequest', 'assignment_cost', 'interpolate', 'current_time', 'PrefillPreTask',
    'selectDecodeBatchSize', 'PrefillRemoteSelector', 'assertRequestRemote', 'PrefillProcTask', 'DecodePreTask',
    'next_prefill_layer', 'preferPrefillProcBeforeDecodeProc', 'preferPrefillPostBeforeDecodePre',
    'chooseScoreAwareAssignments', 'chooseAssignments', 'TransferDoneEvent', 'ProjectedTransfer',
    'preferPrefillPreBeforeDecodePre', 'waiting_policy', 'preferred_batch_size', 'DecodeSelector',
    'TransferStage', 'PrefillPostTask', 'protected_decode', 'enqueue_at', 'chooseBatchedAssignments',
    'TaskTimeTable', 'DecodeProcTask', 'DecodePostTask', 'input_length', 'total_batch_size', 'deadlines',
    'assertEdgeServer', 'TimingCurve', 'ServerType', 'assignments', 'ReadyDecodeProc',
    'chooseAdaptiveAssignments', 'triggeredTransfers', 'pending_triggers', 'LinkDirectionState',
    'estimateTaskDuration', 'ServerId', 'decode_work_per_request', 'baseline_decode_batches',
    'SharedLinkState', 'localizeDecodePreAssignment', 'arrival_time', 'WaitingPrefillProcDone',
    'WaitingPrefillPreDone', 'decode_proc', 'TaskTimeRow', 'cloud_batch_size', 'ReadyPrefillProc',
    'with_candidate', 'preferred_cloud_batch_size', 'preferred_capacity', 'next_trigger_order',
    'decode_batches', 'buildMultiprocessorSchedulerConfig', 'baseline', 'estimateDecodePreLinkCost',
    'committed_tail', 'PendingLinkTrigger', 'CommittedLinkTransfer', 'FutureTransfer', 'RequestPriority',
    'LinkOverloaded', 'SharedLinkFeatures', 'recordLinkAssignment', 'observeLinkFrame',
    'chooseSharedLinkAssignments', 'buildSharedLinkSchedulerConfig', 'deriveTriggeredTransfers',
    'pendingProjection', 'simulateProjection', 'maximumBlockedHotUrgency', 'protectedDecodePreAssignment',
    'blockedUpUrgency', 'blockedDownUrgency', 'shouldDeferPrefill', 'prefillTransferForAssignment',
    'candidate_index', 'candidate_transfer', 'decode_transfers', 'candidate_enqueue', 'decode_duration',
    'without_candidate', 'hot_urgency', 'prefill_urgency', 'candidate_rid', 'original_rid',
    'original_priority', 'selected_rid', 'represented', 'compatibleReplacement', 'higherPriority',
    'Request', 'ServerState', 'TaskSpec', 'Event', 'Frame', 'ArrivalEvent', 'TaskDoneEvent', 'FinishEvent',
    'ReadyPrefillPre', 'WaitingPrefillUpload', 'WaitingPrefillDownload', 'ReadyPrefillPost',
    'WaitingPrefillPostDone', 'WaitingDecodePreDone', 'WaitingDecodeUpload', 'WaitingDecodeProcDone',
    'WaitingDecodeDownload', 'ReadyDecodePost', 'WaitingDecodePostDone', 'Finished', 'Prefill', 'Cold', 'Hot',
    'prefill_pre', 'prefill_proc', 'prefill_post', 'decode_pre', 'decode_post', 'input_task_spec',
    'readSystemConfig', 'readTaskTimeTable', 'buildTimingCurves', 'readFrame', 'applyFrame',
    'startAssignment', 'writeAssignments', 'submissionFeatures', 'bytes_per_token', 'latency_in_ms',
    'bandwidth_gbps', 'layer_begin', 'layer_end', 'cloud_index', 'tokens_produced',
    'observed_tdr_sum', 'observed_tdr_count', 'observed_tpot_sum', 'observed_tpot_count',
    'maximum_reconciliation_error', 'reconciliation_count', 'expected_completion', 'within_trigger_order',
    'maximum_link_reconciliation_error', 'link_reconciliation_count',
)
CPP_KEYWORDS = {
    'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case',
    'catch', 'char', 'char8_t', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const', 'consteval',
    'constexpr', 'constinit', 'const_cast', 'continue', 'co_await', 'co_return', 'co_yield', 'decltype',
    'default', 'delete', 'do', 'double', 'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern',
    'false', 'float', 'for', 'friend', 'goto', 'if', 'inline', 'int', 'long', 'mutable', 'namespace', 'new',
    'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected', 'public',
    'register', 'reinterpret_cast', 'requires', 'return', 'short', 'signed', 'sizeof', 'static',
    'static_assert', 'static_cast', 'struct', 'switch', 'template', 'this', 'thread_local', 'throw', 'true',
    'try', 'typedef', 'typeid', 'typename', 'union', 'unsigned', 'using', 'virtual', 'void', 'volatile',
    'wchar_t', 'while', 'xor', 'xor_eq',
}


def prepare_part(relative_path: str) -> str:
    source = ROOT / relative_path
    kept_lines: list[str] = []

    for line in source.read_text(encoding='utf-8').splitlines():
        if line.strip() == '#pragma once':
            continue

        include = INCLUDE_PATTERN.match(line)
        if include and Path(include.group(1)).name in LOCAL_HEADERS:
            continue

        kept_lines.append(line.rstrip())

    body = '\n'.join(kept_lines).strip()
    return f'// ===== BEGIN {relative_path} =====\n{body}\n// ===== END {relative_path} ====='


def remove_comments(source: str) -> str:
    result: list[str] = []
    index = 0
    state = 'code'
    while index < len(source):
        current = source[index]
        following = source[index + 1] if index + 1 < len(source) else ''
        if state == 'code':
            if current == '/' and following == '/':
                state = 'line_comment'
                index += 2
                continue
            if current == '/' and following == '*':
                state = 'block_comment'
                index += 2
                continue
            if current == '"':
                state = 'string'
            elif current == "'":
                state = 'character'
            result.append(current)
            index += 1
            continue
        if state == 'line_comment':
            if current == '\n':
                result.append(current)
                state = 'code'
            index += 1
            continue
        if state == 'block_comment':
            if current == '*' and following == '/':
                state = 'code'
                index += 2
            else:
                if current == '\n':
                    result.append(current)
                index += 1
            continue
        result.append(current)
        if current == '\\' and following:
            result.append(following)
            index += 2
            continue
        if (state == 'string' and current == '"') or (state == 'character' and current == "'"):
            state = 'code'
        index += 1
    if state not in {'code', 'line_comment'}:
        raise ValueError(f'Unterminated C++ lexical state: {state}')
    return ''.join(result)


def compact_aliases(source: str) -> dict[str, str]:
    if len(COMPACT_IDENTIFIERS) != len(set(COMPACT_IDENTIFIERS)):
        raise ValueError('COMPACT_IDENTIFIERS contains duplicates')
    existing = set(IDENTIFIER_PATTERN.findall(source))
    candidates = (
        first + second
        for first in string.ascii_letters
        for second in string.ascii_letters
    )
    aliases: dict[str, str] = {}
    for identifier in COMPACT_IDENTIFIERS:
        if identifier not in existing:
            continue
        alias = next(
            candidate for candidate in candidates
            if candidate not in existing and candidate not in CPP_KEYWORDS
        )
        aliases[identifier] = alias
        existing.add(alias)
    return aliases


def replace_identifiers(source: str, aliases: dict[str, str]) -> str:
    result: list[str] = []
    index = 0
    state = 'code'
    while index < len(source):
        current = source[index]
        if state == 'code' and (current.isalpha() or current == '_'):
            match = IDENTIFIER_PATTERN.match(source, index)
            assert match is not None
            identifier = match.group(0)
            result.append(aliases.get(identifier, identifier))
            index = match.end()
            continue
        result.append(current)
        if current == '\\' and state in {'string', 'character'} and index + 1 < len(source):
            result.append(source[index + 1])
            index += 2
            continue
        if state == 'code' and current == '"':
            state = 'string'
        elif state == 'code' and current == "'":
            state = 'character'
        elif state == 'string' and current == '"':
            state = 'code'
        elif state == 'character' and current == "'":
            state = 'code'
        index += 1
    return ''.join(result)


def compact_cpp(source: str) -> str:
    without_comments = remove_comments(source)
    shortened = replace_identifiers(without_comments, compact_aliases(without_comments))
    lines = (line.strip() for line in shortened.splitlines())
    return '\n'.join(line for line in lines if line) + '\n'


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Generate a single-file Codeforces scheduler submission.')
    parser.add_argument('--v7-stage', choices=V7_STAGES, default='combined')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--readable', action='store_true', help='Disable competition-size compaction.')
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    sections = [
        '// Generated by scripts/generate_submission.py. Do not edit this file directly.',
        '// Regenerate it after changing project sources.',
        f'#define SCHEDULER_V7_STAGE {V7_STAGES[args.v7_stage]}',
        *(prepare_part(part) for part in PARTS),
    ]

    generated = '\n\n'.join(sections) + '\n'
    if not args.readable:
        generated = compact_cpp(generated)
        if len(generated) > CODEFORCES_CHARACTER_LIMIT:
            raise ValueError(
                f'Compact submission is {len(generated)} characters; '
                f'Codeforces allows {CODEFORCES_CHARACTER_LIMIT}.'
            )
    with output.open('w', encoding='utf-8', newline='\n') as submission:
        submission.write(generated)

    print(f'Generated {output} for V7 stage {args.v7_stage}: {len(generated)} characters')


if __name__ == '__main__':
    main()
