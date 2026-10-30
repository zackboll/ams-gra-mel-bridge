"""CI-001R3 structural workflow contracts, not deployment or source approval.

This is intentionally a reader for the reviewed workflow's block structure, not
a general YAML loader. Unsupported or ambiguous security-relevant forms reject.
"""

import ast
from pathlib import Path
import re
import shlex

# Independently transcribed from merged CI-ACTIONS-001's reviewed authority.
# Exact owner, commit and release comment are required, not any 40-hex token.
ACTION_PINS = {
    'actions/checkout': 'fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09 # v5.1.0',
    'actions/upload-artifact': 'b7c566a772e6b6bfb58ed0dc250532a479d7789f # v6.0.0',
    'actions/download-artifact': '37930b1c2abaa49bbe596cd826c3c89aef350131 # v7.0.0',
    'actions/upload-pages-artifact': 'fc324d3547104276b827a68afc52ff2a11cc49c9 # v5.0.0',
    'actions/deploy-pages': 'cd2ce8fcbc39b97be8ca5fce6e763baed58fa128 # v5.0.0',
}


def require(condition, message):
    if not condition:
        raise AssertionError('CI-001R3: ' + message)


def field(block, name, indent):
    matches = re.findall(r'^' + ' ' * indent + re.escape(name) + r': (.+)$', block, re.M)
    require(len(matches) == 1, 'missing/duplicate field ' + name)
    return matches[0]


def jobs(root):
    text = (Path(root) / '.github/workflows/documentation.yml').read_text()
    references = re.findall(r'^\s*(?:- )?uses: (.+)$', text, re.M)
    require(references and all(ref in {name + '@' + pin for name, pin in ACTION_PINS.items()}
                               for ref in references), 'unreviewed action owner/pin/comment')
    require(text.count('\njobs:\n') == 1, 'ambiguous job map')
    header, body = text.split('\njobs:\n')
    require('secrets.' not in text and 'pull_request_target' not in text, 'privileged event/input')
    permissions(header, 0, {'contents': 'read', 'actions': 'read'})
    parts = re.split(r'^  ([a-z][a-z-]*):\n', body, flags=re.M)
    require(not parts[0].strip(), 'unexpected job preamble')
    names = parts[1::2]
    require(len(names) == len(set(names)) and set(names) == {
        'prepare-documentation', 'enhanced-tests', 'build', 'deploy', 'verify-live'}, 'job inventory')
    mapping = dict(zip(names, parts[2::2]))
    roles = {
        'prepare-documentation': ['checkout', 'upload-artifact', 'upload-artifact'],
        'enhanced-tests': ['checkout', 'download-artifact', 'upload-artifact', 'upload-artifact'],
        'build': ['checkout', 'download-artifact', 'download-artifact', 'upload-artifact'],
        'deploy': ['checkout', 'download-artifact', 'download-artifact',
                   'upload-pages-artifact', 'deploy-pages'],
        'verify-live': ['checkout', 'download-artifact', 'download-artifact', 'upload-artifact'],
    }
    for name, block in mapping.items():
        actual = re.findall(r'^\s*(?:- )?uses: (.+)$', block, re.M)
        expected = ['actions/' + role + '@' + ACTION_PINS['actions/' + role] for role in roles[name]]
        require(actual == expected, 'missing/replaced/reordered action role in ' + name)
    return mapping


def permissions(block, indent, expected):
    marker = ' ' * indent + 'permissions:\n'
    require(block.count(marker) == 1, 'missing/duplicate permissions')
    tail = block.split(marker)[1]
    rows = []
    for line in tail.splitlines():
        if line and len(line) - len(line.lstrip()) <= indent:
            break
        if line.strip():
            match = re.fullmatch(' ' * (indent + 2) + r'([a-z-]+): (read|write|none)', line)
            require(match is not None, 'unsupported permission syntax')
            rows.append(match.groups())
    require(len(rows) == len(dict(rows)) and dict(rows) == expected, 'broadened token permissions')


def steps(job):
    require(job.count('    steps:\n') == 1, 'missing/duplicate steps')
    parts = re.split(r'^      - ', job.split('    steps:\n')[1], flags=re.M)
    require(not parts[0].strip() and len(parts) > 1, 'invalid step sequence')
    result = []
    for part in parts[1:]:
        text = '        ' + part
        # A guard on a required step must never silently omit qualification.
        require(not re.search(r'^        continue-on-error:', text, re.M) and
                (not re.search(r'^        if:', text, re.M) or
                 (re.search(r'^        if: always\(\)$', text, re.M) is not None and
                  'uses: actions/upload-artifact@' in text)),
                'conditional or ignored required step')
        result.append(text)
    return result


def one(sequence, predicate, label):
    found = [(i, step) for i, step in enumerate(sequence) if predicate(step)]
    require(len(found) == 1, 'missing/duplicate ' + label)
    return found[0]


def action(sequence, name):
    require(name in ACTION_PINS, 'unknown action ' + name)
    return one(sequence, lambda step: re.search(
        r'^        uses: ' + re.escape(name + '@' + ACTION_PINS[name]) + r'$', step, re.M), name)


def run(step):
    matches = list(re.finditer(r'^        run: (.+)$', step, re.M))
    require(len(matches) == 1, 'missing/duplicate run')
    match = matches[0]
    if match.group(1) != '|':
        return match.group(1)
    lines = step[match.end():].splitlines()
    require(all(not line.strip() or line.startswith('          ') for line in lines), 'ambiguous run block')
    return '\n'.join(line[10:] for line in lines).strip()


def commands(step):
    return [shlex.split(line) for line in run(step).replace('\\\n', '').splitlines() if line.strip()]


def select(sequence, live=False):
    prefix = '.doc-tools/live-enhancer/bin/python' if live else '.doc-tools/enhancer/bin/python'
    i, step = one(sequence, lambda s: re.search(r'^        id: authority$', s, re.M), 'trusted authority')
    require(commands(step) == [[prefix, 'docs/tools/epoch_ci.py', 'select']], 'trusted runner selection')
    return i, prefix


def qualified_commands(step, prefix, expected):
    actual = commands(step)
    require(len(actual) == len(expected), 'extra/missing qualification command')
    for command, (operation, options) in zip(actual, expected):
        require(command[:4] == [prefix, '${{ steps.authority.outputs.runner }}', operation, '--root'],
                'qualification bypass or untrusted runner')
        require(command[4:] == ['$PWD', *options], 'qualification argument/identity mismatch')


def input_options():
    return ['--bundle', '$RUNNER_TEMP/qualified-download/qualified-inputs.tar',
            '--bundle-sha', '${{ needs.prepare-documentation.outputs.bundle_sha }}',
            '--manifest-sha', '${{ needs.prepare-documentation.outputs.manifest_sha }}']


def deployment_options():
    return ['--gate', '$RUNNER_TEMP/gate/gate.json', '--gate-sha', '${{ needs.build.outputs.gate_sha }}',
            '--artifact-id', '${{ needs.prepare-documentation.outputs.artifact_id }}',
            '--bundle-sha', '${{ needs.prepare-documentation.outputs.bundle_sha }}',
            '--manifest-sha', '${{ needs.prepare-documentation.outputs.manifest_sha }}']


def downloads(sequence):
    found = [step for step in sequence if 'uses: actions/download-artifact@' in step]
    require(len(found) == 2, 'same-run input/gate download inventory')
    for step, artifact, destination in zip(found, ['prepare-documentation.outputs.artifact_id', 'build.outputs.gate_id'],
                                         ['qualified-download', 'gate']):
        require(field(step, 'artifact-ids', 10) == '${{ needs.' + artifact + ' }}', 'same-run artifact ID')
        require(field(step, 'path', 10) == '${{ runner.temp }}/' + destination, 'artifact destination')
        require('run-id:' not in step and 'name:' not in step.split('        with:\n')[1],
                'cross-run or name-based artifact selection')


def function(root, name):
    tree = ast.parse((Path(root) / 'docs/tools/epoch_ci.py').read_bytes())
    nodes = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == name]
    require(len(nodes) == 1, 'missing/duplicate epoch function ' + name)
    return nodes[0]


def contains(node, statement):
    expected = ast.dump(ast.parse(statement).body[0])
    require(any(ast.dump(n) == expected for n in ast.walk(node)), 'epoch contract removed: ' + statement)


def authority_contract(root):
    node = function(root, 'select_trusted_runner')
    contains(node, 'outer = context()')
    contains(node, "value = classify(ROOT, outer['target_sha'], outer['source_sha'])")
    contains(node, "epoch.export(ROOT, outer['target_sha'], target)")
    contains(node, "runner = target / 'docs/tools/epoch_ci.py'")
    node = function(root, 'context')
    contains(node, "require(source.revision(ROOT, ref) == candidate, 'event head differs from fetched PR ref')")
    contains(node, 'source.exact_checkout(ROOT, candidate, source.tree(ROOT, candidate))')


def deployment_contract(root):
    node = function(root, 'deployment')
    contains(node, 'record = verify(args.directory, args.manifest_sha)')
    contains(node, "require(os.environ['GITHUB_EVENT_NAME'] == 'push' and os.environ['GITHUB_REF'] == 'refs/heads/main', 'only merged main may deploy')")
    contains(node, "require(record['identity']['qualification_mode'] == 'hosted', 'local evidence cannot authorize deployment')")
    contains(node, "require(ci.file_sha(args.gate) == args.gate_sha, 'deployment gate digest mismatch')")
    contains(node, "require(gate['mechanically_qualified'] is True and gate['deployable'] is True and gate['identity'] == context() and gate['epochs_sha256'] == digest(record), 'unqualified deployment epoch')")
    contains(node, "require(gate['qualified'] == {'artifact_id': args.artifact_id, 'bundle_sha256': args.bundle_sha, 'manifest_sha256': args.manifest_sha}, 'deployment artifact mismatch')")
    contains(node, "require(set(gate['report_sha256']) == set(ci.SHARDS), 'incomplete deployment shard gate')")
    contains(node, "require(approved is not None and gate['publication_authority'] == approved, 'no independently reviewed publication authority for merged source/tree')")


def live_contract(root):
    job = jobs(root)['verify-live']
    require(field(job, 'if', 4) == "github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", 'main-only live job')
    require(field(job, 'needs', 4) == '[prepare-documentation, build, deploy]', 'live dependencies')
    permissions(job, 4, {'contents': 'read', 'actions': 'read'})
    sequence = steps(job)
    checkout, step = action(sequence, 'actions/checkout')
    require(field(step, 'ref', 10) == '${{ github.sha }}', 'deployed checkout identity')
    confirm, step = one(sequence, lambda s: 'test "$(git rev-parse HEAD)"' in s, 'exact deployed HEAD')
    require(field(step, 'EXPECTED_SHA', 10) == '${{ github.sha }}' and
            run(step) == 'test "$(git rev-parse HEAD)" = "$EXPECTED_SHA"', 'deployed HEAD check')
    downloads(sequence)
    selected, prefix = select(sequence, live=True)
    verified, step = one(sequence, lambda s: "' unpack --root" in s, 'live artifact verification')
    qualified_commands(step, prefix, [('unpack', input_options()), ('deployment', deployment_options())])
    observed, step = one(sequence, lambda s: "' live --root" in s, 'bounded live HTTP verification')
    require(field(step, 'EXPECTED_SHA', 10) == '${{ github.sha }}', 'live expected commit')
    qualified_commands(step, prefix, [('live', ['--manifest-sha', '${{ needs.prepare-documentation.outputs.manifest_sha }}',
        '--base-url', 'https://zackboll.github.io/ams-gra-mel-bridge/', '--expected-commit', '$EXPECTED_SHA',
        '--attempts', '10', '--interval', '30', '--evidence', '$RUNNER_TEMP/live-publication.json'])])
    require(checkout < confirm < selected < verified < observed, 'live trust ordering')
    require('actions/deploy-pages@' not in job and 'actions/upload-pages-artifact@' not in job, 'read-only live job deploys')
    authority_contract(root)
    deployment_contract(root)

    contains(function(root, 'live'), "require(record['identity']['event'] == 'push' and args.expected_commit == record['identity']['source_sha'], 'live source identity mismatch')")


def preparation_contract(root):
    mapping = jobs(root)
    sequence = steps(mapping['prepare-documentation'])
    selected, _ = select(sequence)
    prepared, step = one(sequence, lambda s: re.search(r'^        id: seal$', s, re.M), 'epoch preparation')
    require(field(step, 'POLICY_RUNNER', 10) == '${{ steps.authority.outputs.runner }}', 'prepared runner identity')
    expected = 'cd ../..; .doc-tools/enhancer/bin/python "$POLICY_RUNNER" prepare --root "$PWD" --gnatdoc-prefix "$GNATDOC_PREFIX"'
    cmds = commands(step)
    require(cmds == [['alr', '-n', '-s', '$PWD/.doc-tools/settings', '-C', 'docs/toolchain', 'exec', '--', 'sh', '-c', expected],
                     ['cp', '.doc-tools/epochs/inputs.tar', '$RUNNER_TEMP/qualified-inputs.tar']], 'immutable epoch prepare/seal command')
    require(selected < prepared, 'qualification before authority')
    uploaded, step = action(sequence, 'actions/upload-artifact') if sum('uses: actions/upload-artifact@' in s for s in sequence) == 1 else one(sequence, lambda s: re.search(r'^        id: inputs$', s, re.M), 'sealed inputs')
    require(uploaded > prepared and field(step, 'name', 10) == 'documentation-inputs-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}' and
            field(step, 'path', 10) == '${{ runner.temp }}/qualified-inputs.tar' and field(step, 'if-no-files-found', 10) == 'error', 'source/run-bound input upload')
    enhanced = mapping['enhanced-tests']
    require(field(enhanced, 'needs', 4) == 'prepare-documentation' and field(enhanced, 'fail-fast', 6) == 'false', 'required shard prerequisite/fail-fast')
    require(field(enhanced, 'shard', 8) == '[navigation, publication, live, symbols-catalog, types, remaining]', 'six shard identities')
    require(not re.search(r'^    (?:if|continue-on-error):', enhanced, re.M), 'skipped shard job')
    sequence = steps(enhanced)
    _, download = action(sequence, 'actions/download-artifact')
    require(field(download, 'artifact-ids', 10) == '${{ needs.prepare-documentation.outputs.artifact_id }}' and
            field(download, 'path', 10) == '${{ runner.temp }}/qualified-download' and
            field(download, 'merge-multiple', 10) == 'true' and 'run-id:' not in download and
            'name:' not in download.split('        with:\n')[1], 'same-run qualified shard inputs')
    require(': write' not in enhanced and 'secrets.' not in enhanced, 'privileged shard job')
    selected, prefix = select(sequence)
    unpacked, step = one(sequence, lambda s: "' unpack --root" in s, 'shard unpack')
    qualified_commands(step, prefix, [('unpack', input_options())])
    executed, step = one(sequence, lambda s: "' run --root" in s, 'exact-once shard command')
    qualified_commands(step, prefix, [('run', ['--owner', '${{ matrix.shard }}', '--output', '$RUNNER_TEMP/results/${{ matrix.shard }}.json',
        '--diagnostics', '$RUNNER_TEMP/failure-diagnostics/${{ matrix.shard }}',
        '--artifact-id', '${{ needs.prepare-documentation.outputs.artifact_id }}', '--bundle-sha', '${{ needs.prepare-documentation.outputs.bundle_sha }}',
        '--manifest-sha', '${{ needs.prepare-documentation.outputs.manifest_sha }}'])])
    require(selected < unpacked < executed, 'shard qualification ordering')
    uploads = [(i, s) for i, s in enumerate(sequence) if 'uses: actions/upload-artifact@' in s]
    require(len(uploads) == 2, 'canonical/diagnostic upload inventory')
    canonical, diagnostic = uploads
    suffix = '${{ matrix.shard }}-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}'
    for (index, step), label, destination, missing in (
            (canonical, 'result', '${{ runner.temp }}/results/${{ matrix.shard }}.json', 'error'),
            (diagnostic, 'failure-diagnostics', '${{ runner.temp }}/failure-diagnostics/${{ matrix.shard }}/', 'warn')):
        require(field(step, 'uses', 8) == 'actions/upload-artifact@' + ACTION_PINS['actions/upload-artifact'],
                'unreviewed result/diagnostic action')
        require(index > executed and field(step, 'name', 10) == 'documentation-' + label + '-' + suffix and
                field(step, 'path', 10) == destination and field(step, 'if-no-files-found', 10) == missing,
                'result/diagnostic identity or destination')
    require(canonical[0] < diagnostic[0] and not re.search(r'^        if:', canonical[1], re.M),
            'canonical successful report must remain mandatory')
    require(field(diagnostic[1], 'if', 8) == 'always()', 'failure diagnostics must always upload')
    authority_contract(root)
    node = function(root, 'prepare')
    for statement in ["historical_checkout(history)",
        "execute(['python3', 'scripts/check_doc_source_integrity.py'], history, logs / 'historical-source.log', env)",
        "execute([py, 'docs/tools/documentation_ci.py', 'inventory'], history, logs / 'inventory.log', env)",
        "execute([py, 'docs/tools/documentation_ci.py', 'run', '--owner', 'scripts', '--output', 'qualification/scripts.json'], history, logs / 'scripts.log', env)",
        "execute([py, 'docs/tools/documentation_ci.py', 'seal', '--output', str(directory / 'historical.tar')], history, logs / 'historical-seal.json', env)",
        "require(context() == outer and classify(ROOT, outer['target_sha'], outer['source_sha']) == value, 'candidate qualification changed source or epoch')"]:
        contains(node, statement)
    contains(node, '''commands = (["make", "test-native"], ["make", "test-ada"], ["make", "check"],
        ["make", "check-ada-format"], ["alr", "-n", "-C", "ada", "build"],
        ["alr", "-n", "-C", "ada/tests", "run"], ["make", "test-python"],
        ["make", "test-rust"], ["sh", "docs/tools/qualify_examples.sh"])''')
    contains(node, "for i, command in enumerate(commands):\n    execute(command, ROOT, logs / ('language-' + str(i) + '.log'), env)")
    contains(node, "env = qualified_alire_environment(ROOT / '.doc-tools/settings', logs / 'alire-selection.log', env)")
    contains(node, "candidate = epoch.qualify_evidence(ROOT, value, directory / 'candidate', prefix)")
    contains(function(root, 'run_shard'), 'verify(args.directory, args.manifest_sha)')
    contains(function(root, 'reconcile'), "require(set(reports) == set(ci.SHARDS), 'missing/unknown historical shard')")
    contains(function(root, 'reconcile'), "require(set(needs) == {'prepare-documentation', 'enhanced-tests'} and all(v['result'] == 'success' for v in needs.values()), 'incomplete prerequisite jobs')")
    source = (Path(root) / 'docs/tools/documentation_ci.py').read_text()
    tree = ast.parse(source)
    validate = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'validate_report')
    contains(validate, "require(Counter(e['id'] for e in events if e['event'] == kind) == Counter(expected), 'missing or repeated test execution: ' + kind)")


def deploy_contract(root):
    mapping = jobs(root)
    build = mapping['build']
    require(field(build, 'if', 4) == 'always()' and field(build, 'needs', 4) == '[prepare-documentation, enhanced-tests]', 'aggregate must always evaluate')
    sequence = steps(build)
    _, download = one(sequence, lambda s: 'pattern:' in s, 'canonical result download')
    require(field(download, 'uses', 8) == 'actions/download-artifact@' + ACTION_PINS['actions/download-artifact'] and
            field(download, 'pattern', 10) == 'documentation-result-*-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}' and
            field(download, 'path', 10) == '${{ runner.temp }}/shard-results' and
            'run-id:' not in download and 'failure-diagnostics' not in build,
            'aggregate must consume only same-run canonical reports')
    selected, prefix = select(sequence)
    aggregated, step = one(sequence, lambda s: "' aggregate --root" in s, 'aggregate command')
    require(field(step, 'DOC_NEEDS', 10) == '${{ toJSON(needs) }}' and selected < aggregated, 'aggregate prerequisite authority')
    qualified_commands(step, prefix, [('aggregate', ['--reports', '$RUNNER_TEMP/shard-results',
        '--output', '$RUNNER_TEMP/gate.json', '--artifact-id', '${{ needs.prepare-documentation.outputs.artifact_id }}',
        '--bundle-sha', '${{ needs.prepare-documentation.outputs.bundle_sha }}',
        '--manifest-sha', '${{ needs.prepare-documentation.outputs.manifest_sha }}'])])
    deploy = mapping['deploy']
    require(field(deploy, 'if', 4) == "github.ref == 'refs/heads/main' && github.event_name != 'pull_request'", 'PR Pages deployment')
    require(field(deploy, 'needs', 4) == '[prepare-documentation, build]', 'deploy dependencies')
    permissions(deploy, 4, {'contents': 'read', 'actions': 'read', 'pages': 'write', 'id-token': 'write'})
    sequence = steps(deploy)
    downloads(sequence)
    selected, prefix = select(sequence)
    verified, step = one(sequence, lambda s: "' unpack --root" in s, 'deployment verification')
    qualified_commands(step, prefix, [('unpack', input_options()), ('deployment', deployment_options())])
    upload, step = action(sequence, 'actions/upload-pages-artifact')
    require(field(step, 'include-hidden-files', 10) == 'true', 'Pages hidden-file preservation')
    require(field(step, 'path', 10) == '.doc-tools/epochs/candidate/published/', 'unqualified Pages source')
    name = 'github-pages-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}'
    require(field(step, 'name', 10) == name, 'Pages artifact identity')
    deployed, step = action(sequence, 'actions/deploy-pages')
    require(field(step, 'artifact_name', 10) == name and selected < verified < upload < deployed, 'Pages authorization ordering')
    for key, job in mapping.items():
        if key != 'deploy':
            require('actions/deploy-pages@' not in job and 'actions/upload-pages-artifact@' not in job, 'direct deployment bypass')
    authority_contract(root)
    deployment_contract(root)
