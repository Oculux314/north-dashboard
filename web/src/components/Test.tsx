import { styled } from "@compiled/react";

const Box = styled.div`
  color: red;
  background-color: blue;
`;

export function TestCompiled() {
  return <Box>Compiled Works?</Box>;
}
