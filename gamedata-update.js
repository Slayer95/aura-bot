"use strict";

const assert = require('assert');
const fs = require('fs');
const util = require('util');
const path = require('path');
const http = require('http');
const https = require('https');

function toId(str) {
  if (typeof str == 'number') return `${number}`;
  if (typeof str == 'string') {
    return str.toLowerCase().replace(/[^a-zA-Z0-9 ]/g, '').replace(/ /g, '-');
  }
}

function toAcronym(words) {
  let firstLetters = words.map(toId).filter(x => x).map(x => x[0]);
  return firstLetters.join('');
}

const TWRPG = {
  grades: [`Basic`, `Deltirama`, `Neptinos`, `Gnosis`, `Alteia`, `Arcana`],
  displayItemQuality(rank, grade) {
    if (rank == "[Epic]") return this.grades[grade];
    if (rank == "none") return "Basic";
    return rank || "Basic";
  },
  displayRecipePart(entries) {
    const variants = [];
    for (const entry of entries) {
      variants.push(entry[1] > 1 ? `${entry[1]}x ${entry[0]}` : entry[0]);
    }
    if (variants.length >= 2) return `(${variants.join(' / ')})`;
    return variants[0];
  },
  displayRecipe(recipe) {
    if (!recipe) return [];
    let recipeBuffer = [];
    for (const part of recipe) {
      recipeBuffer.push(this.displayRecipePart(Object.entries(part)));
    }
    return [recipeBuffer.join(` + `)];
  },
  displayDropped(dropSources) {
    if (!dropSources) return [];
    return [`Kill ${dropSources.join(` / `)}`];
  }
};

function downloadFullTWRPG(cb) {
  const fullItemsStream = fs.createWriteStream('items-full.json');
  https.get("https://raw.githubusercontent.com/sfarmani/twrpg-info/master/items.json", function (res) {
    res.pipe(fullItemsStream).on('error', err => console.error(err.message));
  });
  fullItemsStream.on('error', err => console.error(err.message));
  fullItemsStream.on('finish', function() {
    if (cb) cb();
  });
}

function updateFromDownloaded() {
  const fullItems = JSON.parse(fs.readFileSync('items-full.json'));
  const outputTWRPG = {aliases: {}, items: {}};
  for (const fourCC in fullItems) {
    const thisItem = fullItems[fourCC];
    const itemNameWords = thisItem.name.split(' ');
    outputTWRPG.items[toId(thisItem.name)] = [
      `${thisItem.name} (${thisItem.type} - ${TWRPG.displayItemQuality(thisItem.rank, thisItem.grade)})`,
      ...(TWRPG.displayRecipe(thisItem.recipe)),
      ...(TWRPG.displayDropped(thisItem.dropped_by)),
      ...(thisItem.stats?.passive || []),
      ...(thisItem.stats?.active || []),
    ];
    if (1 < itemNameWords.length && itemNameWords.length <= 4 && thisItem.rank == "[Epic]") {
      outputTWRPG.aliases[toAcronym(itemNameWords)] = [2, toId(thisItem.name)]; // 2 is item
    }
  }
  for (const alias in outputTWRPG.aliases) {
    if (outputTWRPG.hasOwnProperty(alias)) {
      delete outputTWRPG.aliases[alias];
    }
  }
  fs.writeFileSync(`twrpg.json`, JSON.stringify(outputTWRPG, null, '\t'));
}

function main() {
  downloadFullTWRPG(updateFromDownloaded);
  //updateFromDownloaded();
}

main();

