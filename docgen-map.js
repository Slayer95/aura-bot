"use strict";

const assert = require('assert');
const fs = require('fs/promises');
const util = require('util');
const path = require('path');

const OUTPUT_PATH = `CONFIG_MAPS.md`;
const FILE_LIST = [
  'src/map.cpp',
  'src/game_result.cpp',
];
const configMaybeKeyRegexp = /CFG(?:\.|\-\>)(GetMaybe[a-zA-Z0-9]+)\("([^"]+)\"/;
const configKeyRegexp = /CFG(?:\.|\-\>)(Get[a-zA-Z0-9]+)\("([^"]+)\", ([^\)]+)\)/;

function getKeyType(keyName) {
  let subName = '';
  if (keyName.startsWith(`GetMaybe`)) {
    subName = keyName.slice(`GetMaybe`.length);
  } else if (keyName.startsWith(`Get`)) {
    subName = keyName.slice(`Get`.length);
  } else {
    console.error(`Unhandled key name ${keyName}`);
  }
  if (subName == `StringIndex`) {
    return `enum`;
  }
  return subName.toLowerCase();
}

function getDefaultValue(rest, keyName) {
  if (keyName === `net.host_port.min`) return `<net.host_port.only>`;
  if (keyName === `net.host_port.max`) return `<net.host_port.min>`;
  if (rest.endsWith('}') && rest.includes('{')) {
    rest = rest.slice(rest.indexOf('{') + 1, -1);
    return rest.split(`,`).map(x => x.trim()).join(` `);
  } else {
    let parts = rest.split(',');
    rest = parts[parts.length - 1].trim();
  }
  if (rest == "emptyString") {
    return "Empty";
  }
  if (rest.startsWith(`m_`)) {
    return "Empty";
  }
  if (rest == `filesystem::path(`) {
    return "Empty";
  }
  if (rest.startsWith(`0x`)) {
    return parseInt(rest, 16);
  }
  if (rest === `MAP_TRANSFERS_AUTOMATIC`) return `auto`;
  if (rest === `COMMAND_PERMISSIONS_AUTO`) return `auto`;
  if (rest === `REALM_AUTH_PVPGN`) return `pvpgn`;
  if (rest === `CFG.GetHomeDir(`) return `Aura home directory`;
  return rest.replace(/^"/, '').replace(/"$/, '');
}

function getValueConstraints(fnName, keyName, rest) {
  let parts = rest.split(',').map(x => x.trim());
  if (fnName === 'GetString') {
    if (parts.length >= 3) {
      return [`Min length: ${parts[0]}`, `Max length: ${parts[1]}`];
    }
  }
  return [];
}

async function main() {
  const optionsMeta = new Map();
  const configOptions = [];
  let isStrictMode = false;
  for (const fileName of FILE_LIST) {
    const filePath = path.resolve(__dirname, fileName);
    const fileContent = await fs.readFile(filePath, 'utf8');
    let lastConfigName = '';
    let lineNum = 0;
    for (const line of fileContent.split(/\r?\n/g)) {
      lineNum++;
      let trimmed = line.trim();
      let maybeKeyMatch = configMaybeKeyRegexp.exec(trimmed);
      if (maybeKeyMatch) {
        const [, fnName, keyName] = maybeKeyMatch;
        lastConfigName = keyName;
        if (!optionsMeta.has(lastConfigName)) optionsMeta.set(lastConfigName, {});
        configOptions.push({keyName, fnName});
        if (isStrictMode) optionsMeta.get(lastConfigName).failIfError = true;
        continue;
      }
      let keyMatch = configKeyRegexp.exec(trimmed);
      if (keyMatch) {
        const [, fnName, keyName, restArgs] = keyMatch;
        lastConfigName = keyName;
        configOptions.push({keyName, fnName, restArgs});
        if (!optionsMeta.has(lastConfigName)) optionsMeta.set(lastConfigName, {});
        if (isStrictMode) optionsMeta.get(lastConfigName).failIfError = true;
        continue;
      }
      if (trimmed.endsWith(`CFG.FailIfErrorLast();`) || trimmed.endsWith(`CFG->FailIfErrorLast();`)) {
        if (!optionsMeta.has(lastConfigName)) console.error({fileName, lineNum, lastConfigName});
        optionsMeta.get(lastConfigName).failIfError = true;
      }
      if (trimmed === `CFG.SetStrictMode(true)` || trimmed === `CFG->SetStrictMode(true)`) {
        isStrictMode = true;
      }
      if (trimmed === `CFG.SetStrictMode(wasStrict)` || trimmed === `CFG->SetStrictMode(wasStrict)`) {
        isStrictMode = false;
      }
    }
  }

  const outContents = [
    `Map`,
    `==========`,
    `# Supported map keys`,
  ];
  configOptions.sort((a, b) => a.keyName.localeCompare(b.keyName));
  for (const configEntry of configOptions) {
    outContents.push('## \\`' + configEntry.keyName + '\\`');
    outContents.push(`- Type: ${getKeyType(configEntry.fnName)}`);
    if (configEntry.restArgs) {
      let constraints = getValueConstraints(configEntry.fnName, configEntry.keyName,configEntry.restArgs);
      if (constraints.length) {
        outContents.push(`- Constraints: ${constraints.join('. ')}.`);
      }
    }
    if (configEntry.fnName.startsWith(`getMaybe`)) {
      outContents.push(`- Optional key: Yes`);
    } else {
      const failIfError = optionsMeta.get(configEntry.keyName)?.failIfError;
      if (configEntry.restArgs) {
        outContents.push(`- Default value: ${getDefaultValue(configEntry.restArgs, configEntry.keyName)}`);
      }
      outContents.push(`- Error handling: ${failIfError ? 'Abort operation' : 'Use default value'}`);
    }
    outContents.push(``);
  }

  await fs.writeFile(
    path.resolve(__dirname, OUTPUT_PATH),
    outContents.join(`\n`).replace(/([<>])/g, '\\$1'),
  );
}

main();
