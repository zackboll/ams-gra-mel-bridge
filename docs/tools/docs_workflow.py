"""Fail-closed structural security contract for the current documentation jobs.

Uses the existing bounded workflow reader and reviewed action pins. Historical
topology is not a security boundary; exact source, run, digest and privilege
bindings are.
"""

from pathlib import Path
import re

from ci001r3_workflow import ACTION_PINS, action, commands, field, one, permissions, require, steps


def check(root):
    text = (Path(root) / '.github/workflows/documentation.yml').read_text()
    require(text.count('\njobs:\n') == 1, 'ambiguous jobs')
    header, body = text.split('\njobs:\n')
    permissions(header, 0, {'contents': 'read', 'actions': 'read'})
    events = header.split('on:\n', 1)[1].split('\npermissions:', 1)[0]
    require(events.strip() == 'pull_request:\n    branches: [main]\n  push:\n    branches: [main]\n  workflow_dispatch:',
            'unsupported documentation event')
    require('  DOC_SOURCE_SHA: ${{ github.event.pull_request.head.sha || github.sha }}\n' in header,
            'unbound documentation source')
    parts = re.split(r'^  ([a-z][a-z-]*):\n', body, flags=re.M)
    require(not parts[0].strip() and parts[1::2] == ['build', 'deploy', 'verify-live'], 'unexpected jobs')
    mapping = dict(zip(parts[1::2], parts[2::2]))
    roles = {'build': ['checkout', 'upload-artifact', 'upload-artifact'],
             'deploy': ['checkout', 'download-artifact', 'upload-pages-artifact', 'deploy-pages'],
             'verify-live': ['checkout', 'download-artifact', 'upload-artifact']}
    for name, job in mapping.items():
        require(not re.search(r'^    (?:uses|continue-on-error|strategy):', job, re.M), 'job bypass')
        actual = re.findall(r'^\s*(?:- )?uses: (.+)$', job, re.M)
        require(actual == ['actions/' + role + '@' + ACTION_PINS['actions/' + role]
                           for role in roles[name]], 'action owner/pin/role mismatch')
        sequence = steps(job)
        allowed_runs = {
            'build': {'Install isolated pinned documentation tools',
                      'Check dependency integrity and deployment boundaries',
                      'Test current documentation build controls',
                      'Generate once from Git and verify source-derived publication'},
            'deploy': {'Install immutable reviewed enhancer',
                       'Verify bundle, source, run, presentation and main-only authority'},
            'verify-live': {'Install immutable reviewed enhancer', 'Verify deployed artifact authority',
                            'Compare every live entity with the verified artifact'},
        }
        run_steps = [step for step in sequence if re.search(r'^        run:', step, re.M)]
        require({field(step, 'name', 8) for step in run_steps} == allowed_runs[name] and
                len(run_steps) == len(allowed_runs[name]), 'extra/missing executable workflow step')
        for step in run_steps:
            label = field(step, 'name', 8)
            expected = {
                'Install isolated pinned documentation tools': [['sh', 'docs/tools/install_docs_toolchain.sh']],
                'Check dependency integrity and deployment boundaries': [['python3', 'scripts/check_doc_source_integrity.py']],
                'Test current documentation build controls': [['make', 'test-docs-ci']],
                'Install immutable reviewed enhancer': [['sh', 'docs/tools/install_enhancer.sh', '.doc-tools/enhancer']],
            }.get(label)
            if expected is not None:
                require(commands(step) == expected, 'replaced required workflow command')
        _, checkout = action(sequence, 'actions/checkout')
        require(field(checkout, 'ref', 10) == '${{ env.DOC_SOURCE_SHA }}' and
                field(checkout, 'persist-credentials', 10) == 'false' and
                field(checkout, 'fetch-depth', 10) == '0', 'checkout identity/credential boundary')
        if name != 'deploy':
            require('permissions:' not in job, 'unexpected job permissions')
        if name != 'build':
            require(field(job, 'if', 4) == "github.ref == 'refs/heads/main' && github.event_name == 'push'",
                    'only main pushes may deploy/verify live')
            require(field(job, 'needs', 4) == ('[build]' if name == 'deploy' else '[build, deploy]'),
                    'missing successful prerequisite')
            downloaded, download = action(sequence, 'actions/download-artifact')
            require(field(download, 'artifact-ids', 10) == '${{ needs.build.outputs.artifact_id }}' and
                    field(download, 'path', 10) == '${{ runner.temp }}/verified-download' and
                    not re.search(r'^          (?:run-id|name|pattern):', download, re.M),
                    'must select exact same-run artifact ID')
            verified, step = one(sequence, lambda s: 'docs_ci.py unpack' in s, 'artifact verification')
            require(commands(step) == [[
                '.doc-tools/enhancer/bin/python', 'docs/tools/docs_ci.py', 'unpack', '--deployment',
                '--bundle', '$RUNNER_TEMP/verified-download/current.tar',
                '--bundle-sha', '${{ needs.build.outputs.bundle_sha }}',
                '--manifest-sha', '${{ needs.build.outputs.manifest_sha }}']], 'artifact verification arguments')
            require(downloaded < verified, 'verify before download')
            if name == 'deploy':
                permissions(job, 4, {'contents': 'read', 'actions': 'read', 'pages': 'write', 'id-token': 'write'})
                require('    environment:\n      name: github-pages\n' in job, 'Pages environment missing')
                uploaded, upload = action(sequence, 'actions/upload-pages-artifact')
                deployed, deploy = action(sequence, 'actions/deploy-pages')
                artifact = 'github-pages-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}'
                require(field(upload, 'name', 10) == field(deploy, 'artifact_name', 10) == artifact and
                        field(upload, 'path', 10) == '.doc-tools/current/published/' and
                        field(upload, 'include-hidden-files', 10) == 'true' and
                        verified < uploaded < deployed, 'unverified Pages upload/deployment')
            else:
                live, step = one(sequence, lambda s: 'docs_ci.py live' in s, 'live verification')
                require(verified < live and commands(step) == [[
                    '.doc-tools/enhancer/bin/python', 'docs/tools/docs_ci.py', 'live', '--deployment',
                    '--manifest-sha', '${{ needs.build.outputs.manifest_sha }}',
                    '--base-url', 'https://zackboll.github.io/ams-gra-mel-bridge/',
                    '--evidence', '$RUNNER_TEMP/live-publication.json']], 'live verification bypass')
    build = mapping['build']
    require(not re.search(r'^    (?:if|needs):', build, re.M), 'build may not be skipped')
    require('      artifact_id: ${{ steps.inputs.outputs.artifact-id }}\n' in build and
            '      bundle_sha: ${{ steps.seal.outputs.bundle_sha }}\n' in build and
            '      manifest_sha: ${{ steps.seal.outputs.manifest_sha }}\n' in build, 'unbound build outputs')
    sequence = steps(build)
    generated, generate = one(sequence, lambda s: 'docs_ci.py build' in s, 'single generation')
    require(field(generate, 'id', 8) == 'seal' and commands(generate) == [[
            'alr', '-n', '-s', '$ALIRE_SETTINGS_DIR', '-C', 'docs/toolchain', 'exec', '--', 'sh', '-c',
            'cd ../..; .doc-tools/enhancer/bin/python docs/tools/docs_ci.py build --gnatdoc-prefix "$GNATDOC_PREFIX"']],
            'source generation bypass')
    uploaded, upload = one(sequence, lambda s: 'id: inputs\n' in s, 'verified input upload')
    require(generated < uploaded and field(upload, 'path', 10) == '.doc-tools/current.tar' and
            field(upload, 'if-no-files-found', 10) == 'error' and
            field(upload, 'name', 10) == 'documentation-inputs-${{ github.run_id }}-${{ github.run_attempt }}-${{ env.DOC_SOURCE_SHA }}',
            'wrong/missing build artifact')
    return mapping
