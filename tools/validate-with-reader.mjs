import fs from 'node:fs';
import {
  isKeyboardDefinitionV2,
  isKeyboardDefinitionV3,
  isVIADefinitionV2,
  isVIADefinitionV3,
} from '@the-via/reader';

const [version, ...files] = process.argv.slice(2);
if (!['v2', 'v3'].includes(version) || !files.length) {
  console.error('usage: npm run validate:reader -- v2|v3 file.json [...]');
  process.exit(2);
}
const validators = version === 'v2'
  ? [isKeyboardDefinitionV2, isVIADefinitionV2]
  : [isKeyboardDefinitionV3, isVIADefinitionV3];
let failed = false;
for (const file of files) {
  try {
    const value = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (validators.some(validate => validate(value))) {
      console.log(`VALID ${version} ${file}`);
      continue;
    }
    failed = true;
    console.error(`INVALID ${version} ${file}`);
    const errors = validators.flatMap(validate => validate.errors ?? []);
    for (const error of errors) {
      const path = error.instancePath || error.dataPath || 'Object';
      console.error(`  ${path}: ${error.message}`);
    }
  } catch (error) {
    failed = true;
    console.error(`INVALID ${version} ${file}: ${error.name}: ${error.message}`);
  }
}
process.exitCode = failed ? 1 : 0;
