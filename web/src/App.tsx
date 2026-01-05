import { Main, TopNav, TopNavStart, Root } from "@atlaskit/navigation-system";
import { TestCompiled } from "./components/Test";

export default function App() {
  return (
    <Root>
      <TopNav>
        <TopNavStart>North Dashboard</TopNavStart>
      </TopNav>
      <Main>
        <TestCompiled />
      </Main>
    </Root>
  );
}
