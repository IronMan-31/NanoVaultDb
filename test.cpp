    #include<bits/stdc++.h>
    using namespace std;
    int mod = 1e9 + 7;
    
    int count(vector<int>&arr,int target,int i,vector<vector<int>>&dp){
        if(target==0){
            return 0;
        }
        if(i<0) return INT_MAX;
        if(dp[i][target]!=-1){
            return dp[i][target];
        }

        int pick = INT_MAX;
        int notpick  = INT_MAX;
        if(target>=arr[i]){
            int res = count(arr,target-arr[i],i,dp);
            if(res!=INT_MAX){
                pick = 1  + res;
            }
        }

        notpick = count(arr,target,i-1,dp);

        return dp[i][target] = min(pick,notpick);

    }
    
    void solve(){
        int n,x;
        cin>>n>>x;
        vector<int>v;
        for(int i=0;i<n;i++){
            int e;cin>>e;
            v.push_back(e);
        }
        // sort(v.begin(),v.end());
        vector<vector<int>>dp(n,vector<int>(x+1,-1));
        int ans = count(v,x,v.size()-1,dp);
        if(ans==INT_MAX){
            cout<<"-1"<<endl;
            
        }else{

            cout<<ans<<"\n";
        }

    }

    int main(int argc, char const *argv[])
    {
        cin.tie(0);
        ios::sync_with_stdio(false);
        solve();
    }
