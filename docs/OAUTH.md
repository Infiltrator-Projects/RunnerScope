# GitHub sign-in

Choose **Sign in with GitHub**. RunnerScope first reuses an existing GitHub CLI session if it can access the selected organisation's runners. Otherwise it opens GitHub and copies the one-time code. Paste the code and approve access. Sign-in saves and connects automatically.

No application registration, Client ID, personal token, terminal command, separate helper installation or manual credential copying is required. GitHub may require account sign-in, MFA, SSO or organisation approval.

An existing organisation is retained. On a new setup, a single active organisation administered by the signed-in account is selected automatically. When there are several, the app offers a selection. A manually specified organisation remains supported for delegated runner administrators.

## Sign-in boundary

The release supplies the official, unmodified GitHub CLI 2.102.0 as an isolated sign-in broker. cmake/GitHubSignIn.cmake records its exact official archive URLs and SHA-256 release-asset digests. Linux installs the private helper under libexec; the Windows executable embeds it and extracts it into a new private directory only while needed. Neither platform searches PATH or asks the user to install it.

The browser's authorization identifies **GitHub CLI**, the registered identity of the official broker. RunnerScope does not copy that application's Client ID or secret into its own authentication implementation. It invokes the official program with fixed arguments, without a shell. GitHub CLI's normal scopes plus admin:org provide organisation runner access and private repository activity.

The helper runs with noninteractive input, bounded output, a finite deadline and cancellation. API debugging and environment-token overrides are removed from its environment. Only the pinned one-time-code line is interpreted as a challenge; arbitrary helper output is never displayed as an error or treated as a browser URL.

New authorization uses a fresh owner-restricted temporary configuration. The resulting session is transferred automatically into RunnerScope's existing private settings, and the temporary files are removed. Other GitHub CLI accounts, git configuration and credential-store entries are left intact. Reading an existing session never changes it.

RunnerScope still makes every monitoring request directly through libcurl or WinHTTP. Its provider, services, UI and configuration remain native C. The broker is used only for sign-in and the version probe.

## Verification

Core tests cover session reuse, permission failures, token validation, cancellation and organisation discovery. Native process tests run an offline executable through the production process adapter, including fragmented challenge output, private environment, cancellation and malformed output. The installed GTK test proves automatic clipboard copy, removal of registration/token controls, organisation selection and automatic save. The native self-test launches the actual supplied broker with --version.

Offline qualification does not approve a real GitHub account or prove its organisation permissions. Those remain GitHub-controlled account actions.

## References

- https://cli.github.com/manual/gh_auth_login
- https://cli.github.com/manual/gh_auth_token
- https://github.com/cli/cli/releases/tag/v2.102.0
- https://github.com/cli/cli/blob/v2.102.0/internal/authflow/flow.go
- https://docs.github.com/en/rest/actions/self-hosted-runners#list-self-hosted-runners-for-an-organization
