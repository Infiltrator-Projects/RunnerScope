# GitHub sign-in registration

Runner Monitor uses GitHub's native OAuth device flow. Users choose **Sign in with GitHub**, approve a one-time code in their browser, and the application saves the resulting session in its existing owner-restricted settings. No GitHub CLI, client secret, password entry or manually copied personal token is used.

## Setup inside the application

Open **Settings → GitHub setup**. The setup window contains the complete registration instructions, an **Open GitHub registration** button/link, and a **Public Client ID** field. **Sign in with GitHub** opens this setup automatically if no Client ID has been supplied. After registration, enter the Client ID in that same window and choose **Save and sign in**. No rebuild or return to a support chat is required. Existing project Client IDs can be reused; builds with a supplied project ID bypass registration.

## Optional shared project setup

The project owner registers one OAuth app at https://github.com/settings/applications/new:

| Field | Value |
| --- | --- |
| Application name | Runner Monitor |
| Homepage URL | https://github.com/Infiltrator-Projects/RunnerScope |
| Description | Native desktop monitor for GitHub Actions self-hosted runners |
| Authorization callback URL | http://127.0.0.1/ |
| Enable Device Flow | Enabled |

The callback is required by the registration form but is not used by device flow. Copy the **public Client ID** into `packaging/github-oauth-client-id.txt`. Never add a client secret or a personal access token to the repository. A shared build configuration lets users bypass this setup. Alternatively the application saves a locally entered public Client ID in its existing private settings. Local builds can override it with `-DRUNNERSCOPE_GITHUB_CLIENT_ID=<public-client-id>`.

Builds without a shared project registration provide the tested in-app setup instead. Offline CI uses an explicitly labelled fixture client and makes no real authorization requests. A successful production sign-in must still be verified with the registered app before release.

## Sessions and access

GitHub's organization runner endpoint requires OAuth `admin:org`; activity in private repositories also requires `repo`. Runner Monitor requests these during device authorization. The server remains the authority for organization membership, SSO and app restrictions. After sign-in the normal runner provider checks access and reports permission failures inside the application.

Existing settings and existing valid sessions are preserved. Cancelling or closing sign-in discards its temporary codes and cannot replace an existing account. Session expiry or revocation asks the user to sign in again in Settings.

## References

- https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/creating-an-oauth-app
- https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow
- https://docs.github.com/en/rest/actions/self-hosted-runners#list-self-hosted-runners-for-an-organization
