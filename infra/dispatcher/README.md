# Build dispatcher

"Start Build" in the [web builder](https://tomquist.github.io/esphome-b2500/)
posts the build request here. A static page has nowhere to keep a GitHub token,
so this Lambda function holds the credentials of a GitHub App installed on the
repository and turns the request into a `workflow_dispatch` of
[`build-esphome.yml`](../../.github/workflows/build-esphome.yml).

It replaces `publicactiontrigger.azurewebsites.net`, a third-party service that
did the same for `repository_dispatch` and went down in October 2026
([#330](https://github.com/tomquist/esphome-b2500/issues/330)).

| File | What it is |
| ---- | ---------- |
| [`index.mjs`](./index.mjs) | The handler. No dependencies beyond what the Lambda Node.js runtime ships. |
| [`index.test.mjs`](./index.test.mjs) | `node --test infra/dispatcher/*.test.mjs`; run by the deploy workflow. |
| [`template.yaml`](./template.yaml) | CloudFormation: the function, its function URL with CORS, role and log group. |

## Request

```http
POST <function URL>
Content-Type: application/json

{"identifier": "...", "config": "<base64>", "public_key": "<base64>"}
```

The three fields are the workflow's inputs and nothing else is accepted. Each
is checked against the pattern the workflow checks again before it compiles
anything; `index.test.mjs` reads those patterns out of the workflow so the two
cannot drift apart.

| Answer | Meaning |
| ------ | ------- |
| `202` | The build was dispatched. The page then polls the bucket for its status. |
| `400` | The request is malformed. The body's `error` says which field. |
| `405`, `413` | Not a POST, or a body far larger than any real configuration. |
| `502` | GitHub could not be reached, refused the token, or refused the dispatch. The function's log says which. |

## What the credentials can do

The endpoint is public by design: anyone can start a build, as anyone can open
the page. What matters is what its credentials could do if they leaked.

- The app has a single repository permission, **Actions: read and write**, and
  each token the function mints is narrowed again to that permission on this
  one repository. It can start, cancel and re-run workflows and delete their
  runs and artifacts. It cannot push code, which is why the workflow is started
  with `workflow_dispatch` (which needs `actions: write`) and not
  `repository_dispatch` (which needs `contents: write`).
- The private key sits in an SSM SecureString parameter, readable only by the
  function's role, not in the function's environment.

Anyone can also start builds at will, as they could with the old service. Each
one costs some Actions minutes (free on a public repository) and, at worst,
clobbers a build in flight that reuses its identifier, which
[`../README.md`](../README.md#object-lifetime) covers. The reserved concurrency
in the template caps what a flood costs on the AWS side.

## Setting it up

All of this is done by hand, like the bucket configuration: the IAM user the
build workflow authenticates as should not be able to create functions or
roles. Run it with credentials that can manage CloudFormation, IAM roles,
Lambda, CloudWatch Logs and SSM parameters in `eu-west-1`.

### 1. Create the GitHub App

Under **Settings > Developer settings > GitHub Apps > New GitHub App**:

- **Name**: anything, e.g. `esphome-b2500-builds`. It shows as the actor of
  every build run.
- **Homepage URL**: `https://tomquist.github.io/esphome-b2500/`.
- **Webhook**: untick **Active**. The app receives nothing.
- **Repository permissions**: **Actions: Read and write**. Leave everything
  else at **No access** (Metadata is read-only and mandatory).
- **Where can this GitHub App be installed?** Only on this account.

After creating it, note the **App ID**, then under **Private keys** choose
**Generate a private key** and keep the downloaded `.pem`. Then **Install
App**, choose **Only select repositories** and select `esphome-b2500`.

### 2. Store the private key

```bash
aws ssm put-parameter --region eu-west-1 \
  --name /esphome-b2500/dispatcher/github-app-private-key \
  --type SecureString \
  --value file://path/to/the-app.private-key.pem
```

Then delete the `.pem`. To rotate the key, generate a new one in the app's
settings, run this again with `--overwrite`, delete the old key in the app,
and run step 4 again so the function loads the new key.

### 3. Deploy the stack

```bash
aws cloudformation deploy --region eu-west-1 \
  --stack-name esphome-b2500-dispatcher \
  --template-file infra/dispatcher/template.yaml \
  --capabilities CAPABILITY_IAM \
  --parameter-overrides GitHubAppId=<App ID>
```

The function is named after the stack. If the account's concurrency quota is
too low to reserve from (new accounts start at 10), the deploy fails on
`ReservedConcurrentExecutions`; add `ReservedConcurrency=0` to the overrides.

### 4. Upload the code

The stack creates the function with a placeholder that answers 503, and does
not own the code afterwards, so redeploying the stack never rolls it back. The
code is uploaded separately, and uploaded again whenever `index.mjs` changes:

```bash
(cd infra/dispatcher && zip -q -j /tmp/dispatcher.zip index.mjs)
```

```bash
aws lambda update-function-code --region eu-west-1 \
  --function-name esphome-b2500-dispatcher \
  --zip-file fileb:///tmp/dispatcher.zip
```

### 5. Point the page at it

```bash
DISPATCH_URL=$(aws cloudformation describe-stacks --region eu-west-1 \
  --stack-name esphome-b2500-dispatcher \
  --query "Stacks[0].Outputs[?OutputKey=='DispatchUrl'].OutputValue" \
  --output text)
```

```bash
echo "$DISPATCH_URL"
```

Set that URL as the repository variable **`BUILD_DISPATCH_URL`** under
**Settings > Secrets and variables > Actions > Variables**. The deploy workflow
bakes it into the page as `REACT_APP_BUILD_DISPATCH_URL`. Until it is set, the
page says builds are not configured instead of failing on "Start Build". It
takes effect at the next deploy of `main`.

### 6. Check it

With `DISPATCH_URL` from step 5 still set, a malformed request should be
refused by the function itself, with the CORS header the page needs:

```bash
curl -sS -i -X POST "$DISPATCH_URL" \
  -H 'Origin: https://tomquist.github.io' \
  -H 'Content-Type: application/json' \
  -d '{}'
```

That should answer `400` with `{"error":"Invalid build identifier"}` and an
`access-control-allow-origin: https://tomquist.github.io` header. A `503` means
step 4 is missing, and a `403` means the function URL permissions are.

A real build can only be dispatched once the `workflow_dispatch` trigger is on
`main`, because GitHub only dispatches workflows that are on the default
branch. After that, start one from the page and watch it appear under
**Actions** with the app as its actor. If it does not, the function's log
group `/aws/lambda/esphome-b2500-dispatcher` says why.

### 7. Remove the old app

The workflow no longer listens for `repository_dispatch`, so the
**public-action-trigger** app has nothing left to do here. Uninstall it under
**Settings > GitHub Apps** on the repository.
