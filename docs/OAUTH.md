# GitHub sign-in registration

Runner Monitor uses GitHub's native OAuth device flow. Users choose **Sign in with GitHub**, approve a one-time code in their browser, and the application saves the resulting session in its existing owner-restricted settings. No GitHub CLI, client secret, password entry or manually copied personal token is used.

## One-time project setup

The project owner registers one OAuth app at https://github.com/settings/applications/new:

| Field | Value |
| --- | --- |
| Application name | Runner Monitor |
| Homepage URL | https://github.com/Infiltrator-Projects/RunnerScope |
| Description | Native desktop monitor for GitHub Actions self-hosted runners |
| Authorization callback URL | http://127.0.0.1/ |
| Enable Device Flow | Enabled |

The callback is required by the registration form but is not used by device flow. Copy the **public Client ID** into `packaging/github-oauth-client-id.txt`. Never add a client secret or a personal access token to the repository. This is project configuration, not an end-user setup step. Local builds can override it with `-DRUNNERSCOPE_GITHUB_CLIENT_ID=<public-client-id>`.

The publication workflow refuses to release a build without this project registration. Offline CI uses an explicitly labelled fixture client and makes no real authorization requests. A successful production sign-in must still be verified with the registered app before release.

## Sessions and access

GitHub's organization runner endpoint requires OAuth `admin:org`; activity in private repositories also requires `repo`. Runner Monitor requests these during device authorization. The server remains the authority for organization membership, SSO and app restrictions. After sign-in the normal runner provider checks access and reports permission failures inside the application.

Existing settings and existing valid sessions are preserved. Cancelling or closing sign-in discards its temporary codes and cannot replace an existing account. Session expiry or revocation asks the user to sign in again in Settings.

## References

- https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/creating-an-oauth-app
- https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow
- https://docs.github.com/en/rest/actions/self-hosted-runners#list-self-hosted-runners-for-an-organization
