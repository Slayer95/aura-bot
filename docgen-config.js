"use strict";

const assert = require('assert');
const fs = require('fs/promises');
const util = require('util');
const path = require('path');

const OUTPUT_PATH = `CONFIG.md`;
const COMMAND_FILES = ['src/net.cpp', 'src/config_bot.cpp', 'src/config_game.cpp', 'src/config_irc.cpp', 'src/config_net.cpp', 'src/config_realm.cpp'];
const configMaybeKeyRegexp = /CFG\-\>(GetMaybe[a-zA-Z0-9]+)\("([^"]+)\"/;
const configKeyRegexp = /CFG\-\>(Get[a-zA-Z0-9]+)\("([^"]+)\", ([^\)]+)\)/;

const ReloadModes = {NONE: 0, INSTANT: 1, NEXT: 2};

function getKeyType(keyName) {
  let subName = '';
  if (keyName.startsWith(`GetMaybe`)) {
    subName = keyName.slice(`GetMaybe`.length);
  } else if (keyName.startsWith(`Get`)) {
    subName = keyName.slice(`Get`.length);
  } else {
    console.error(`Unhandled key name ${keyName}`);
  }
  return subName.toLowerCase();
}

function getDefaultValue(rest) {
  let parts = rest.split(',');
  rest = parts[parts.length - 1].trim();
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
  let isCannotBeReloadedOn = false;
  for (const fileName of COMMAND_FILES) {
    isCannotBeReloadedOn = false;
    const filePath = path.resolve(__dirname, fileName);
    const fileContent = await fs.readFile(filePath, 'utf8');
    let lastConfigName = '';
    for (const line of fileContent.split(/\r?\n/g)) {
      const trimmed = line.trim();
      let maybeKeyMatch = configMaybeKeyRegexp.exec(trimmed);
      if (maybeKeyMatch) {
        const [, fnName, keyName] = maybeKeyMatch;
        lastConfigName = keyName;
        configOptions.push({keyName, fnName});
        continue;
      }
      let keyMatch = configKeyRegexp.exec(trimmed);
      if (keyMatch) {
        const [, fnName, keyName, restArgs] = keyMatch;
        lastConfigName = keyName;
        configOptions.push({keyName, fnName, restArgs});
        if (!optionsMeta.has(lastConfigName)) optionsMeta.set(lastConfigName, {});
        if (isCannotBeReloadedOn) {
          optionsMeta.get(lastConfigName).reload = ReloadModes.NONE;
        } else if (fileName == 'src/config_game.cpp') {
          optionsMeta.get(lastConfigName).reload = ReloadModes.NEXT;
        }
        continue;
      }
      if (trimmed === 'CFG->FailIfErrorLast();') {
        if (!optionsMeta.has(lastConfigName)) {
          optionsMeta.set(lastConfigName, {});
          optionsMeta.get(lastConfigName).failIfError = true;
        }
      }
      if (trimmed === `// == SECTION START: Cannot be reloaded ==`) {
        isCannotBeReloadedOn = true;
      }
      if (trimmed === `// == SECTION END ==`) {
        isCannotBeReloadedOn = false;
      }
    }
  }

  const outContents = [
    `Config`,
    `==========`,
    `# Supported config keys`,
  ];
  configOptions.sort((a, b) => a.keyName.localeCompare(b.keyName));
  for (const configEntry of configOptions) {
    if (configEntry.keyName.endsWith(`--but_its_hardcoded`)) continue;
    if (configEntry.keyName.includes(`.gameranger.`)) continue;
    outContents.push('## \\`' + configEntry.keyName + '\\`');
    outContents.push(`Type: ${getKeyType(configEntry.fnName)}`);
    if (configEntry.restArgs) {
      let constraints = getValueConstraints(configEntry.fnName, configEntry.keyName,configEntry.restArgs);
      if (constraints.length) {
        outContents.push(`Constraints: ${constraints.join('. ')}.`);
      }
    }
    if (configEntry.fnName.startsWith(`getMaybe`)) {
      outContents.push(`Optional key: Yes`);
    } else {
      const failIfError = optionsMeta.get(configEntry.keyName)?.failIfError;
      if (configEntry.restArgs) {
        outContents.push(`Default value: ${getDefaultValue(configEntry.restArgs)}`);
      }
      outContents.push(`Error handling: ${failIfError ? 'Abort operation' : 'Use default value'}`);
    }
    const reloadMode = optionsMeta.get(configEntry.keyName)?.reloadMode;
    if (reloadMode === ReloadModes.NEXT) {
      outContents.push(`Reloadable: Yes, but it doesn't affect currently hosted games.`);
    } else if (reloadMode === ReloadModes.INSTANT) {
      outContents.push(`Reloadable: Yes.`);
    } else if (reloadMode === ReloadModes.NONE) {
      outContents.push(`Reloadable: Cannot be reloaded.`);
    }
    outContents.push(``);
  }

  await fs.writeFile(
    path.resolve(__dirname, OUTPUT_PATH),
    outContents.join(`\n`),
  );
}

main();
