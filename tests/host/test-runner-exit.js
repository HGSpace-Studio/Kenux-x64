const { spawnSync } = require('child_process');

const root = require('path').resolve(__dirname, '../..');

const passing = spawnSync(process.execPath, ['test-runner.js'], {
  cwd: root,
  encoding: 'utf8'
});
if (passing.status !== 0) {
  process.stderr.write(passing.stdout || '');
  process.stderr.write(passing.stderr || '');
  process.exit(1);
}

const failing = spawnSync(process.execPath, ['test-runner.js'], {
  cwd: root,
  env: { ...process.env, KENUX_TEST_INJECT_FAILURE: '1' },
  encoding: 'utf8'
});
if (failing.status === 0 || !`${failing.stdout}${failing.stderr}`.includes('测试失败')) {
  process.stderr.write(failing.stdout || '');
  process.stderr.write(failing.stderr || '');
  process.exit(1);
}

console.log('test-runner exit propagation: pass');
