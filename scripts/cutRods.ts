type CutPattern = {
  originalLength?: number; // original length of the rod before cuts
  cuts: number[]; // lengths of pieces cut from the rod
  remainder: number; // waste length from the rod after cuts
};

/**
 * Brute force program to find the optimal way to cut m rods of variable lengths into n pieces of given lengths, maximising the remaining length squared.
 * That is, for each rod, we want to cut pieces such that the sum of the squares of the remaining lengths (waste) is maximized.
 * Tractable for small m and n only: m^n <= 1B. Speed is ~5M/s.
 *
 * @param rodLengths - Array of lengths of the rods available.
 * @param pieceLengths - Array of lengths of pieces to cut from the rods.
 * @returns Array of CutPattern objects representing the cuts made and waste for each rod.
 */
export function cutRods(
  rodLengths: number[],
  pieceLengths: number[]
): CutPattern[] | null {
  const m = rodLengths.length;
  const n = pieceLengths.length;

  let bestReward = -Infinity;
  let bestCutPatternSet: CutPattern[] | null = null;
  let currentCutPatternId = new Array(n).fill(0);

  while (true) {
    const cutPatternSet = idToCutPatternSet(
      currentCutPatternId,
      rodLengths,
      pieceLengths
    );
    if (cutPatternSet) {
      const reward = getReward(cutPatternSet);
      if (reward > bestReward) {
        bestReward = reward;
        bestCutPatternSet = cutPatternSet;
      }
    }

    // Increment the cut pattern id
    if (incrementId(currentCutPatternId, m)) {
      break; // Overflow occurred (finished enumeration), exit loop
    }
  }

  return bestCutPatternSet;
}

// Assign pieces to rods based on the id array.
function idToCutPatternSet(
  id: number[],
  rodLengths: number[],
  pieceLengths: number[]
): CutPattern[] | null {
  const cutPatterns: CutPattern[] = [];
  for (let i = 0; i < rodLengths.length; i++) {
    // Initialise cut patterns from rod lengths
    cutPatterns.push({ cuts: [], remainder: rodLengths[i] });
  }
  for (let j = 0; j < id.length; j++) {
    // Assign pieces to rods based on id
    const pieceLength = pieceLengths[j];
    const rodIndex = id[j];
    cutPatterns[rodIndex].cuts.push(pieceLength);
    cutPatterns[rodIndex].remainder -= pieceLength;
    if (cutPatterns[rodIndex].remainder < 0) {
      return null; // Invalid pattern
    }
  }
  return cutPatterns;
}

// Get the sum of squares reward for a set of cut patterns.
function getReward(cutPatterns: CutPattern[]): number {
  return cutPatterns.reduce((acc, pattern) => acc + pattern.remainder ** 2, 0);
}

// Increment the id array representing the current cut pattern. Return true if overflow occurs, false otherwise.
function incrementId(id: number[], base: number): boolean {
  let position = id.length - 1;
  while (position >= 0) {
    if (id[position] < base - 1) {
      id[position]++;
      return false; // No overflow
    } else {
      id[position] = 0;
      position--;
    }
  }
  return true; // Overflow occurred
}

// Helper function to test tractability by generating n random floats within [0, max)
function randomFloats(n: number, max: number): number[] {
  return Array.from({ length: n }, () => Math.random() * max);
}

// Make printing prettier
function arrayTo2dp(arr: number[]): number[] {
  return arr.map((x) => parseFloat(x.toFixed(2)));
}

// Create 5 rods and 13 pieces with random lengths for testing
const rods: number[] = [311.5, 364.5, 391.5, 453.5, ];
// const pieces: number[] = [91, 73, 48, 48, 137, 48, 48, 91.5, 44.5, 87.5, 227];
const pieces: number[] = [227, 90.5, 45, 88, 48, 48, 48, 48, 137, 74, 91.5];

// Initialisation
const rodsSorted = rods.sort((a, b) => a - b);
const piecesSorted = pieces.sort((a, b) => a - b);
console.log("Rods:", arrayTo2dp(rodsSorted));
console.log("Pieces:", arrayTo2dp(piecesSorted));
const startTime = Date.now();

const solution = cutRods(rodsSorted, piecesSorted);
if (solution) {
  solution.forEach((pattern, i) => {
    pattern.cuts = arrayTo2dp(pattern.cuts);
    pattern.remainder = parseFloat(pattern.remainder.toFixed(2));
    pattern.originalLength = pattern.cuts.reduce((acc, cut) => acc + cut, pattern.remainder);
  });
  console.log("Solution:", solution);
} else {
  console.log("No valid cutting pattern exists.");
}

const endTime = Date.now();
console.log(`Time taken: ${(endTime - startTime) / 1000} seconds`);
