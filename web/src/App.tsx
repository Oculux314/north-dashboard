import { Main, Root } from "@atlaskit/navigation-system";
import { Nav } from "./components/Nav";
import __noop from "@atlaskit/ds-lib/noop";
import { Box, Flex, Grid } from "@atlaskit/primitives/compiled";
import { cssMap } from "@compiled/react";
import { Graph } from "./components/Graph";
import { Stats } from "./components/Stats";
import { Controls } from "./components/Controls";

export default function App() {
  return (
    <Root>
      <Nav />
      <Main>
        <Grid xcss={styles.grid}>
          <div />
          <Box as="main" xcss={styles.main}>
            <Flex direction="column" xcss={styles.vFlex} gap="space.200">
              <Controls />
              <Flex xcss={styles.hFlex} gap="space.200">
                <Graph />
                <Stats />
              </Flex>
            </Flex>
          </Box>
        </Grid>
      </Main>
    </Root>
  );
}

const styles = cssMap({
  grid: {
    gridTemplateColumns: "1fr min(92vw, 64rem) 1fr",
    height: "100%",
    paddingBlock: "3vw",
  },
  main: {
    height: "100%",
  },
  vFlex: {
    height: "100%",
  },
  hFlex: {
    flex: 1,
  },
});
