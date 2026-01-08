import {
  TopNav,
  TopNavStart,
  TopNavMiddle,
  CustomLogo,
} from "@atlaskit/navigation-system";
import { CustomTitle } from "@atlaskit/navigation-system/top-nav-items";
import { Flex } from "@atlaskit/primitives/compiled";
import LozengeWrapper from "@atlaskit/lozenge";
import StatusSuccessIcon from "@atlaskit/icon/core/status-success";
import Heading from "@atlaskit/heading";

export function Nav() {
  return (
    <TopNav>
      <TopNavStart>
        <CustomLogo
          label={"North Dashboard"}
          href={"/"}
          logo={Logo}
          icon={Logo}
        />
        <CustomTitle>Energy Monitor</CustomTitle>
      </TopNavStart>
      <TopNavMiddle>
        <LozengeWrapper appearance="success" iconBefore={StatusSuccessIcon}>
          <Flex alignItems="center" gap="space.050">
            <StatusSuccessIcon size="small" label="" />
            Connected
          </Flex>
        </LozengeWrapper>
      </TopNavMiddle>
    </TopNav>
  );
}

function Logo() {
  return (
    <Flex alignItems="center" gap="space.100">
      <img src="/favicon.svg" alt="North Dashboard" />
      <Heading size="small">North Dashboard</Heading>
    </Flex>
  );
}
