import {promises as fs} from 'node:fs';
import path from 'node:path';
import {argv, exit} from 'node:process';

interface CompileCommand {
  directory: string;
  command?: string;
  arguments?: string[];
  file: string;
  output?: string;
}

async function walkDir(dir: string,
                       fileList: string[] = []): Promise<string[]> {
  const files = await fs.readdir(dir, {withFileTypes : true});
  for (const file of files) {
    const fullPath = path.join(dir, file.name);
    if (file.isDirectory()) {
      await walkDir(fullPath, fileList);
    } else if (fullPath.endsWith('.hpp')) {
      fileList.push(path.resolve(fullPath).replaceAll('\\', '/'));
    }
  }
  return fileList;
}

// can be wrapped in a "main()" function cleaner but cba

const dbPath = argv[2];
if (!dbPath) {
  console.error("Error: Please provide the path to compile_commands.json");
  exit(1);
}

try {
  const dbRaw = await fs.readFile(dbPath, 'utf-8');
  const db: CompileCommand[] = JSON.parse(dbRaw);

  const baseEntry =
      db.find((e) => e.file.includes('src') || e.file.includes('main.cpp'));

  if (!baseEntry) {
    console.error("Could not find a base compilation entry. Exiting.");
    exit(1);
  }
  const includeDir = path.resolve(import.meta.dirname, '../include');
  const hppFiles = await walkDir(includeDir);
  let addedCount = 0;
  const existingFiles = new Set(db.map((e) => e.file.replaceAll('\\', '/')));

  for (const hpp of hppFiles) {
    if (!existingFiles.has(hpp)) {
      db.push({...baseEntry, file : hpp});
      addedCount++;
    }
  }

  await fs.writeFile(dbPath, JSON.stringify(db, null, 2));
  console.log(`[TS Patch] Patched compile_commands.json: added ${
      addedCount} orphaned headers.`);

} catch (error) {
  console.error("Failed to process compile_commands.json:", error);
  exit(1);
}